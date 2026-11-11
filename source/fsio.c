#include "fsio.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define CHUNK (256 * 1024)
#define ROOT "sdmc:/3ds/HomeTidy"

Result fsio_open(HomeFs *fs, const Region *region)
{
    memset(fs, 0, sizeof *fs);
    u32 ext_data[3] = {MEDIATYPE_SD, region->ext_id, 0};
    Result rc = FSUSER_OpenArchive(&fs->ext, ARCHIVE_EXTDATA, (FS_Path){PATH_BINARY, sizeof ext_data, ext_data});
    if (R_FAILED(rc)) return rc;
    fs->ext_open = true;
    u32 sys_data[2] = {MEDIATYPE_NAND, region->sys_id};
    rc = FSUSER_OpenArchive(&fs->sys, ARCHIVE_SYSTEM_SAVEDATA, (FS_Path){PATH_BINARY, sizeof sys_data, sys_data});
    if (R_FAILED(rc)) {
        fsio_close(fs);
        return rc;
    }
    fs->sys_open = true;
    return 0;
}

void fsio_close(HomeFs *fs)
{
    if (fs->ext_open) FSUSER_CloseArchive(fs->ext);
    if (fs->sys_open) FSUSER_CloseArchive(fs->sys);
    fs->ext_open = fs->sys_open = false;
}

Result fsio_read_at(FS_Archive ar, const char *path, uint64_t off, uint8_t *buf, size_t len)
{
    Handle fh;
    Result rc = FSUSER_OpenFile(&fh, ar, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
    if (R_FAILED(rc)) return rc;
    for (size_t done = 0; done < len;) {
        u32 got = 0, want = len - done > CHUNK ? CHUNK : (u32)(len - done);
        rc = FSFILE_Read(fh, &got, off + done, buf + done, want);
        if (R_FAILED(rc)) break;
        if (got == 0) { rc = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_OUT_OF_RANGE); break; }
        done += got;
    }
    FSFILE_Close(fh);
    return rc;
}

#define BLOCK 4096

Result fsio_read_all(FS_Archive ar, const char *path, uint8_t **buf, size_t *len, size_t *bad)
{
    Handle fh;
    Result rc = FSUSER_OpenFile(&fh, ar, fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
    if (R_FAILED(rc)) return rc;
    u64 size = 0;
    rc = FSFILE_GetSize(fh, &size);
    if (R_FAILED(rc)) { FSFILE_Close(fh); return rc; }
    uint8_t *b = calloc(size ? (size_t)size : 1, 1);
    if (!b) { FSFILE_Close(fh); return MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY); }

    size_t failed = 0;
    for (u64 off = 0; off < size;) {
        u32 want = size - off > CHUNK ? CHUNK : (u32)(size - off), got = 0;
        if (R_SUCCEEDED(FSFILE_Read(fh, &got, off, b + off, want)) && got == want) {
            off += want;
            continue;
        }
        for (u32 k = 0; k < want; k += BLOCK) { /* the big read failed: find which blocks are bad */
            u32 n = want - k > BLOCK ? BLOCK : want - k;
            got = 0;
            if (R_FAILED(FSFILE_Read(fh, &got, off + k, b + off + k, n)) || got != n) {
                memset(b + off + k, 0, n);
                failed++;
            }
        }
        off += want;
    }
    FSFILE_Close(fh);
    if (bad) *bad = failed;
    if (failed && !bad) {
        free(b);
        return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_OUT_OF_RANGE);
    }
    *buf = b;
    *len = (size_t)size;
    return 0;
}

Result fsio_write_at(FS_Archive ar, const char *path, uint64_t off, const uint8_t *data, size_t len)
{
    Handle fh;
    Result rc = FSUSER_OpenFile(&fh, ar, fsMakePath(PATH_ASCII, path), FS_OPEN_READ | FS_OPEN_WRITE, 0);
    if (R_FAILED(rc)) return rc;
    for (size_t done = 0; done < len;) {
        u32 put = 0, want = len - done > CHUNK ? CHUNK : (u32)(len - done);
        rc = FSFILE_Write(fh, &put, off + done, data + done, want, FS_WRITE_FLUSH);
        if (R_FAILED(rc)) break;
        if (put != want) { rc = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_OUT_OF_RANGE); break; }
        done += put;
    }
    FSFILE_Close(fh);
    return rc;
}

Result fsio_title_icons(uint64_t tid, uint8_t *out)
{
    static uint8_t smdh[SMDH_SIZE]; /* off the small 3DS stack */
    static const u32 file[5] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369 /* "icon" */, 0x00000000};
    Result rc = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
    static const u32 medias[2] = {MEDIATYPE_SD, MEDIATYPE_NAND};
    for (int m = 0; m < 2; m++) {
        u32 media = medias[m];
        u32 arch[4] = {(u32)tid, (u32)(tid >> 32), media, 0};
        Handle fh;
        rc = FSUSER_OpenFileDirectly(&fh, ARCHIVE_SAVEDATA_AND_CONTENT, (FS_Path){PATH_BINARY, sizeof arch, arch},
                                     (FS_Path){PATH_BINARY, sizeof file, file}, FS_OPEN_READ, 0);
        if (R_FAILED(rc)) continue;
        u32 got = 0;
        rc = FSFILE_Read(fh, &got, 0, smdh, SMDH_SIZE);
        FSFILE_Close(fh);
        if (R_SUCCEEDED(rc) && got == SMDH_SIZE && smdh_valid(smdh)) {
            memcpy(out, smdh + SMDH_ICON_SMALL, SMDH_ICONS_LEN);
            return 0;
        }
        rc = MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NO_DATA);
    }
    return rc;
}

Result fsio_commit(FS_Archive ar) { return FSUSER_ControlArchive(ar, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0); }

static void mkdirs(const char *path)
{
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = strchr(tmp, '/'); p; p = strchr(p + 1, '/')) {
        if (p == tmp || p[-1] == ':') continue;
        *p = '\0';
        mkdir(tmp, 0777);
        *p = '/';
    }
    mkdir(tmp, 0777);
}

bool sd_write(const char *path, const uint8_t *data, size_t len)
{
    char dir[256];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdirs(dir); }
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

uint8_t *sd_read(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n >= 0 ? malloc(n ? (size_t)n : 1) : NULL;
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) *len = (size_t)n;
    return b;
}

bool sd_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool backup_dir_new(char *out, size_t cap)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", t);
    snprintf(out, cap, ROOT "/backup/%s", stamp);
    mkdirs(out);
    return sd_exists(out);
}

bool backup_dir_latest(char *out, size_t cap)
{
    DIR *d = opendir(ROOT "/backup");
    if (!d) return false;
    char best[64] = "";
    for (struct dirent *e; (e = readdir(d));) {
        if (e->d_name[0] == '.') continue;
        if (strcmp(e->d_name, best) > 0) snprintf(best, sizeof best, "%.63s", e->d_name);
    }
    closedir(d);
    if (!best[0]) return false;
    snprintf(out, cap, ROOT "/backup/%s", best);
    return true;
}

/** @file i18n.h
 *  UI strings (US English).
 */
#ifndef HT_I18N_H
#define HT_I18N_H

/* X(id, text) */
#define HT_STRINGS(X) \
    X(TAGLINE, "Sort the HOME Menu, check off finished games") \
    X(MENU_SORT, "Sort HOME Menu") \
    X(MENU_SORT_SUB, "Alphabetical, folders last") \
    X(MENU_MARK, "Green check on a game") \
    X(MENU_MARK_SUB, "Mark or unmark finished games") \
    X(HINT_MENU, "A  Select      START  Exit") \
    X(LOADING, "Reading HOME data...") \
    X(INFO_HOME, "%d titles in %d folders") \
    X(INFO_NAMES, "%d names read from the icon cache") \
    X(INFO_UNNAMED, "%d titles without a name stay in front") \
    X(SORT_BLOCKED, "%d names could not be read, so sorting is off.") \
    X(SORT_TITLE, "Sort HOME Menu") \
    X(SORT_CHANGES, "%d positions change") \
    X(SORT_SORTED, "Already sorted") \
    X(SORT_NOTE, "A backup goes to the SD card first.") \
    X(BTN_APPLY, "Apply") \
    X(BTN_BACK, "Back") \
    X(BTN_RESTORE, "Restore last backup") \
    X(BTN_REBOOT, "Reboot now") \
    X(BTN_YES, "Yes") \
    X(BTN_NO, "No") \
    X(CONFIRM_SORT, "Write the new order?") \
    X(CONFIRM_RESTORE, "Restore the HOME layout from the last backup?") \
    X(WORK_BACKUP, "Backing up...") \
    X(WORK_WRITE, "Writing...") \
    X(WORK_VERIFY, "Checking...") \
    X(DONE_SORT, "New order saved") \
    X(DONE_MARK, "Icons updated") \
    X(DONE_RESTORE, "Backup restored") \
    X(DONE_BACKUP_AT, "Backup: %s") \
    X(DONE_NOHOME, "Do not press HOME: it would overwrite the change.") \
    X(DONE_REBOOT, "Reboot now, or hold power and choose Reboot (Luma).") \
    X(DONE_LEAVE, "B  Leave without rebooting") \
    X(REBOOTING, "Rebooting...") \
    X(REBOOT_FAIL, "Reboot failed (%08lX). Hold power and choose Reboot.") \
    X(MARK_TITLE, "Green check") \
    X(MARK_HINT, "A  Mark/unmark    X  Games/All    B  Back") \
    X(MARK_GAMES, "in folders")\
    X(MARK_ALL, "all")\
    X(MARK_ON, "Marked") \
    X(MARK_OFF, "Not marked") \
    X(MARK_ORIGINAL, "Original") \
    X(MARK_BEFORE, "Now") \
    X(MARK_AFTER, "With check") \
    X(MARK_NO_ORIGINAL, "Could not find the original icon of this title.") \
    X(MARK_EMPTY, "No games found in the icon cache.") \
    X(ERR_TITLE, "Something went wrong") \
    X(ERR_OPEN, "Could not open the HOME Menu saves (%08lX).") \
    X(ERR_OPEN_HINT, "Install the %s build of HomeTidy with FBI (needs Luma3DS).") \
    X(ERR_READ, "Could not read %s (%08lX).") \
    X(ERR_FORMAT, "Unexpected HOME file layout (not a HOME Menu?).") \
    X(ERR_BACKUP, "Could not write the backup to the SD card.") \
    X(ERR_WRITE, "Write failed (%08lX). The original was put back.") \
    X(ERR_VERIFY, "Check failed: what was read back differs. The original was put back.") \
    X(ERR_NOBACKUP, "No backup found on the SD card.") \
    X(ERR_BAD_BACKUP, "The backup files look wrong. Nothing was written.") \
    X(ERR_MEMORY, "Out of memory.") \
    X(HINT_BACK, "A / B  Back")

typedef enum {
#define X(id, text) S_##id,
    HT_STRINGS(X)
#undef X
    S_COUNT
} StrId;

/** Text for an id in the current language. */
const char *tr(StrId id);

#endif

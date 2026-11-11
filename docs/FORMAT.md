# Files HomeTidy reads and writes

All numbers are little endian. Offsets for the layout files follow [HomeMenuEditor3DS](https://github.com/mrissaoussama/HomeMenuEditor3DS).

## Where the files live

| Region | SD extdata | System save (NAND) |
| --- | --- | --- |
| JPN | `0x82` | `0x00020082` |
| USA | `0x8F` | `0x0002008F` |
| EUR, AUS | `0x98` | `0x00020098` |
| CHN | `0xA1` | `0x000200A1` |
| KOR | `0xA9` | `0x000200A9` |
| TWN | `0xB1` | `0x000200B1` |

| File | Archive | Size |
| --- | --- | --- |
| `Launcher.dat` | system save | 9560 |
| `SaveData.dat` | extdata | 11680 |
| `Cache.dat` | extdata | 360 x 16 |
| `CacheD.dat` | extdata | 360 x 0x36C0 |

The app opens each archive only while it reads or writes, and closes it right after. HOME can't open its own saves while another process holds them.

## Layout files

360 title slots plus 60 folders.

| | `Launcher.dat` | `SaveData.dat` |
| --- | --- | --- |
| game card slot (u16) | `0x02` | |
| title ids (u64 x 360) | `0x008` | `0x008` |
| positions (s16 x 360) | `0xD9A` | `0xCB0` |
| folder index (s8 x 360) | `0x106A` | `0xF80` |
| folder positions (s16 x 60) | `0x11DC` | |
| folder names (UTF-16, `0x22` bytes x 60) | `0x1560` | |

An empty title slot has the id `0xFFFFFFFFFFFFFFFF`. A position of `-1` means unused, and a folder index of `-1` means the title is at the top level. HomeTidy rewrites only the position arrays.

## Icon cache

Line `i` of `Cache.dat` has the title id in its last 8 bytes, and block `i` of `CacheD.dat` is that title's SMDH:

| Field | Offset in the SMDH |
| --- | --- |
| magic `SMDH` | `0x0000` |
| titles, 16 languages x `0x200` (short title `0x80` bytes, UTF-16) | `0x0008` |
| small icon, 24x24 RGB565 | `0x2040` |
| large icon, 48x48 RGB565 | `0x24C0` |

Icons are stored as 8x8 tiles in Morton order, the same order the GPU uses, so the app uploads them as textures without converting. The check badge is drawn into both icons and nothing else in the SMDH changes.

## Exheader rights

`meta/hometidy.rsf.in` is filled in per region by `scripts/build-cia.sh`. It declares the region's extdata as the app's own extdata so it can be written, the region's system save, and the file system access bits of the HOME Menu and system tools. It also lists `ptm:sysm` for the reboot request.

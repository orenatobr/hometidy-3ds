#!/bin/sh
# Build out/hometidy-<region>.cia from the ELF made by `make` (devkitARM).
# The exheader can name only one extdata and one system save, so there is one CIA per
# region; all share the title id, so installing the right one replaces a wrong one.
# Needs makerom and bannertool on PATH. Run from the repository root.
#
#   ./scripts/build-cia.sh            # all regions
#   ./scripts/build-cia.sh EUR USA    # some regions
set -eu

out=out
mkdir -p "$out"

bannertool makebanner -i meta/banner.png -a meta/audio.wav -o "$out/banner.bnr"
bannertool makesmdh -s "HomeTidy" -l "Sort the HOME Menu, check off finished games" -p "orenatobr" -i meta/icon.png -o "$out/icon.icn"

# region extdata_id system_save_id (HOME Menu of each region)
ids() {
	case "$1" in
	JPN) echo "0x82 0x00020082" ;;
	USA) echo "0x8F 0x0002008F" ;;
	EUR) echo "0x98 0x00020098" ;;
	CHN) echo "0xA1 0x000200A1" ;;
	KOR) echo "0xA9 0x000200A9" ;;
	TWN) echo "0xB1 0x000200B1" ;;
	*) echo "unknown region: $1" >&2; exit 1 ;;
	esac
}

[ $# -gt 0 ] || set -- JPN USA EUR CHN KOR TWN
for region in "$@"; do
	set -- $(ids "$region") # ext sys
	sed -e "s/@REGION@/$region/g" -e "s/@EXT@/$1/g" -e "s/@SYS@/$2/g" meta/hometidy.rsf.in > "$out/hometidy-$region.rsf"
	makerom -f cia -o "$out/hometidy-$region.cia" -elf hometidy.elf -rsf "$out/hometidy-$region.rsf" \
		-icon "$out/icon.icn" -banner "$out/banner.bnr" -exefslogo -target t
	ls -l "$out/hometidy-$region.cia"
done

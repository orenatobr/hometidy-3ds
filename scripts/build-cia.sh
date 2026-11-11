#!/bin/sh
# Build out/hometidy.cia from the ELF made by `make` (devkitARM).
# Needs makerom and bannertool on PATH. Run from the repository root.
set -eu

out=out
mkdir -p "$out"

bannertool makebanner -i meta/banner.png -a meta/audio.wav -o "$out/banner.bnr"
bannertool makesmdh -s "HomeTidy" -l "Sort the HOME Menu, check off finished games" -p "orenatobr" -i meta/icon.png -o "$out/icon.icn"

makerom -f cia -o "$out/hometidy.cia" -elf hometidy.elf -rsf meta/hometidy.rsf \
	-icon "$out/icon.icn" -banner "$out/banner.bnr" -exefslogo -target t

ls -l "$out/hometidy.cia"

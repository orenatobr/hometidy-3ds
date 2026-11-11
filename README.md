# HomeTidy

A 3DS app that does two chores on the HOME Menu, on the console itself:

1. **Sort the HOME Menu.** Titles go in alphabetical order, folders go last, and the titles inside each folder are packed and sorted too.
2. **Put a green check on a game.** Pick a game and a green badge is drawn on its HOME icon, handy for marking what you finished. Pick it again to bring the original icon back.

It replaces the usual routine of dumping `Launcher.dat` and `SaveData.dat` over FTP, sorting them on a computer and writing them back.

## Requirements

- A 3DS or 2DS with [Luma3DS](https://github.com/LumaTeam/Luma3DS). The CIA asks the system for file system rights that only a custom firmware grants.
- Any console region (JPN, USA, EUR/AUS, CHN, KOR, TWN). The app detects the region by itself, but the CIA's exheader can name only one extdata and one system save, so there is one CIA per region. Pick the one for your console.
- [FBI](https://github.com/nh-server/FBI-NH) or any other CIA installer.

## Install

1. Download `hometidy-<region>.cia` for your console from the [latest release](../../releases/latest). Australian consoles use EUR.
2. Copy it to the SD card and install it with FBI. All regions share one title id, so installing the right build replaces a wrong one.
3. Open **HomeTidy** from the HOME Menu.

## Use

### Sort HOME Menu

The app reads the layout, works out the new order and lists it on the top screen. Rows marked with an orange dot change position. **A** applies it.

Before writing anything it:

1. saves the current `Launcher.dat` and `SaveData.dat` to `sdmc:/3ds/HomeTidy/backup/<date-time>/`;
2. writes only the bytes that change;
3. reads both files back and compares them with what it meant to write;
4. puts the original bytes back if any step fails.

HOME keeps its own copy of the layout in memory and writes it over yours when it gets control back. So after a write the app disables the HOME button and offers **Reboot now**, which asks the system to reboot. If you would rather do it by hand, hold power and pick Reboot in the Luma menu.

**Y** restores the most recent backup (after taking a fresh backup of the current layout).

The order is:

1. system apps and the DS launcher, by title id;
2. titles whose name is unknown, by title id;
3. named titles, A to Z, ignoring case, punctuation, tags in `()` and `[]`, and a leading "The" or trailing ", The". Numbers sort by value, so "Zero 2" comes before "Zero 10";
4. folders, A to Z.

The game card slot never moves. Positions already in use at the top level are reused, so the layout works with any grid size and running the app twice gives the same result.

### Green check

Pick a game on the bottom screen. The top screen shows the icon now next to the icon with the check. **A** toggles it. Games already marked show a small check on the list.

The list shows games only, meaning titles that sit inside a HOME folder; apps and homebrew at the top level are hidden. **X** switches between games and all titles. System apps never show.

Unmarking puts the original icon back. It uses the copy HomeTidy saved to `sdmc:/3ds/HomeTidy/icons/<title id>.bin` when it marked the game, and otherwise reads the icon from the title itself, so checks drawn by other tools can be undone too. Reboot afterwards to see the change.

### Names

Titles are named from the HOME icon cache (`Cache.dat` and `CacheD.dat`), the same names HOME shows, including homebrew and injects. Every name seen is also kept in `sdmc:/3ds/HomeTidy/names.txt` (`<title id><TAB><name>`), which fills the gaps if part of the cache can't be read on a later run. You can edit that file to rename a title for sorting. If a title has no name from either source the sort is switched off rather than guess.

## Build

You need [devkitPro](https://devkitpro.org) with `devkitARM`, `libctru` and `citro2d`, or just Docker:

```sh
docker run --rm -v "$PWD":/work -w /work devkitpro/devkitarm make
```

That produces `hometidy.3dsx` and `hometidy.elf`. For the CIA, put `makerom` and `bannertool` on your `PATH` and run:

```sh
./scripts/build-cia.sh        # writes out/hometidy-<region>.cia for every region (or name some: EUR USA)
```

### Releasing

The CIA is built by hand, not in CI, because `makerom` and `bannertool` aren't packaged for the CI image:

1. Build the ELF with the Docker command above.
2. Get `makerom` ([3DSGuy/Project_CTR](https://github.com/3DSGuy/Project_CTR), tag `makerom-v0.18.4`) and `bannertool` ([carstene1ns/3ds-bannertool](https://github.com/carstene1ns/3ds-bannertool)) and put them on your `PATH`.
3. Run `./scripts/build-cia.sh`, then `shasum -a 256 out/hometidy-*.cia > out/SHA256SUMS.txt`.
4. Upload `out/hometidy-*.cia` and the sums to a GitHub release.

Tests for the portable core (sorting, layout, icons, safe writes with injected failures) run on the host with a normal C compiler:

```sh
make -C tests test
```

See [docs/FORMAT.md](docs/FORMAT.md) for the file layouts the code relies on.

## Limits

- One CIA per region (see above). If you install the wrong region's build, the app says which one your console needs.
- The layout offsets come from HomeMenuEditor3DS and are checked by file size. I have only run this against an EUR console's files, so the other regions are untested.
- If HOME has changed its files since the app started (for example after you moved an icon without leaving the app), re-open the app before applying.
- This writes to system files. The backup and read-back checks are there for a reason, but you use it at your own risk.

## License

MIT. See [LICENSE](LICENSE).

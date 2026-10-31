# Toy Story 3 Companion

Unofficial 100% companion for **Toy Story 3 Complete Edition** on PC (Steam). The app itself is in English and Spanish.

- 41 Steam achievements, 88 story collectibles, 289 Toy Box capsules with location hints, 103 gold stars and the Pict-O-Matic
- Reads your save (and Steam's achievement cache) to tick things off for you
- The desktop app shows the game's own icons and collectible pictures, read from your installed copy (none are distributed)

## Desktop app (Windows)

1. Download `TS3Companion.exe` from [Releases](../../releases) or Nexus Mods and run it.
2. It opens in its own window and finds your save. If it can't, press **Find save…**; the location is remembered.
3. Press **Sync now**, or leave auto-sync on to sync every time you save in-game.

Options: `--save "path\to\Saved Game"`, `--browser`, `--no-window`.

## Web version

Open the [web version](https://keyloor.github.io/ts3-companion/) and press **Load save…**. The save is read in your browser and never uploaded.

## What syncs

| | |
|---|---|
| Capsules, Pict-O-Matic discs, gold stars | automatic |
| Story collectibles | automatic (cards are ticked per level once the level is complete) |
| Steam achievements | automatic in the desktop app |
| Pict-O-Matic scenes | by hand (the save only stores the total) |

Sync only adds marks; it never unticks anything. Details of the save format are in `app/save.js`.

## Build

```
python tools/build_content.py
python -m PyInstaller desktop/TS3Companion.spec
```

## License

Code: MIT. Our own hints and tips: CC BY-NC 4.0. Game names and achievement texts belong to Disney/Pixar. Fonts: SIL OFL 1.1. Button icons: Kenney (CC0). The desktop app shows game icons read from your own installed copy; none are distributed. Not affiliated with Disney/Pixar. See [CREDITS](app/CREDITS.md).

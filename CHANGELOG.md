# Changelog

## 1.2.0 — 2026-11-02

- New: the Companion inside the game (overlay). Press F2 or D-pad down while playing to open the whole app on top of the game: every tab, search, filters, hide completed and ticking by hand, with the game's own icons and pictures, and the same look as the app: its fonts, its layout and its light and dark themes. Use the mouse (with the game's cursor), the keyboard or the controller. It reads the save by itself each time you open it, and what you tick by hand is shared with the desktop app. Two ways to get it: install it from the desktop app's Settings (In the game), and the app keeps it up to date; or download the in-game version on its own (`TS3Companion-InGame-<version>.zip`) and extract it into the game folder, without the app. It is a dxgi.dll in the game folder (the same way ReShade loads) that only draws: it changes nothing in the game or the save, and it works alongside the TS3 Mod Menu. It can't be used together with ReShade.
- Star Soldier Outfit (ZB2) is now detected from the save: it was matched to the wrong game item. It is `CL_Alien_StarCom2`, the only one of the 46 space items that no capsule used.
- Goo Doors (CP123) and Gothic Girl Outfit (KN4) moved to the Main town, where they are. New hint for KN4: at the bottom of the area where you save Stinky Pete's mule.
- Candy shop: 7 of its 29 capsules (Leaf Boy Hat, Groom Hat and Outfit, Strongman Hair, Red Braids Hair, Blue Curls Wig, Elf Boy Hair) were matched to the wrong game items, so they never ticked from the save. Fixed by comparing the game's own item thumbnails with the descriptions.
- Spaceport: the Zurg Commander, Trooper and Sentry outfits and hats were swapped among themselves.
- Gold stars: Spooky Customizer, Enchanted Customizer and Customizer of the Future (all items of a theme) are now worked out from the save. Six stars still can't be read from the save (they depend on how the town is decorated or on challenge tags) and stay manual: the sync message says when the count differs from the game's.

## 1.1.0 — 2026-10-30

- Desktop app: game icons. Tabs, story collectible types, achievement trophies and capsule categories use the game's own icons, and the unique story collectibles (special cards, figures, memorabilia, Zurg parts, aliens) show their picture. They are read from your installed copy of the game (found through Steam) the first time they are needed and cached; none are shipped with the app. Without the game, or in the web version, the app keeps its own icons.

## 1.0.1 — 2026-10-26

- Sync messages show what was added ("+3 capsules · +1 gold star"). Auto-sync only shows a message when something is new, and also runs when the app opens.
- Army men: the paratrooper targets you have (x/30) appear on the Airborn Ranger achievement and the Parachute King gold star.
- Two windows or tabs open at once no longer overwrite each other's progress. The desktop app keeps a backup of the progress file and recovers from it.
- Clearer error when importing a file that is not a backup. The app window always loads the latest interface after an update.
- Hardening of the local server (link opening and malformed requests).

## 1.0.0 — 2026-10-26

First public release.

- 41 Steam achievements, 88 story collectibles, 289 Toy Box capsules with location hints, 103 gold stars and the Pict-O-Matic (6 discs, 40 scenes). English and Spanish.
- Save sync: capsules, Pict-O-Matic discs, gold stars and story collectibles. The desktop app also reads Steam achievements from Steam's local cache and can sync on its own when you save in-game.
- Desktop app for Windows: its own window, finds the save automatically or lets you pick it (the location is remembered), single instance, quits with the window.
- Web version: load the save file in the browser; nothing is uploaded.
- Settings: language, light/dark/system theme, Xbox or PlayStation button prompts, auto-sync, backup export/import.

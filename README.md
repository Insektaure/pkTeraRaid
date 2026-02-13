# pkTeraRaid - Map Viewer

A Nintendo Switch homebrew application for viewing Tera Raid details in Pokemon Scarlet & Violet.

## Features

- **Dual mode support**
  - **Save file mode** (Title Override): Reads raid data from the game's save file. Supports profile selection and multiple game versions.
  - **Live mode** (Applet/HBMenu overlay): Reads raid data directly from the running game's memory via dmntcht. Auto-detects Scarlet or Violet.
- **Interactive map view** with raid locations for all three regions:
  - Paldea
  - Kitakami (The Teal Mask DLC)
  - Blueberry Academy (The Indigo Disk DLC)
- **Detailed raid information** including:
  - Species with sprite preview
  - Star rating (1-6 stars, including 6-star black raids)
  - Tera type
  - IVs, Nature, Ability
  - Moves
  - Shiny status
  - Gender
  - PID and EC
- **Raid list** with scrollable list panel alongside the map
- **Profile selector** with support for multiple Switch user profiles
- **Game selector** for switching between Scarlet and Violet save data (when available)
- **About screen** with credits and controls

## Compatibility

Scarlet / Violet game version **3.0.1 / 4.0.0 only** !

## Requirements

- Nintendo Switch with [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) custom firmware
- Pokemon Scarlet or Violet (physical or digital)
- **Save file mode**: Launch via title override (hold R while launching a game)
- **Live mode**: Launch from HBMenu overlay (album applet) while the game is running

## Controls

| Button | Action |
|--------|--------|
| D-Pad / Left Stick | Navigate |
| A | Select / View details |
| B | Back / Close details |
| L / R | Switch map tab (Paldea / Kitakami / Blueberry) |
| - | About |
| + | Quit |

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with libnx
- Switch portlibs: `SDL2`, `SDL2_ttf`, `SDL2_image`, `freetype`, `harfbuzz`, `libpng`, `libjpeg`, `libwebp`

Install portlibs:
```bash
sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf switch-sdl2_image switch-freetype switch-harfbuzz
```

### Build

```bash
export DEVKITPRO=/opt/devkitpro
make all
```

This produces `pkTeraRaid.nro`.

### Clean

```bash
make clean
```

## Installation

1. Copy `pkTeraRaid.nro` to `/switch/pkTeraRaid/` on your SD card.
2. Launch from HBMenu:
   - **Save file mode**: Hold R while launching any game to open HBMenu via title override, then select pkTeraRaid.
   - **Live mode**: With Pokemon SV running, open the album to access HBMenu overlay, then select pkTeraRaid.

## Credits

- [PKHeX](https://github.com/kwsch/PKHeX) by kwsch - Save file structure and Pokemon data
- [Tera-Finder](https://github.com/Manu098vm/Tera-Finder) by Manu098vm - Raid encounter logic and data resources
- [RaidCrawler](https://github.com/LegoFigure11/RaidCrawler) by LegoFigure11 - Map coordinate formulas
- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K - Save data access approach
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) - dmntcht for live memory reading

## License

This project is for personal/educational use. It relies on several open-source projects, each with their own licenses. Please refer to the individual projects linked above for their respective license terms.

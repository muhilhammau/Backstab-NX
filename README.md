<div align="center">

<img src="launcher/icon.jpg" alt="Backstab HD" width="180">

# backstab_nx

**Backstab HD on Nintendo Switch**

An unofficial Nintendo Switch wrapper/port of Gameloft's Android game.

[![Switch](https://img.shields.io/badge/Nintendo_Switch-Homebrew-E60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)](#installation)
[![Version](https://img.shields.io/badge/version-v0.1.0-4C8BF5?style=for-the-badge)](#)
[![Android](https://img.shields.io/badge/Android-v1.2.8d-3DDC84?style=for-the-badge&logo=android&logoColor=white)](#about)
[![AArch32](https://img.shields.io/badge/AArch32-32--bit-0091BD?style=for-the-badge&logo=arm&logoColor=white)](#building)

</div>

---

## About

`Backstab_nx` runs the original 32-bit Android version of **Backstab HD** on Nintendo Switch, using the open-source [vita2hos](https://github.com/vita2hos) toolchain and [android32](https://github.com/aks796/android32) runtime.

The supported game build is:

```ext
Package:      com.gameloft.android.GAND.GloftBSBH
Version:      v1.2.8d (latest)
Version code: 128
ABI:          armeabi-v7a / AArch32
```

The original ARM game library runs directly inside a 32-bit Horizon OS process. Players must provide their own APK and game data files; these are not included in the repository or release download.

**No game files are included.** You need your own copy of the game's APK. This project is not affiliated with or endorsed by Gameloft, Ubisoft, or Nintendo.

> Work in progress.

## Status

| Feature | State |
| --- | --- |
| Boot, loading, 3D gameplay | investigating |
| Touchscreen | working ✓ |
| Audio (OpenSL ES via audout) | not implemented |
| Gamepad: cursor (right stick / D-pad), A, + | working ✓ |
| Gamepad: left stick to the on-screen joystick | working ✓ |
| Intro and cutscene videos | skipped (the game is told they finished) |
| Online features | not implemented |
| PVRTC texture decoding | implemented (custom decoder) |

## Features

- Original Android **1.0.0** game running natively in AArch32 on Switch.
- **1280 × 720** widescreen presentation with touchscreen and gamepad integration.
- Dynamic cursor navigation for touch-oriented menus (Right Stick / D-Pad + A button).
- Direct game data mounting from SD card (`external/files`).
- Native checkpoint saves and settings persistence.
- Audio playback via android32 AudioTrack / OpenSL ES driver (WIP).
- Custom **PVRTC 2bpp/4bpp → RGBA8 decoder** (Mesa32 lacks PVRTC support).
- GL shader logging/debug via `Backstab_glslog.c` (samplerCube fix).
- 64-bit NRO launcher carrying the 32-bit payload with sphaira HOME-menu forwarder support.

## Installation

### Requirements

- A Nintendo Switch running Atmosphère custom firmware.
- [sphaira](https://github.com/ITotalJustice/sphaira) for installing the HOME-menu forwarder.
- Your own supported **v1.2.8d (latest) ARMv7 APK** (com.gameloft.android.GAND.GloftBSBH).
- Extracted game data files (GLA archives, saves, etc.).

Copy the `switch` folder to the root of your SD card. Ensure the game files are placed according to the layout below:

```	ext
sd:/switch/backstab_nx/
├── backstab_nx.nro
├── game.apk
├── libPirates.so
├── title_id.txt
├── classes.txt
├── config.ini
├── .setup
├── data/
│   ├── cache/
│   ├── files/
│   └── shared_prefs/
└── external/
    ├── cache/
    └── files/
        ├── actors.gla
        ├── cinematics.gla
        ├── commons.gla
        ├── effects.gla
        ├── game_config.gla
        ├── globalsave0.dat
        ├── globalsaveli0.dat
        ├── intro.mp4
        ├── location_001_britishcityfort.gla
        ├── location_002_spanishcity.gla
        ├── location_003_piratecity.gla
        ├── location_004_jungle_01.gla
        ├── location_005_vulcanocity.gla
        ├── location_007_britishcity.gla
        ├── location_008_openworld.gla
        ├── location_010_islands.gla
        ├── location_011_islands.gla
        ├── location_012_islands.gla
        ├── location_017_jungle_02.gla
        ├── location_018_piratecity2.gla
        ├── location_019_jungle_03.gla
        ├── location_020_britishcity2.gla
        ├── location_900_testfps.gla
        ├── location_901_testfps2.gla
        ├── particlesystems.gla
        ├── save0.dat
        ├── saveli0.dat
        ├── settings.dat
        ├── sounds.xml
        ├── sounds.zip
        ├── sprites_1920x1080.gla
        ├── strings.gla
        └── trophy.dat
```

### First launch

1. Open **sphaira** and locate **Backstab HD** in Homebrew.
2. Select **Install Forwarder**.
3. Launch the game icon from the Switch **HOME menu**.
4. On initial launch, the runtime extracts necessary native libraries from the APK and initializes local configurations.

## Controls

### Gameplay

| Nintendo Switch | Input |
| --- | --- |
| **Left Stick** | Character movement / On-screen joystick |
| **Right Stick / D-Pad** | Move virtual cursor |
| **A** | Tap cursor position / Primary action |
| **B / X / Y** | Context action buttons |
| **+** | Pause / Game menu |
| **Touchscreen** | Native touch controls |

### Cursor / Navigation

- Move the **Right Stick** or **D-Pad** to reveal the yellow selection cursor.
- Press **A** to execute a touch tap at the cursor's location.
- Button positions can be modified by creating `uttons.txt` inside `/switch/backstab_nx/` (refer to `uttons.example.txt`).

## File Structure & Saves

The application directory is located under `sd:/switch/backstab_nx/`:

```	ext
data/              Android internal storage and native saves
external/          Game data (GLA archives, saves, sounds, configs)
game.apk           Game APK (com.gameloft.android.GAND.GloftBSBH)
libPirates.so      Native game library
title_id.txt       Title ID (0x0100000000001040)
classes.txt        Java class mappings
config.ini         Runtime configuration
debug.log          Current execution log
crash.log          Crash diagnostics output
.setup             Setup completion marker
```

## Building

### Requirements

- Docker with Linux container support.
- [vita2hos runtime](https://github.com/vita2hos/vita2hos) in 
`runtime` (Git submodule).
- [libnx32](https://github.com/aks796/libnx32) prefix (version 4.12.0 or newer) — clone next to this repo.
- [mesa32](https://github.com/aks796/mesa32) in `portlibs32`.
- devkitPro `devkita64` for launcher compilation.

### Build steps

```powershell
# Clone libnx32 next to this repo
git clone https://github.com/aks796/libnx32 ../libnx32
cd ../libnx32 && ./build.sh

# Build the 32-bit wrapper (NSP)
cd ../Backstab-NX
./build.sh

# Build the launcher NRO (64-bit)
cd launcher
./build.sh
```

The compiled binaries will be located at:
- `Backstab_nx.nsp` — 32-bit wrapper (installable via DBI/Goldleaf/Tinfoil)
- `launcher/backstab_nx.nro` — 64-bit launcher (runs via HBLoader/sphaira)

## Credits

- [vita2hos](https://github.com/vita2hos) — toolchain, devcontainer, runtime, libnx32.
- [aks796/android32](https://github.com/aks796/android32) — core AArch32 Android runtime.
- [aks796/libnx32](https://github.com/aks796/libnx32) — AArch32 libnx for Switch.
- [aks796/mesa32](https://github.com/aks796/mesa32) — Mesa32 graphics stack for Horizon.
- [tasm2_nx](https://github.com/muhilhammau/tasm2-nx) — documentation and structure reference.
- Gameloft, Ubisoft — original game developers and rights holders.

## License

The port code is licensed under MIT (see `LICENSE`). Game assets and binaries remain the property of their respective owners.

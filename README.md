# Backstab-NX

Nintendo Switch port of **Backstab HD** (mobile) — the Vita/32-bit Android version, running natively on Horizon via the [vita2hos](https://github.com/vita2hos) toolchain.

![Backstab-NX](launcher/icon.jpg)

---

## 📦 Installation

### Option 1: NSP (recommended)
1. Download the latest backstab_nx.nsp from [Releases](https://github.com/muhilhammau/Backstab-NX/releases)
2. Install via [DBI](https://github.com/rashevskyv/dbi), [Goldleaf](https://github.com/XorTroll/Goldleaf), or [Tinfoil](https://tinfoil.io/)
3. Launch from the Home Menu

### Option 2: Launcher NRO (for HBLoader/Sphaira)
1. Download backstab_nx.nro from [Releases](https://github.com/muhilhammau/Backstab-NX/releases)
2. Place in /switch/ on your SD card
3. Launch via HBLoader or create a Sphaira forwarder

> The launcher NRO carries the NSP in its romfs and installs it automatically on first run.

---

## 🎮 Game Info

| Property | Value |
|----------|-------|
| **Title** | Assassin's Creed Pirates |
| **Original Platform** | Android / iOS / PS Vita |
| **Genre** | Action-Adventure / Open World |
| **Developer** | Gameloft |
| **Port Author** | muhilhammau |
| **Port Status** | Playable (WIP) |

---

## ⚙️ Building from Source

### Prerequisites
- Docker (for the vita2hos toolchain container)
- libnx32 (patched 32-bit libnx) — clone next to this repo:
  \\\ash
  git clone https://github.com/aks796/libnx32 ../libnx32
  cd ../libnx32 && ./build.sh
  \\\
- portlibs32 (Mesa32 for Switch) — included as submodule or download release

### Build the 32-bit wrapper (NSP)
\\\ash
# From the port root
./build.sh
\\\
Output: backstab_nx.nsp + backstab_nx.build

### Build the launcher NRO (64-bit)
\\\ash
cd launcher
./build.sh
\\\
Output: backstab_nx.nro (carries the NSP in romfs)

> Requires the wrapper NSP to be built first.

---

## 🏗️ Project Structure

\\\
Backstab-NX/
├── source/              # Port-specific C sources
│   ├── backstab_assets.c    # Asset loading (GLA/PVR)
│   ├── backstab_boot.c      # Game entry, JNI bootstrap
│   ├── backstab_config.c    # Device/config handling
│   ├── backstab_exc.c       # Exception handler
│   ├── backstab_gl.c        # GL wrapper (GLES1 → GLES2)
│   ├── backstab_glslog.c    # GL shader logging/debug
│   ├── backstab_java.c      # JNI ↔ Java bridge
│   ├── backstab_loader.c    # APK/DEX loading
│   ├── backstab_pvrtc.c     # PVRTC → RGBA8 decoder (Mesa lacks PVRTC)
│   ├── backstab_shims.c     # Platform shims (pthread, fs, etc.)
│   ├── imports.c            # Generated JNI stubs
│   ├── pvrtc_decode.h       # PVRTC decoder header
│   └── port_config.h        # Port metadata (title, version, paths)
├── launcher/            # 64-bit launcher NRO (HBLoader/Sphaira)
│   ├── build.sh
│   ├── Makefile
│   ├── source/main.c
│   └── romfs/             # NSP payload lives here at build time
├── runtime/             # vita2hos runtime (git submodule)
├── portlibs32/          # Mesa32 libraries (git submodule)
├── tools/               # Reverse-engineering / build helpers
├── Makefile             # Port Makefile (includes runtime/runtime.mk)
├── build.sh             # Wrapper build script (docker)
└── backstab_nx.build    # Build timestamp
\\\

---

## 🔧 Technical Details

### Renderer
- **Mesa32 (Nouveau)** via portlibs32 — GLES 1.1 → GLES 2.0 translation
- Null renderer fallback: DCR_GL_MESA=0 ./build.sh

### Texture Handling
- Game uses **PVRTC** (PowerVR) compressed textures (.pvr in .gla archives)
- Mesa on Switch lacks PVRTC support → custom decoder in backstab_pvrtc.c
- Decodes PVRTC 2bpp/4bpp → RGBA8 at load time via glCompressedTexImage2D hook

### JNI Bridge
- Java classes reimplemented in C (backstab_java.c)
- Generated stubs in imports.c / generated_jni_stubs.c
- Uses FalsoJNI logger for debugging

### Asset Pipeline
- .gla archives contain .bsprite + .pvr (no .tga fallback)
- sprites_1920x1080.gla → splash screen, UI, etc.
- Loader in backstab_assets.c / backstab_loader.c

---

## 🐛 Known Issues

| Issue | Status | Notes |
|-------|--------|-------|
| Black screen on launch | Investigating | Likely texture format / shader sampler |
| PVRTC decoder validation | Pending | Need to verify t1.bmp / t2.bmp output |
| Audio (OpenSL ES) | Not implemented | Stubbed in opensles.c |
| Save/load | Untested | Uses runtime rt_apkfind.c paths |

---

## 📋 Credits

- **vita2hos toolchain** — [vita2hos](https://github.com/vita2hos) (devcontainer, runtime, libnx32)
- **libnx32** — [aks796/libnx32](https://github.com/aks796/libnx32) (AArch32 libnx)
- **Mesa32** — Mesa port for Horizon (Nouveau backend)
- **FalsoJNI** — JNI logging/debug framework
- **Original game** — Gameloft (Assassin's Creed Pirates)

---

## 📄 License

This port is for educational/research purposes only.  
Original game assets © Gameloft / Ubisoft.  
Port code MIT licensed (see runtime/launcher licenses).

---

## 🔗 Links

- **Releases**: [github.com/muhilhammau/Backstab-NX/releases](https://github.com/muhilhammau/Backstab-NX/releases)
- **Issues**: [github.com/muhilhammau/Backstab-NX/issues](https://github.com/muhilhammau/Backstab-NX/issues)
- **vita2hos Discord**: [discord.gg/vita2hos](https://discord.gg/vita2hos)

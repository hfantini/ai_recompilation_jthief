# Jewel Thief — a faithful native port

A native reimplementation of **Jewel Thief** (Paul Ligeski / ServantWare,
1991–1993), the 16-bit Windows 3.x game where the mouse cursor is a thief
stealing jewels from guards. Rebuilt in C99 from a reverse engineering of
the original, it plays like the original, down to its formulas, its pace and
its quirks, built for every Windows from XP to 11, for Linux, for the
browser and for Android.

The repository holds **no original code or assets**. The game loads its
graphics, cursors, menu, dialogs and texts at runtime from your own copy of
the original `JWLTHIEF.EXE` v1.4.

## Highlights

- **Same rules, same numbers.** Guard counts and speeds, jewel placement,
  scoring, extra lives and the order of every `rand()` call are transcribed
  from the original code, including the Microsoft C random number generator.
- **Same pace.** The game ticks every 54.9 ms, the Windows 3.1 timer rate the
  original actually ran at, also while dialogs are open.
- **Same look.** The seven scenes are redrawn from the original's shapes and
  coordinates, and sprites get the colors they had in 256 colors on any
  display, without the original's high-color glitch.
- **Same dialogs and menu on Windows.** They are built from the original's
  own Windows 3.x templates, converted to Win32 on the fly. The Linux build
  uses native GTK 4 menus and dialogs instead, the browser build HTML ones
  and the Android build Android ones, still with the original's texts; only
  their gameplay is kept faithful.
- **One game everywhere.** The browser and Android builds run the same C
  code, compiled to WebAssembly and for the NDK, so they play exactly like
  the desktop ones.
- **Compatible settings.** Options and high scores live in the original's
  `SERVANT.INI`, in its own format, next to `JWLTHIEF.EXE`. The browser keeps
  the same keys and values in its local storage.

How the original works and what the port does about it is documented in
[`docs/research.md`](docs/research.md).

## Playing

1. Get the original **Jewel Thief v1.4**; it ships in `JWLTHF14.ZIP`. The
   executable is recognized by its SHA-256, whatever its name. See
   [`original/`](original/README.md).
2. Put `jthief_32.exe` or `jthief_64.exe` in the folder that holds
   `JWLTHIEF.EXE`, and run it.

The folder can also be given on the command line:

```sh
jthief_64.exe [--debug] [folder of JWLTHIEF.EXE]
```

| Executable      | Built for                     |
|-----------------|-------------------------------|
| `jthief_32.exe` | Windows XP to Windows 11      |
| `jthief_64.exe` | 64-bit Windows                |
| `jthief`        | Linux, with GTK 4             |
| `app-debug.apk` | Android 8.0 and newer         |

### In the browser

Open the page and pick or drop your `JWLTHIEF.EXE`. It is checked and read
in the browser, never uploaded, and kept in the browser's storage so it is
asked for only once. The game is shown at the largest whole number of
screen pixels per game pixel, up to two, so it stays sharp at any zoom.

### On Android

The first start asks for your `JWLTHIEF.EXE` and copies it into the app,
where `SERVANT.INI` is kept too. The screen works as a trackpad: drag
anywhere and the thief moves as far as your finger, so it never jumps and
stays put when you lift your finger, like a mouse let go. Back pauses, and
the pause box holds the menu.

### Controls

| Input | Action |
|-------|--------|
| Mouse | Move the thief; touch a jewel to take it |
| F2    | New game |
| F3    | Pause |

### Debug mode

`--debug`, or `?debug` in the browser's address, turns on keys the original
never had, and the title bar shows `[DEBUG]` while it is on. On Android, the
app's `debug` extra adds the same commands to the pause box:

```sh
adb shell am start -n io.github.hfantini.jthief/.MainActivity --ez debug true
```


| Key                     | Action |
|-------------------------|--------|
| Home                    | Toggle god mode: guards and leaving the field cost no lives |
| Page Up / Page Down     | Next / previous level |

## Building

The Windows executables are cross-compiled with MinGW-w64, and the Linux
one needs GTK 4; all need CMake 3.21 or newer. On Fedora:

```sh
sudo dnf install cmake mingw32-gcc mingw64-gcc   # wine, to run the builds on Linux
sudo dnf install gcc gtk4-devel                  # the Linux build
sudo dnf install nodejs                          # the browser build, with emsdk below
```

The browser build compiles the game with [Emscripten](https://emscripten.org/),
installed with its SDK, which the `wasm` preset finds through `$EMSDK`:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh                      # in each shell, or in ~/.bashrc
```

Each preset builds in `build/<preset>/` and puts the game in
`bin/<config>/`:

| Preset          | Output                       |
|-----------------|------------------------------|
| `win32-release` | `bin/release/jthief_32.exe`  |
| `win64-release` | `bin/release/jthief_64.exe`  |
| `win32-debug`   | `bin/debug/jthief_32.exe`    |
| `win64-debug`   | `bin/debug/jthief_64.exe`    |
| `linux`         | `bin/debug/jthief` and tests |
| `wasm`          | `build/wasm/jthief.js` and `jthief.wasm`, for the browser build |

```sh
cmake --preset win64-release
cmake --build --preset win64-release
wine bin/release/jthief_64.exe original/

cmake --preset linux
cmake --build --preset linux
bin/debug/jthief original/
```

The browser build is driven by npm from the repository's root: `npm run wasm`
runs the `wasm` preset, `npm run dev` serves the game at
`http://localhost:5173` and reloads it on changes, and `npm run build` puts a
static site in `bin/web/`:

```sh
npm install
npm run wasm
npm run dev
```

The Android build needs JDK 17 or newer and the Android SDK with the
command-line tools, platform 36, NDK 30 and CMake 4.1.2, with `ANDROID_HOME`
pointing at the SDK. Its Gradle project in `src/platform/android/` builds the
game with the repository's `CMakeLists.txt` and writes everything to
`build/android/`; the Gradle wrapper downloads Gradle itself:

```sh
cd src/platform/android
./gradlew assembleDebug                       # build/android/app/outputs/apk/debug/app-debug.apk
./gradlew installDebug                        # onto the phone adb is connected to
```

Android launchers only show icons built into the app. When `original/`
holds the original, the build makes the launcher icon from the original's
own icon, with a small tool it compiles from the game's C code with the host's
`cc`; the icon stays in `build/`, and such an app is for your own device.
Without the original, the app gets the port's own icon.

The tests run on the `linux` preset; those needing the original look for it
in `original/` (override with `-DJT_ORIGINAL_DIR=<dir>`) and are skipped
without it:

```sh
cmake --preset linux && cmake --build --preset linux
ctest --test-dir build/linux --output-on-failure
```

## Architecture

Everything but `src/platform/` is portable C99 with no Windows, GTK,
browser or Android dependency. The browser build compiles it to WebAssembly together
with `platform/web/bridge`, a flat API that the TypeScript side in
`platform/web/app` calls: the browser owns the loop, and the C code answers
ticks and mouse moves and renders the frames. The Android build works the
same way, with `platform/android/jni` as the C side of its Kotlin app.

```mermaid
flowchart TD
    platform["platform/win32, gtk, web, android<br/>window, input, clock, dialogs"]
    profile["profile<br/>SERVANT.INI"]
    render["render<br/>frames and scenery"]
    core["core<br/>rules, RNG, high scores, settings"]
    gfx["gfx<br/>framebuffer, DIBs, shapes"]
    assets["assets<br/>bitmaps, cursors, dialogs, texts"]
    neres["neres<br/>NE resource reader"]
    original["original<br/>finds JWLTHIEF.EXE by hash"]

    platform --> render
    platform --> profile
    profile --> core
    platform --> gfx
    platform --> core
    platform --> assets
    render --> core
    render --> gfx
    render --> assets
    assets --> gfx
    assets --> neres
    assets --> original
```

| Path                     | Purpose |
|--------------------------|---------|
| `src/core/`              | Game rules and state, the Microsoft C `rand()`, high scores, settings |
| `src/render/`            | Draws frames and the seven scenes into a framebuffer |
| `src/gfx/`               | 32-bit framebuffer, DIB and cursor decoding, the original's sprite raster operation, GDI-like shapes |
| `src/assets/`            | Reads bitmaps, cursors, icons, resources and texts from the original executable |
| `src/neres/`             | Reader for 16-bit NE executables: resources and data segment |
| `src/original/`          | Finds the user's v1.4 executable by its SHA-256 |
| `src/profile/`           | Options and high scores in `SERVANT.INI` |
| `src/common/`            | SHA-256, file system and INI helpers |
| `src/platform/win32/`    | Window, menu, clock thread, dialogs, Windows 3.x template conversion |
| `src/platform/gtk/`      | Linux: GTK 4 window, menu, clock thread and dialogs |
| `src/platform/web/bridge/` | Browser: the C API compiled to WebAssembly with the game |
| `src/platform/web/app/`  | Browser: TypeScript page, canvas, clock, menu and dialogs |
| `src/platform/android/`  | Android: Gradle project, Kotlin app (view, trackpad, clock, dialogs) and `jni/`, its C side |
| `tests/`                 | CTest suite |
| `tools/ne.py`            | Inspect an NE executable or extract its resources |
| `tools/ghidra/`          | Headless Ghidra pipeline used to reverse engineer the original |
| `cmake/`                 | MinGW-w64 toolchains for 32- and 64-bit Windows |
| `docs/research.md`       | The original, its internals, and the port's decisions |

## Reverse engineering

The port was written from a static analysis of the original in
[Ghidra](https://ghidra-sre.org/) 12, driven from the command line:

```sh
tools/ghidra/analyze.sh   # import, Win16 fixups, auto-analysis, recovered symbols
tools/ghidra/export.sh    # decompiled C and TSV indexes of functions, globals, strings
```

`analyze.sh` keeps its Ghidra project in `../<repo>.ghidra` and `export.sh`
writes to `../<repo>.re/export`, both outside the repository: they derive
from the copyrighted original and are never committed. The scripts fix what
Ghidra gets wrong in Microsoft C Win16 code (inline switch tables, FAR
PASCAL callbacks, Win16 API prototypes) and apply the names and types
recovered so far, kept in
[`tools/ghidra/symbols/jwlthief.symbols`](tools/ghidra/symbols/jwlthief.symbols).

## Credits

Jewel Thief was written by Paul Ligeski and published by ServantWare. This
project is an independent reimplementation for preservation and study; it
contains none of the original's code or assets, and needs a copy of the
original to run.

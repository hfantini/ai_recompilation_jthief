# ai_recompilation_win32_jthief

AI-assisted reverse engineering and reimplementation of **Jewel Thief**
(Paul Ligeski / ServantWare, 1991–1993), a 16-bit Windows 3.x game. The goal
is a native Windows build first, and a native Linux port later.

## Original game files

This repository contains **no original code or assets**. The game loads its
graphics at runtime from your own copy of the original `JWLTHIEF.EXE` v1.4,
which is identified by SHA-256, not by name. See [`original/`](original/README.md).

## Layout

| Path            | Purpose                                                  |
|-----------------|----------------------------------------------------------|
| `src/common/`   | SHA-256 and portable file system helpers                 |
| `src/neres/`    | Reader for 16-bit NE executable resources                |
| `src/original/` | Finds the user-supplied v1.4 executable by its hash      |
| `tests/`        | CTest suite; tests needing the original skip without it  |
| `tools/ne.py`   | Inspect an NE executable or extract its resources        |
| `docs/`         | Research and reverse engineering notes                   |

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests look for the original executable in `original/`; override with
`-DJT_ORIGINAL_DIR=<dir>`.

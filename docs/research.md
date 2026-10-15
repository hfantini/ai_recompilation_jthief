# Jewel Thief — research notes

What is known about the original game and how this port reproduces it. The
first sections hold the background gathered before reverse engineering
started (2026-10-13); facts marked *(inferred)* there come from strings or
resource names, not from code. The later sections come from the code and
resources of v1.4.

## The game

- **Title:** Jewel Thief.
- **Author:** Paul Ligeski, Ann Arbor, MI. CompuServe 76636,1166.
- **Publisher:** self-published as shareware in 1991, then republished by
  **ServantWare**, Ligeski's own company and a member of the Association of
  Shareware Professionals (ASP).
- **Platform:** Windows 3.x. All releases are 16-bit **NE** executables.
- **Language:** Microsoft C, per the author. The first prototype was written
  in Actor, an OO language, but that build was ~500 KB.
- **Registration:** $10, or $15 including "the Microsoft C source code". That
  source has never surfaced publicly.
- **Other releases:** included in the first edition of the *FunPack*
  compilation. An unofficial open source remake (libGDX, GPL-3) reuses the
  original art, but it is a reimplementation, not a decompilation.

## Gameplay

- The mouse cursor is the thief. Collect every jewel on the screen while
  avoiding the guards. Mouse only; the keyboard does nothing.
- 9 lives at the start, plus an extra life every 30,000 points.
- A life is lost when a guard touches you, or when the cursor leaves the
  playing field. v1.4 adds an option to disable death on leaving the window.
- 14 levels. The scene changes every 2 levels. Each level adds at least one
  new guard, faster than the previous ones. There is no time limit.
- Jewel value grows per level: Emerald (level 1) is worth 100 points and
  Diamond (level 14) is worth 1,400.
- There are three speeds: beginner, intermediate and expert. Finishing the
  game promotes you to the next speed. Finishing it on expert reveals "the
  jewel of great price" (Matthew 13:45-46). There is also a message for
  finishing it more than five times.
- "Trade in all your jewels for nine lives?" is an in-game option.
- The window is deliberately non-resizable ("to prevent you from cheating").
  The game was designed for 640x480 with 256 colors.
- In high color some sprites vanish *(inferred: a palette-dependent raster
  operation or mask trick)*.
- The only sound is the Windows `MessageBeep` ("it can't sing").

### Content (v1.4 resource names)

- **Jewels**, in string-table order: Emerald, Opal, Sapphire, Garnet, Pearl,
  Onyx, Ruby, Aquamarine, Gold, Amethyst, Peridot, Topaz, Citrine, Diamond.
- **Scenes:** star, sphinx, water, cave, castle, city, cloud
  *(inferred: 7 scenes x 2 levels = 14 levels)*.
- **Guards:** Ogre, Nazgul, Camel, Mask, Clipper, Battleship, Spider, Bat,
  Soldier, Wizard, Helicopter, Apache, Raindrop, Cloud.
- **Dialogs:** options, high score, register, about, hints, instructions,
  story, pause and wait.
- **Exported procedures:** `MAINWNDPROC`, `OPTIONSDLGPROC`,
  `HIGHSCOREDLGPROC`, `REGISTER`, `SERVANTWAREABOUT`,
  `GETHIGHSCORERDLGPROC` and `SPLASHWINDOWPROC`.
- **Settings:** stored in `SERVANT.INI`, section `[Jewel Thief]`, with the
  keys `Speed`, `LevelWait`, `Cheat`, `Scores` and `Names` (top 10).
  v1.4 also stores a serial number.

## Releases

| Version | Date     | Code seg | Data seg | Linker | Notes |
|---------|----------|----------|----------|--------|-------|
| 1.0     | Oct 1991 | 12056 B  | 2582 B   | 5.10   | numeric resource ids only |
| 1.2a    | Apr 1992 | 14184 B  | 5700 B   | 5.10   | fixes an ASP nag-screen rule violation |
| 1.3     | 1992     | 14392 B  | 5700 B   | 5.10   | difficulty setting |
| 1.4     | Oct 1993 | 15190 B  | 5728 B   | 5.30   | named resources, help file, setup, serial registration |

### Version 1.4

**v1.4 is the reference version for this project.** Its executable comes from
`JWLTHF14.ZIP`:

- **File:** `JWLTHIEF.EXE`, 131072 bytes.
- **SHA-256:** `c764beb9d16d5a4a2870d794b65219b4e92649ceac1bffbd13f31254ee6d21f2`
- **MD5:** `1e57e850ccdad7da14b3996529fd28ee`

The image has two segments:

- **CODE:** 1 segment, 15190 bytes.
- **DATA:** 1 segment, 5728 bytes. This is the automatic data segment.

It imports from KERNEL (32), USER (37) and GDI (19), all by ordinal. The
runtime banner reads "(c) 1983-1993 Microsoft Corporation" *(inferred:
Microsoft C 7 or Visual C++ 1.x)*.

Most of the file is bitmap data. The game logic plus the C runtime fit in
about 15 KB of code.

## Implementation (v1.4 code)

Read statically from the Ghidra export (see `tools/ghidra/`). Names refer to
`tools/ghidra/symbols/jwlthief.symbols`.

### Layout

- Game code spans `1000:0010`–`1000:2c8c` (about 50 functions). The Microsoft
  C runtime starts at `entry` (`1000:2cce`) and is not ported.
- One window of 500x300 pixels at (60, 30), `WS_SYSMENU | WS_MINIMIZEBOX`, so
  it cannot be resized. The client area minus a 32-pixel status bar at the
  bottom is the playfield. Under Windows 3.1 that client area is 498x260: the
  floor polygon the game paints ends exactly at y 228, the bottom of the
  playfield.
- The status bar shows the level name, one jewel icon per level reached, the
  lives (`"%i man"` / `"%i men"`) and the score. The score is stored divided
  by 100 and drawn as `"%i00"`.

### Game rules

- The player is the mouse cursor. `WM_MOUSEMOVE` drives everything the player
  does; there is no keyboard input besides menu accelerators.
- **Lives:** 9 at the start, 9 at most. One extra life each time the score
  reaches the next multiple of 300 (30,000 on screen).
- **Jewels:** `(level + 2) * 2` per level, at random positions. Taking one
  adds `level + 1` to the score (level counted from 0).
- **Guards:** `(level + 1) / 2 + 3` per level, except levels 7 and 12
  (0-based: Aquamarine and Citrine), which are swarms of `level + 10`.
- **Guard speed:** per axis, `(speed + 1) * 2 + level / 2 + rand() % 4`,
  where `speed` is the 0–2 setting. Guards bounce off the playfield edges;
  on levels 0, 2, 8 and 9 their upper bound is 82 pixels below the top, so
  they stay out of the scenery. On level 12 (Raindrop) they wrap from the
  bottom back to the top instead of bouncing.
- **Death:** on a timer tick, when the cursor box overlaps a guard; and on a
  mouse move, when the cursor leaves the playfield, unless `Cheat=1`.
  Collision boxes are axis-aligned; the cursor box is its position plus 8 to
  24 pixels on each axis.
- **Levels:** 14, named after the jewels. The scene is `level / 2` (star,
  sphinx, water, cave, castle, city, cloud), each drawn with GDI primitives
  plus one bitmap. Guard and jewel bitmaps come from per-level name tables in
  the data segment.
- **End:** after level 14, below Expert speed the game says "Now try it at
  the 'Expert' speed"; at Expert it shows the Matthew 13:45-46 parable. Then
  the high score check, the high score table (shown after every game) and
  "Play again?".
- **Spawning order:** a new game spawns jewels before guards; a level-up
  spawns guards before jewels. Guards already on the field stay through a
  level-up; only free slots (negative x) are filled.

### Quirks

Kept by the port on purpose:

- Every mouse move outside the playfield costs a life, not just leaving it,
  so moving over the status bar drains lives quickly.
- A guard catching the thief on the last life leaves 0 lives and the game
  goes on; the next catch ends it. Leaving the playfield ends it at 0.
- At 9 lives the extra life threshold does not advance, so the first jewel
  taken after dropping below 9 grants a life at once.
- After a catch the thief is harmless until the mouse moves again, so a
  guard sitting on a still cursor takes one life only.

### Scenery

Each pair of levels has a scene. Besides the scenery at the top, the game
fills two polygons it builds on `WM_CREATE`: a band around the horizon
(y 92–150) and the floor, from the horizon down to y 228. The floor covers
almost all of the playfield, so the white window background barely shows.

| Levels | Scene  | Top of the screen | Horizon band | Floor |
|--------|--------|-------------------|--------------|-------|
| 1–2    | star   | black sky, 10 random `STAR`s, a dark red planet | black | dark red |
| 3–4    | sphinx | cyan sky, sun, two two-tone pyramids, `SPHINX` | orange | yellow |
| 5–6    | water  | tiled `WATER` | (tiles) | navy |
| 7–8    | cave   | `CAVE` | (bitmap) | olive |
| 9–10   | castle | cyan sky, sun, two green hills, `CASTLE` | dark green | dark green |
| 11–12  | city   | `CITY` | black | gray |
| 13–14  | cloud  | cyan sky, sun, a rainbow of five 7-pixel arcs | cyan | cyan |

The sprites are drawn for the floor of their own level: their bitmaps hold
the colors that the raster operation below turns into the intended ones over
that floor. The emerald, for instance, is stored navy and cyan and shows
green with a black outline over the dark red floor of level 1; the clouds
are stored cyan and turn white over the cyan sky of level 13.

### Engine details that matter for fidelity

- The game tick is `SetTimer` with 5 ms. Windows 3.1 runs timers on the PC
  timer chip at 1193182 / 65536 Hz, so a tick really comes every ~54.9 ms;
  that is the pace of the game. Modern Windows honors much shorter intervals.
- Timer ticks keep arriving while a modal dialog is open, since the dialog's
  own message loop dispatches them. Guards therefore keep moving during the
  pause bar, the "Get ready" box and the options; the thief is safe because
  the main window has lost the focus.
- Guards and jewels are drawn with `BitBlt` raster operation `0x990066`
  (DSxn, `NOT(dest XOR src)`): draw to erase, move, draw again, with no back
  buffer. Video cards apply it to palette indices. In the 16 VGA colors,
  color `15 - n` is the complement of color `n`, and the 256-color static
  palette keeps those colors at indices 0–7 and 248–255, so both modes give
  the same result. In high color the operation runs on RGB values instead,
  which is why sprites look wrong or vanish there. White sprite pixels leave
  the background untouched.
- All randomness uses the Microsoft C `rand()` (`state * 214013 + 2531011`,
  returning bits 16–30), seeded with the low 16 bits of `time(NULL)` on
  `WM_CREATE`. The star scene also draws its stars with `rand()`.
- The cursor switches between a right- and a left-facing shape depending on
  the horizontal direction of the last move, inside the playfield only. While
  a level is set up it is the hourglass.

### Settings (`SERVANT.INI`, section `[Jewel Thief]`)

| Key         | Values | Effect |
|-------------|--------|--------|
| `Speed`     | 0–2    | Guard speed; 2 is Expert and unlocks the real ending. Missing means 1 |
| `LevelWait` | 0/1    | Shows a 3-second "Get ready" box at startup, on a new game and whenever the scene changes |
| `Cheat`     | 0/1    | Leaving the playfield no longer costs a life |
| `Scores`    | 10 longs | High scores, comma-separated (read with spaces and tabs too) |
| `Names`     | 10 names | High score names, comma-separated, up to 25 characters when entered |
| `Serial`    | string | Compared with a fixed string; only suppresses the About dialog at startup |

The options dialog writes the three options and restarts the game.

### Differences from the background notes

- No code promotes the player to the next speed after finishing, and none
  counts how many times the game was finished. In v1.4 the only effect of
  finishing below Expert is the "try it at Expert" message. The "mastered
  Jewel Thief over five times" text is still in the data segment, but the
  only code that could show it is unreachable.
- The guard table orders Bat before Spider (`..., Battleship, Bat, Spider,
  ...`).
- Guards are added every second level, not every level.

## Resources (v1.4)

Listed with `tools/ne.py info`; the menu, dialogs and accelerators were
decoded from the extracted resources.

### Bitmaps

All bitmaps are 16-color (4 bpp), except `STAR` (1 bpp). The art itself is
therefore palette-safe; the high-color glitch comes from drawing it with
ROP `0x990066`.

| Group   | Bitmaps | Size |
|---------|---------|------|
| Jewels  | the 14 jewel names | 10x10 to 19x20 |
| Guards  | the 14 guard names | 11x18 (`RAINDROP`) to 62x21 (`CLOUD`) |
| Scenery | `STAR` 9x9, `SPHINX` 72x55, `WATER` 72x72, `CAVE` 500x150, `CASTLE` 88x100, `CITY` 500x92 | |
| Status  | `STATUSBR`, source of the status bar icons | 280x19 |

The cloud scene has no bitmap of its own: it is drawn with arcs and reuses
the `CLOUD` guard bitmap.

### Cursors and icon

`RIGHTCUR` and `LEFTCUR` are 32x32 with the hotspot at (0, 0), which matches
the 8–24 pixel collision box. `FUNICON` is the application icon.

### Menu `JWLTHIEFMENU`

| Id  | Command | Accelerator |
|-----|---------|-------------|
| 110 | Game › New... (asks for confirmation) | F2 |
| 115 | New game without confirmation; posted internally after "Play again?" | |
| 120 | Game › Pause | F3 |
| 130 | Game › Options... | |
| 140 | Game › High Scores | |
| 190 | Game › Exit | Alt-F4 |
| 210 | Help › Contents | |
| 220 | Help › Search for Help on... | |
| 230 | Help › How to Use Help | |
| 240 | Help › About Jewel Thief... | |

### Dialogs

| Resource   | Purpose |
|------------|---------|
| `400`      | Options: speed (Beginner, Intermediate, Expert = `Speed` 0, 1, 2), pause after each level (`LevelWait`), cheat mode (`Cheat`) |
| `600`      | Registration: serial number entry |
| `700`      | High score table, 10 lines |
| `800`      | New high score: name entry |
| `900`      | About, with two `SPLASH` controls |
| `PAUSEBOX` | Pause bar with a Play button |
| `WAITBOX`  | "Get ready" box shown between levels |
| `HINTSBOX`, `INSTRBOX`, `STORYBOX` | Hints, instructions and story. Not referenced by v1.4 code, which sends help to WinHelp; likely left over from earlier versions |

`SPLASH` is a custom control class used only in the About dialog. Each
instance paints a `.bmp` file from the game directory, chosen by its window
style: 0 is `winlogo.bmp`, 1 is `jwlthief.bmp`.

## Port decisions

The goal is a modern port that preserves the gameplay as closely as
possible (rules, formulas, pace, quirks), not a bit-exact replica.

- **Assets:** bitmaps, cursors, icon, menu, accelerators, dialogs and texts
  are all read at runtime from the user's `JWLTHIEF.EXE`. Dialog and menu
  templates are converted from the Windows 3.x format to Win32 on the fly,
  so the original layouts, fonts and wording appear unchanged.
- **Sprite colors:** the raster operation is reproduced on palette indices
  of the nearest VGA colors, as 16- and 256-color displays did, so sprites
  look as they did in 256 colors on any display.
- **Rendering:** each frame is drawn into a 32-bit buffer and copied to the
  window once, instead of drawing and erasing sprites on screen. The scenery
  is redrawn only when the scene changes.
- **Scenery colors:** brushes the original asked for outside the 20 system
  colors were dithered by Windows 3.1; the port paints them as solid colors.
- **Stars:** drawn when a game starts or the star scene comes back, using
  `rand()` there. The original also redrew them on every repaint.
- **Window:** the client area is fixed at 498x260, the Windows 3.1 size,
  since modern frames are thicker than the original's 500x300 allowed for.
- **Pace:** a clock thread posts a tick every 54.9 ms. It keeps ticking under
  modal dialogs, coalesces late ticks and does not catch up after a stall,
  like `WM_TIMER`.
- **Help:** the original opened `JWLTHIEF.HLP` in WinHelp, which modern
  Windows no longer ships. The three Help commands open the Instructions,
  Hints and Story boxes that v1.4 still carries, and are renamed to match;
  the Help button of the options does the same.
- **Registration:** the shareware nag is gone. The About box no longer shows
  at startup and its Register button is disabled.
- **Debug mode:** `--debug` enables keys the original never had: Home toggles
  a god mode, Page Up / Page Down change the level.
- **Windows versions:** the 32-bit build is compiled against the Windows XP
  API (`_WIN32_WINNT=0x0501`) and runs from XP to 11; the 64-bit build
  targets modern Windows.
- **No floating point:** the shapes of the scenery are computed with
  integers only, in doubled coordinates where pixel centers and pen edges
  fall on whole numbers, and angles are compared with cross products
  instead of `atan2`. The result is the same, pixel for pixel, and the
  MS-DOS build runs on a 386 without a math coprocessor.

### MS-DOS

The DOS build has to stand in for Windows 3.1 itself. Much of what the other
ports emulate is native there.

- **Screen:** VGA mode 12h, 640x480 in 16 colors, the resolution and depth
  the original was made for. The DAC holds the Windows VGA palette in
  Windows order, so pixel values are the palette indices the original's
  raster operations worked on. The window is drawn where the original's
  `CreateWindow` put it, 500x300 at (60, 30), with the Windows 3.1 default
  colors, caption, menu bar and dialogs.
- **Colors:** the scenery brushes outside the 16 colors are dithered, as
  Windows 3.1 did on a VGA, with the mix of up to three palette colors that
  comes closest to the brush, spread with an 8x8 ordered dither. Bitmaps
  from files, such as the About box's pictures, take the nearest colors,
  like `SetDIBitsToDevice`.
- **Pace:** the game ticks with the BIOS clock, the very 18.2 Hz timer that
  paced Windows 3.1. Nothing is reprogrammed: the clock is polled.
- **Menus and dialogs:** they are laid out from the original's templates,
  with the dialog unit arithmetic of Windows. The VGA's ROM fonts, read from
  the video BIOS and trimmed into a proportional face, stand in for the
  Windows fonts; they are wider than Helv, so a dialog's horizontal unit
  grows until its texts fit their controls. Dialogs without a font of their
  own keep the VGA system font's units: the pause bar still covers the status
  bar exactly.
- **Cursor:** drawn by the game, with the AND and XOR masks of `RIGHTCUR`
  and `LEFTCUR` applied to palette indices, as on the original's display.
  The arrow and the hourglass, which the original took from Windows, are
  the port's own.
- **Speed:** only the parts of the screen that change are written to the
  VGA, which sits on a slow bus on a 386, and nearest-color searches for
  the sprites' raster operation are cached.
- **Mouse moves** reach the game only over the client area, as
  `WM_MOUSEMOVE` did, so the menu bar is free to use; moves over the status
  bar still cost lives.

## Sources

- Internet Archive items:
  - [win3_JewelThi (1.0)](https://archive.org/details/win3_JewelThi)
  - [win3_JWLTHF12](https://archive.org/details/win3_JWLTHF12)
  - [win3_JWLTHF13](https://archive.org/details/win3_JWLTHF13)
  - [JWLTHF12_ZIP](https://archive.org/details/JWLTHF12_ZIP)
  - [SERVANT_201612 (1.3)](https://archive.org/details/SERVANT_201612)
  - [JewelThief_1020 (1.4)](https://archive.org/details/JewelThief_1020)
- Game pages:
  - [MobyGames](https://www.mobygames.com/game/12951/jewel-thief/)
  - [TheAlmightyGuru](https://www.thealmightyguru.com/Wiki/index.php?title=Jewel_Thief)
  - [Giant Bomb](https://giantbomb.com/wiki/Games/Jewel_Thief)
- Reviews:
  - [raebear.net review](https://raebear.net/comp/jewelthief.html)
  - [LGR review](https://www.youtube.com/watch?v=EIcImzLNMz8)
- Open source remake:
  [libGDX remake (SwitchGDX)](https://github.com/SwitchGDX/jewelthief-switchgdx)
- The original archives themselves: `README.TXT`, `CORRECT.TXT`,
  `PACKING.LST` and `JWLTHIEF.HLP`.

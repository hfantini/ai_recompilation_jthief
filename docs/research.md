# Jewel Thief — research notes

Background gathered before reverse engineering started (2026-10-13).
Facts marked *(inferred)* come from strings or resource names, not from code.

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

# Original game files

Place your own copy of the original **Jewel Thief v1.4** files here. Nothing in
this directory (besides this README) is tracked by git.

The supported executable is identified by content, not by name:

| File           | Size (bytes) | SHA-256                                                            |
|----------------|--------------|--------------------------------------------------------------------|
| `JWLTHIEF.EXE` | 131072       | `c764beb9d16d5a4a2870d794b65219b4e92649ceac1bffbd13f31254ee6d21f2` |

It ships inside `JWLTHF14.ZIP` (ServantWare, 1993).

The port reads everything it needs from `JWLTHIEF.EXE`. Other files of the
same folder are used when present:

| File           | Used for |
|----------------|----------|
| `SERVANT.INI`  | Options and high scores, shared with the original; created on the first save |
| `WINLOGO.BMP`, `JWLTHIEF.BMP` | The pictures of the About box |

While developing, pass this folder on the command line:

```sh
wine bin/debug/jthief_32.exe original/
```

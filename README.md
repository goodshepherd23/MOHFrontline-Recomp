# Medal of Honor: Frontline (Recompiled)

A native Windows build of **Medal of Honor: Frontline** (GameCube, USA), made from your own disc.

The game's program is statically recompiled to C with [DolRecomp](https://github.com/ExpansionPak/DolRecomp)
and compiled on your PC into a native module. That module runs on
[ModernGekko](https://github.com/ExpansionPak/ModernGekko), a runtime built on Dolphin's hardware emulation
(graphics, sound, input, disc access). This kit ships no game code or data. Setup reads everything from
your disc.

## What you need

- Windows 10 or 11, 64-bit.
- **Medal of Honor: Frontline, USA release (GMFE69)**. Either:
  - a disc image as **.iso**, **.gcm** or **.ciso**, or
  - a disc extracted with Dolphin (right-click the game > Properties > Filesystem > Extract Entire Disc).
    Setup then asks for the `files\Moh2RelGC.elf` inside it.

  Other formats (.rvz, .gcz, .nkit) must be converted to ISO first: in Dolphin, right-click the
  game > Convert File > Format: ISO. Other regions and revisions are refused, because the recompiled
  program and its tuning are for this exact file.
- About 3 GB of free space (2 GB if you point it at an extracted disc) and an internet connection the
  first time. Setup downloads a portable clang (llvm-mingw) and Python, both pinned and checksum-verified.

## Playing

Run `MOHFrontline.exe`. The first run asks where to install the game files (about 3 GB; the default is
`%LOCALAPPDATA%\MOHFrontline-Recomp`), then opens a setup window that asks for your disc and builds the
game. **This takes 20 to 60 minutes** depending on your CPU (about 20 minutes on a plugged-in laptop with an
Intel i7-1355U), and the window shows progress and time estimates. Your disc is only read once. Updates to
this kit only rebuild the game when the build itself changed.

After that, a settings window opens before each start:

- **Resolution:** native (fastest) up to 4x.
- **Graphics:** Direct3D 11 (recommended), Vulkan or OpenGL.
- **Fullscreen**, **Widescreen** and **Show speed in the title bar**.
- **Reset controls:** puts the default controls below back.
- **Change disc**, **Rebuild** and **Open folder**.

Tick "Don't show this window" to start straight into the game; hold **Shift** while starting it to get the
window back. `MOHFrontline.exe --setup` and `--rebuild` still work too.

Your settings and saves stay in the folder you chose, so updating the app keeps them. To keep everything in
the app folder instead, put an empty `portable.txt` next to `MOHFrontline.exe`.

### Controls

An Xbox-style (XInput) controller and keyboard and mouse both work out of the box. The game is set to its
**MOH Sharpshooter** layout (choose it when a new game asks):

| Action (GameCube) | Controller | Keyboard and mouse |
|---|---|---|
| Move (stick) | Left stick | W A S D (or the arrow keys) |
| Look (C-stick) | Right stick | Mouse |
| Fire (R) / Aim (L) | Right / left trigger | Left / right mouse button |
| Action (A) | A | E (Enter in menus) |
| Melee (B) | X | F (Backspace goes back in menus) |
| Crouch (X) / Jump (Y) | B / Y | C or Left Ctrl / Space |
| Reload (Z) | Right bumper | R |
| Weapons (D-pad left/right) | D-pad | Mouse wheel, or Z / X |
| D-pad up / down | D-pad | 1 / 2 |
| Pause (Start) | Start | Esc |

The mouse is locked to the game window while it has focus. If you changed the controls in v0.1.0, press
**Reset controls** in the settings window to get this layout. The mapping is Dolphin's, in
`user\Config\GCPadNew.ini` in the game folder.

### Widescreen

The kit turns on Dolphin's widescreen hack, so the 3D view is a true 16:9 picture. Menus and the HUD come out
stretched sideways, which the hack can't avoid. Untick **Widescreen** in the settings window for the original
4:3 picture.

## Performance

How fast it runs depends on your CPU's single-core speed. On a 15 W laptop CPU (Intel i7-1355U), plugged in,
menus run at full speed and the opening mission at about **70 to 85%**. On battery it is much slower. Faster
desktop CPUs do better. The kit raises the game's priority and turns off Windows power throttling
for it. Direct3D 11 at the native resolution is the fastest setting.

The game's movies use the VP6 codec, which Dolphin draws with vertical stripes unless "Manual Texture
Sampling" is on. The kit turns it on for this game (`user\GameSettings\GMFE69.ini`).

Two choices trade exactness for speed:

- **Fast floating point:** float math runs as plain host math, the same trade Dolphin's JIT makes. It doesn't
  keep the GameCube's FPSCR status flags or its 25-bit multiply rounding. This made gameplay about 20% faster
  and no difference has been seen.
- **Profile-guided optimization:** the shipped profile (`kit\profile`) was recorded in the first mission. It
  makes the compiled game about 20% faster.

## Known issues

- **Speed:** see above. It is the main open issue.
- **Sound:** a player reported missing sound effects. Dolphin's wiki suggests DSP LLE for this game, but LLE
  produces no sound at all in this runtime, so the kit stays on HLE. The recompiled game's audio matches
  Dolphin's own JIT in the same runtime. Reports with a description of which sounds are missing help.
- Menus and the HUD are stretched in widescreen.

## How it works

1. Setup unpacks a CISO to a temporary ISO if needed, extracts the disc, then checks the disc ID (GMFE69) and the SHA-256 of `files\Moh2RelGC.elf`, the real game program.
   The disc's `main.dol` is only a boot stub.
2. `kit\elf2dol.py` converts that ELF into the DOL the recompiler reads.
3. `runtime\dolrecomp.exe` translates the DOL into about 90 MB of C.
4. `kit\build_module.py` compiles that C with clang (ThinLTO, `-O3`, the PGO profile) into
   `module\gGMFE69_recomp.dll`.
5. `MOHFrontline.exe` starts `runtime\moderngekko-run.exe`, which loads the module and runs the game.

`runtime\msvcp140_atomic_wait.dll` is a small wrapper (source in `tzshim\`). Some newer Windows time
zones (for example Alberta's) are unknown to the system's ICU data, and Microsoft's C++ library then aborts
the runtime at start. The wrapper supplies a zone name instead.

## Source and license

GPL-3.0 (see `LICENSE`), like ModernGekko and DolRecomp. The runtime binaries in `runtime\` are built from:

| Project | Commit | Local changes |
|---|---|---|
| [ExpansionPak/ModernGekko](https://github.com/ExpansionPak/ModernGekko) | `edd460a` | `patches/ModernGekko-edd460a.patch`: D3D11 backend option, speed in the title bar, PGO run fix, no mod-hook hash lookups when no mods are loaded |
| [ExpansionPak/RecompCore](https://github.com/ExpansionPak/RecompCore) (Dolphin fork, `vendor/dolphin`) | `10f7ac4` | `patches/RecompCore-10f7ac4.patch`: `ppc_native_region_available` stub, command FIFO fields on separate cache lines (less CPU/GPU thread contention) |
| [ExpansionPak/DolRecomp](https://github.com/ExpansionPak/DolRecomp) (`vendor/dolphin/DolRecomp`) | `1bec355` | `patches/DolRecomp-1bec355.patch`: direct cross-chunk calls enabled, fast floating point, env defaults |

These were built with Visual Studio 2026 (MSVC 14.51). The MSVC runtime DLLs are Microsoft's redistributables.

Made with the help of an AI coding assistant. This is a fan project, not affiliated with or endorsed
by Electronic Arts or Nintendo. You need your own copy of the game.

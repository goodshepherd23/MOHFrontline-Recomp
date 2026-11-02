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
  - a disc image as **.iso** or **.gcm**, or
  - a disc extracted with Dolphin (right-click the game > Properties > Filesystem > Extract Entire Disc).
    Setup then asks for the `files\Moh2RelGC.elf` inside it.

  Other formats (.rvz, .ciso, .gcz, .nkit) must be converted to ISO first: in Dolphin, right-click the
  game > Convert File > Format: ISO. Other regions and revisions are refused, because the recompiled
  program and its tuning are for this exact file.
- About 3 GB of free space (2 GB if you point it at an extracted disc) and an internet connection the
  first time. Setup downloads a portable clang (llvm-mingw) and Python, both pinned and checksum-verified.

## Playing

Run `MOHFrontline.exe`. The first run opens a setup window that asks for your disc and builds the
game. **This takes 30 to 60 minutes** on a laptop and about 15 minutes on a fast desktop. After that,
the game starts straight away.

- `MOHFrontline.exe --setup`: choose another disc.
- `MOHFrontline.exe --rebuild`: rebuild the game from your disc.

Your settings and saves live in `%LOCALAPPDATA%\MOHFrontline-Recomp`, so updating the app keeps them.
To keep everything in the app folder instead, put an empty `portable.txt` next to `MOHFrontline.exe`.

### Controls

An Xbox-style (XInput) controller and the keyboard both work out of the box:

| GameCube | Controller | Keyboard |
|---|---|---|
| Move (stick) | Left stick | Arrow keys |
| Look (C-stick) | Right stick | I J K L |
| Fire (R) / Aim (L) | Right / left trigger | W / Q |
| A (action) | A | X |
| B (melee) | X | Z |
| X (crouch) / Y (jump) | B / Y | C / S |
| Z (reload) | Right bumper | D |
| D-pad (weapons) | D-pad | T F G H |
| Start | Start | Enter |

The mapping is Dolphin's, in `user\Config\GCPadNew.ini` under the data folder.

### Widescreen

The kit turns on Dolphin's widescreen hack, so the 3D view is a true 16:9 picture. Menus and the HUD come out
stretched sideways, which the hack can't avoid. For the original 4:3 picture, set
`AspectRatio = 0` and `wideScreenHack = False` in `user\Config\GFX.ini`.

## Performance

How fast it runs depends on your CPU's single-core speed. On a 15 W laptop CPU (Intel i7-1355U), menus run at
full speed, the intro movies at about 70% and missions at roughly **two-thirds speed**. Faster
desktop CPUs do better. The kit pins the game to your fastest cores and turns off Windows power
throttling for it. Graphics use Direct3D 11 at the native resolution, because the Vulkan and OpenGL backends
draw striped movies on Intel Iris Xe. Both settings are in `user\config.ini`.

Two choices trade exactness for speed:

- **Fast floating point:** float math runs as plain host math, the same trade Dolphin's JIT makes. It doesn't
  keep the GameCube's FPSCR status flags or its 25-bit multiply rounding. This made gameplay about 20% faster
  and no difference has been seen.
- **Profile-guided optimization:** the shipped profile (`kit\profile`) was recorded in the first mission. It
  makes the compiled game about 20% faster.

## How it works

1. Setup checks the disc ID (GMFE69) and the SHA-256 of `files\Moh2RelGC.elf`, the real game program.
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
| [ExpansionPak/ModernGekko](https://github.com/ExpansionPak/ModernGekko) | `edd460a` | `patches/ModernGekko-edd460a.patch`: D3D11 backend option, speed in the title bar, PGO run fix |
| [ExpansionPak/RecompCore](https://github.com/ExpansionPak/RecompCore) (Dolphin fork, `vendor/dolphin`) | `10f7ac4` | `patches/RecompCore-10f7ac4.patch`: `ppc_native_region_available` stub |
| [ExpansionPak/DolRecomp](https://github.com/ExpansionPak/DolRecomp) (`vendor/dolphin/DolRecomp`) | `1bec355` | `patches/DolRecomp-1bec355.patch`: direct cross-chunk calls enabled, fast floating point, env defaults |

These were built with Visual Studio 2022 Build Tools. The MSVC runtime DLLs are Microsoft's redistributables.

Made with the help of an AI coding assistant. This is a fan project, not affiliated with or endorsed
by Electronic Arts or Nintendo. You need your own copy of the game.

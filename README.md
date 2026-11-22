# Medal of Honor: Frontline (Recompiled)

A native Windows build of **Medal of Honor: Frontline** (GameCube, USA), made from your own disc.

> **What's new in v0.2.4.** Thanks to everyone who left feedback. v0.2.4 is out and runs a lot better:
>
> - Much faster in missions: the opening mission went from about 73% to 93% speed on a 15 W laptop CPU
>   (Intel i7-1355U). The audio chip was being run on its own thread on CPUs with more than 4 cores, and the
>   game spent much of its time waiting on it. That's fixed.
> - Start-up takes seconds again (v0.2.2 and v0.2.3 re-checked every game file on every launch).
> - Fullscreen is real borderless fullscreen, with no border around the picture.
> - Sound keeps its pitch when the game runs below full speed.
> - Earlier updates added keyboard and mouse with smooth mouse look, a settings window, quick save (F5) and
>   load (F9), fixed movie stripes, and all sound effects.
>
> The first start after updating rebuilds the game from your disc once (20 to 60 minutes, progress shown);
> settings and saves are kept. If it still runs poorly for you, please say what CPU and GPU you have.

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

- **Resolution:** the game's own 640 x 528 (fastest), or 2x / 3x / 4x for a sharp picture on a 1080p, 1440p
  or 4K screen. The window itself always fills your screen; this sets how detailed the 3D picture is.
- **Graphics:** Direct3D 11 (recommended), Vulkan or OpenGL.
- **Fullscreen** (borderless, the whole screen), **Widescreen** and **Show speed in the title bar**.
- **Accurate audio:** Dolphin's DSP LLE, on by default. The faster HLE drops sound effects in this game. Untick it for a few percent more speed.
- **Mouse sensitivity:** how fast the mouse turns the view (1 to 10, default 5).
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

While the game window is in front, the mouse pointer is hidden and kept inside it. Press **Alt** (opens the
window's menu), **Alt+Tab** or minimize the game to get the pointer back.

Mouse look turns the view smoothly while the mouse moves and stops when it stops. Change **Mouse
sensitivity** in the settings window if it is too slow or too fast. The game itself has a top turning speed,
so very fast flicks turn no faster than the right stick pushed all the way.

| Key | Does |
|---|---|
| F5 (or F1) | Quick save (a new save each time, in `user\StateSaves`) |
| F9 | Quick load: loads the newest save. **File > Load State** lists them all |
| Tab (hold) | Fast-forward |
| F11 or Alt+Enter | Fullscreen on/off |

Quick saves are emulator snapshots, separate from the game's own saves on the memory card.

If you changed the controls in v0.1.0, press **Reset controls** in the settings window to get this layout.
The mapping is Dolphin's, in `user\Config\GCPadNew.ini` in the game folder. The launcher updates only the
four mouse-look lines, and only while they still use the mouse, so your own remapping is kept.

### Widescreen

The kit turns on Dolphin's widescreen hack, so the 3D view is a true 16:9 picture. Menus and the HUD come out
stretched sideways, which the hack can't avoid. Untick **Widescreen** in the settings window for the original
4:3 picture.

## Performance

How fast it runs depends on your CPU's single-core speed. On a 15 W laptop CPU (Intel i7-1355U), plugged in,
menus run at full speed and the opening mission at about **70 to 85%**. On battery it is much slower. Faster
desktop CPUs do better. The kit raises the game's priority and turns off Windows power throttling
for it. Direct3D 11 at the native resolution is the fastest setting.

Shaders are compiled in the background (Dolphin's ubershaders draw in the meantime), so an effect that appears
for the first time doesn't freeze the game, and shaders the game has used before are built while it loads. When the game runs below full speed, the sound keeps its pitch instead of
dropping. The game's frame rate is the GameCube game's own (it can't be raised).

The game's movies use the VP6 codec, which Dolphin draws with vertical stripes unless "Manual Texture
Sampling" is on. The kit turns it on for this game (`user\GameSettings\GMFE69.ini`).

Two choices trade exactness for speed:

- **Fast floating point:** float math runs as plain host math, the same trade Dolphin's JIT makes. It doesn't
  keep the GameCube's FPSCR status flags or its 25-bit multiply rounding. This made gameplay about 20% faster
  and no difference has been seen.
- **Profile-guided optimization:** the shipped profile (`kit\profile`) was recorded in the first mission. It
  makes the compiled game about 20% faster.

Other speed work, none of which changes what the game does:

- **Fewer ways into each block of code:** DolRecomp writes the game as large C functions with a `switch`
  that lets execution start at any instruction. The kit now keeps only real entry points (branch and call
  targets, return addresses, and code addresses found in the game's data), so the compiler can keep values
  in registers across instructions. Any other address still works: it runs one instruction in Dolphin's
  interpreter and carries on. About 5 points faster in the first mission (v0.2.4).
- **Audio DSP on the game's CPU thread:** Dolphin put the DSP on its own thread by itself on CPUs with more
  than 4 cores, and with Accurate audio the game then waited on and woke that thread thousands of times a
  second. Running it inline measured **93% vs 73%** in the first mission (v0.2.4). The setting is
  `[DSP] DSPThread` in `user\Config\Dolphin.ini`, which the runtime now honours.
- **No mod lookups without mods**, a leaner path for the game's graphics commands, and a runtime built with
  whole-program optimization.

## Known issues

- **Speed:** see above. It is the main open issue.
- **Sound:** with Dolphin's faster DSP HLE this game drops sound effects (a player report, and Dolphin's wiki).
  The kit uses DSP LLE by default (**Accurate audio** in the settings window), which costs a few percent of speed.
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
| [ExpansionPak/ModernGekko](https://github.com/ExpansionPak/ModernGekko) | `edd460a` | `patches/ModernGekko-edd460a.patch`: D3D11 backend option, speed in the title bar, PGO run fix, no mod-hook hash lookups when no mods are loaded, cached game-file hashes (start-up in seconds instead of minutes after the first run), no mod host-call hooks when no mods are loaded, a shader compilation mode set in GFX.ini is kept |
| [ExpansionPak/RecompCore](https://github.com/ExpansionPak/RecompCore) (Dolphin fork, `vendor/dolphin`) | `10f7ac4` | `patches/RecompCore-10f7ac4.patch`: `ppc_native_region_available` stub, command FIFO fields on separate cache lines (less CPU/GPU thread contention), a leaner gather-pipe write path for the game's graphics commands, game window: mouse kept in the window (LockCursor), F5/F9 quick save/load, Tab fast-forward, Esc no longer quits, real borderless fullscreen at start without the sunken window edge, `[DSP] DSPThread` honoured instead of chosen from the core count |
| [ExpansionPak/DolRecomp](https://github.com/ExpansionPak/DolRecomp) (`vendor/dolphin/DolRecomp`) | `1bec355` | `patches/DolRecomp-1bec355.patch`: direct cross-chunk calls enabled, fast floating point, env defaults, sparse chunk entries (`DOLRECOMP_C_SPARSE_ENTRIES=1`) |

These were built with Visual Studio 2026 (MSVC 14.51), Release with whole-program optimization
(`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DENABLE_LTO=ON`, plus `/wd5049`). The MSVC runtime DLLs are Microsoft's redistributables.

Made with the help of an AI coding assistant. This is a fan project, not affiliated with or endorsed
by Electronic Arts or Nintendo. You need your own copy of the game.

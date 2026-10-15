# Super Mario World 2 - Yoshi's Island Recomp

A native recompilation of **Super Mario World 2 - Yoshi's Island** (USA), built on
[snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp).

Local experiment to evaluate reuse of the existing Super FX LLE implementation in Yoshi's Island.



> **You must legally own a copy of the game.** No ROM data is distributed with
> this project, in the repository or in any release. The recompiled C is
> generated locally from your own copy and is never committed.

## Status

Scaffolded on 2026-10-14 — **experimental port; runtime compatibility is unverified.** The layout, local build,
ROM regeneration pipeline, and packaging are wired up; the game does not run
until the host work in `src/game_rtl.c` is done. See
[Porting from here](#porting-from-here).

## ROM identity

| | |
|---|---|
| File | `Super Mario World 2 - Yoshi's Island (USA).sfc` |
| Publisher | Nintendo |
| Developer | Nintendo EAD |
| Year | 1995 |
| Mapping | lorom |
| Region | USA (North America) |
| Coprocessor | SuperFX (GSU-2) |
| CRC32 | `d138f224` |
| SHA-256 | `9b4957466798bbdb5b43a450bbb60b2591ae81d95b891430f62d53ca62e8bc7b` |

`tools/regen.sh` refuses to run against anything else, so a mismatched
revision fails immediately instead of producing subtly wrong output.

## Build

```sh
git submodule update --init --recursive
bash scripts/apply-framework-patches.sh
bash tools/regen.sh --rom /path/to/Super Mario World 2 - Yoshi's Island (USA).sfc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The framework and launcher are pinned submodules under `modules/`. The two
patches in `patches/` preserve this port's local framework changes; apply them
after initializing the submodules and before configuring the project.

The ROM does not have to live in the repository — keeping it on your own
drive is the better habit, and `SNESRECOMP_ROM` sets the path once for a
shell. `tools/regen.sh` also finds `Super Mario World 2 - Yoshi's Island (USA).sfc` at the repo root if you
prefer that; `.gitignore` blocks it from ever being committed either way.

`tools/regen.sh` verifies the ROM, generates `src/gen/*.c`, and re-syncs
`recomp/funcs.h`. Re-run it whenever you change anything under `recomp/`.

## Run

```sh
./build/"Yoshi's Island recomp"                     # launcher picks the ROM
./build/"Yoshi's Island recomp" "/path/to/Super Mario World 2 - Yoshi's Island (USA).sfc" # or name it and skip the launcher
```

With no ROM on the command line the build opens the recomp-ui launcher: a ROM
picker with a verification badge, plus display / audio / input settings. Your
choice is cached in `rom.cfg` beside the executable, so the next launch opens
on it and "Skip launcher on boot" makes it immediate. If the launcher is not
built in (`--no-recomp-ui`), the host falls back to a native file picker.

Either way the ROM is checked against the digests above before anything boots
— a dump that could not have produced this build is refused at the door rather
than mis-executing ten frames in.

## Layout

| Path | What lives there |
|---|---|
| `recomp/` | Analysis input: `bank*.cfg`, `symbols.toml`, generated `funcs.h` |
| `rom_identity.txt` | ROM digests — read by the build, `tools/regen.sh` and CI |
| `src/` | Host code you own: `main.c`, `game_rtl.c` |
| `src/gen/` | Generated C. Never committed — regenerate locally |
| `modules/snesrecomp/` | Framework submodule (owns `lib/recomp-net`, `lib/retcomm-rbengine`) |
| `modules/recomp-ui/` | Shared launcher submodule |
| `patches/` | Port-specific changes applied to the pinned framework and launcher |
| `tools/` | `regen.sh` — the ROM → C pipeline |
| `scripts/` | `package_release.sh` — player-facing zip |
| `framework_pins.txt` | Exact framework commits this project was scaffolded against |

## Porting from here

The scaffold stops where the game-specific work starts. In rough order:

1. **Make it boot.** `src/game_rtl.c` holds the frame driver. The generic
   LLE-first shape is there; a real title usually needs a frame boundary and
   NMI delivery that understand its own main loop. This is the bulk of the
   work.
2. **Name things.** Add entries to `recomp/symbols.toml` as you identify
   routines, then re-run `tools/regen.sh`. Set `emit = true` to promote one
   into ahead-of-time codegen; leave it false to keep it interpreted.
3. **Resolve dispatch misses.** After every run, deal with unresolved
   indirect targets before anything else — they are the reason a port
   diverges, and they are cheap to fix early.
4. **Never synthesise a result** to get past uncovered code, and never edit
   `src/gen/` by hand. Fix the config or the framework and regenerate.

## Multiplayer

Two SNES seats. Netplay is enabled for cooperative minigames; outside them, the game may ignore the second controller.

Netplay includes delay-sync and optional rollback (`SNES_NET_MODE=rollback`).

## License

This project's own source is under the license in `LICENSE`. The framework
carries its own terms — see `snesrecomp/LICENSE` and
`snesrecomp/THIRD_PARTY_ATTRIBUTION.md`. Neither covers the game data, which
is not distributed here.


## Hardware and online experiment

This ROM uses Super FX 2 (GSU-2). The project reuses the framework revision's Super FX LLE core and cartridge mapping, previously validated only with Star Fox; Yoshi's Island has not been qualified. The header indicates SlowROM, so the host does not force FastROM. Netplay with two standard SNES controllers is enabled for cooperative minigames in online sessions. The shared host keeps the session active outside minigames as well, where the game may ignore the second controller; no title-specific mode barrier has been added. The pinned revision builds rollback and selects it by default, but the frame driver's continuation state (`g_resume_pc`) is not part of the common snapshot. For the first online validation, force delay-sync on both peers with `SNES_NET_MODE=delay`. Rollback needs title-specific integration and validation before it can be considered reliable. The shared host forces effective VSync to 0 during online sessions and records the required breadcrumb, while preserving the offline preference.

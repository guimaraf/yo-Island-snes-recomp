# Yoshi's Island Recomp — Rendering and Troubleshooting Guide

Last reviewed: October 17, 2026.
Source baseline: port `c9e5aee`, framework `d01b7d1`.

This guide explains the rendering path implemented by this port and the
diagnostic lessons established while resolving its visual defects. It is a
maintenance reference for future investigations, not a complete description
of the original cartridge's graphics engine or a claim of hardware accuracy.

For game-specific GSU source formats, named routines, cartridge-RAM/HDMA
handoffs and candidate semantic probes from external reverse engineering, see
[External Super FX Research and Diagnostic Applications](YOSHIS_ISLAND_SUPERFX_RESEARCH.md).
That review distinguishes community annotations from local implementation
and runtime evidence; it does not change the policies described here.

Use [_errorRender.md](_errorRender.md) for individual issue histories,
validation scope, executable hashes and capture provenance. Its current
resolution paragraphs supersede the earlier plans preserved in that file.
ER-08 remains open. The other recorded issues are resolved in their tested
scenarios; some adjustments remain opt-in build variants.

## Navigation

- [Rendering stages and state ownership](#2-separate-the-rendering-stages)
- [Visible-line ordering](#3-the-exact-visible-line-order)
- [Build and scroll policies](#4-understand-the-three-scroll-policies)
- [Lessons from each issue](#6-what-each-resolved-issue-teaches)
- [Capture selection](#7-choose-a-capture-that-fits-the-scene)
- [Telemetry and integrity checks](#8-interpret-the-records-before-interpreting-the-defect)
- [Next-report decision path](#9-a-practical-decision-path-for-the-next-visual-report)
- [Regression controls](#10-regression-controls-and-evidence-limits)
- [Source and evidence index](#11-source-and-evidence-index)

## 1. Establish which implementation is actually running

The supported source identity is **Super Mario World 2: Yoshi's Island (USA)**,
as specified in [rom_identity.txt](rom_identity.txt). Its expected SHA-256 is
`9b4957466798bbdb5b43a450bbb60b2591ae81d95b891430f62d53ca62e8bc7b`.
The project uses Super FX/GSU emulation, but the presence of that coprocessor
does not identify the source of a rendering artifact.

Before diagnosing a new report, record:

- Port commit, dirty diff, framework gitlink, framework HEAD and local diff.
- Pins in [framework_pins.txt](framework_pins.txt), including nested modules.
- The framework root actually selected by CMake. This checkout selects
  `modules/snesrecomp`; do not silently substitute the other `snesrecomp/`
  directory or another game's framework.
- Executable path, SHA-256, process start time and build options. A copied
  script does not add its receiver to an old executable.
- Scene, exact object, reproduction steps, effective renderer, presentation
  backend, scale, filters, widescreen, mods, speed and input configuration.

At this baseline, `snesrecomp` is pinned to
`d01b7d14e8323e1712876ee6acfa969c6f20a206`. The launcher gitlink/checkout is
`529ba37747529db90be592378b2438400a6a41d4`, while its text pin remains
`15c76d386bb114709292e76702ac2733adceb1dd`. This existing discrepancy is a
provenance detail, not evidence of a rendering cause; do not update unrelated
dependencies as part of visual triage.

The README retains scaffold-era status text, and some older capture documents
still describe experiments as pending. Use current sources, build metadata
and the latest issue closure to resolve those differences.

## 2. Separate the rendering stages

The useful model is a sequence of state production, reconstruction and
presentation. A defect can originate at any of those boundaries.

```text
GameRunOneFrame
  -> beam-aligned guest execution, interrupts and coprocessor progress
  -> display/memory history associated with PPU fields
  -> field boundary at V=225; HDMA source snapshot at line 0

GameDrawPpuFrame
  -> save live guest state and restore the field selected for reconstruction
  -> replay line state + execute the real HDMA unit
  -> apply the selected scroll-consumption policy
  -> renderer A or B draws the native line
  -> observer records the consumed state and native pixels
  -> restore this slot's real HDMA scroll result
  -> restore the live guest state after the entire render walk

Desktop presentation
  -> backend, scaling and window/GPU output
```

The HDMA snapshot and field-boundary notifications occur in the beam path;
they are not performed by an image exporter. The diagram is an integration
model, not an instruction to reorder beam events.

### 2.1 One beam owns guest time

[src/game_rtl.c](src/game_rtl.c) delegates execution to
`snes_beam_frame_driver_run_frame()`. The framework derives the field endpoint
from the shared beam, rather than advancing a second independent beam or
guessing a CPU budget that happens to resemble a frame.

The driver keeps interrupt delivery in the execution half. Coprocessor and
device synchronization belong to that timeline. Drawing reconstructs display
state while guest execution is parked; observing GSU counters on every output
line does not mean the GSU executed separately for each reconstructed line.

The change from the old custom driver eliminated full-screen intro blackouts
in the user's tests. Preserve this baseline when investigating a localized
artifact. See the matching framework
[frame-model contract](modules/snesrecomp/docs/FRAME_MODEL_HOSTS.md).

### 2.2 A host call, a field and a presentation are different identifiers

The journal closes fields when the beam reaches V=225. The capture frame ID
is the port's call counter, not an independently authenticated original-game
frame number. The presented image is another stage and is not captured by
the current native collector.

A register value observed after guest execution can belong to a later field
than the one being drawn. Always record which field is consumed and which
phase produced a sample. Guest V/H values parked during drawing do not locate
the reconstructed output row by themselves.

### 2.3 Registers and graphics memories need coherent history

The raster implementation in
[ppu.c](modules/snesrecomp/runner/src/snes/ppu.c) maintains field baselines and
bounded histories for display registers, OAM and committed VRAM/CGRAM updates.
`ppu_rasterApplyLine()` reconstructs the state appropriate to the output line.
The AOT `WriteVramWord` path also participates in the memory history.

Replaying old coordinates while reading a newer pose's artwork or palette is
not a coherent reconstruction. ER-04 demonstrated why recording scroll or
OAM alone was insufficient. Memory history is scanline-granular; it does not
establish arbitrary sub-scanline accuracy. A caller-owned `renderVram` binding
must also be distinguished from the internal reconstructed VRAM.

### 2.4 HDMA sources must belong to the same field

ER-05 exposed a different mismatch: completed-field display registers were
combined with live WRAM HDMA tables and DMA configuration prepared for another
stage. The implementation now snapshots WRAM and DMA state at beam line 0,
associates them with the field, uses them during reconstruction and restores
the live guest RAM/DMA state afterward.

This is a field source snapshot, **not a per-scanline WRAM-write journal**.
An effect that rewrites HDMA tables during visible execution needs its own
investigation. `hdma_sources_valid=1` confirms snapshot availability, not that
every possible source mutation is represented.

### 2.5 RenderEnd must restore the live guest state exactly

Rendering temporarily changes registers, memory, DMA state and write latches.
`ppu_rasterRenderEnd()` restores the saved live state rather than treating
the final drawing registers as the next guest baseline.

ER-06 made this distinction observable: writes on line 224 were outside the
visible replay through line 223, but still had to survive as guest state.
Losing them caused alternating layer masks and selector flicker. When a
defect appears on every other frame, compare pre-render, post-render and next
baseline state before adding buffer clears or frame delays.

## 3. The exact visible-line order

The authoritative loop is
[beam_frame_driver.c](modules/snesrecomp/runner/src/beam_frame_driver.c),
`snes_beam_frame_driver_draw_ppu_frame_observed_deferred_scroll()`.

1. Begin reconstruction and apply pre-visible journal entries, including
   HDMAEN, before initializing HDMA.
2. For physical lines 1–224, apply journal entries through `line - 1`.
3. Determine which selected scroll ports the active HDMA slot will write.
   Descriptor reloads, terminated channels and held values must not be
   mistaken for a new deferred write.
4. Save pre-slot scroll, then execute `dma_doHdma()`.
5. Retain the real HDMA results, temporarily selecting preceding values for
   the axes covered by the current policy.
6. Call `ppu_runLine()` and then the read-only observer.
7. Restore the real HDMA scroll results for the following line.
8. Finish with `ppu_rasterRenderEnd()`.

Some comments describe the observer as being before H-blank HDMA. For this
implementation, the current slot's HDMA already ran before drawing. The
observer sees the state used by the line **before temporary scroll is undone**.
This ordering matters more than that older wording.

ER-03 established the existing HDMA-before-render ordering and removed separate
first-line HDMA priming. Do not move all HDMA after drawing to fix one effect;
the later VOFS policies are selective consumption changes within this order.

## 4. Understand the three scroll policies

Masks use bits BG1=`01`, BG2=`02`, BG3=`04`, BG4=`08`.

| Build/policy | Deferred VOFS policy | Paired HOFS policy | Lifetime |
| --- | --- | --- | --- |
| Normal build, no experiment | `04` | `00` | Existing BG3 policy throughout play |
| `YI_DELAY_BG2_TEST=ON`, pair option OFF | `06` | `00` | Continuous BG2 and BG3 vertical delay |
| `YI_ER12_BG3_PAIR_TEST=ON` | `06` | `04`, conditional | Continuous; requires the BG2 option ON |
| Historical boss `DelayBG2` request, normal build | Adds BG2 to existing BG3 | `00` | While the experiment is armed/active; ends with the timed request |

Both build options default to OFF in [CMakeLists.txt](CMakeLists.txt).
Versioning a validated test build did **not** promote its policy to the normal
default. The pair-build name is `Yoshi's Island recomp.ER12-BG3Pair-test.exe`.

- **ER-10 / BG3 VOFS:** delaying active HDMA BG3VOFS by one line removed the
  tested passage/weight-bar and descending-door corruption.
- **ER-07 / BG2 VOFS:** adding BG2 removed the external boss lines in the
  complete user-tested fight. Returning lines outside earlier capture windows
  did not test the continuously enabled option.
- **ER-12 / paired BG3:** retaining previous VOFS while consuming current HOFS
  mixed coordinates from two HDMA slots. The pair variant also delays HOFS,
  only when the same slot writes both axes and VOFS is deferred. It consumes
  the preceding pair without changing the actual HDMA transfer or latches.

These are validated policies for specific effects in this integration, not a
universal SNES timing rule. ER-12 is a reminder that a successful adjustment
to one coordinate can expose inconsistency with another coordinate later.

`phase_line` records the requested vertical policy, not transfer activity.
`phase_h_line` records horizontal policy **and actual application** separately.
Use `video_state` to establish the values really consumed.

The older boss script expects `04` for `Baseline`. A continuous BG2 build can
still consume `06` under that request; an acknowledgment describes the request,
not an override of the compiled option. Likewise, `-BG3PairTest` checks the
pair build; it does not turn pairing on and off inside the process.

## 5. Renderer identity, layers and composition

Keyboard **R** toggles the internal PPU renderer: A/legacy=`0`, B/new=`1`.
This is different from SNES controller R and from the desktop presentation
backend, such as Vulkan or OpenGL. `Padrao`, `UmaTrocaR` and executable names
are procedure labels, not evidence of the effective renderer.

Use `row.renderer` on each valid line. In the second ER-12 capture, the user
initially attributed the final segment to B, but it was A after frame 5897.
In ER-09, a same-run switch exposed a renderer-specific interpretation.

Do not assign visual objects to hardware layers by appearance:

- An object called a sprite may be a background effect. The pink rotating
  bar is also distinct from the nearby orange-block walkway.
- In captured Mode 1 bar scenes, `TM=11`, `TS=06`, `CGWSEL=22` and
  `CGADSUB=20` show why main/subscreen composition must be examined.
- A BG's raw nontransparent pixels may lose priority, be windowed, or
  participate in color math rather than appear directly in the final image.
- `obj_line` describes the native OBJ buffer before BG/window/math composition.
  Its occupancy does not identify an object or prove final visibility.
- In Mode 2, BG3 can supply offset-table data for other backgrounds. An invalid
  Mode 1-only `bg3_sample` is not evidence that BG3 is irrelevant.

Decode more than the mode number. For example, captured `BGMODE=69` selects
Mode 1 with large tiles for BG2/BG3 in the current PPU model; it is not
interchangeable with `BGMODE=01`. Follow the applicable big-tile/mosaic policy
in `ppu.c` before assuming that an optimized 8x8 path drew the affected layer.

ER-09 was an addressing/interpretation fix, separate from timing: renderer B's
Mode 2 offset path selected the wrong table rows/column and applied a vertical
offset with an extra increment. The fix corrected those calculations in
`PpuDrawBackground_4bpp_opt`, without changing guest camera or physics.

## 6. What each resolved issue teaches

The table groups lessons without assuming a common cause. Full evidence and
historical IDs remain in [_errorRender.md](_errorRender.md).

| Case | Established correction or result | First useful check for a similar future symptom |
| --- | --- | --- |
| Intro full-screen blackouts, before numbered local artifacts | Beam-aligned driver replaced custom timing; user's intro validation and bounded trace | Beam ownership, field boundary, IRQ delivery and guest clocks during draw |
| ER-01, sky band | Resolved in repeated user-tested opening; no separate isolated mechanism established in its closure | Locate affected rows, then compare effective state and memory; do not invent a unique cause from its status |
| ER-02, transformation tile corruption | Resolved in user-tested animation; not proof that GSU was the cause | Before/during/after frames, upload metadata and consumed memory history |
| ER-03, title first line | HDMA before scanline rendering; removed extra priming; framework `cc8c0ae`, port `46fc4fa` | First visible line versus HDMA initialization and first transfer |
| ER-04, object/scenery drift and subsequent character flicker | Coherent OAM plus VRAM/CGRAM raster history; framework `97111a4`, port `7264416` | Baseline/commit history for position, graphics and palette together |
| ER-05, premature text-box appearance | Field-associated WRAM/DMA source snapshot and pre-init HDMAEN replay; framework history includes `ba659c5` | Table/config provenance as well as windows and logical inputs |
| ER-06, alternating selector output | Exact guest-state restoration, including late line-224 writes; `493f08b` | RenderBegin/RenderEnd/next-field masks and latches |
| ER-07, external boss lines | Selective continuous BG2 VOFS delay in named test build | Actual BG2 source coordinate, consumed phase and experiment lifetime |
| ER-08, Bonus text-box line | Still open; no sufficient capture | Identify message/scene and capture before assigning a layer or cause |
| ER-09, displaced stage 1-7 terrain | Mode 2 offset-table interpretation; framework `5b18d6c`, port `00acb61` | Effective mode; matched A/B state, table rows/column and fine scroll |
| ER-10, passage/door background corruption | Selective BG3 VOFS delay; original captures carry the older ER-07 label | Consumed vertical phase in the exact interaction; do not confuse it with the boss |
| ER-11, bright title horizon | User-confirmed disappearance and static post-fix image; no separately isolated cause | Native/reference horizon region; keep separate from ER-03 |
| ER-12, rotating-bar lateral extensions | Paired BG3 HOFS/VOFS consumption; framework `d01b7d1`, port `c9e5aee` | Whether the two axes belong to the same HDMA slot |

### Two particularly useful controlled comparisons

**ER-09:** frames 2726/B and 2727/A in
`G:\x2\yoshi-render-ER09-Placa-Padrao-20261017-103711-40d92b07`
had identical captured registers on all 224 lines and matching hashes for all
16 consumed-VRAM pages. Yet 208 output-line hashes changed and the terrain
shifted immediately. Movement inputs were zero around the switch. This
isolated interpretation far better than the visual impression of bad collision.
OAM/CGRAM equality and complete-state identity were not established.

**ER-12:** the before/after pairs 5682/5453, 5685/5459, 5691/5469 and 5720/5515
shared identical BG3 vertical sequences over 223 comparable lines. The old
variant had current horizontal/previous vertical consumption; the pair build
had both axes from the previous slot, and the abnormal protrusions disappeared.
The post-fix capture has 300 B frames, 5243–5542, and 62,700 applied-pair lines.
Camera, objects and some memory hashes differed, so these are comparable
scroll states rather than identical guest snapshots. The user then switched
renderers at the bar and during the boss fight without ER-12 or ER-07 returning.
That later playtest was not a recorded complete-fight capture.

## 7. Choose a capture that fits the scene

| Existing collector | Preparation / recording | Scope and important behavior |
| --- | --- | --- |
| `scripts/capture-render.ps1` | 5 s / 2 s | Generic older timed receiver; optional ER-04/ER-05 probes |
| `scripts/capture-render-er05-10s.ps1`, `capture-render-er05-inputs-10s.ps1` | 5 s / 10 s | Text opening and logical input correlation; identify the actual message |
| `scripts/capture-render-er07.ps1` | 5 s / 10 s | Historical passage/door case, now ER-10; explicit renderer selection |
| `scripts/capture-render-er09.ps1` | 5 s / 10 s | Passive stage 1-7 collector; fixed scene labels |
| `scripts/capture-render-er07-boss.ps1` | 5 s / 20 s by default; 10 s option | Boss zoom/hits; optional renderer override and BG2 experiment |
| `scripts/capture-render-er12.ps1` | 5 s / 5 s | Rotating bar; passive renderer selection; optional pair-build requirement |
| `tools/render-capture.ps1` | Historical 8-before/16-after circular mode, or scheduled trigger | Launches a process; not equivalent to attaching the timed collector |

Read the matching script and receiver before using one for a new issue. Do
not silently label a new scene as the boss, ER-09 or a dialogue box. In
particular, the boss collector is not automatically suitable for a bar near
lava. Budget and capabilities belong to the receiver compiled into the EXE.

For the validated ER-12 pair build, from the test directory:

```powershell
.\scripts\capture-render-er12.ps1 -BG3PairTest
```

This requires the already-open pair-build process and then waits for Enter
in the console. Five seconds allow returning to the game; the following five
seconds are recorded. **There is no pre-trigger history in this timed mode.**
Start before the defect, include the interaction and retain the aftermath.
If it does not fit, prepare a closer start point or design a longer bounded
window; retain the unsuccessful folder as well.

The ER-12 cap is 360 complete frames within a 512 MiB allocation budget;
approximately 300 frames at 60 Hz is an expectation, not a required count.
The analyzed pair build allocated 407,511,360 bytes. A previous shorter
allocation cannot automatically serve a larger request in the same process;
the receiver can require a restart for a longer window.

Wait for export completion before repeating or closing. Keep exactly one
intended process, or supply its PID. Do not replace its EXE on disk while it
is running. A disk hash cannot authenticate a previously loaded image.
Portable scripts copied to `G:\x2\scripts` may have empty source hashes because
that location is not the repository; retain the corresponding build manifest.

Use normal manual controls. Do not inject input, change mappings or capture
keys globally. Keep turbo, rewind, save/load and other execution changes out
of comparisons. Record focus loss and pauses; they can reduce the retained
frames. Preserve every full folder, including failed runs, manifests, notes,
markers and images. Never export ROM, tile payloads or proprietary memory dumps.

## 8. Interpret the records before interpreting the defect

| Record/file | What it establishes | What it does not establish |
| --- | --- | --- |
| `session.json`, build JSON, `capture.armed` | Requested configuration, executable identity and runtime receiver acknowledgment | That filenames or source hashes authenticate the loaded process by themselves |
| `native-XXXXXXXX.ppm`, `row` | Native pixels and per-line state keyed by port frame/output row | Final GPU presentation or a reference emulator frame |
| `video_state`, `regs64` | Consumed mode, scroll, masks, windows, math and other captured PPU registers | Original hardware's intended state or guest camera coordinates |
| `consumed_vram` | Page hashes of the memory selected for rendering, field/cursor information and losses | Raw equality proof, exact tile identity or an upload's visual responsibility |
| `phase_line` | Deferred vertical policy mask | A write or delay applied on every line |
| `phase_h_line` | Paired horizontal policy and actual applied mask | A universal rule for nonselected layers |
| `obj_line` | OAM/CGRAM hashes, OBJ buffer hash/occupancy, flags | Final visible object identity, or independent per-line overflow counts |
| `bg3_sample` | Restricted Mode 1 BG3 sampling before composition, when valid | A final composition winner; validity in Mode 2 or relevant mosaic cases |
| `event`, `dma_event`, `hdma_sample` | Register/transfer metadata with source and phase | Arbitrary per-byte hardware timing or a single shared coordinate system |
| `raster_probe`, `memory_hash` | Samples at named boundary/render phases and region hashes | Complete unbounded history |
| `motion_probe`, `obj_phase`, `obj_slot` | Bounded slot geometry and signatures | Stable object identity, collision or physics; slots can be reused |
| `pad`, `keyboard_r`, `r_summary` | Logical SNES masks and separately recorded window R events | Physical input timing, an unmonitored key, or A/B without `row.renderer` |
| `gsu_video`, `job_recent` | GSU configuration/counters and retained recent jobs | Every historical job; eviction completeness is explicitly unknown |
| `capture.done`, `summary` | Export completion and reported I/O status | Visual success or absence of every kind of loss |

### Line coordinates are not interchangeable

`row`, `video_state` and `phase_*` use output y=0–223. The driver callback's
`line` is 1–224 and writes framebuffer row `line - 1`.

`event.drawLine` is exported from `trace_line`, not from `row.y`. In the
current draw walk, after the first observer, the following slot's events
normally carry the one-based driver line. For such slots, pair output y with
draw-event coordinate y+1. The initial slot/pre-visible events have special
initialization labeling; CPU-phase events use a different context. Inspect
`rc_event`, `trace_drawn_line` and phase/source before joining records. Do not
apply an unconditional minus-one conversion to all event types or phases.

The local PPU source enumeration is CPU=0, DMA=1, HDMA=2, replay=3. A register
write during reconstruction is not new guest execution. The observer records
temporary consumed scroll before restoration; a later write or post-render
probe is not automatically the value that produced the pixel.

### Verify image/telemetry association independently

1. Check frame IDs, gaps, all expected images and dimensions, then 224 valid,
   unique output rows per full native frame.
2. Compare exact `(frame, y)` key sets for the enabled per-line probes, not
   only equal record counts. Older builds do not have `phase_h_line`.
3. Check frame-cap drops, invalid rows, PPU event loss, DMA loss, raster probe
   loss, memory-journal drops, object probe loss, keyboard R loss and I/O
   failures separately. Define whether losses overlap the interval used.
4. Verify renderer and policies per line, including any mid-window changes.
5. Recompute native line hashes when association matters. `row.hashBGR` is
   FNV-1a starting at 2166136261, consuming B,G,R bytes per pixel with
   multiplication by 16777619 modulo 2^32. PPM stores R,G,B; reverse each
   pixel's channel order for that comparison. Do not hash the PPM header.
6. Locate the last normal, first affected and following frames. Preserve
   native coordinates and enlarge with nearest-neighbor sampling for review.

Zero recorded loss is scoped to the available counters. Zero frames or a
capture starting after the event cannot demonstrate a successful fix.

## 9. A practical decision path for the next visual report

1. **Identify the object and symptom.** Ask for the exact room, object, side,
   action and first affected frame. ER-12 briefly targeted the orange walkway
   after an ambiguous reply; the supplied image corrected the target. Keep
   visual appearance separate from the suspected layer.
2. **Authenticate the run.** Distinguish normal, continuous-delay and timed
   experimental builds. Confirm whether an out-of-window report used the
   same active policy.
3. **Validate the collector.** Use consecutive native images with matching
   consumed-state records before recommending more instrumentation.
4. **Localize the divergence.** If native output is correct but the window is
   wrong, investigate presentation. If both renderers fail under the same
   policy, prioritize shared reconstruction/consumption without ruling out
   independent renderer defects. If one differs under matching consumed
   state, inspect interpretation as in ER-09.
5. **Follow the relevant mechanism.** For alternating whole frames, inspect
   restored guest state. For correct geometry with wrong artwork/palette,
   inspect memory history. For premature effects despite correct registers,
   inspect HDMA source-field provenance. For line-local edge distortions,
   compare writes with the axes actually consumed, not just the final values.
6. **Run one controlled change.** Preserve the previous EXE and complete
   capture folders. Use an explicit opt-in variant, bounded probes and a
   distinguishable output name. Keep unrelated clocks, GSU behavior,
   controls and presentation settings unchanged.
7. **Validate the target and affected controls.** Separate compile/link,
   synthetic checks, capture evidence and user playtest. Document the exact
   scope before calling the issue resolved or promoting a default.

Do not clear framebuffers, hide edge pixels, add a global scroll bias or
retime the guest merely because an artifact disappears. A visually successful
workaround must still preserve the state's temporal coherence and guest
restoration contract.

The user compares against **Bnes**. Record its version, settings and equivalent
animation/input sequence; do not substitute a presumed emulator/core name or
align solely by absolute frame number since boot.

## 10. Regression controls and evidence limits

Choose controls according to the change, rather than replaying every scene
indiscriminately:

| Change | Relevant gameplay controls |
| --- | --- |
| Beam/frame ownership | Intro blackouts, transition into gameplay, clocks/IRQs at the field endpoint |
| Register/OAM/VRAM/CGRAM reconstruction | Stationary object with scrolling camera, ball/block interactions, character flicker |
| HDMA source snapshots or initialization | Full text opening, first visible line, transitions and selector return |
| RenderEnd restoration | Level completion/selector and late-field writes across consecutive fields |
| Mode 2 addressing | Stage 1-7 plate/terrain path, fine scroll and both backgrounds |
| BG2/BG3 consumption policies | Passage weight bar, descending castle door, complete boss growth/combat/shrink, both rotating-bar ends under both renderers |

The recorded ER-12 follow-up validates the bar under renderer switching and
reports no ER-07 recurrence during boss combat. It does not separately repeat
the original ER-10 passage/door cases or authenticate every ending animation.
Broader validation is appropriate before promoting the build options to the
normal default. ER-08 still needs its own identified scene and capture.

Existing ROM-free test sources provide useful starting points:

- `modules/snesrecomp/tests/ppu/ppu_raster_journal_test.c`: field/replay state,
  memory history, HDMA source isolation and restoration cases.
- `modules/snesrecomp/tests/ppu/ppu_sprite_limit_test.c`: includes the Mode 2
  offset regression cases despite its filename.
- `modules/snesrecomp/tests/ppu/ppu_bg2_boundary_test.c`: synthetic BG2
  addressing boundaries under A/B; does not exercise the real HDMA phase policy.
- The world-mirror, elastic-band and Mode 7 HD test files in the same directory
  are broader neighboring-renderer controls used in earlier fixes.

Historical passing results belong to their recorded revisions. Do not claim
these tests were rerun for ER-12: that change had compile/link, capture and
user-playtest validation, without a new standalone suite run. Build or test
execution still requires authorization under the project's working rules.

## 11. Source and evidence index

| File | Responsibility |
| --- | --- |
| [src/game_rtl.c](src/game_rtl.c) | Port hooks, selected scroll masks, renderer override lifetime and observer integration |
| [src/render_capture.h](src/render_capture.h) | Receiver, timed buffers, images, telemetry schemas, export and loss reporting |
| [beam_frame_driver.c](modules/snesrecomp/runner/src/beam_frame_driver.c) / [.h](modules/snesrecomp/runner/src/beam_frame_driver.h) | Beam endpoint, HDMA/replay ordering and selective consumed-scroll policies |
| [ppu.c](modules/snesrecomp/runner/src/snes/ppu.c) / [ppu.h](modules/snesrecomp/runner/src/snes/ppu.h) | Raster history, state restoration, renderer implementations and composition |
| [snes.c](modules/snesrecomp/runner/src/snes/snes.c) | Beam-side field boundary and line-0 HDMA source snapshot notifications |
| [dma.c](modules/snesrecomp/runner/src/snes/dma.c) | Real DMA/HDMA channels, initialization and transfer execution |
| [common_rtl.c](modules/snesrecomp/runner/src/common_rtl.c) | Shared integration, including AOT graphics-memory writes |
| [superfx.c](modules/snesrecomp/runner/src/snes/superfx.c) | GSU emulation and existing job/counter observations; not an automatic diagnosis |
| [CMakeLists.txt](CMakeLists.txt), [framework_pins.txt](framework_pins.txt) | Variant defaults, selected implementation and revision provenance |
| [_errorRender.md](_errorRender.md) | Canonical per-issue evidence, hashes, validation and historical renumbering |
| [ER12-capture.md](scripts/ER12-capture.md), [ER07-boss-capture.md](scripts/ER07-boss-capture.md), [ER09-capture.md](scripts/ER09-capture.md) | Collector-specific usage; read experiment statements against the current issue closure |
| [FRAME_MODEL_HOSTS.md](modules/snesrecomp/docs/FRAME_MODEL_HOSTS.md), [SDL_BACKENDS.md](modules/snesrecomp/docs/SDL_BACKENDS.md) | Matching framework integration and presentation documentation |

Important local evidence anchors are preserved under `G:\x2`:

- `yoshi-trace-20261014-202555`: bounded intro trace after the driver change.
- `yoshi-render-20261015-095903-0001eecf`: ER-06 post-fix restoration evidence.
- `yoshi-render-20261015-235732-d47fba32`: ER-05 pre-fix input-correlated sequence.
- `yoshi-render-ER09-Placa-Padrao-20261017-103711-40d92b07`: strong same-run A/B control.
- `yoshi-render-ER09-Placa-Padrao-20261017-104621-232a4556`: ER-09 post-fix sequence.
- `yoshi-render-ER07-Boss14-Zoom-Padrao-B-DelayBG2-20261017-150256-e28744d3`: boss phase experiment.
- `yoshi-render-ER12-BarraLava-Padrao-20261017-201855-3445ad22`: original B bar sequence.
- `yoshi-render-ER12-BarraLava-UmaTrocaR-20261017-203333-410ca730`: second run, B then A.
- `yoshi-render-ER12-BG3Pair-BarraLava-Padrao-20261017-204258-13658d49`: validated pair-build capture.

These directories are external artifacts, not bundled repository fixtures.
Earlier ER-04 folders explicitly deleted by the user cannot be revalidated;
the remaining notes about them are historical analysis. Preserve surviving
folders whole and record new evidence without rewriting its original identity.

For every future resolution, add the observation, first divergent state,
controlled change, exact revision/build, native evidence, validation scope and
remaining uncertainty to the error log. Update this guide only when the case
establishes a reusable mechanism or a new limit of the diagnostic method.

# Yoshi’s Island — Rendering Artifacts Log

Initial record: October 14, 2026. Project: **Super Mario World 2: Yoshi’s Island (USA)**.
Purpose: maintain a list of observed issues, their evidence, and validation
criteria so each occurrence can be investigated and resolved separately.

This document is in the workspace root. The seven images are in
`Yoshi's Island recomp_errorRender/`, also outside the port directory.
The images are complete copies of the attachments provided by the user, without
cropping or editing. Their numbering preserves the order in the message, not
necessarily the video's chronological order.

## Evidence context and limitations

- The user confirmed that full-screen flickering disappeared in the intro after
  replacing the custom driver with the local `beam_frame_driver`.
- The capture at `G:\x2\yoshi-trace-20261014-202555` measured frames 640–699:
  no completely black frames, one V=225 boundary per call, no recorded drops,
  and no CPU clock advancement during drawing.
- That run's report recorded 4,807 simulations and presentations in
  approximately 80.35 seconds. Detailed telemetry covers only the configured
  interval, not all 80 seconds.
- The artifacts listed below remain open. Fixing the flicker does not
  demonstrate that every rasterization detail is correct.
- The user also tested the emulator named **Bnes** for comparison. No version or
  configuration for that emulator, identified reference/recomp frame pairs, or
  equivalent frame numbers were provided. Therefore, the captures are not
  attributed to either emulator here, and no difference from the original is
  claimed as proven. Preserve the reported spelling; do not assume another core.
- Static frames show appearance but do not prove intermittency, duration, or
  temporal sequence. Those aspects are recorded as user reports.
- The hypotheses below guide measurements; none is a confirmed cause. Super FX
  is part of the execution path, not an automatic explanation.

## Tracking list

| ID | Scene / symptom | Evidence | Status |
| --- | --- | --- | --- |
| ER-01 | Dark horizontal band near the bottom of the sky/clouds | Images 1 and 2; reported intermittency | Open |
| ER-02 | Block corruption while the enemy approaches or leaves | Image 3 for comparison and affected image 4 | Open |
| ER-03 | First line of the title screen has pixels with discrepant colors | Image 5; reported intermittency | Open |
| ER-04 | Fixed bright horizontal line behind the title-screen mountains | Image 5; reported persistence | Open |
| ER-05 | Complete text box appears before the opening animation | Images 6 and 7; temporal order described by the user | Open |

## ER-01 — dark band in the intro sky

**What the images show:** there is a horizontal discontinuity near the bottom
of the scene area, above the lower border. In image 1 it appears as a black
line between cloud regions; image 2 shows a band with a change in tone/pattern,
including darker and black areas. The border and captions remain visible: this
is a localized defect, unlike the full-screen blackouts already addressed.

**Temporal report:** the line flickers during almost the entire presentation
of this moving scene. Sometimes only a few pixels darken.

**Hypotheses to distinguish:** raster/HDMA state transition at the wrong
vertical position; window, color math, or layer change; tile/palette read at a
time inconsistent with the rest of the line. There is not enough data to choose.

**Next evidence:** consecutive frames with and without the band in the same
scene, identified by run and frame; applied state, line hashes, and display
writes around the band. Also compare against a reference sequence.

**Resolution criterion:** the full scene animation plays without the additional
intermittent band while preserving transitions present in the reference.

![Image 1 — black line behind the clouds](<Yoshi's Island recomp_errorRender/imagem01_YoshisIsland_errorRender.png>)

![Image 2 — band with darkened pixels and black areas](<Yoshi's Island recomp_errorRender/imagem02_YoshisIsland_errorRender.png>)

## ER-02 — corruption during the enemy's transformation

**What the images show:** image 3 shows the small enemy, recognizable in the
scene. In image 4, large areas of the scene contain blocks and graphic
fragments, including parts that resemble the enemy. The caption and border
remain legible. The corruption should not be reduced to simple sprite
disappearance.

**Temporal report:** the enemy appears in the background, grows, and the final
frame of that animation seems to corrupt its tiles. Image 3 was identified as
unaffected in the enemy's appearance; this does not establish that ER-01 is
absent from that scene.

**Hypotheses to distinguish:** graphic content updated during a transition;
timing between GSU production, transfer, and VRAM reads; or renderer addressing
or mode state. There is no proof that image 4 contains only enemy tiles or that
GSU execution is the cause.

**Next evidence:** a short sequence before, during, and after the event; hashes
by memory region, transfer metadata, and GSU job cadence, without exporting ROM
content or tile dumps. Correlate these with the native frame.

**Resolution criterion:** the complete animation and final transition have no
additional fragmentation compared with the reference.

![Image 3 — enemy recognizable before the reported defect](<Yoshi's Island recomp_errorRender/imagem03_YoshisIsland_errorRender.png>)

![Image 4 — block fragmentation across the scene](<Yoshi's Island recomp_errorRender/imagem04_YoshisIsland_errorRender.png>)

## ER-03 and ER-04 — two distinct lines on the title screen

**ER-03, observation:** a line of discrepant pixels appears at the top edge of
the game image. The user reports that it alternates with a normal line.

**ER-04, observation:** a thin bright/yellowish line crosses the background at
the horizon, behind the mountains. The user reports that it is fixed and does
not flicker.

Treat these as separate cases: their different positions, persistence, and
appearance do not justify assuming a single cause.

**ER-03 hypotheses:** first-line initialization, framebuffer origin/pitch,
raster state applied to the first line, or updated content. **ER-04
hypotheses:** raster transition involving layers/palette/color math, or a window
boundary. A detail present in the reference still needs to be ruled out.

**Next evidence:** title-screen pairs with ER-03 present/absent and an
equivalent reference for ER-04; compare the native line with the presented
image to separate rendering from scaling/presentation. Do not infer the exact
SNES scanline from its position in a scaled capture.

**Resolution criteria:** ER-03 has no intermittent residual pixels; ER-04 has
color continuity compatible with the reference, without removing legitimate
detail.

![Image 5 — discrepant first line and bright horizon line](<Yoshi's Island recomp_errorRender/imagem05_YoshisIsland_errorRender.png>)

## ER-05 — text-box contents appear too early

**What the images show:** image 6 contains a large green box with its text
already visible. Image 7 contains a large black rectangle and only a small
green region in the center, with fragments of letters. In isolation, each
image could belong to a different animation stage; the temporal defect depends
on the report and a future frame sequence.

**Temporal report:** the expected behavior is for the black window to grow
first, followed by the colored box and text expanding. In the recomp, the full
box and text appear on the first frame, disappear, and then the opening
progresses. Image 7 represents a later stage where the colored area begins to
grow.

**User-suggested hypothesis:** an image already present in the buffer. Record
this as a hypothesis, not a conclusion. Other possibilities include window or
mask state from a different moment than the content, upload/replay ordering, or
a line that was not redrawn. Do not apply buffer clearing or a latch indiscriminately.

**Next evidence:** a continuous sequence starting before the opening, including
the first frame where the box appears, its disappearance, and its growth;
track windows, layers, transfers, and line hashes. Compare against the same
opening in the reference.

**Resolution criterion:** no premature appearance, with opening order and
geometry compatible with the reference.

![Image 6 — complete box identified as appearing too early](<Yoshi's Island recomp_errorRender/imagem06_YoshisIsland_errorRender.png>)

![Image 7 — black area with a small colored region during the opening](<Yoshi's Island recomp_errorRender/imagem07_YoshisIsland_errorRender.png>)

## Gameplay observations and limits on generalization

The user reports that the text-opening corruption did not occur in gameplay
boxes and that no corruption was noticed on the pause screen. Keep these as
comparison controls; do not conclude that every box, stage, or pause state has
been validated. ER-05's visual scope is the text transitions in the opening
sequence observed by the user.

## Suggested investigation order

1. **ER-03:** isolate the first line; it is spatially small and easy to compare
   in the native framebuffer and the presented image.
2. **ER-01 and ER-04:** measure the horizontal bands separately; share the
   raster-observation method without assuming a shared cause.
3. **ER-02:** capture the enemy transition and verify upload/content consistency.
4. **ER-05:** capture the order of window opening versus content availability,
   using gameplay boxes with no reported issue as a comparison.

This order is a diagnostic proposal, not a required priority. If one event is
easier to reproduce, start with the most complete evidence.

## Minimum data for each new capture

- Error ID; executable/revision; settings; scene and input sequence.
- Source of each image: recomp or reference emulator, version, and settings.
- Previous, affected, and following frame, with a counter or video timestamp.
- Defect region and duration; whether it occurs every time or depends on the run.
- A short capture configured for the scene: the 640–699 interval used for the
  initial intro does not automatically cover the title screen, later text, or gameplay.

Preserve previous captures in `G:\x2`. Do not export the ROM or raw proprietary
memory contents to this workspace. The images in this log were explicitly
provided by the user to document the symptoms.

## Future follow-up plan — not yet implemented

**Status at the end of this phase:** documentation only. No key-triggered
capture, circular image history, or new transfer logs have been implemented by
this plan. No tests are scheduled or run automatically. The user will test at a
later time.

### 1. Preserve the baseline that eliminated the blackouts

- Keep `beam_frame_driver` as the experimentally validated intro baseline. Do
  not reintroduce guest IRQs during drawing, a second beam, or a frame budget
  independent of the field boundary.
- Before making future edits, reread the current instructions, this document,
  `src/game_rtl.c`, and `snesrecomp/docs/FRAME_MODEL_HOSTS.md` in the port.
  Check the local sources and pins: the previously declared pin could not be
  authenticated using Git metadata from the local framework copy.
- Record the reference executable's SHA-256, settings, and capture used. The
  executable copied after the driver change had SHA-256
  `3320A1C1391DE0A67F711C1A0B48AEA84161E0D9E81177660309CD667B99D095`.
  Check it again; do not assume the file is unchanged.
- Preserve `G:\x2\yoshi-trace-20261014-201927` (before) and
  `G:\x2\yoshi-trace-20261014-202555` (after). Do not replace these folders.

### 2. Expand capture without changing game behavior

**Proposal:** an opt-in mode with a bounded in-memory circular history and a
keyboard trigger yet to be chosen, without conflicts with controls or
shortcuts. The trigger should only mark the capture: it must not pause, reset,
manually advance frames, or write to guest state.

- Proposed initial window: **8 frames before and 16 frames after the marked
  frame**, plus the trigger frame itself (25 frames total). Allow the interval
  to be adjusted, with explicit memory, frame, and event caps.
- Store native images and telemetry in the same history; logging only after the
  trigger would not recover the appearance of earlier frames.
- Calculate the memory limit from the effective resolution/pitch and event
  capacity. Preallocate buffers; avoid allocation and disk writes per pixel or
  event. Flush each batch in a controlled manner and record any data loss,
  without blocking the game indefinitely.
- Export one batch per trigger, with a per-session batch limit. If the buffer or
  window is not ready, report that in the result; do not silently produce an
  incomplete sequence.
- Human reaction may take more than eight frames. For ER-02 and ER-05, locate
  the timestamp in the video first and use a scheduled window when replaying the
  scene, or increase the history within the budget. A late keypress does not
  prove that the defect is absent.
- Keep the existing fixed-interval mode as an alternative for scenes that can
  be reproduced from boot. Do not reuse frames 640–699 for a later scene without
  identifying its interval first.

**Proposed output per batch, only in a new subfolder of `G:\x2`:**

| File / group | Expected contents |
| --- | --- |
| Manifest | ER ID, schema version, EXE hash, settings, resolution/pitch/format, scene, counter source, marked frame, and limits/drops |
| Native images | Before/during/after sequence, named with the same frame IDs as the logs; visual screen contents only, no tile or ROM dumps |
| Presented images | When available without rerunning the guest, explicitly associated with the simulated frame and presentation |
| Per-line state | Effective state, hashes, and pixel counts; drawing line and framebuffer line identified |
| Events | Display writes, DMA/HDMA metadata, and GSU samples/jobs, with clocks, phase, beam, and origin |
| Summary | Present/missing frames, triggers, drops, and correlation instructions |

Do not export VRAM/OAM/CGRAM payloads, SRAM, WRAM, or ROM code. For graphic
content, use region hashes and destination/size metadata. Global hashes detect
changes but cannot identify the responsible tile by themselves.

### 3. Measurements that should accompany the images

- **Synchronization:** port frame, PPU field, presentation, drawing line, V/H,
  CPU master clock, and beam. Mark the phase of each sample; do not confuse a
  beam parked during replay with the scanline that originated the guest write.
- **PPU state:** INIDISP, mode, main/sub layers, scroll, Mode 7 parameters,
  window selection/logic/bounds, masks, and color math. The current
  instrumentation register window already contains several of these values;
  export readable names so users do not have to interpret 64 bytes manually.
- **State actually consumed:** use the existing observer after each line. If
  needed, add observation before drawing and of the calculated window bounds,
  distinguishing window configuration from the effective result.
- **Framebuffer:** hash by line and region of interest, count non-black pixels,
  record valid dimensions and pitch, and indicate redrawn lines. Compare native
  output with presentation to locate ER-03 without masking the defect.
- **DMA/HDMA:** channel, destination, size, start/end when the model allows,
  enable state, and table advancement. Record the limits of per-byte timing
  precision; do not invent timestamps the API does not provide. Correlate hash
  changes with transfers and renderer consumption.
- **GSU:** job start/end and clocks, instruction progress, SFR/IRQ, and relevant
  configuration. Check the local `superfx_job_log` before adding more
  infrastructure. Do not export opcodes or memory contents.
- **Integrity:** distinguish journal drops, capture drops, and overflows in
  other rings; clearly document any unavailable information.

### 4. Implementation map to confirm before editing

All paths below are relative to `Yoshi's Island recomp/`. They are candidates
for future implementation, not files changed in this phase. Inspect the APIs
and announce the exact paths actually needed before editing.

| Path | Intended role |
| --- | --- |
| `src/game_rtl.c` | Bounded capture, frame/line correlation, and integration with the existing observer |
| `snesrecomp/runner/src/desktop/host_main.c` | Possible trigger and presentation observation; first confirm an existing extension point to avoid unnecessary changes |
| `snesrecomp/runner/src/debug_server.c` | Check local support for consecutive images, `ppu_lines`, and `ppu_window`; confirm build availability and per-frame retention before relying on it |
| `snesrecomp/runner/src/snes/ppu.c` and `ppu.h` | Only if state/line observers are missing; keep hooks optional and do not mutate guest state |
| `snesrecomp/runner/src/snes/dma.c` and `superfx.c` | Check existing logs; edit only if required metadata is missing and after defining scope |
| `tools/flicker-trace.md` | Update instructions, file schema, and commands when the feature is implemented |

Do not assume that queries for the last line/frame can recover an older frame.
Do not use state injection or rendering that executes the guest as a substitute
for passive capture.

### 5. Future test sessions

1. **Validate capture:** confirm that trigger, images, IDs, and logs agree;
   check limits and drops. Repeat the intro segment without blackouts as a
   control. Capture must not modify guest clocks, IRQs, or image output.
2. **Title screen — ER-03:** capture normal and affected first lines, including
   neighboring lines, in native and presented output. Investigate origin, pitch,
   valid area, and redraw only if the divergence points to that path.
3. **Sky — ER-01:** capture the band present/absent and compare applied state on
   neighboring lines. Find the first difference that corresponds to the band.
4. **Horizon — ER-04:** first confirm a difference from the reference; measure
   it separately from ER-01, even if the instrumentation is shared.
5. **Enemy — ER-02:** capture the growth and final transition. Cross-check
   content/address changes against uploads and GSU jobs; do not infer causality
   only because two events occurred in the same frame.
6. **Text — ER-05:** start before the opening. Capture the complete appearance,
   disappearance, and growth. Compare effective mask and available content.
   Repeat a gameplay box with no reported issue and the pause screen as controls.
7. **Gameplay observation:** play normally while recording scene and inputs;
   check that blackouts do not return and that input, audio, text boxes, and
   pause have no regressions. Not observing a defect in one session does not
   validate the entire game.

Keep frameskip, turbo, run-ahead, rewind, reset, and save/load disabled during
comparative capture. Normal input is needed for the title screen/gameplay; only
initial intro playback requires no input. Keep the window visible and note any
focus or setting changes during the interval.

For the Bnes reference, record its version, settings, and equivalent sequence.
Align by animation stage and inputs, not only by the absolute frame number since
boot. Compare at native resolution when possible; presentation scaling and
aspect ratio may shift apparent coordinates.

### 6. How to decide and validate each fix

For each ID, record in this document: evidence that located the divergence,
hypothesis tested, paths changed, the build actually run, before/after capture,
and the result observed by the user.

Apply one scoped change at a time, preserving a reference executable and
previous captures. Do not clear buffers, latch VRAM, adjust clocks, or hide
lines without evidence justifying the change. Do not use a visual filter to hide
the defect.

Mark an issue **resolved in the tested scenario** only after repeating the
complete sequence for that ID and confirming the change did not reintroduce
blackouts or affect comparison controls. Record successful build, static
inspection, and visual test separately; one does not replace the others.

### 7. Resumption point

The next technical action is to implement and verify the short capture with
synchronized images and telemetry, starting with ER-03. Before that, confirm
whether the user prefers to start with another ID and follow the current
authorization/instructions for editing, building, and copying the EXE. This
documentation update does not automatically perform or authorize those future
steps.

## History

| Date | Record |
| --- | --- |
| October 14, 2026 | Full-screen flicker eliminated in the intro according to the user's test and the instrumented interval; this does not mean the local rendering artifacts are resolved. |
| October 14, 2026 | ER-01 through ER-05 added with seven supplied images; no further fix or build was performed for this document. |
| October 14, 2026 | Links updated to `Yoshi's Island recomp_errorRender/`; future instrumentation plan, per-issue sessions, and validation criteria documented. Only this Markdown file was changed; implementation and tests remain pending. |

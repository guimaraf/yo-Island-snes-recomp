# Yoshi’s Island — Rendering Artifacts Log

Initial record: October 14, 2026. Project: **Super Mario World 2: Yoshi’s Island (USA)**.
Purpose: maintain a list of observed issues, their evidence, and validation
criteria so each occurrence can be investigated and resolved separately.

This document is in the workspace root. The seven images are preserved in
`_errorRender/`, inside the port directory.
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
- ER-01, ER-02, and ER-07 remain open. ER-03, ER-04, ER-05, and ER-06 are
  resolved in the tested scenarios documented below. These fixes do not
  demonstrate that every rasterization detail is correct.
- The user also tested the emulator named **Bnes** for comparison. No version or
  configuration for that emulator, identified reference/recomp frame pairs, or
  equivalent frame numbers were provided. Therefore, the captures are not
  attributed to either emulator here, and no difference from the original is
  claimed as proven. Preserve the reported spelling; do not assume another core.
- Static frames show appearance but do not prove intermittency, duration, or
  temporal sequence. Those aspects are recorded as user reports.
- The hypotheses for the open issues ER-01, ER-02, and ER-07 guide measurements; none is a confirmed cause. Super FX
  is part of the execution path, not an automatic explanation.

## Tracking list

| ID | Scene / symptom | Evidence | Status |
| --- | --- | --- | --- |
| ER-01 | Dark horizontal band near the bottom of the sky/clouds | Images 1 and 2; reported intermittency | Open |
| ER-02 | Block corruption while the enemy approaches or leaves | Image 3 for comparison and affected image 4 | Open |
| ER-03 | First line of the title screen has pixels with discrepant colors | Image 5; user confirmed the fix in the tested title-screen scenario | Resolved in the tested scenario on October 15, 2026; commits `cc8c0ae`, `46fc4fa` |
| ER-04 | Interactive objects (including stationary enemies and the pushable ball) shift horizontally relative to the scenery | User-validated World 1-2 block and World 1 ball scenarios after raster-history fixes | Resolved in the tested scenarios on October 15, 2026; commits `7264416`, `97111a4` |
| ER-05 | Text box appears too early or opens in the wrong sequence | Continuous capture with per-frame input; user repeated text-box openings after the HDMA source-history fix and confirmed the defect no longer occurs | Resolved in the tested scenario on October 16, 2026 |
| ER-06 | Flicker when returning to the level selector after completing a level | Before/after captures and user repetition of the trigger | Resolved in the tested scenario on October 15, 2026; commit `493f08b` |
| ER-07 | Fixed bright horizontal line behind the title-screen mountains | Image 5; reported persistence | Open |

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

![Image 1 — black line behind the clouds](<_errorRender/imagem01_YoshisIsland_errorRender.png>)

![Image 2 — band with darkened pixels and black areas](<_errorRender/imagem02_YoshisIsland_errorRender.png>)

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

![Image 3 — enemy recognizable before the reported defect](<_errorRender/imagem03_YoshisIsland_errorRender.png>)

![Image 4 — block fragmentation across the scene](<_errorRender/imagem04_YoshisIsland_errorRender.png>)

## ER-03 and ER-07 — two distinct lines on the title screen

**ER-03, original observation:** a line of discrepant pixels appeared at the
top edge of the game image. The user reported that it alternated with a normal
line.

**ER-07, observation:** a thin bright/yellowish line crosses the background at
the horizon, behind the mountains. The user reports that it is fixed and does
not flicker.

Treat these as separate cases: their different positions, persistence, and
appearance do not justify assuming a single cause.

**ER-03 correction and validation:** the user confirmed that the title-screen
first-line color artifact was corrected in the tested scenario. The associated
change moved HDMA processing before scanline rendering and removed the separate
first-line HDMA priming call. This change did not resolve ER-05; the text-box
issue remains a separate open case.

**Implementation:** `modules/snesrecomp/runner/src/beam_frame_driver.c`;
framework commit `cc8c0ae` (`fix: apply HDMA before scanline rendering`), pinned
by project commit `46fc4fa` (`chore: pin HDMA scanline fix`).

**ER-07 hypotheses:** raster transition involving layers/palette/color math, or
a window boundary. A detail present in the reference still needs to be ruled
out.

**Next evidence for ER-07:** an equivalent reference capture; compare the native
line with the presented image to separate rendering from scaling/presentation.
Do not infer the exact SNES scanline from its position in a scaled capture.

**Resolution criteria:** ER-03 is resolved in the tested scenario. ER-07 still
requires color continuity compatible with the reference, without removing
legitimate detail.

![Image 5 — discrepant first line and bright horizon line](<_errorRender/imagem05_YoshisIsland_errorRender.png>)

## ER-04 — horizontal displacement of interactive objects

**Scope:** investigate objects that should stay fixed relative to the scenery
but shift horizontally in the recomp. This includes the pushable ball before
contact and stationary enemies. Keep this issue separate from the opening
text-box sequence in ER-05.

**User's comparison:** in Bnes, the user tested the pushable ball found inside
a pipe, trying left/right input, jumps, and slow/fast movement, and did not see
the recomp's horizontal drift. In the recomp that drift persisted with its
launcher renderer set to Vulkan. This is useful runtime evidence, but no
versioned Bnes capture pair is available yet, so it does not establish a
frame-matched comparison or the cause in the recomp.

**October 15 captures:** the first three folders held ball scenarios in World
1-3. Their manifests used ER-05 because the capture script had not yet separated
the issue scopes. At the user's request, those three folders and two earlier
ball captures (`yoshi-render-20261015-105833-0c2f6d70` and
`yoshi-render-20261015-122101-9fe4c4db`) were deleted so the tests can be
repeated with ER-04. Their prior analysis is historical and cannot be rechecked
from the deleted artifacts. The fourth folder, whose scene note said “caixa;
abertura completa,” was also deleted and is excluded from both ER-04 and ER-05;
its World 1-3 gameplay images did not show the opening text-box sequence.

Before deletion, an identical 14×14 image patch on the ball in the earlier
no-contact capture was tracked in 80 frames. Adding same-frame BG1 scroll to
its position yielded up to three pixels of horizontal variation; adding the
following frame's scroll yielded the same coordinate pair in all 80 cases. An
identical ground detail confirmed one-pixel relative displacement in frames
8755–8757. Those measurements supported a one-frame alignment hypothesis, but
did not prove the object's guest physics, absence of contact, or cause.

The expanded `yi-render-motion-v1` probe records BG1–BG4 scroll, separate
low/high OAM and CGRAM hashes, OAM write counters, and bounded snapshots of up
to 128 object slots at two phases. Slots can be reused; their signatures and
geometry do not prove persistent object identity, visibility, or physics.
Coordinates precede widescreen/HUD adjustments, and nominal size follows the
renderer model. At the time, the probes were passive and did not correct ER-04.
The later fix came from preserving effective PPU state over raster time, not
from treating the earlier frame-alignment correlation as proof of the cause.

The timed capture retains approximately 120 complete frames after five seconds
of preparation and two seconds of recording. For future ER-04 comparisons,
record a moving camera without contact, a stationary camera, intentional
contact, and a stationary interactive object such as an enemy. Use the native
images to confirm object identity and compare object position with scenery,
camera scroll, and line/frame timing. Captures made under the former ER-05
manifest label retain that provenance.

**Correction:** the root cause was temporal inconsistency during visual
reconstruction. The renderer replayed scanline register/OAM changes, but
objects could be drawn with memory from a different point in the field. The
framework now keeps a per-field OAM baseline and records the effective OAM
updates with their scanline positions. It also keeps VRAM and CGRAM baselines
and a bounded per-field journal of committed writes. During rendering, it
reconstructs those memories as of each scanline, so sprite coordinates,
graphics, and palette are read from a coherent point in raster time. At the end
of rendering, guest memory, PPU ports, latches, and counters are restored.
Writes through both the SNES PPU ports and the AOT `WriteVramWord` path are
recorded. Memory-journal capacity and overflow accounting are independent of
the register/OAM journal.

**Files changed in `modules/snesrecomp`:**
`runner/src/common_rtl.c`, `runner/src/snes/ppu.c`, `runner/src/snes/ppu.h`,
and `tests/ppu/ppu_raster_journal_test.c`. The framework fix is commit
`97111a4`; the project pins it in commit `7264416`.

**Validation:** the user confirmed the growing block in World 1-2 remains
stable while the scene scrolls, and the ball in World 1 remains stable while
jumping and moving left and right. The user also confirmed that the character
flicker introduced during the first alignment fix is gone after the VRAM/CGRAM
history change. Five ROM-free PPU tests passed, covering the raster journal,
sprite limits, world mirroring, elastic band, and Mode 7 HD. The tested build
was copied to `G:\x2\Yoshi's Island recomp.exe` (SHA-256
`247A8E4DF1C9BAD343476E33EAC72A396AAEECDF9BE8C3DF988A5739222AE97E`).

**Status:** resolved in the tested scenarios. The reported block and ball
scenarios and character flicker were checked by the user; this does not claim
that every interactive object, level, or raster effect in the game has been
validated. The original resolution criterion is met for these scenarios;
additional runtime cases can be recorded if they reproduce the defect.

## ER-05 — text-box opening sequence — resolved in the tested scenario

**Resolution confirmed on October 16, 2026:** after testing the updated
executable, the user exercised the text-box opening several times and
confirmed that the premature full-box appearance, disappearance and repeated
growth no longer occur. ER-05 is resolved in that tested scenario. This is
user-reported post-fix validation; no new post-fix capture was supplied, and
the report does not separately identify which renderers were retested or
establish frame-exact equivalence with a reference emulator.

**Pre-fix evidence:** `G:\x2\yoshi-render-20261015-235732-d47fba32` contains
600 consecutive frames (3133–3732), per-frame controller masks and the
`yi-render-input-v1` runtime marker. In renderer B/new, the complete box
appears prematurely in frame 3276, followed by the black window with a small
colored area in 3277; the first logical SNES A press is in 3289. In renderer
A/legacy, the equivalent transition is 3549–3550, before the first logical A
press in 3581. The controller mask is zero at both defective transitions.
These events therefore do not coincide with an A advance. Normal advances
after the down arrow are not classified as premature input or acceleration.
The renderer changes from new to legacy in frame 3439, with a recorded
keyboard R event. The user's reported E press was not monitored.

**Corrective mechanism:** PPU registers were reconstructed from the completed
field's journal while render-walk HDMA could read live WRAM tables and DMA
configuration prepared for a different stage. The fix captures WRAM and DMA
configuration when the beam enters line 0, associates them with that field,
uses them during reconstruction and restores the guest's live RAM/DMA state
afterward. Vblank journal writes, including HDMAEN, are applied before HDMA
initialization. HDMA remains before each line's rendering, preserving the
ordering used by the ER-03 fix; no buffer clearing or frame hiding was added.

Changed framework files: `runner/src/snes/ppu.c`, `runner/src/snes/ppu.h`,
`runner/src/snes/snes.c`, `runner/src/beam_frame_driver.c`, and
`tests/ppu/ppu_raster_journal_test.c`, all under `modules/snesrecomp/`.
The source snapshot represents the start of the visible field, not a
per-scanline WRAM-write journal; the successful scenario does not certify
effects that rewrite HDMA tables during visible lines.

**Build and checks:** the game build completed successfully. The raster
journal test, including completed-field HDMA source isolation, repeated
reconstruction and guest-state restoration, passed. The sprite-limit,
world-mirror, elastic-band and Mode 7 regression tests also passed. The EXE
was copied to `G:\x2\Yoshi's Island recomp.exe`, with matching source and
destination SHA-256:
`8B4E074FF8E8C5B72EBACDDD004C78BDA9CB42365139EAB8BA9DB08946A3BAFD`.
These automated checks and the user's visual confirmation are distinct
evidence. No separate post-fix ER-03 retest was reported in this confirmation.

The material below preserves the original symptom and investigation history;
pending steps described there are superseded by the resolution above.

**Scope:** this issue concerns only the opening **text box** (the dialogue
window and its text), including its reported premature appearance and opening
order. Horizontal displacement of gameplay objects belongs to ER-04. Recheck
the text-box behavior with the ER-06 fix without assuming a shared cause.

**What the images show:** image 6 contains a large green text box with its text
already visible. Image 7 contains a large black rectangle and only a small
green region in the center, with fragments of letters. In isolation, each
image could belong to a different animation stage; the temporal defect depends
on the report and the subsequent continuous capture documented above.

**Temporal report:** the expected behavior is for the black window to grow
first, followed by the colored text box and text expanding. In the recomp, the
full text box and text appear on the first frame, disappear, and then the opening
progresses. Image 7 represents a later stage where the colored area begins to
grow.

**User-suggested hypothesis:** an image already present in the buffer. Record
this as a hypothesis, not a conclusion. Other possibilities include window or
mask state from a different moment than the content, upload/replay ordering, or
a line that was not redrawn. Do not apply buffer clearing or a latch indiscriminately.

**Historical next evidence (obtained before the fix):** a continuous sequence starting before the opening, including
the first frame where the text box appears, its disappearance, and its growth;
track windows, layers, transfers, and line hashes. Compare against the same
opening in the reference.

**Resolution criterion:** no premature appearance, with opening order and
text-box geometry compatible with the reference during the tested sequence.

**Initial instrumentation:** the first ER-05 probe was built and deployed.
Its `scripts/capture-render.ps1 -ErrorId ER-05` command used the default native
region `0,0,256,112` (x,y,width,height), adjustable with `-BoxRegion`. The probe
records hashes and per-line measurements of black/uniform runs, non-black
pixel bounds, BG1–BG4 scroll, and both window bounds. Native images, 64 PPU
registers, events, and journal probes remain synchronized by frame. Uniform
runs do not automatically identify the text box; confirm it in the images. No
memory payloads are exported. The initial probe lasted five seconds after Enter
and had no pre-trigger history; its timing is superseded by the update below.
The script requires `yi-render-box-v1` in the executable before it starts.

The initial probe executable deployed to `G:\x2\Yoshi's Island recomp.exe` had SHA-256
`E201C9E40A7E1F9AA12B71E6FCC491AE318A249BEEF4ADB061177E871C5F1F27`. The
updated script is `G:\x2\scripts\capture-render.ps1`. Runtime capture and
analysis were pending at that initial stage and were subsequently completed
as documented above. The later motion-probe work is
documented under ER-04 and does not redefine this text-box issue.

The motion probe, capture timing, and object-motion test cases are documented
under ER-04. They are kept separate from this text-box issue even though the
same capture executable and telemetry format can serve both investigations.

The original ER-05 investigation question was specifically whether the opening text box
appears too early, disappears, and then grows in the expected order. Capture
the complete text-box opening and use a gameplay or pause **text box** as a
control. Identify the exact message and stage in `-Scene`; do not treat an
ordinary scenery object or a wooden crate as the ER-05 text box. Compare the
text-box position and opening sequence in successive native images with the
window state and available content. See `tools/flicker-trace.md`.

![Image 6 — complete text box identified as appearing too early](<_errorRender/imagem06_YoshisIsland_errorRender.png>)

![Image 7 — black text-box window with a small colored region during the opening](<_errorRender/imagem07_YoshisIsland_errorRender.png>)

## ER-06 — flicker in the selector after completing a level — resolved in the tested scenario

**Symptom:** after completing a level and returning to the selector, the full
map alternated every frame with an incomplete scene whose upper region was
black. The user reported no flicker during gameplay and recurring flicker on
returning to the map. This was separate from the intro flicker already removed
by the driver change.

**Runtime mechanism identified:** in `yoshi-render-20261015-094907-75410bfb`,
frame 6119 records TM/TS=`17/00` writes on line 224, with no drops in that
frame. The next baseline receives those values, but the renderer restores the
previous field (`04/01`). The visual loop applies the journal only through
line 223. The old `ppu_rasterRenderEnd` reapplied only the next journal and
left the drawing registers as guest state. Two baselines then fed the
alternation. The probes show this cycle in the final 150 frames without
drops; it was not merely a timing coincidence with DMA or GSU.

**Versioned fix:** commit `493f08b` (`fix stage select flicker`).
`modules/snesrecomp/runner/src/snes/ppu.c` preserves the 64 register bytes and
relevant latches before visual reconstruction, then restores them afterward.
This includes scroll, Mode 7, mosaic, and the OAM port. It does not force
masks, hide frames, or change clocks to mask the symptom. The isolated test
without a ROM, `modules/snesrecomp/tests/ppu/ppu_raster_journal_test.c`, failed
before and passed after the fix, including line 224 writes and following fields.

**Validation:** the user repeated level completion and return to the selector
several times, reporting that flicker was completely eliminated in this flow.
In capture `G:\x2\yoshi-render-20261015-095903-0001eecf`, the same trigger was
recorded in frame 14635: `renderEnd` returns `17/00` to the guest, and the next
baseline inherits the correct state. The capture contains 300 valid images and
eight probes per frame, with no probe/DMA/journal drops or export errors. The
five monitored registers return to their pre-draw state in all 300 frames.
After the transition, 213 consecutive frames keep TM/TS=`17/00`, with no
alternation or black frames. There were 11,319 dropped events in two transition
frames; those drops do not affect the trigger frame or the stable interval used
for this conclusion.

Validated executable, SHA-256:
`E8C4ABA393A42E315F3A09CB9BE176525061328F273C86986F212D769BBF6EE8`.
Also preserve the earlier captures in `G:\x2`, including
`yoshi-render-20261015-093437-81dd767d`. The ER-06 validation does not resolve
ER-01 through ER-05 or ER-07, or certify every game scene and configuration.

## Gameplay observations and limits on generalization

The user reports that the text-opening corruption did not occur in gameplay
text boxes and that no corruption was noticed on the pause screen. Keep these
as comparison controls; do not conclude that every message, stage, or pause
state has been validated. ER-05's visual scope is only the text-box transitions
in the opening sequence observed by the user.

## Suggested investigation order

1. **ER-01 and ER-07:** measure the horizontal bands separately; share the
   raster-observation method without assuming a shared cause.
2. **ER-02:** capture the enemy transition and verify upload/content consistency.
ER-05 is resolved in the tested scenario. Reopen its investigation only if the
text-box opening defect returns; preserve the existing captures for comparison.

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

## Historical follow-up plan — recorded on October 14, 2026

The plan below preserves the original phase. Current capture implementation,
ER-06 validation and ER-04 priority supersede this historical resumption point;
see the updated issue sections above and `YoshisIsland_errorRender.md`.

**Status at the end of that phase:** documentation only. No key-triggered
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
- Human reaction may take more than eight frames. For ER-02, ER-04, and ER-05, locate
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
  output with presentation if the ER-03 title-screen artifact regresses; the
  tested scenario is currently resolved.
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
2. **Title screen — ER-03 regression check:** the tested scenario is resolved.
   Repeat the native/presented first-line comparison only if the artifact
   returns.
3. **Sky — ER-01:** capture the band present/absent and compare applied state on
   neighboring lines. Find the first difference that corresponds to the band.
4. **Horizon — ER-07:** first confirm a difference from the reference; measure
   it separately from ER-01, even if the instrumentation is shared.
5. **Enemy — ER-02:** capture the growth and final transition. Cross-check
   content/address changes against uploads and GSU jobs; do not infer causality
   only because two events occurred in the same frame.
6. **Text — ER-05:** start before the opening. Capture the complete appearance,
   disappearance, and growth. Compare effective mask and available content.
   Repeat a gameplay text box with no reported issue and the pause screen as controls.
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
synchronized images and telemetry for an open issue. ER-03 is resolved in the
tested title-screen scenario; resume its investigation only if the artifact
returns. Follow the current authorization/instructions for editing, building,
and copying the EXE. This documentation update does not automatically perform
or authorize those future steps.

## History

| Date | Record |
| --- | --- |
| October 16, 2026 | ER-05 resolved in the tested scenario: the user repeated text-box openings several times and confirmed that the defect no longer occurs after the completed-field HDMA source-history fix. Game build and five PPU regression tests passed; deployed EXE SHA-256 `8B4E074FF8E8C5B72EBACDDD004C78BDA9CB42365139EAB8BA9DB08946A3BAFD`. Post-fix visual confirmation is user-reported, without a new capture. |
| October 15, 2026 | ER-03 resolved in the tested title-screen scenario after moving HDMA processing before scanline rendering and removing separate first-line HDMA priming; user confirmed the fix. Framework commit `cc8c0ae`, pinned by project commit `46fc4fa`. ER-05 remained open. |
| October 15, 2026 | ER-06 recorded as resolved in the tested scenario, with its cause and before/after captures. ER-05 was initially prioritized for horizontal movement measurements; those measurements are now tracked as ER-04, while ER-05 is reserved for the opening text box. |
| October 15, 2026 | ER-05 instrumentation built and deployed to `G:\x2`; runtime capture and analysis pending. |
| October 15, 2026 | Issue scopes clarified: ER-04 is horizontal movement of interactive objects; ER-05 is the opening text box; the title-screen horizon line formerly labeled ER-04 is now ER-07. Earlier capture manifests and history retain their original IDs. |
| October 14, 2026 | Full-screen flicker eliminated in the intro according to the user's test and the instrumented interval; this does not mean the local rendering artifacts are resolved. |
| October 14, 2026 | ER-01 through ER-05 added with seven supplied images; no further fix or build was performed for this document. |
| October 14, 2026 | Links updated to `Yoshi's Island recomp_errorRender/`; future instrumentation plan, per-issue sessions, and validation criteria documented. Only this Markdown file was changed; implementation and tests remain pending. |

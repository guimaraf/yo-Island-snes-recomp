# Yoshi’s Island — Rendering Artifacts Log

Initial record: October 14, 2026. Project: **Super Mario World 2: Yoshi’s Island (USA)**.
Purpose: maintain a list of observed issues, their evidence, and validation
criteria so each occurrence can be investigated and resolved separately.

This document is in the workspace root. The supplied images are preserved in
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
- ER-01 through ER-06 and ER-09 are resolved in the scenarios the user tested.
  ER-09 was confirmed resolved on October 17, 2026 after the Mode 2 offset fix.
  ER-07 was confirmed resolved in the complete Boss 1-4 fight on October 17,
  2026 using the continuously enabled `DelayBG2-test` build. ER-08, a line
  above the text box in the Bonus stage, remains open. ER-12 was resolved in
  the tested castle 1-4 rotating-bar scenario using `ER12-BG3Pair-test`; the
  user also switched renderers at the bar and during the boss fight without
  reproducing ER-07. Both adjustments remain explicit build options. The previously resolved
  gameplay background issue and title-screen horizon were renumbered ER-10 and
  ER-11 to keep these new IDs unique.
- The user also tested the emulator named **Bnes** for comparison. No version or
  configuration for that emulator, identified reference/recomp frame pairs, or
  equivalent frame numbers were provided. The original supplied screenshots therefore do not establish a
  frame-matched reference comparison. Later native capture folders identify
  recomp builds and effective renderers explicitly; no synchronized Bnes
  comparison is claimed. Preserve the reported spelling; do not assume another core.
- Static frames show appearance but do not prove intermittency, duration, or
  temporal sequence. Those aspects are recorded as user reports.
- ER-10 and ER-11 are resolved in their previously tested scenarios. Their
  original records and capture filenames retain the IDs used at the time.
  Super FX is part of the execution path, not an automatic explanation.

## Tracking list

| ID | Scene / symptom | Evidence | Status |
| --- | --- | --- | --- |
| ER-01 | Dark horizontal band near the bottom of the sky/clouds during the opening | User reports no black line during repeated opening tests | Resolved in the tested opening scenario on October 16, 2026 |
| ER-02 | Tile corruption in the enemy's final animation frame | User reports the enemy now crosses the screen without corrupting tiles | Resolved in the tested scenario on October 16, 2026 |
| ER-03 | First line of the title screen has pixels with discrepant colors | Image 5; user confirmed the fix in the tested title-screen scenario | Resolved in the tested scenario on October 15, 2026; commits `cc8c0ae`, `46fc4fa` |
| ER-04 | Interactive objects (including stationary enemies and the pushable ball) shift horizontally relative to the scenery | User-validated World 1-2 block and World 1 ball scenarios after raster-history fixes | Resolved in the tested scenarios on October 15, 2026; commits `7264416`, `97111a4` |
| ER-05 | Text box appears too early or opens in the wrong sequence | Continuous capture with per-frame input; user repeated text-box openings after the HDMA source-history fix and confirmed the defect no longer occurs | Resolved in the tested scenario on October 16, 2026 |
| ER-06 | Flicker when returning to the level selector after completing a level | Before/after captures and user repetition of the trigger | Resolved in the tested scenario on October 15, 2026; commit `493f08b` |
| ER-07 | Extra lines outside the Boss 1-4 during growth, combat and shrinking | Per-line captures, complete-fight validation with continuous BG2 VOFS delay, and a later user boss playtest with renderer switching in BG3Pair | Resolved in the tested Boss 1-4 fights on October 17, 2026; no recurrence reported in `ER12-BG3Pair-test`; normal-build default remains unchanged |
| ER-08 | Line above the text box in the Bonus stage | User report; no capture or exact stage/message supplied yet | Open; reproduction details pending |
| ER-09 | Stage 1-7 background and walkable terrain render at incorrect positions | Images 14–15; before/after captures and same-run B-to-A comparison; user confirms normal progression after the Mode 2 offset fix | Resolved in the tested Stage 1-7 scenario on October 17, 2026 |
| ER-10 | Background corruption while moving the World 1-3 secret-passage weight bar and while a door descends in the first castle | Images 8–12 and user-confirmed post-fix captures; originally tracked as ER-07 | Resolved in both tested scenarios on October 16, 2026; one-line BG3 HDMA VOFS timing correction |
| ER-11 | Fixed bright horizontal line behind the title-screen mountains | Image 5 documents the original report; image 13 shows the current title screen without the reported line; originally tracked as ER-08 | Resolved in the title-screen scenario shown on October 16, 2026 |
| ER-12 | Pixels extend beyond the rotating pink bar near lava in castle 1-4 | Before/after native captures, effective per-line scroll, and user confirmation after switching renderers and continuing to the boss | Resolved in the tested bar scenario on October 17, 2026, in `ER12-BG3Pair-test`; paired BG3 delay remains opt-in |

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

## ER-03 and ER-11 — two distinct lines on the title screen

**ER-03, original observation:** a line of discrepant pixels appeared at the
top edge of the game image. The user reported that it alternated with a normal
line.

**ER-11, original observation (formerly ER-08):** a thin bright/yellowish line
crossed the background at the horizon, behind the mountains. The original
report described it as fixed and non-flickering. Its first tracking ID was
ER-07 before the gameplay background issue was assigned that ID.

Treat these as separate cases: their different positions, persistence, and
appearance do not justify assuming a single cause.

**ER-03 correction and validation:** the user confirmed that the title-screen
first-line color artifact was corrected in the tested scenario. The associated
change moved HDMA processing before scanline rendering and removed the separate
first-line HDMA priming call. At that time, ER-05 remained a separate issue;
it was resolved in the tested opening scenario on October 16, 2026.

**Implementation:** `modules/snesrecomp/runner/src/beam_frame_driver.c`;
framework commit `cc8c0ae` (`fix: apply HDMA before scanline rendering`), pinned
by project commit `46fc4fa` (`chore: pin HDMA scanline fix`).

**ER-11 validation on October 16, 2026:** the user reports that the title-screen
corruption is no longer present and supplied a new screenshot. The visible
horizon and surrounding title-screen background in image 13 show no obvious
bright line matching the original report. ER-11 is resolved in this tested
title-screen scenario. This is a user-reported visual check supported by one
static screenshot; no reference comparison or temporal capture was supplied.

![Image 5 — discrepant first line and bright horizon line](<_errorRender/imagem05_YoshisIsland_errorRender.png>)

![Image 13 — title screen without the previously reported bright horizon line](<_errorRender/imagem13_ER08_tela_titulo_sem_corrupcao.png>)

## ER-10 — background corruption during moving scenery and doors

**Scope:** the background corruption reported in two situations: Yoshi moving
the weight bar in the World 1-3 secret passage, and a door descending in the
first castle. Both tested scenarios are now resolved. The same correction
cleared both symptoms, but the captures do not prove that their underlying
cause is identical.

**Secret passage, images 8–9:** as Yoshi moves the weight bar, large portions
of the background appear replaced or displaced by tile patterns and solid
areas. The corruption changes as the bar moves. The images do not identify
which layer, tiles, or transfer produced those pixels.

**Castle, images 10–12:** image 10 shows the first door in its normal scene.
Images 11–12 show a door/opening transition and a descending door with large
background regions replaced by flat brown areas or displaced scenery. The
similarity to the passage corruption motivated testing the two scenes together
under the original ER-07, while leaving their relationship unverified.

**User report:** the weight-bar corruption occurs while moving it. In the
castle, some doors are interactive and fall toward the player; the user reports
the corruption as the door descends. Image 10 is the unaffected comparison
provided for the castle.

**Correction and validation:** the candidate delays active HDMA writes to
BG3VOFS by one scanline. The user repeated both triggers with renderer B and
reported that the corruption disappeared completely. The corresponding
executable was `Yoshi's Island recomp.ER07-candidate.exe`, SHA-256
`24F3F765795EF31209DEC25F5CAFBD6DE051D2EA201981BD2F062208CF7DA12E`.

The secret-passage capture contains 600 consecutive frames (5857–6456); the
first-castle capture contains 599 consecutive frames (10526–11124). Both record
renderer B on every valid scanline, with 224 valid lines per frame, matching
consumed-VRAM and BG3 sample records for every line, and no invalid renderer
rows, frame-cap drops, or export I/O failures. Visual inspection of frames
throughout both sequences found no remaining large displaced or solid-color
background regions matching the reports. The capture metadata says presentation
is unavailable and does not provide a reference comparison; the user's visual
confirmation remains the validation for the displayed result. The exact castle
room and door were not recorded.

These results resolve ER-10 in the two tested scenarios. They support the same
correction for both scenes, while the common underlying cause remains a
hypothesis.

The original screenshots and capture folders retain the ER-07 label used
before this issue was renumbered ER-10.

![Image 8 — background corruption while moving the weight bar in the secret passage](<_errorRender/imagem08_ER07_passagem_secreta_075642.png>)

![Image 9 — another view of the secret-passage corruption with the weight bar moved](<_errorRender/imagem09_ER07_passagem_secreta_075654.png>)

![Image 10 — castle door before the reported corruption](<_errorRender/imagem10_ER07_castelo_porta_normal.png>)

![Image 11 — corrupted background during the castle door transition](<_errorRender/imagem11_ER07_castelo_porta_corrompida.png>)

![Image 12 — background corruption while the castle door descends](<_errorRender/imagem12_ER07_castelo_porta_descendo.png>)

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
`yoshi-render-20261015-093437-81dd767d`. At the time, the ER-06 validation did
not resolve ER-01, ER-02, ER-05, or the then-numbered horizon issue (now ER-11),
or certify every game scene and configuration. ER-01, ER-02, and ER-05 were subsequently reported fixed in their tested scenarios.

## Gameplay observations and limits on generalization

The user reports that the text-opening corruption did not occur in gameplay
text boxes and that no corruption was noticed on the pause screen. Keep these
as comparison controls; do not conclude that every message, stage, or pause
state has been validated. ER-05's visual scope is only the text-box transitions
in the opening sequence observed by the user.

## ER-07 — sprite-side corruption during the Boss 1-4 zoom — resolved in the tested fight

**Resolution confirmed on October 17, 2026:** the user played the complete
Boss 1-4 fight using `G:\x2\Yoshi's Island recomp.DelayBG2-test.exe` and
reported no corrupted lines throughout the fight. This closes ER-07 in that
tested scenario. This final validation is a user playtest report, not a new
recording of the entire fight or a frame-matched hardware/reference comparison.

**Later regression check on October 17, 2026:** during ER-12 validation with
`ER12-BG3Pair-test`, the user reached and fought this boss, switched renderer
during combat, and reported no recurrence of ER-07. This is a separate user
playtest report without a new boss capture. The build preserves continuous BG2
VOFS delay and additionally pairs BG3 HOFS with its existing VOFS delay; see
ER-12 for the executable hash and scope. The normal-build defaults remain unchanged.

**Adjustment:** `src/game_rtl.c` passes deferred-VOFS mask `06` to
`snes_beam_frame_driver_draw_ppu_frame_observed_deferred_vofs`: BG2 (`02`)
is added to the existing BG3 (`04`) policy previously validated for ER-10.
When HDMA writes BG2VOFS, rendering consumes the preceding vertical-scroll
value for that line; the new value is available to the following line.
The existing driver restores guest state after reconstruction. BG2 horizontal
scroll, GSU execution, controls and the previous ER-03/ER-04/ER-05 corrections
were not changed by this adjustment. The evidence supports this BG2 vertical
consumption-phase correction for the reported boss; it does not establish a
universal HDMA timing rule for every game effect.

**Evidence:** the horizontal artifact was also reproduced in renderer A,
so it was not exclusive to renderer B. In the original B capture, frame
12779/y45 selected source y=511 and drew the external horizontal line. With
`DelayBG2`, frame 9139 in
`G:\x2\yoshi-render-ER07-Boss14-Zoom-Padrao-B-DelayBG2-20261017-150256-e28744d3`
matched the preceding-line BG2 scroll sequence across all 223 comparable
lines; the transition instead reached source y=512 on y46 and the external
line disappeared in that comparison. That capture contains 1,201 consecutive
frames, all in B, with policy `06` throughout. Follow-up folders were preserved:

- `G:\x2\yoshi-render-ER07-Boss14-Acertos-Padrao-B-DelayBG2-20261017-150937-6e083ccb`
  (1,201 frames, 10330–11530).
- `G:\x2\yoshi-render-ER07-Boss14-Acertos-Padrao-B-DelayBG2-20261017-152121-53ad4215`
  (600 frames, 20518–21117).

Both follow-ups recorded renderer B and policy `06` on every line, with no
missing images or checked video-record losses. They did not contain the
complete final shrinking sequence. Earlier reports that lines returned after
capture were clarified by the user: the timed experiment had already stopped.
The capture-only implementation intentionally reverts at the deadline; those
out-of-window observations did not test continuous `DelayBG2`. The separately
reported TAB run was not captured and does not establish a turbo-related cause.

**Validated build and implementation scope:** `CMakeLists.txt` adds
`YI_DELAY_BG2_TEST`, default `OFF`, with a definition scoped to `src/game_rtl.c`
and the distinct output name `Yoshi's Island recomp.DelayBG2-test.exe`.
With the option enabled, BG2 delay stays active throughout execution, without
any capture request or deadline, using the normally selected renderer.
Compile/link and deployment hash verification passed. Executable SHA-256:
`446F68D552A15BB5B7EE72F59C59C4CD3B201515C28364A499AC5BEC9AB8C29D`.
The build cache was restored to `OFF` afterward; the normal build still uses
the prior default and only enables BG2 delay during an explicitly armed
experiment. Resolution is therefore validated in this named build, not yet
promoted to the normal-build default. The 110 ROM-free synthetic BG2 cases
passed in A/B; they check addressing, not real hardware HDMA timing.
The final user playtest did not separately record its effective renderer.

The following material preserves the investigation history. Open/pending
statements below describe the earlier stage and are superseded by this closure.

**User report:** in the Boss 1-4 scene, a rendering error appears outside the
boss. When the scene zooms, the sprite becomes corrupted along its sides.

**Evidence and status:** open. Capture
`G:\x2\yoshi-render-ER07-Boss14-Zoom-Padrao-20261017-130545-4dd3fdaf`
contains 554 consecutive frames (12739–13292), all 124,096 lines in B/Mode 1,
with no reported losses or missing images. Sampled images show a thin
horizontal line outside the boss. In examples 12770/y76, 12779/y45 and
12839/y48, that line coincides with a BG2VOFS transition selecting source y=511.
In 12779/y45 the OBJ buffer occupies only x=162–165, while the artifact extends
well outside those bounds. This narrows the investigation toward BG2/composition;
it does not prove a timing fix or exclude other causes. No renderer A comparison
or matched reference was supplied for this effect.

**Additional user context:** the boss starts small and grows after a magic
spell. During combat, egg hits make it grow further until it stops moving.
The user also reports black vertical lines at the sides during those hits.
The term "sprite" describes the visible character, not a technical diagnosis.
The vertical-line report must be investigated separately from the horizontal
line already observed; its first defective frame is not identified yet.

**Capture preparation on October 17, 2026:** use
`scripts/capture-render-er07-boss.ps1` and `scripts/ER07-boss-capture.md` with
the instrumented `ER07-boss-20s` executable. Its default passive twenty-second window
starts after five seconds of preparation and must include before/during/after
the zoom. It preserves the active renderer and adds consumed OAM/CGRAM hashes,
native OBJ buffer metadata, sprite-evaluation flags and GSU video configuration
to the existing native images, per-line PPU/VRAM and DMA/HDMA records. These
measurements do not establish that the affected pixels are OBJ rather than
background/composition. The twenty-second extension supports separate `Zoom`
and `Acertos` scenarios, with a ten-second option and bounded frame/memory
limits. Capture of the vertical-line event and confirmation of cause remain
pending. No boss rendering correction has been applied.
The historical `scripts/capture-render-er07.ps1` covers the scenes now tracked
as ER-10 and must not be used for this boss report. Preserve all earlier fixes
and each complete capture folder. This preparation does not resolve ER-07.

## ER-08 — line above the Bonus-stage text box

**User report:** a line appears above the text box during the Bonus stage.

**Evidence and status:** report only. No screenshot, message identity, renderer,
or sequence timing has been provided yet. The line's appearance and duration
are not documented.

## ER-09 — Stage 1-7 background prevents progress — resolved

**Original report:** Stage 1-7 rendered the walkable scenery incorrectly,
preventing progress. Images 14–15 show the forest/arrow-sign area with terrain
displaced relative to Yoshi. Static screenshots alone did not establish the
layer, renderer, camera behavior or underlying cause.

![Image 14 — original Stage 1-7 report](<_errorRender/imagem14_ER-09-errorRender.png>)

![Image 15 — terrain displaced relative to Yoshi](<_errorRender/imagem15_ER-09-errorRender.png>)

**Isolation on October 17, 2026:** the separate captures recorded renderer B/new
without pressing keyboard R and A/legacy after one R press. The stronger
same-run comparison is preserved in
`G:\x2\yoshi-render-ER09-Placa-Padrao-20261017-103711-40d92b07`.
It contains 600 consecutive frames (2407–3006): B on frames 2407–2726 and A on
2727–3006. The keyboard R press is associated with frame 2727. Across frames
2726/B and 2727/A, all 224 lines have identical captured PPU registers and
matching hashes for all 16 consumed-VRAM pages, but 208 output-line hashes
change and the terrain immediately changes position. Logical movement inputs
are zero around that transition, although other parts of the capture contain
left/right masks. This isolates a renderer-dependent interpretation rather
than proving a guest camera or collision failure. Hashes are not raw-memory
comparisons; OAM/CGRAM equality was not established by this comparison.

**Correction:** the captured scene uses Mode 2 (`BGMODE=$22`), with BG3 acting
as an offset table. Renderer B's `PpuDrawBackground_4bpp_opt` selected the
horizontal table row using BG3VOFS minus one and the vertical row eight pixels
later. With the recorded BG3VOFS=128, B selected rows 15/16 whereas the legacy
path selected 16/17. The correction uses BG3VOFS and BG3VOFS+8, selects the
table column from the source tile column including fine horizontal scroll,
and applies the masked vertical offset without the extra +1. It does not
change GSU execution, guest camera/physics, raster memory history, HDMA order,
or the existing selective BG3VOFS timing adjustment.

**Files changed for the correction, under `modules/snesrecomp/`:**
`runner/src/snes/ppu.c` and `tests/ppu/ppu_sprite_limit_test.c`.
Synthetic Mode 2 cases cover both backgrounds, fine scroll, distinct
offset-table rows and 8/16-pixel tiles. The expanded test fails against the
pre-fix PPU and passes with the correction. Raster-journal, world-mirror,
elastic-band and Mode 7 checks also pass. These are synthetic checks, not
proof of full-game compatibility. The Release game build succeeded.
The passive collection is implemented by `src/render_capture.h`,
`scripts/capture-render-er09.ps1` and `scripts/ER09-capture.md`; it records the
actual renderer without forcing a mode or injecting inputs.

**Post-fix capture:**
`G:\x2\yoshi-render-ER09-Placa-Padrao-20261017-104621-232a4556`
contains 600 consecutive frames (2125–2724), all 134,400 valid lines using
renderer B and Mode 2. Frame/line keys match across image-state and consumed
memory records; no duplicate records, missing native images, frame-cap drops,
reported PPU/DMA/probe/journal losses, or export I/O failures were found.
Sampled native frames throughout the sequence show coherent ground and ramps
without the previously observed large terrain displacement. Presentation is
not captured (`presentation=unavailable`), and no frame-matched reference
emulator comparison was supplied. The Mode 1-only `bg3_sample` is invalid in
this Mode 2 scene by design; its row count is not evidence of sampled BG3 pixels.

The tested executable is `G:\x2\Yoshi's Island recomp.ER09-candidate.exe`,
SHA-256 `D79638A577BDD2A33B1BDACE3225C442BCA566F52BDD631A29627B418E5D4ED6`.
The previous instrumented EXE and earlier capture folders remain preserved.

**User validation and status:** the user confirmed that the problem disappeared
completely and played several additional minutes, advancing through the game
without noticing further problems. ER-09 is **resolved in the tested Stage 1-7
scenario on October 17, 2026**. The ten-second capture and the longer user
playtest are separate evidence; neither certifies every stage or effect.

## ER-12 — pixels extending beyond the rotating bar — resolved in the tested scenario

**Original report on October 17, 2026:** pixels extend sideways beyond the
rotating pink bar near the lava in castle 1-4. Both extremities are affected.
The user clarified that this is the pink rotating bar, not the nearby walkway
of small orange blocks. The report concerns appearance; the user noticed no
change in gameplay behavior. The same part of the same ROM did not show the
artifact in Bnes according to the user. Bnes version/settings and synchronized
reference images remain unavailable.

**Before-fix evidence:** preserve both complete folders:

- `G:\x2\yoshi-render-ER12-BarraLava-Padrao-20261017-201855-3445ad22`:
  300 consecutive frames, 5454–5753, all 67,200 valid lines in renderer B.
  Frames 5680–5691 show irregular left-end protrusions; frames 5718–5729
  document the right end. All native image-line hashes match the TSV.
- `G:\x2\yoshi-render-ER12-BarraLava-UmaTrocaR-20261017-203333-410ca730`:
  299 consecutive frames, 5812–6110. Actual renderer B covers 5812–5896;
  A covers 5897–6110, following the keyboard R press associated with 5897.
  The final bar segment, including the conspicuous extension in 6064–6065
  and the following irregular contour in 6066–6075, is in A. The run label
  and the user's initial B attribution do not override `row.renderer`.

Both used `Yoshi's Island recomp.ER12-DelayBG2-test.exe`, SHA-256
`C055E1FEAC02D4BA6B015E8E5EB4E2847E15FCEAFD1F53F2D21175C35ABABC91`,
with continuous BG2/BG3 VOFS policy `06`. Neither occurrence is explained by
an experiment expiring. The native images already contain the artifact;
GPU presentation is not required to produce it. No missing images, duplicate
checked frame/line records, frame-cap drops, export I/O failures or recorded
PPU/DMA/probe/memory-journal losses were found in these captures. Recent GSU
job retention is bounded, with eviction completeness unknown.

**Mechanism and correction:** the existing BG3 policy deferred HDMA VOFS by
one line while consuming the current slot's HOFS. This combined coordinates
from different transfer slots during rotation. The new opt-in driver policy
also defers HOFS when the same HDMA slot writes both BG3 axes and BG3 VOFS is
already deferred. The line consumes the preceding pair together; afterward,
the real HDMA scroll results are restored for subsequent lines. `RenderEnd`
continues restoring guest state. The existing vertical BG2/BG3 mask remains
`06`; paired horizontal policy is `04`, with actual application recorded per
line. No PPU drawing algorithm, GSU execution or input behavior was changed.
The before/after evidence strongly supports mixed scroll phases as the cause
in this tested bar effect, without establishing a universal HDMA timing rule.

**Post-fix capture:**
`G:\x2\yoshi-render-ER12-BG3Pair-BarraLava-Padrao-20261017-204258-13658d49`
contains 300 consecutive frames, 5243–5542, all in B/Mode 1 without mosaic.
The 67,200 native line hashes match the TSV. Checked frame/line keys agree,
with no missing images, duplicates, frame-cap drops, export failures or
recorded PPU/DMA/probe/memory-journal losses. Vertical policy stays `06`.
Horizontal policy is `04`, applied on 62,700 lines; on every applied line,
both consumed BG3 axes match the preceding HDMA slot. The remaining 4,500
lines record no horizontal application, as required by the conditional policy.

Old/new frame pairs 5682/5453, 5685/5459, 5691/5469 and 5720/5515 share
identical BG3 vertical sequences across all 223 comparable lines. Enlarged
native-image comparisons show the former left-end protrusions absent in the
paired variant. These are equivalent vertical-scroll states, not identical
complete guest states: camera position, objects and some VRAM-page hashes
differ. The capture does not include final GPU presentation or a synchronized
Bnes reference.

**User validation and resolution:** after the successful capture, the user
switched renderer at the bar and did not see the artifact. The user then
reached and fought Boss 1-4, switching renderer during the fight, and reported
that ER-07 did not return. This closes ER-12 in the tested bar scenario on
October 17, 2026, with user-reported validation under both renderers and a
follow-up ER-07 regression check. That later playtest is separate from the
five-second B capture; no telemetry for the renderer switches or boss fight
was supplied. It does not separately revalidate the original ER-10 passage
and door scenarios or certify every game effect.

**Validated build and implementation scope:**
`G:\x2\Yoshi's Island recomp.ER12-BG3Pair-test.exe`, SHA-256
`B24245847FE94270846527F9EBE34678F43B0546C729C1BF4A7546DA46AFF3A1`.
Release compile/link, binary markers and deployed hashes were verified.
`YI_ER12_BG3_PAIR_TEST` defaults to `OFF` and requires `YI_DELAY_BG2_TEST=ON`
for this variant. Both adjustments remain continuously active in this named
build, independent of capture requests. The normal-build defaults were not
changed, and the result is not yet promoted to them. No new standalone
regression test suite was run for this change.

Framework implementation: commit `d01b7d1`,
`modules/snesrecomp/runner/src/beam_frame_driver.c` and `.h`; existing draw
entry points retain their previous policy. Port/build
integration: `src/game_rtl.c` and `CMakeLists.txt`. Capture support:
`src/render_capture.h`, `scripts/capture-render-er12.ps1` and
`scripts/ER12-capture.md`, using five seconds of preparation and five seconds
of recording. `phase_h_line` distinguishes requested policy from actual
application; `video_state` records the scroll consumed by each line.
Preserve every full capture folder and the earlier executables. ER-07 and
ER-10 retain their separate histories and previous validated scenarios.

## Suggested investigation order

1. **ER-07:** resolved in the complete user-tested Boss 1-4 fight with
   `DelayBG2-test`; reopen only if the reported lines return with the adjustment active.
2. **ER-08:** record the Bonus-stage text box and the line above it.
3. **ER-09:** resolved in the tested Stage 1-7 scenario; reopen only if the
   displaced-terrain artifact returns, preserving the before/after evidence.
4. **ER-10 and ER-11:** previously resolved in the tested scenarios; reopen
   only if either reported artifact returns.
5. **ER-12:** resolved in the user-tested rotating-bar scenario with
   `ER12-BG3Pair-test`; reopen only if its extensions return, recording the
   effective paired-scroll policy and renderer.

ER-01 through ER-07 and ER-09 through ER-12 are resolved in the user-tested
scenarios. Reopen a resolved item only if its reported artifact returns, and
retain the prior captures for comparison.

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
3. **Boss 1-4 — ER-07:** resolved in `DelayBG2-test`; capture only if the
   reported corruption returns with continuous BG2 delay active.
4. **Bonus stage — ER-08:** capture the text box and the line above it.
5. **Stage 1-7 — ER-09:** resolved on October 17, 2026; repeat only if the
   displaced-terrain artifact returns.
6. **Resolved issues ER-01 through ER-07 and ER-09 through ER-11:** repeat a
   tested scenario only if its artifact returns.
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

ER-08 remains open and is deferred to a later session at the user's request.
ER-12 is resolved in the rotating-bar scenario tested with `ER12-BG3Pair-test`;
the user also switched renderer at the bar and during Boss 1-4 without an
ER-07 recurrence. Paired BG3 delay remains an explicit build option, not the
normal default.
ER-07 is resolved in the complete Boss 1-4 fight tested with `DelayBG2-test`;
continuous BG2 delay remains an explicit build option, not the normal default.
ER-09 was resolved in the tested Stage 1-7 scenario on October 17, 2026.
ER-01 through ER-07 and ER-09 through ER-12 are resolved in the scenarios the
user tested; resume a resolved item only if its artifact returns.

## History

| Date | Record |
| --- | --- |
| October 17, 2026 | ER-12 resolved in the tested rotating-bar scenario with `ER12-BG3Pair-test` (SHA-256 `B24245847FE94270846527F9EBE34678F43B0546C729C1BF4A7546DA46AFF3A1`). Paired BG3 HDMA HOFS/VOFS consumption removes the captured lateral protrusions while preserving vertical policy `06`. Post-fix capture: 300 consecutive B frames, 5243–5542; 62,700 lines confirm both axes consumed from the previous slot. The user subsequently switched renderers at the bar and during Boss 1-4 without reproducing ER-12 or ER-07. Both build options remain opt-in; no promotion to normal defaults. |
| October 17, 2026 | Added ER-12: the user reports that some pixels stretch sideways from a bar when it is almost touching the lava in castle 1-4. No images or telemetry supplied; no cause assigned. Recorded as a separate open issue for later investigation, without reopening ER-07 or ER-10. |
| October 17, 2026 | ER-07 resolved in the complete user-tested Boss 1-4 fight with `Yoshi's Island recomp.DelayBG2-test.exe` (SHA-256 `446F68D552A15BB5B7EE72F59C59C4CD3B201515C28364A499AC5BEC9AB8C29D`). Continuous one-line BG2 HDMA VOFS consumption delay adds BG2 to the preserved BG3 policy (mask `06`). Timed captures validated the active policy and growth improvement; the final complete-fight confirmation is a user report. Previously observed returns happened outside the active capture window. `YI_DELAY_BG2_TEST` defaults to `OFF`, so the validated behavior remains specific to the named test build. Further issue investigations are deferred. |
| October 17, 2026 | ER-09 resolved in the tested Stage 1-7 scenario after correcting renderer B Mode 2 offset-table row/column selection and vertical offsets. Post-fix capture `G:\x2\yoshi-render-ER09-Placa-Padrao-20261017-104621-232a4556`: 600 consecutive frames (2125–2724), all valid lines in B/Mode 2, no recorded losses or export failures. The user confirms complete disappearance and several additional minutes of progression without noticed problems. Synthetic regressions and Release build passed; see ER-09 for scope, executable hash and preserved evidence. |
| October 16, 2026 | The user reported three new rendering problems: sprite-side corruption during the Boss 1-4 zoom (ER-07), a line above the Bonus-stage text box (ER-08), and a severely incorrect Stage 1-7 background that prevents progress (ER-09). No new captures or screenshots were supplied. To keep IDs unique, the previously resolved secret-passage/first-castle background issue was renumbered ER-10, and the previously resolved title-screen horizon was renumbered ER-11. Earlier history and capture names retain their original IDs. |
| October 16, 2026 | ER-07 resolved in both user-tested scenes with the one-line BG3 HDMA VOFS timing correction. The user reports the passage weight-bar and first-castle door corruption disappeared completely. Follow-up captures: `G:\x2\yoshi-render-ER07-Passagem-B-20261016-094631-31dbccd7` (600 consecutive frames) and `G:\x2\yoshi-render-ER07-Castelo-B-20261016-094740-bc639c63` (599 consecutive frames). Renderer B was effective for every valid line; no invalid lines, frame-cap drops, or export I/O failures were found, and visual inspection found no remaining reported corruption. Executable SHA-256 `24F3F765795EF31209DEC25F5CAFBD6DE051D2EA201981BD2F062208CF7DA12E`. The same fix works in both tests, but a common underlying cause is not proven; castle room/door and reference comparison remain undocumented. |
| October 16, 2026 | User reports ER-01, ER-02 and ER-03 resolved during opening/title-screen tests; together with prior confirmations, ER-01 through ER-06 are resolved in the tested scenarios. Added ER-07 for similar background corruption during the World 1-3 secret-passage weight-bar movement and descending first-castle doors; shared cause remains a hypothesis. Preserved the former title-screen horizon ER-07 as ER-08 and saved its five screenshots under `_errorRender/`. |
| October 16, 2026 | ER-08 resolved in the title-screen scenario shown: the user reports the bright horizon line is no longer present and supplied image 13. Status is limited to the shown scenario; no reference comparison or temporal capture was provided. |
| October 16, 2026 | ER-05 resolved in the tested scenario: the user repeated text-box openings several times and confirmed that the defect no longer occurs after the completed-field HDMA source-history fix. Game build and five PPU regression tests passed; deployed EXE SHA-256 `8B4E074FF8E8C5B72EBACDDD004C78BDA9CB42365139EAB8BA9DB08946A3BAFD`. Post-fix visual confirmation is user-reported, without a new capture. |
| October 15, 2026 | ER-03 resolved in the tested title-screen scenario after moving HDMA processing before scanline rendering and removing separate first-line HDMA priming; user confirmed the fix. Framework commit `cc8c0ae`, pinned by project commit `46fc4fa`. ER-05 remained open. |
| October 15, 2026 | ER-06 recorded as resolved in the tested scenario, with its cause and before/after captures. ER-05 was initially prioritized for horizontal movement measurements; those measurements are now tracked as ER-04, while ER-05 is reserved for the opening text box. |
| October 15, 2026 | ER-05 instrumentation built and deployed to `G:\x2`; runtime capture and analysis pending. |
| October 15, 2026 | Issue scopes clarified: ER-04 is horizontal movement of interactive objects; ER-05 is the opening text box; the title-screen horizon line was assigned ER-07. On October 16 the horizon item was renumbered ER-08 to reserve ER-07 for the newly reported gameplay background corruption. Earlier capture manifests and history retain their original IDs. |
| October 14, 2026 | Full-screen flicker eliminated in the intro according to the user's test and the instrumented interval; this does not mean the local rendering artifacts are resolved. |
| October 14, 2026 | ER-01 through ER-05 added with seven supplied images; no further fix or build was performed for this document. |
| October 14, 2026 | Links updated to `Yoshi's Island recomp_errorRender/`; future instrumentation plan, per-issue sessions, and validation criteria documented. Only this Markdown file was changed; implementation and tests remain pending. |

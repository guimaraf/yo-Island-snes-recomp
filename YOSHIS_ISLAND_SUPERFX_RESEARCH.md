# Yoshi's Island — External Super FX Research and Diagnostic Applications

Review date: October 17, 2026.
Local baseline: port `c9e5aee`, framework `d01b7d1`.

This research note complements the
[rendering guide](YOSHIS_ISLAND_RENDERING_GUIDE.md). It compares the supplied
community references with this port's implementation. External reverse
engineering is evidence about the documented game revision; it is not a
substitute for tracing this executable or validating hardware timing.

No external tool, assembler, ROM-generation script or emulator was executed
for this review. Only documentation and selected source files were inspected.
No external source, ROM, graphics asset or binary was imported into the project.

## 1. Sources and revision identity

| Source | Inspected revision | Role |
| --- | --- | --- |
| [yoshisisland-disassembly](https://github.com/brunovalads/yoshisisland-disassembly) | [`fb79f7994fb19146de35c02fb98fee8af5877372`](https://github.com/brunovalads/yoshisisland-disassembly/commit/fb79f7994fb19146de35c02fb98fee8af5877372), August 15, 2026 | Annotated original-game routines, register names and RAM definitions |
| [Super FX Graphics wiki](https://github.com/brunovalads/yoshisisland-disassembly/wiki/Super-FX-Graphics) | Live wiki reviewed October 17, 2026; page attributes its July 13, 2017 edit to Raidenthequick | Game-specific input bitmap organization |
| [YoshisIsland_BizHawkTool](https://github.com/brunovalads/YoshisIsland_BizHawkTool) | [`579d5b44fd6735c14b4c5914a8aa71c0c710c37a`](https://github.com/brunovalads/YoshisIsland_BizHawkTool/commit/579d5b44fd6735c14b4c5914a8aa71c0c710c37a), September 7, 2026 | C# external-tool infrastructure, version recognition and memory-domain handling |
| [yoshis-island](https://github.com/brunovalads/yoshis-island) | [`657c2dd34c422511708c57c6711d346ac041a0a2`](https://github.com/brunovalads/yoshis-island/commit/657c2dd34c422511708c57c6711d346ac041a0a2), August 6, 2026 | Lua instrumentation, game-state addresses and object/camera interpretation |

The disassembly README identifies NTSC-US 1.0 and publishes SHA-256
`9B4957466798BBDB5B43A450BBB60B2591AE81D95B891430F62D53CA62E8BC7B`.
That matches [rom_identity.txt](rom_identity.txt), making these labels more
directly applicable than addresses from an unidentified regional revision.
This matches declared identities; no ROM was assembled or byte-compared here.
See the pinned [README](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/README.md).

The wiki is separate from the repository commit and can change independently.
Its contributors include authors other than the repository owner. Some
comments contain explicit uncertainty or conflicting explanations; those are
research leads, not automatically established chip specifications. The VRAM
Maps and Super FX Registers wiki pages could not be retrieved in this review;
their contents are not claimed as inspected. Register definitions were read
from the pinned repository instead.

## 2. New distinction: input bitmaps are not the PPU's output tile format

The [graphics wiki](https://github.com/brunovalads/yoshisisland-disassembly/wiki/Super-FX-Graphics)
describes game-specific GSU source blocks of `0x2000` bytes. Each block packs
two 256-by-32, 16-color images: one in each byte's high nibble and one in its
low nibble. Source rows therefore advance by `0x100` bytes, rather than by
the width of an individual object drawn from that image.

The routine's graphics selector uses bit 0 to choose the high/low image;
that bit is not an odd-byte source-address displacement. In the described
routine, the `MERGE`-based index represents `256*y + x`.

These are **source conventions used by the game**, not a universal GSU input
format. Do not decode these source bytes as SNES planar tiles or interpret
the selector bit as an alignment bug.

**Local cross-check:** `color()` in
[superfx.c](modules/snesrecomp/runner/src/snes/superfx.c) already implements
upper-nibble selection through POR bit `04`. Its `pixel_address()` and
`flush_pixel()` separately produce planar output in cartridge RAM. The new
information is how this game's source selectors and row layout exercise
those existing mechanisms; it does not establish a missing core feature.

For future wrong-artwork or wrong-color reports, distinguish source selector,
GSU output, transfer into VRAM and PPU interpretation before changing decoding.

## 3. The GSU also produces control data, not only plotted artwork

Selected annotations in pinned
[bank08.asm](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/disassembly/bank08.asm)
identify useful entry points:

| Routine | Research value |
| --- | --- |
| `gsu_opt_moving_platforms`, `$089DCE` | Builds offset-per-tile data for oscillating platforms and wavy effects using time and camera inputs |
| `gsu_draw_door`, `$08D317` | Draws a door using an angle and bitmap parameters, including side-edge geometry rather than only a flat image |
| `gsu_compute_fuzzy_BG1_offsets` | A named starting point for the fuzzy background-offset path |
| `gsu_decompress_lc_lz1` | Identifies decompression as another GSU workload |
| `gsu_update_cross_section`, `gsu_draw_cross_section_masked` | Starting points for cross-section masking |

The same source marks `BUG_wavy_column_bounds`, describing a boundary check
that can touch a neighboring table entry. This is a documented original-game
bug candidate, **not evidence that it caused ER-09, ER-12 or another local
artifact**. It is a reason to compare against the exact reference ROM before
silently correcting guest behavior.

The [routine inventory](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/docs/gsu_routines.txt)
also supplies known GSU entry addresses. Its list alone does not identify the
current job or prove a routine's role in a particular capture.

**Diagnostic inference:** a perfectly valid bitmap can still be presented
incorrectly if generated offsets, HDMA tables or their consumption belong to
different moments. Conversely, a failure in control-data generation might
look like a PPU addressing defect. Existing `job_recent`/PC metadata can be
mapped to these labels as an investigation aid, without exporting opcodes.

Do not assume `gsu_draw_door` implements the exact ER-10 door, or that the
rotating-bar effect calls any routine above, without tracing the relevant
entry point. Similar names and appearance are not that trace.

## 4. A more explicit path from cartridge RAM to HDMA consumption

The community [SRAM map](https://github.com/brunovalads/yoshisisland-disassembly/wiki/SRAM-Map)
documents cartridge-RAM scroll buffers at `$703372` and `$703516`, copied to
WRAM `$7E5040` and `$7E51E4`. Each scroll buffer spans 420 bytes. It also
identifies a window buffer at `$703A02`, a plotting/decompression workspace
at `$705800`, and dynamic graphics reservation state at `$701ECC/$701ECE`.

The map distinguishes a 24-slot main-object system and a 16-slot ambient-object
system from an intermediate 256-entry OAM buffer at `$700200`. OAM low/high
mirrors are at `$700A00/$700C00`. Those structures must not be conflated with
the PPU's 128 hardware OBJ entries or this port's OAM probe slots.

The [WRAM map](https://github.com/brunovalads/yoshisisland-disassembly/wiki/WRAM-Map)
describes `$7E5040` and `$7E51E4` as scroll tables whose destinations change
with the active effect: BG3 vertical/horizontal, or BG2 horizontal/vertical
in the documented OPT setup. `$7E56D0` holds window data; `$7E5B98` and
`$7E5C18` are documented indirect-table locations for channels 4 and 3.

Consequently, diagnose from the **effective DMA channel destination and mode**,
not a permanent meaning assigned to a WRAM address. Reused buffers do not
prove shared causes across scenes. The map also documents text-box state at
`$7E0D0F` and black-box transition size at `$7E0D19`, useful leads for ER-08
or a future dialogue regression.

```text
GSU/game calculations
    -> cartridge-RAM output or control-data buffers
    -> transfer/staging into VRAM or WRAM tables
    -> field-associated replay sources and DMA configuration
    -> HDMA destination registers
    -> actual scroll/window/color state consumed by the PPU
    -> native image and later desktop presentation
```

The buffer descriptions are external documentation, not a tracing result for
every transition in this port. Selected
[bank00.asm](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/disassembly/bank00.asm)
also exposes `NMI`, `process_vram_dma_queue`, `process_tilemap_dma_queue` and
completion-gated processing around `$00C29E`. These provide concrete places
to investigate handoff and upload cadence; do not infer one task or upload
per captured frame from the routine names alone.

**Local cross-check:** the port already records register/transfer metadata,
consumed VRAM hashes and field-associated HDMA WRAM/DMA snapshots. These
external addresses explain additional upstream stages but do not expand the
existing WRAM snapshot into a per-line cartridge-RAM history. No new snapshots
or payload dumps were added in this research.

## 5. Semantic game state can complement PPU-state captures

The pinned
[RAM definitions](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/disassembly/vars/ram_vars.asm)
name the following small set of candidate fields:

| CPU address | Meaning documented for this revision | Candidate use |
| --- | --- | --- |
| `$7E0030` | Global frame counter | Separate guest updates from the port-call counter |
| `$7E0039/$7E003B` | BG1 camera X/Y | Check scene-relative movement without treating raster HOFS as camera position |
| `$7E003D/$7E003F` | BG2 camera X/Y | Compare layer/camera intent with consumed scroll |
| `$7E0118` | Game mode | Distinguish gameplay, loading and other state reuse |
| `$7E021A` | Level index | Identify the scene without relying only on a screenshot |
| `$7E094A` | HDMAEN mirror | Compare intended enable state with the effective DMA unit |

These are meanings to validate at a named guest phase, not telemetry already
present in `video_state`. A register mirror can lag the actual register and a
game counter can wrap. Neither is an independent hardware timing authority.

The pinned Lua
[BizHawk script](https://github.com/brunovalads/yoshis-island/blob/657c2dd34c422511708c57c6711d346ac041a0a2/BizHawk/Yoshi's-Island-BizHawk.lua)
reads that frame counter and the camera as 16-bit values, and derives a room
index by comparing the cartridge-RAM sprite-data pointer at offset `0x2600`
with a known pointer list. That is a lookup-derived identifier, not a universal
room-ID register. Its lagmeter is a separate beam-position heuristic triggered
at a game-loop execution callback; it is not the host's dropped-frame metric.

**Proposed future instrumentation, not implemented:** bounded scalar samples
of mode/level/camera/counter at guest-update and field-boundary phases would
improve scene identification. Main/ambient object IDs and coordinates could
then complement OAM observations when object identity is genuinely needed.
Use domain-correct internal storage, with no side-effecting guest reads,
input injection or bulk memory export. Keep these semantic fields separate
from the per-line state actually consumed for rendering.

## 6. What the two tools can and cannot contribute

### C# external tool

The pinned
[YIData.cs](https://github.com/brunovalads/YoshisIsland_BizHawkTool/blob/579d5b44fd6735c14b4c5914a8aa71c0c710c37a/YoshisIsland_BizHawkTool/Core/Game/YIData.cs)
contains original-revision hashes, including this USA 1.0 identity. Its
[MemoryManager.cs](https://github.com/brunovalads/YoshisIsland_BizHawkTool/blob/579d5b44fd6735c14b4c5914a8aa71c0c710c37a/YoshisIsland_BizHawkTool/Core/APIWrappers/MemoryManager.cs)
selects distinct ROM, CARTRAM and WRAM memory domains and supports typed,
little-endian reads. Domain offsets must not be copied directly into SNES
CPU address-space reads: CARTRAM offset `0x2600` and CPU `$702600` describe
different addressing interfaces.

The UI advertises many options, but
[GuiManager.DrawEverything](https://github.com/brunovalads/YoshisIsland_BizHawkTool/blob/579d5b44fd6735c14b4c5914a8aa71c0c710c37a/YoshisIsland_BizHawkTool/Core/APIWrappers/GuiManager.cs)
currently clears surfaces, draws the dark filter and optionally runs debug
performance drawing. The inspected path does not implement the displayed
sprite/camera overlays. Treat this revision as useful infrastructure, not a
fully verified replacement for the Lua script. Nothing was built or installed.

### Lua utility scripts

The Lua implementation has substantially more game-specific reading and
display logic than that inspected C# drawing path. Its memory definitions
agree with several disassembly fields, improving confidence in the research
leads, but not independently proving this port's runtime values.

It also includes memory-changing features: spawning/dragging objects, warps
and other cheats. The inspected script initializes `Cheat.allow_cheats=false`,
but the UI can enable it. Do not execute the entire tool as though every
feature were a passive observer. No script was run or copied into the port;
only definitions and relevant functions were reviewed.

## 7. Chip facts already represented locally, and one correction

The pinned disassembly's
[register definitions](https://github.com/brunovalads/yoshisisland-disassembly/blob/fb79f7994fb19146de35c02fb98fee8af5877372/disassembly/vars/reg_vars.asm)
distinguish:

- `COLR`: internal plotting color, not a CPU-mapped color register at `$303B`.
- `POR`: internal plot options, including high-nibble selection and OBJ layout.
- `SCBR=$3038`: plotting base in 1 KiB units.
- `CLSR=$3039`: clock selection.
- `SCMR=$303A`: screen layout/depth and RON/RAN ownership controls.
- `VCR=$303B`: **read-only version code**, not current plotting color.

The older local [Super FX report](Super%20FX2-deep-research-report.md)
incorrectly called VCR a color register. Its VCR description has been corrected.
The same report contains conflicting memory-limit statements; no new physical
chip capacity claim is derived from them here. The project ROM's 2 MiB file
size, emulator address masks and a chip family's theoretical capacity are
different quantities.

In local [superfx.c](modules/snesrecomp/runner/src/snes/superfx.c), `f->colr`
drives pixel color; `f->vcr` is returned on `$303B` reads and initialized to 4.
The same file already models plot options, output layout, instruction cache,
two pixel-cache entries and buffered RAM/ROM activity. `RPIX` flushes pending
pixel entries before reading. Do not infer a missing nibble conversion, cache
flush or bank mask just because it was absent from the high-level guide.

The [instruction-set wiki](https://github.com/brunovalads/yoshisisland-disassembly/wiki/Super-FX-Instruction-Set)
also distinguishes operations by ALT state, such as `PLOT` versus `RPIX` and
`COLOR` versus `CMODE`. A PC/opcode observation without prefix state is not a
complete instruction interpretation. This provides terminology for future
traces; no instruction-semantics or bus-ownership conformance test was run.

Register comments include hardware-access and transparency claims that need
care: some are uncertain or internally inconsistent. Compare the applicable
local path with stronger chip references or targeted tests before proposing
a core change; this review does not certify every comment or access rule.

## 8. How this changes the next investigation

| Future symptom | Newly available lead | Evidence still required |
| --- | --- | --- |
| Wrong transformed image or palette | Source nibble selector versus planar plotted output | Captured job/configuration and the first divergent output/transfer, not a guessed source-format patch |
| Background deformation or OPT displacement | Named GSU offset-generation routines and OPT data structures | Correct generated table, handoff and PPU interpretation for the actual scene |
| Line-local scroll/window distortion | Cartridge buffer -> WRAM table -> channel destination -> consumed state | Actual channel/mode, paired axes and field provenance; table addresses alone are insufficient |
| Apparent object/camera drift | Named camera variables and main/ambient object tables | Phase-aligned scalar samples and object identification, distinct from OAM slots |
| Text-window sequence or Bonus line | Documented message state/transition fields | Exact scene/message and before/during/after capture; ER-08 remains open |
| Door edge or masking artifact | `gsu_draw_door` and cross-section entry points | Proof that the scene invokes that path and comparison before composition |

The repository review adds upstream context and semantic identifiers. It does
**not** prove that selective delayed consumption is original hardware behavior,
change the validated ER-07/ER-10/ER-12 histories, or justify altering normal
build defaults. It also does not establish a newly reproducible core defect.

For now, keep this material as a source-linked research map. Add runtime
instrumentation only when a concrete report requires one of these fields,
and preserve the existing passive-capture and guest-state restoration contracts.

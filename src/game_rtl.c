/* Yoshi's Island: beam-aligned experimental driver with bounded telemetry.
 * Field/IRQ/HDMA ownership follows snesrecomp/docs/FRAME_MODEL_HOSTS.md. */

#include "game_rtl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "beam_frame_driver.h"
#include "common_rtl.h"          /* SimpleHdma_*, g_snesrecomp_last_hdmaen */
#include "cpu_state.h"
#include "snes/dma.h"
#include "snes/interp_bridge.h"
#include "snes/ppu.h"
#include "snes/snes.h"
#include "snes/superfx.h"

extern CpuState g_cpu;
extern Ppu *g_ppu;

/* Opt-in bounded telemetry. No guest reads, timing changes or memory dumps. */
static FILE *trace_file;
static unsigned trace_frame, trace_start, trace_end, trace_events, trace_lost;
static int trace_initialized, trace_line = -1;
static const char *trace_phase = "cpu";
static void trace_state(const char *kind, int row);
static void trace_observe(int event, uint16_t reg, uint8_t val);

static unsigned trace_number(const char *name, unsigned fallback) {
    const char *s = getenv(name);
    char *end;
    unsigned long n;
    if (!s || !*s) return fallback;
    n = strtoul(s, &end, 10);
    return *end || n > 10000000ul ? fallback : (unsigned)n;
}
static int trace_active(void) {
    return trace_file && trace_frame >= trace_start && trace_frame <= trace_end;
}
#include "render_capture.h"
static void trace_prefix(const char *kind) {
    fprintf(trace_file, "%s\t%u\t%s\t%d\t%llu\t%llu\t%u\t%u\t%lld\t",
        kind, trace_frame, trace_phase, trace_line,
        (unsigned long long)g_cpu.master_cycles,
        (unsigned long long)g_snes->beamMasterLast,
        g_snes->vPos, g_snes->hPos, (long long)clock());
}
static void trace_observe(int event, uint16_t reg, uint8_t val) {
    rc_event(event, reg, val);
    if (!trace_active()) return;
    if (trace_events++ >= 16384) { trace_lost++; return; }
    if (event) trace_state("boundary_state", -1);
    trace_prefix(event ? "boundary225" : "write");
    /* Bulk transfer payload is deliberately redacted. */
    if (reg == 0x2104 || reg == 0x2118 || reg == 0x2119 || reg == 0x2122)
        fprintf(trace_file, "%04X\tredacted\t%u\n", reg, g_ppu_wlog_src);
    else
        fprintf(trace_file, "%04X\t%02X\t%u\t%u\n", reg, val,
            g_ppu_wlog_src, reg == 0x2100 ? !!(val & 0x80) : 0);
}
static void trace_state(const char *kind, int row) {
    unsigned count, dropped, next, completed, x, nonblack = 0;
    uint32_t hash = 2166136261u;
    SuperFx *fx;
    if (!trace_active()) return;
    ppu_rasterStats(&count, &dropped, &next, &completed);
    trace_prefix(kind);
    fprintf(trace_file, "%02X\t%u\t%u\t%02X\t%02X\t%02X\t%u\t%u\t%u\t%u\t%u\t",
        g_ppu->inidisp, !!PPU_forcedBlank(g_ppu), PPU_brightness(g_ppu),
        g_ppu->bgmode, g_ppu->screenEnabled[0], g_ppu->screenEnabled[1],
        count, dropped, next, completed, trace_lost);
    /* Renderer register window only; no VRAM/OAM/CGRAM or ROM payload. */
    for (x = 0; x < PPU_SAVESTATE_REGS_SIZE; x++)
        fprintf(trace_file, "%02X", ((const uint8_t *)&g_ppu->inidisp)[x]);
    fputc('\t', trace_file);
    fx = g_snes->cart ? g_snes->cart->superfx : NULL;
    if (fx)
        fprintf(trace_file, "%llu\t%llu\t%lld\t%04X\t%02X\t%04X\t%u\t%02X\t%02X\t",
            (unsigned long long)fx->master_clock,
            (unsigned long long)fx->instruction_count, (long long)fx->clock_credit,
            fx->sfr, fx->pbr, fx->r[15].data, fx->irq_pending, fx->scmr, fx->scbr);
    else fputs("0\t0\t0\t0\t0\t0\t0\t0\t0\t", trace_file);
    /* Native output row, including widescreen if enabled, excluding padding/X. */
    if (row >= 0 && g_ppu->renderBuffer &&
        g_ppu->renderPitch >= (256u + 2u*g_ppu->extraLeftRight)*4u) {
        const uint8_t *p = g_ppu->renderBuffer + (size_t)row*g_ppu->renderPitch;
        for (x = 0; x < 256u + 2u*g_ppu->extraLeftRight; x++) {
            unsigned c;
            nonblack += !!(p[4*x] | p[4*x+1] | p[4*x+2]);
            for (c = 0; c < 3; c++) hash = (hash ^ p[4*x+c])*16777619u;
        }
        fprintf(trace_file, "%08X\t%u\n", hash, nonblack);
    } else fputs("NA\tNA\n", trace_file);
}
static void trace_begin(void) {
    g_host_r_key_observer = rc_input_r;
    if (!trace_initialized) {
        const char *path = getenv("YI_TRACE_FILE");
        trace_initialized = 1;
        trace_start = trace_number("YI_TRACE_START", 640);
        { unsigned n = trace_number("YI_TRACE_FRAMES", 60);
          if (n > 120) n = 120;
          trace_end = trace_start + (n ? n : 1) - 1; }
        /* Exclusive creation preserves earlier captures. Explicit path required. */
        if (path && *path) {
            trace_file = fopen(path, "wx");
            if (!trace_file) perror("YI_TRACE_FILE");
        }
        if (trace_file) {
            fprintf(trace_file, "# yi-trace-v2 driver=beam_frame_driver start=%u end=%u epoch=%lld clocks_per_sec=%ld\n",
                trace_start, trace_end, (long long)time(NULL), (long)CLOCKS_PER_SEC);
            { const char *scene = getenv("YI_TRACE_SCENE");
              fputs("# scene=", trace_file);
              if (scene) { unsigned i;
                  for (i = 0; scene[i] && i < 80; i++)
                      fputc((unsigned char)scene[i] < 32 ? '_' : scene[i], trace_file);
              } else fputs("unknown", trace_file);
              fputc('\n', trace_file); }
            ppu_set_display_observer(trace_observe);
        }
    }
    ++trace_frame;
    trace_events = trace_lost = 0;
    trace_phase = "cpu"; trace_line = -1;
    rc_begin();
    rc_input_sample();
    trace_state("begin", -1);
}

void GameRunOneFrame(void)
{
    trace_begin();
    snes_beam_frame_driver_run_frame();
}

/* The framework invokes this after drawing the line, before H-blank HDMA.
 * No guest execution or state mutation is allowed in this observer. */
static void trace_drawn_line(const Ppu *ppu, unsigned line, void *context)
{
    (void)ppu;
    (void)context;
    trace_line = (int)line;
    rc_row(ppu, line);
    trace_state("drawn", -1);
    trace_state("row", (int)line - 1);
    /* The following HDMA/replay writes prepare the next visible line. */
    trace_line = (int)line + 1;
}

void GameDrawPpuFrame(void)
{
    rc_probe(8);
    trace_state("cpu_end", -1);
    trace_phase = "draw";
    trace_line = 0;
    snes_beam_frame_driver_draw_ppu_frame_observed(
        (trace_active() || rc_current) ? trace_drawn_line : NULL, NULL);
    trace_state("draw_end", -1);
    rc_end();
    if (trace_active()) fflush(trace_file);
    if (trace_file && trace_frame >= trace_end) {
        if (!rc_ring) ppu_set_display_observer(NULL);
        fclose(trace_file);
        trace_file = NULL;
    }
}

const RtlGameInfo kGameInfo = {
    .title = "@CATALOG_SLUG@",
    .initialize = NULL,
    .run_frame = &GameRunOneFrame,
    .draw_ppu_frame = &GameDrawPpuFrame,
    .save_name_prefix = "save",
};

void GameSessionReset(void)
{
    rc_input_previous_valid = 0;
    rc_pending_r_count = rc_pending_r_lost = 0;
    if (rc_ring) {
        fputs("YI capture: aborted by session reset; restart process for a new capture\n", stderr);
        if(rc_timed) {
            /* Timer owns completed slots; reset must not free them. */
            rc_done=1; rc_current=NULL;
            SDL_AtomicSet(&rc_timed_reset,1);
            SDL_AtomicSet(&rc_timed_stop,1);
        } else {
            free(rc_ring); rc_ring = NULL; rc_current = NULL; rc_done = 1; rc_resets++;
        }
    }
    snes_beam_frame_driver_reset();
    ppu_rasterReset();
}

/* Private to game_rtl.c. Passive, bounded, opt-in capture; no guest bus reads. */
#include "ppu_dma_trace.h"
#include <stddef.h>
#include "desktop/sdl_compat.h"
#ifdef _WIN32
#include <process.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/* SDL base paths and attach requests are UTF-8; Windows CRT narrow fopen
 * instead uses the active ANSI code page. Convert explicitly for Latin paths. */
static FILE *rc_fopen(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wide_path[2048], wide_mode[8];
    unsigned i;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            wide_path, (int)(sizeof(wide_path)/sizeof(wide_path[0])))) return NULL;
    for(i=0;mode[i] && i<7;i++) wide_mode[i]=(wchar_t)(unsigned char)mode[i];
    wide_mode[i]=0;
    return _wfopen(wide_path, wide_mode);
#else
    return fopen(path, mode);
#endif
}

#define RC_FRAMES 25u
#define RC_TIMED_FRAMES 360u
#define RC_TIMED_ER05_FRAMES 620u
#define RC_TIMED_PREP_MS 5000u
#define RC_TIMED_CAPTURE_MS 2000u
#define RC_TIMED_ER05_CAPTURE_MS 10000u
#define RC_EVENTS 8192u
#define RC_DMA_EVENTS 512u
#define RC_WIDTH 800u
#define RC_INPUT_KEYS 32u
typedef struct { uint64_t counter; unsigned pressed; } RcInputKey;
extern void (*g_host_r_key_observer)(int pressed, uint64_t counter);
static RcInputKey rc_pending_r[RC_INPUT_KEYS];
static unsigned rc_pending_r_count, rc_pending_r_lost;
static unsigned rc_input_previous[2], rc_input_previous_valid;
static uint64_t rc_input_previous_counter;
/* Main-thread only. Bounded metadata; no input injection or global key hook. */
static void rc_input_r(int pressed, uint64_t counter) {
    if (rc_pending_r_count < RC_INPUT_KEYS) {
        rc_pending_r[rc_pending_r_count].counter = counter;
        rc_pending_r[rc_pending_r_count++].pressed = !!pressed;
    } else rc_pending_r_lost++;
}
typedef struct {
    uint64_t cpu, beam;
    unsigned v, h, line, phase, source, reg, value, boundary;
} RcEvent;
/* Image-derived metadata, not object detection or guest memory contents. */
typedef struct {
    unsigned valid, x, width, hash, nonblack, first, last, sum_x;
    unsigned flat_x, flat_len, flat_color_hash, black_x, black_len;
} RcBoxRow;
typedef struct {
    unsigned valid, pitch, width, hash, nonblack, journal, dropped, cursor, completed, renderer;
    uint64_t cpu, beam, gsu_clock, instructions;
    unsigned v, h, sfr, irq;
    uint8_t regs[PPU_SAVESTATE_REGS_SIZE];
    DmaChannel channels[8]; /* exported as metadata only */
    uint8_t hdma_pending_init;
    RcBoxRow box;
} RcLine;
typedef struct {
    uint64_t cpu, beam;
    unsigned v, h, line, phase, kind, address, value, channel;
    unsigned active_mask, pending_init;
    DmaChannel state;
} RcDmaEvent;
#define RC_PROBES 12u
typedef struct {
    unsigned stage, v, h;
    uint64_t cpu, beam;
    uint64_t boundary_serial;
    uint16_t hscroll[4], vscroll[4];
    uint32_t oam_low_hash, oam_high_hash, cgram_hash, oam_writes;
    PpuRasterProbe state;
} RcProbe;
/* Decoded geometry and a content signature only; never export OAM bytes.
 * A slot can be reused and does not identify a game object. Coordinates are
 * native SNES coordinates, before widescreen/HUD adjustments and clipping. */
typedef struct {
    int16_t x;
    uint8_t y, size;
    uint32_t signature;
} RcObjSlot;
typedef struct {
    unsigned valid, stage;
    uint64_t boundary_serial;
    RcObjSlot slot[128];
} RcObjPhase;
typedef struct {
    unsigned probes, probe_lost;
    RcProbe probe[RC_PROBES];
    RcObjPhase obj[2]; /* boundary_before and render_begin_after */
    unsigned obj_lost;
    unsigned id, events, lost, invalid_rows, dma_lost, dma_count, jobs;
    unsigned dma_events, dma_event_lost;
    unsigned box_bulk[4][5][2]; /* port, source (4=unknown), CPU/draw */
    RcEvent event[RC_EVENTS];
    RcDmaEvent dma_event[RC_DMA_EVENTS];
    RcLine line[224];
    uint8_t rgb[224 * RC_WIDTH * 3];
    PpuDmaInfo dma[512];
    SuperFxJob job[64];
    uint64_t dma_begin;
    uint32_t memory_hash[2][18];
    uint64_t host_begin, host_end;
    uint64_t input_counter, input_previous_counter;
    unsigned pads[2], pads_previous[2], input_previous_valid;
    unsigned r_count, r_lost;
    RcInputKey r_key[RC_INPUT_KEYS];
} RcFrame;
static RcFrame *rc_ring, *rc_current;
static void rc_input_sample(void) {
    unsigned p, pads[2];
    uint64_t now = SDL_GetPerformanceCounter();
    pads[0] = g_snes->input1_currentState & 0xfff;
    pads[1] = g_snes->input2_currentState & 0xfff;
    if (rc_current) {
        rc_current->input_counter = now;
        rc_current->input_previous_counter = rc_input_previous_counter;
        rc_current->input_previous_valid = rc_input_previous_valid;
        for (p=0;p<2;p++) {
            rc_current->pads[p] = pads[p];
            rc_current->pads_previous[p] = rc_input_previous[p];
        }
        rc_current->r_count = rc_pending_r_count;
        rc_current->r_lost = rc_pending_r_lost;
        memcpy(rc_current->r_key, rc_pending_r, rc_pending_r_count*sizeof(RcInputKey));
    }
    for (p=0;p<2;p++) rc_input_previous[p] = pads[p];
    rc_input_previous_counter = now;
    rc_input_previous_valid = 1;
    rc_pending_r_count = rc_pending_r_lost = 0;
}
static unsigned rc_initialized, rc_mark, rc_schedule, rc_done, rc_resets, rc_armed_frame;
static const char *rc_dir;
static char rc_attached_dir[1024];
static unsigned rc_timed, rc_timed_prep_ms, rc_timed_capture_ms = RC_TIMED_CAPTURE_MS;
static unsigned rc_timed_long_er05, rc_capacity = RC_FRAMES, rc_timed_used;
static SDL_atomic_t rc_timed_stop, rc_timed_completed, rc_timed_reset;
static SDL_atomic_t rc_timed_writer_done;
static uint64_t rc_deadline_counter;
static uint64_t rc_capture_start_counter;
static uint64_t rc_capture_start_utc_ms;
static uint64_t rc_requested_deadline_utc;
static SDL_atomic_t rc_frame_cap_drops;
static unsigned rc_box_enabled, rc_box_x, rc_box_y, rc_box_w, rc_box_h;
static uint64_t rc_boundary_serial; /* observer notifications, not guest state */
static int SDLCALL rc_timed_writer(void *unused);
static uint64_t rc_utc_ms(void) {
#ifdef _WIN32
    FILETIME ft; ULARGE_INTEGER ticks;
    GetSystemTimeAsFileTime(&ft);
    ticks.LowPart=ft.dwLowDateTime; ticks.HighPart=ft.dwHighDateTime;
    return (ticks.QuadPart-116444736000000000ull)/10000ull;
#else
    return (uint64_t)time(NULL)*1000ull;
#endif
}
/* Small host-only rendezvous. Never interpret commands or access guest state. */
static const char *rc_attach_request(void) {
#ifdef _WIN32
    char path[2048];
    const char *base = SDL_GetBasePath();
    FILE *request;
    int n;
    if (!base) return NULL;
    n = snprintf(path, sizeof(path), "%syi-capture-request-%u.txt", base, (unsigned)_getpid());
#if !SNESRECOMP_SDL3
    SDL_free((void *)base);
#endif
    if (n < 0 || (size_t)n >= sizeof(path)) return NULL;
    request = rc_fopen(path, "rb");
    if (!request) return NULL;
    if (!fgets(rc_attached_dir, sizeof(rc_attached_dir), request)) rc_attached_dir[0] = 0;
    { char mode[32];
      if (fgets(mode, sizeof(mode), request)) {
          if(!strncmp(mode,"timed2prep5cap10\n",17)) {
              rc_timed=1; rc_timed_prep_ms=RC_TIMED_PREP_MS;
              rc_timed_capture_ms=RC_TIMED_ER05_CAPTURE_MS; rc_timed_long_er05=1;
          }
          else if(!strncmp(mode,"timed2prep5\n",12)) {
              rc_timed=1; rc_timed_prep_ms=RC_TIMED_PREP_MS;
              rc_timed_capture_ms=RC_TIMED_CAPTURE_MS; rc_timed_long_er05=0;
          }
          else if(!strncmp(mode,"timed5\n",7)) {
              rc_timed=1; rc_timed_prep_ms=0;
              rc_timed_capture_ms=RC_TIMED_CAPTURE_MS; rc_timed_long_er05=0;
          }
      } }
    rc_box_enabled=rc_box_x=rc_box_y=rc_box_w=rc_box_h=0;
    { char roi[96]; unsigned x,y,w,h;
      if (fgets(roi,sizeof(roi),request) &&
          sscanf(roi,"box %u %u %u %u",&x,&y,&w,&h)==4 &&
          x<RC_WIDTH && y<224 && w>0 && w<=RC_WIDTH-x && h>0 && h<=224-y) {
          rc_box_enabled=1; rc_box_x=x; rc_box_y=y; rc_box_w=w; rc_box_h=h;
      } }
    fclose(request);
    rc_attached_dir[strcspn(rc_attached_dir, "\r\n")] = 0;
    /* Consume the single per-process rendezvous so this same game can be
     * armed again by a later script run. Each run has a fresh output folder. */
    if (rc_attached_dir[0]) {
        wchar_t wide_path[2048];
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                                wide_path, (int)(sizeof(wide_path)/sizeof(wide_path[0]))) ||
            !DeleteFileW(wide_path)) {
            rc_attached_dir[0]=0;
            return NULL;
        }
    }
    return rc_attached_dir[0] ? rc_attached_dir : NULL;
#else
    return NULL;
#endif
}
static void rc_status(const char *name, const char *message) {
    char path[2048]; FILE *f;
    int n = snprintf(path, sizeof(path), "%s/%s", rc_dir, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return;
    f = rc_fopen(path, "wx");
    if (f) { fputs(message, f); fclose(f); }
}
static void rc_memory(unsigned phase) {
    unsigned region, i;
    if (!rc_current) return;
    for (region=0;region<18;region++) {
        const uint8_t *p; size_t n;
        uint32_t hash=2166136261u;
        if(region<16) { p=(const uint8_t *)g_ppu->vram+region*4096; n=4096; }
        else if(region==16) { p=(const uint8_t *)g_ppu->cgram; n=sizeof(g_ppu->cgram); }
        else { p=(const uint8_t *)g_ppu->oam; n=sizeof(g_ppu->oam); }
        for(i=0;i<n;i++) hash=(hash^p[i])*16777619u;
        rc_current->memory_hash[phase][region]=hash;
    }
}

static uint32_t rc_hash_bytes(const void *data, size_t bytes) {
    const uint8_t *p=(const uint8_t *)data;
    uint32_t hash=2166136261u;
    size_t i;
    for(i=0;i<bytes;i++) hash=(hash^p[i])*16777619u;
    return hash;
}
static void rc_obj_phase(unsigned stage) {
    static const uint8_t sizes[8][2]={{8,16},{8,32},{8,64},{16,32},
                                    {16,64},{32,64},{16,32},{16,32}};
    RcObjPhase *out=&rc_current->obj[stage==4];
    unsigned slot;
    if(out->valid) { rc_current->obj_lost++; return; }
    out->valid=1; out->stage=stage; out->boundary_serial=rc_boundary_serial;
    for(slot=0;slot<128;slot++) {
        unsigned high=(g_ppu->highOam[slot>>2]>>((slot&3)*2))&3;
        unsigned x=(g_ppu->oam[slot*2]&255)|((high&1)<<8);
        uint16_t attr=g_ppu->oam[slot*2+1];
        uint8_t signature[3]={(uint8_t)attr,(uint8_t)(attr>>8),sizes[(g_ppu->obsel>>5)&7][high>>1]};
        RcObjSlot *s=&out->slot[slot];
        s->x=(int16_t)(x>=256?(int)x-512:(int)x);
        s->y=(uint8_t)(g_ppu->oam[slot*2]>>8);
        s->size=sizes[(g_ppu->obsel>>5)&7][high>>1];
        s->signature=rc_hash_bytes(signature,sizeof(signature));
    }
}
static void rc_probe(unsigned stage) {
    RcProbe *p;
    if(stage==2) rc_boundary_serial++;
    if (!rc_current) return;
    if (rc_current->probes == RC_PROBES) { rc_current->probe_lost++; return; }
    p=&rc_current->probe[rc_current->probes++];
    p->stage=stage; p->cpu=g_cpu.master_cycles; p->beam=g_snes->beamMasterLast;
    p->v=g_snes->vPos; p->h=g_snes->hPos;
    ppu_rasterProbe(g_ppu, &p->state);
    if(rc_box_enabled) {
        p->boundary_serial=rc_boundary_serial;
        memcpy(p->hscroll,g_ppu->hScroll,sizeof(p->hscroll));
        memcpy(p->vscroll,g_ppu->vScroll,sizeof(p->vscroll));
        p->oam_low_hash=rc_hash_bytes(g_ppu->oam,sizeof(g_ppu->oam));
        p->oam_high_hash=rc_hash_bytes(g_ppu->highOam,sizeof(g_ppu->highOam));
        p->cgram_hash=rc_hash_bytes(g_ppu->cgram,sizeof(g_ppu->cgram));
        p->oam_writes=g_ppu->oamWriteCount;
        if(stage==1 || stage==4) rc_obj_phase(stage);
    }
}
static unsigned rc_hdma_mask(const Dma *dma) {
    unsigned i, mask = 0;
    for (i = 0; i < 8; i++)
        if (dma->channel[i].hdmaActive) mask |= 1u << i;
    return mask;
}
static void rc_dma_trace(DmaTraceKind kind, const Dma *dma,
                         uint16_t address, uint8_t value) {
    unsigned first = 0, last = 0, channel;
    if (!rc_current || !dma) return;
    if (kind == kDmaTraceHdmaInitBefore || kind == kDmaTraceHdmaInitAfter) {
        first = 0; last = 7;
    } else if (address >= 0x4300 && address < 0x4380) {
        first = last = (address >> 4) & 7;
    } else {
        first = last = 8; /* $420C changes the mask as a whole. */
    }
    for (channel = first; channel <= last; channel++) {
        RcDmaEvent *e;
        if (rc_current->dma_events == RC_DMA_EVENTS) {
            rc_current->dma_event_lost++;
            continue;
        }
        e = &rc_current->dma_event[rc_current->dma_events++];
        e->cpu = g_cpu.master_cycles; e->beam = g_snes->beamMasterLast;
        e->v = g_snes->vPos; e->h = g_snes->hPos;
        e->line = (unsigned)(trace_line + 1);
        e->phase = !strcmp(trace_phase, "draw");
        e->kind = (unsigned)kind; e->address = address; e->value = value;
        e->channel = channel;
        e->active_mask = rc_hdma_mask(dma);
        e->pending_init = dma_hdma_pending_init_get(dma);
        if (channel < 8) e->state = dma->channel[channel];
    }
}
static void rc_event(int boundary, uint16_t reg, uint8_t value) {
    RcEvent *e;
    if (!rc_current) return;
    /* ER-05 needs control-register timing, not thousands of redacted bytes.
     * Keep bulk-port counts by origin/phase; DMA metadata remains separate. */
    if(rc_box_enabled && !boundary &&
       (reg==0x2104 || reg==0x2118 || reg==0x2119 || reg==0x2122)) {
        unsigned port=reg==0x2104?0:reg==0x2118?1:reg==0x2119?2:3;
        unsigned source=g_ppu_wlog_src<4?g_ppu_wlog_src:4;
        rc_current->box_bulk[port][source][!strcmp(trace_phase,"draw")]++;
        return;
    }
    if (rc_current->events == RC_EVENTS) { rc_current->lost++; return; }
    e = &rc_current->event[rc_current->events++];
    e->cpu = g_cpu.master_cycles; e->beam = g_snes->beamMasterLast;
    e->v = g_snes->vPos; e->h = g_snes->hPos;
    e->line = (unsigned)(trace_line + 1); /* encoded: 0 means -1 */
    e->phase = !strcmp(trace_phase, "draw"); e->source = g_ppu_wlog_src;
    e->reg = reg; e->boundary = boundary;
    e->value = (reg == 0x2104 || reg == 0x2118 || reg == 0x2119 || reg == 0x2122)
        ? 256 : value; /* sentinel, never retain payload */
}
static void rc_begin(void) {
    /* Re-arm the existing bounded allocation only after the previous writer
     * has closed its files and the render thread released its current slot. */
    if (rc_timed && SDL_AtomicGet(&rc_timed_writer_done) && !rc_current) {
        rc_initialized=0; rc_done=0; rc_mark=0; rc_schedule=0;
        rc_timed_used=0; rc_armed_frame=0; SDL_AtomicSet(&rc_frame_cap_drops,0);
        SDL_AtomicSet(&rc_timed_completed,0); SDL_AtomicSet(&rc_timed_stop,0);
        SDL_AtomicSet(&rc_timed_reset,0); SDL_AtomicSet(&rc_timed_writer_done,0);
        rc_requested_deadline_utc=0; rc_capture_start_utc_ms=0;
        rc_capture_start_counter=0; rc_deadline_counter=0;
        rc_status("capture.rearmed", "yi-render-repeat-v1\n");
        /* Do not clear hundreds of MiB: each published slot is initialized
         * before use and the new manifest bounds the exported frame IDs. */
    }
    if (!rc_initialized) {
        rc_dir = NULL;
        if (!getenv("YI_CAPTURE_DIR") || !*getenv("YI_CAPTURE_DIR")) {
            /* Dormant polling at most once per 30 simulated frames. */
            if (trace_frame % 30u != 1u) return;
            rc_dir = rc_attach_request();
        } else rc_dir = getenv("YI_CAPTURE_DIR");
        if (rc_dir && *rc_dir) {
            rc_initialized = 1;
            rc_capacity=rc_timed?(rc_timed_long_er05?RC_TIMED_ER05_FRAMES:RC_TIMED_FRAMES):RC_FRAMES;
            if (sizeof(RcFrame) * rc_capacity > (rc_timed?(rc_timed_long_er05?768u:512u):64u) * 1024u * 1024u) {
                fputs("YI capture: exceeds memory budget\n", stderr);
                rc_status("capture.failed", "Memory budget exceeded\n"); return;
            }
            if (!rc_ring) rc_ring = calloc(rc_capacity, sizeof(RcFrame));
            if (!rc_ring) { fputs("YI capture: allocation failed\n", stderr);
                rc_status("capture.failed", "Allocation failed\n");
                rc_initialized=0; return; }
            rc_armed_frame = trace_frame;
            /* Attach is explicitly manual even if a stale schedule was inherited. */
            rc_schedule = rc_dir == rc_attached_dir ? 0 : trace_number("YI_CAPTURE_AT", 0);
            ppu_set_display_observer(trace_observe);
            ppu_set_raster_probe_observer(rc_probe);
            dma_set_trace_observer(rc_dma_trace);
            rc_status("capture.armed", rc_timed_long_er05
                ? "yi-render-input-v1 yi-render-timed-window-v2 yi-render-timed-window-er05-10s-v1 preparation_seconds=5 capture_seconds=10 frame_cap=620 pre=0 memory_budget_mib=768\n"
                : (rc_timed ? (rc_timed_prep_ms ? "yi-render-timed-window-v2 preparation_seconds=5 capture_seconds=2 frame_cap=360 pre=0\n" : "yi-render-timed-v1 seconds=5 frame_cap=360 pre=0\n") : "yi-render-attach-v1 pre=8 post=16 batch_cap=1\n"));
            fprintf(stderr, "YI capture: armed, %zu bytes, one batch\n", sizeof(RcFrame)*rc_capacity);
        }
    }
    if (!rc_initialized || !rc_ring || rc_done) return;
    if (!rc_mark) {
        char path[1024]; FILE *signal;
        int n = snprintf(path, sizeof(path), "%s/trigger", rc_dir);
        signal = n > 0 && (size_t)n < sizeof(path) ? rc_fopen(path, "rb") : NULL;
        if (signal || (rc_schedule && trace_frame >= rc_schedule)) {
            rc_mark = trace_frame;
            if (rc_timed) {
                unsigned long long requested_start=0, requested_deadline=0;
                uint64_t now=rc_utc_ms(), remaining, deadline_remaining, counter_now;
                SDL_Thread *timer;
                if (!signal || (rc_timed_prep_ms
                    ? fscanf(signal,"%llu%llu",&requested_start,&requested_deadline)!=2
                    : fscanf(signal,"%llu",&requested_deadline)!=1)) {
                    if(signal) fclose(signal);
                    rc_status("capture.failed","Invalid timed trigger\n"); rc_done=1;
                    SDL_AtomicSet(&rc_timed_writer_done,1); return;
                }
                if(rc_timed_prep_ms &&
                   (requested_deadline<requested_start ||
                    requested_deadline-requested_start!=rc_timed_capture_ms ||
                    requested_start>now+(uint64_t)rc_timed_prep_ms+1000u)) {
                    fclose(signal); rc_status("capture.failed","Invalid timed window\n"); rc_done=1;
                    SDL_AtomicSet(&rc_timed_writer_done,1); return;
                }
                rc_capture_start_utc_ms=rc_timed_prep_ms?requested_start:now;
                remaining=rc_timed_prep_ms?(requested_start>now?requested_start-now:0):0;
                deadline_remaining=requested_deadline>now?requested_deadline-now:0;
                rc_requested_deadline_utc=requested_deadline;
                if(remaining>RC_TIMED_PREP_MS+1000u) remaining=RC_TIMED_PREP_MS+1000u;
                if(deadline_remaining>RC_TIMED_PREP_MS+rc_timed_capture_ms+1000u)
                    deadline_remaining=RC_TIMED_PREP_MS+rc_timed_capture_ms+1000u;
                counter_now=SDL_GetPerformanceCounter();
                rc_capture_start_counter=counter_now+remaining*SDL_GetPerformanceFrequency()/1000u;
                rc_deadline_counter=counter_now+deadline_remaining*SDL_GetPerformanceFrequency()/1000u;
                timer=SDL_CreateThread(rc_timed_writer,"yi-capture-timed",NULL);
                if(timer) SDL_DetachThread(timer);
                else { rc_status("capture.failed","Timer/writer thread failed\n"); rc_done=1;
                    SDL_AtomicSet(&rc_timed_writer_done,1); }
            }
        }
        if (signal) fclose(signal); /* preserve marker; one batch per process */
    }
    if(rc_done) return;
    if(rc_timed) {
        if(!rc_mark || SDL_AtomicGet(&rc_timed_stop)) return;
        if(SDL_GetPerformanceCounter()<rc_capture_start_counter) return;
        if(SDL_GetPerformanceCounter()>=rc_deadline_counter) return;
        if(rc_timed_used==rc_capacity) { SDL_AtomicSet(&rc_frame_cap_drops,SDL_AtomicGet(&rc_frame_cap_drops)+1); return; }
        rc_current=&rc_ring[rc_timed_used];
    } else rc_current = &rc_ring[(trace_frame - 1) % RC_FRAMES];
    /* Large pixel storage need not be cleared; valid rows govern export. */
    memset(rc_current, 0, offsetof(RcFrame, rgb));
    rc_current->id = trace_frame;
    rc_current->host_begin=SDL_GetPerformanceCounter();
    rc_current->dma_begin = ppudma_dma_count();
    rc_memory(0);
    rc_probe(7); /* cpu begin */
}
static void rc_box_measure(RcBoxRow *b, const uint8_t *rgb, unsigned width) {
    unsigned x, end, run_x, run_len=0, run_color=0;
    b->x=rc_box_x;
    if (b->x>=width) return;
    b->width=rc_box_w < width-b->x ? rc_box_w : width-b->x;
    b->valid=1; b->hash=2166136261u;
    b->first=b->last=RC_WIDTH; /* sentinel when no nonblack pixel */
    end=b->x+b->width; run_x=b->x;
    for(x=b->x;x<end;x++) {
        const uint8_t *p=rgb+x*3;
        unsigned color=((unsigned)p[0]<<16)|((unsigned)p[1]<<8)|p[2], c;
        for(c=0;c<3;c++) b->hash=(b->hash^p[c])*16777619u;
        if(color) {
            if(!b->nonblack) b->first=x;
            b->last=x; b->nonblack++; b->sum_x+=x;
        }
        if(!run_len || color!=run_color) {run_x=x;run_len=0;run_color=color;}
        run_len++;
        if(!color && run_len>b->black_len) {b->black_x=run_x;b->black_len=run_len;}
        if(color && run_len>b->flat_len) {
            b->flat_x=run_x;b->flat_len=run_len;
            b->flat_color_hash=2166136261u;
            for(c=0;c<3;c++) b->flat_color_hash=(b->flat_color_hash^p[c])*16777619u;
        }
    }
}
static void rc_row(const Ppu *p, unsigned line) {
    unsigned x, c, width = 256u + 2u*p->extraLeftRight;
    RcLine *r;
    SuperFx *fx;
    if (!rc_current || !line || line > 224) return;
    r = &rc_current->line[line-1];
    r->renderer = (p->renderFlags & kPpuRenderFlags_NewRenderer) != 0;
    r->cpu = g_cpu.master_cycles; r->beam = g_snes->beamMasterLast;
    r->v = g_snes->vPos; r->h = g_snes->hPos;
    r->pitch = p->renderPitch; r->width = width;
    memcpy(r->regs, &p->inidisp, sizeof(r->regs));
    memcpy(r->channels, g_snes->dma->channel, sizeof(r->channels));
    r->hdma_pending_init = dma_hdma_pending_init_get(g_snes->dma);
    ppu_rasterStats(&r->journal, &r->dropped, &r->cursor, &r->completed);
    fx = g_snes->cart ? g_snes->cart->superfx : NULL;
    if (fx) { r->gsu_clock = fx->master_clock; r->instructions = fx->instruction_count;
        r->sfr = fx->sfr; r->irq = fx->irq_pending; }
    if (!p->renderBuffer || width > RC_WIDTH || p->renderPitch < width*4u) {
        rc_current->invalid_rows++; return;
    }
    r->valid = 1; r->hash = 2166136261u;
    for (x = 0; x < width; x++) {
        const uint8_t *pixel = p->renderBuffer + (size_t)(line-1)*p->renderPitch + x*4u;
        r->nonblack += !!(pixel[0] | pixel[1] | pixel[2]);
        for (c = 0; c < 3; c++) r->hash = (r->hash ^ pixel[c])*16777619u;
        /* Native framebuffer is little-endian ARGB (B,G,R,X). */
        for (c = 0; c < 3; c++) rc_current->rgb[((line-1)*RC_WIDTH+x)*3+c] = pixel[2-c];
    }
    if(rc_box_enabled && line-1>=rc_box_y && line-1-rc_box_y<rc_box_h)
        rc_box_measure(&r->box,rc_current->rgb+(line-1)*RC_WIDTH*3,width);
}
static FILE *rc_open(const char *name) {
    char path[1024];
    int n = snprintf(path, sizeof(path), "%s/%s", rc_dir, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return NULL;
    return rc_fopen(path, "wbx");
}
static struct { RcFrame *ring; unsigned last, count, reset; } rc_export;
static int SDLCALL rc_flush(void *unused) {
    unsigned last = rc_export.last;
    RcFrame *ring = rc_export.ring;
    (void)unused;
    unsigned first = rc_timed ? (rc_export.count?ring[0].id:0) : (rc_mark > 8 ? rc_mark-8 : 1), id, y, i, failures = 0, slot;
    unsigned retained=rc_timed?rc_export.count:(last>=first?last-first+1:0);
    FILE *f = rc_open("capture.tsv");
    if (!f) { perror("YI capture.tsv"); rc_status("capture.failed", "Cannot create capture.tsv\n"); if(!rc_timed) free(ring); return 1; }
    fprintf(f, "# yi-render-v3 request_mark=%u first=%u last=%u pre=%u post=%s preparation=%s bytes=%zu event_cap=%u batch_cap=1 resets=%u frame_cap=%u\n",
        rc_mark, first, last, rc_timed?0:8, rc_timed?"2seconds":"16frames", rc_timed?"5seconds":"none", sizeof(RcFrame)*rc_capacity, RC_EVENTS, rc_resets,rc_capacity);
    if(rc_timed) fprintf(f,"# timed completed=%u frame_cap_drops=%u capture_start_utc_ms=%llu capture_end_utc_ms=%llu start_counter=%llu end_counter=%llu host_counter_frequency=%llu end=%s incomplete_current_frame=excluded\n",rc_export.count,(unsigned)SDL_AtomicGet(&rc_frame_cap_drops),(unsigned long long)rc_capture_start_utc_ms,(unsigned long long)rc_requested_deadline_utc,(unsigned long long)rc_capture_start_counter,(unsigned long long)rc_deadline_counter,(unsigned long long)SDL_GetPerformanceFrequency(),rc_export.reset?"reset_partial":"monotonic_deadline");
    fputs("# yi-render-raster-probe-v1 cap=12 states=INIDISP,TM,TS,CGADSUB,CGWSEL stages=1:boundary_before,2:boundary_after,3:render_begin_before,4:render_begin_after,5:render_end_before,6:render_end_after,7:cpu_begin,8:cpu_end physical_slots=0,1 recording=current completed=current^1_when_have_completed\n", f);
    fputs("# counters=port-call; field=V225-journal-not-independent-ID; presentation=unavailable; beam during draw is parked; line=after-runLine-before-HDMA; clocks=master\n", f);
    fputs("# event: frame cpu beam V H drawLine phase(0cpu/1draw) source boundary reg value(256=redacted)\n", f);
    fputs("# yi-render-dma-hdma-v1 events=guest_writes_and_hdma_init_before_after channel_state=metadata_only pending_init=global_mask line=0_based_cpu_minus1\n", f);
    fputs("# dma_event: frame cpu beam V H drawLine phase kind address value channel active_mask pending_init aBank aAdr bAdr size mode indirect indBank tableAdr repCount doTransfer terminated dmaActive fixed decrement fromB offIndex\n", f);
    fputs("# hdma_sample: frame y channel hdmaActive terminated tableAdr repCount doTransfer indirect bAdr mode size aBank aAdr indBank dmaActive fixed decrement fromB unusedBit offIndex pending_init\n", f);
    fputs("# yi-render-ab-v1 hotkey=R row_renderer=0:legacy,1:new\n", f);
    fprintf(f,"# yi-render-input-v1 frequency=%llu clock=SDL_GetPerformanceCounter phase=before_guest same_frame_id_as_native r_cap=%u initial_R_state=unknown\n",(unsigned long long)SDL_GetPerformanceFrequency(),RC_INPUT_KEYS);
    fputs("# pad: frame slot counter previous_counter previous_valid mask pressed released A_down A_up; hex runner bits B=001 Y=002 Select=004 Start=008 Up=010 Down=020 Left=040 Right=080 A=100 X=200 L=400 SNES_R=800; edges=sampled_not_physical_time\n",f);
    fputs("# keyboard_r: next_guest_frame dequeue_counter pressed; repeats_preserved not_SNES_R; r_summary: frame count lost; actual_renderer_in_row\n",f);
    fputs("# row: frame y valid width pitch cpu beam V H hashBGR nonblack journal dropped cursor completed gsuClock instructions SFR IRQ regs64 renderer\n", f);
    fputs("# regs64 layout=local Ppu.inidisp..cgwsel; offsets follow\n", f);
    fprintf(f,"# yi-render-box-v1 enabled=%u requested_roi=%u,%u,%u,%u max_rows=224 per_row_bytes=%zu coords=native_zero_based first_last_empty=800 run_tie=leftmost hash=FNV1a_RGB color_hash=FNV1a_RGB3 bulk_ports=counts_by_source_phase_when_enabled heuristic_not_object_tracking\n",rc_box_enabled,rc_box_x,rc_box_y,rc_box_w,rc_box_h,sizeof(RcBoxRow));
    fprintf(f,"# yi-render-motion-v1 enabled=%u probe_cap=%u obj_phases=2 slots_per_phase=128 obj_bytes_per_frame=%zu signature=FNV1a_attr_and_size_no_coordinates hashes=FNV1a_raw_internal_bytes_no_payload boundary_serial=observer_stage2_notifications coords=SNES_signed_x_unsigned_y_before_widescreen size=square_renderer_model not_visibility_or_object_identity\n",rc_box_enabled,RC_PROBES,sizeof(ring[0].obj));
#define RC_OFFSET(member) fprintf(f, "# reg %s offset=%zu bytes=%zu\n", #member, offsetof(Ppu, member)-offsetof(Ppu,inidisp), sizeof(g_ppu->member))
    RC_OFFSET(inidisp); RC_OFFSET(obsel); RC_OFFSET(bgmode); RC_OFFSET(mosaic);
    RC_OFFSET(bgXsc); RC_OFFSET(bgTileAdr); RC_OFFSET(m7sel); RC_OFFSET(setini);
    RC_OFFSET(fixedColor); RC_OFFSET(hScroll); RC_OFFSET(vScroll);
    RC_OFFSET(m7matrix); RC_OFFSET(windowsel); RC_OFFSET(window1left); RC_OFFSET(window1right);
    RC_OFFSET(window2left); RC_OFFSET(window2right); RC_OFFSET(wbgobjlog);
    RC_OFFSET(screenEnabled); RC_OFFSET(screenWindowed); RC_OFFSET(cgadsub); RC_OFFSET(cgwsel);
#undef RC_OFFSET
    for (slot=0;slot<retained;slot++) {
        RcFrame *b; char name[64]; FILE *image;
        id=rc_timed?ring[slot].id:first+slot;
        b=rc_timed?&ring[slot]:&ring[(id-1)%RC_FRAMES];
        if (b->id != id) { fprintf(f,"missing\t%u\n",id); continue; }
        fprintf(f,"frame\t%u\tevent_lost=%u\tinvalid_rows=%u\tdma_lost=%u\tjobs_recent=%u\n",id,b->lost,b->invalid_rows,b->dma_lost,b->jobs);
        for(i=0;i<2;i++) {
            unsigned down=b->input_previous_valid?(b->pads[i]&~b->pads_previous[i]):0;
            unsigned up=b->input_previous_valid?(b->pads_previous[i]&~b->pads[i]):0;
            fprintf(f,"pad\t%u\t%u\t%llu\t%llu\t%u\t%03X\t%03X\t%03X\t%u\t%u\n",id,i,
                (unsigned long long)b->input_counter,(unsigned long long)b->input_previous_counter,
                b->input_previous_valid,b->pads[i],down,up,!!(down&0x100),!!(up&0x100));
        }
        fprintf(f,"r_summary\t%u\t%u\t%u\n",id,b->r_count,b->r_lost);
        for(i=0;i<b->r_count;i++) fprintf(f,"keyboard_r\t%u\t%llu\t%u\n",id,(unsigned long long)b->r_key[i].counter,b->r_key[i].pressed);
        fprintf(f,"dma_event_summary\t%u\tcount=%u\tlost=%u\n",id,b->dma_events,b->dma_event_lost);
        for(i=0;i<b->dma_events;i++) {
            RcDmaEvent *e=&b->dma_event[i];
            const char *kind=e->kind==kDmaTraceRegisterWrite?"register":
                e->kind==kDmaTraceHdmaInitBefore?"hdma_init_before":"hdma_init_after";
            DmaChannel *d=&e->state;
            fprintf(f,"dma_event\t%u\t%llu\t%llu\t%u\t%u\t%d\t%u\t%s\t%04X\t%02X\t%u\t%02X\t%02X",
                id,(unsigned long long)e->cpu,(unsigned long long)e->beam,e->v,e->h,
                (int)e->line-1,e->phase,kind,e->address,e->value,e->channel,
                e->active_mask,e->pending_init);
            if(e->channel<8)
                fprintf(f,"\t%02X\t%04X\t%02X\t%04X\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                    d->aBank,d->aAdr,d->bAdr,d->size,d->mode,d->indirect,
                    d->indBank,d->tableAdr,d->repCount,d->doTransfer,d->terminated,
                    d->dmaActive,d->fixed,d->decrement,d->fromB);
            else fputs("\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-",f);
            fprintf(f,"\t%u\n",e->channel<8?d->offIndex:0u);
        }
        if(rc_box_enabled) {
            static const unsigned ports[4]={0x2104,0x2118,0x2119,0x2122};
            unsigned port,source,phase;
            for(port=0;port<4;port++) for(source=0;source<5;source++) for(phase=0;phase<2;phase++)
                if(b->box_bulk[port][source][phase])
                    fprintf(f,"box_bulk_writes\t%u\treg=%04X\tsource=%u\tphase=%u\tcount=%u\n",id,ports[port],source,phase,b->box_bulk[port][source][phase]);
        }
        fprintf(f,"raster_probe_summary\t%u\tcount=%u\tlost=%u\n",id,b->probes,b->probe_lost);
        for(i=0;i<b->probes;i++) {
            RcProbe *p=&b->probe[i]; PpuRasterProbe *q=&p->state; unsigned k;
            fprintf(f,"raster_probe\t%u\tstage=%u\tcpu=%llu\tbeam=%llu\tV=%u\tH=%u\tcurrent=%u\tarmed=%u\thave_completed=%u\trendering=%u\tcursor=%u\tlive=",id,p->stage,(unsigned long long)p->cpu,(unsigned long long)p->beam,p->v,p->h,q->current,q->armed,q->have_completed,q->rendering,q->cursor);
            for(k=0;k<5;k++) fprintf(f,"%02X",q->live[k]);
            for(k=0;k<2;k++) fprintf(f,"\tslot%u=%02X%02X%02X%02X%02X,count=%u,dropped=%u",k,q->base[k][0],q->base[k][1],q->base[k][2],q->base[k][3],q->base[k][4],q->count[k],q->dropped[k]);
            fputc('\n',f);
            if(rc_box_enabled) {
                fprintf(f,"motion_probe\t%u\tstage=%u\tboundary_serial=%llu\toam_low_hash=%08X\toam_high_hash=%08X\tcgram_hash=%08X\toam_writes=%u",id,p->stage,(unsigned long long)p->boundary_serial,p->oam_low_hash,p->oam_high_hash,p->cgram_hash,p->oam_writes);
                for(k=0;k<4;k++) fprintf(f,"\tBG%u_h=%u\tBG%u_v=%u",k+1,(unsigned)p->hscroll[k],k+1,(unsigned)p->vscroll[k]);
                fputc('\n',f);
            }
        }
        if(rc_box_enabled) {
            unsigned phase,k;
            fprintf(f,"obj_summary\t%u\tlost=%u\n",id,b->obj_lost);
            for(phase=0;phase<2;phase++) {
                RcObjPhase *q=&b->obj[phase];
                fprintf(f,"obj_phase\t%u\tphase=%u\tvalid=%u\tstage=%u\tboundary_serial=%llu\tslots=%u\n",id,phase,q->valid,q->stage,(unsigned long long)q->boundary_serial,q->valid?128:0);
                if(!q->valid) continue;
                for(k=0;k<128;k++) {
                    RcObjSlot *s=&q->slot[k];
                    fprintf(f,"obj_slot\t%u\tphase=%u\tslot=%u\tx=%d\ty=%u\tsize=%u\tsignature=%08X\n",id,phase,k,(int)s->x,(unsigned)s->y,(unsigned)s->size,s->signature);
                }
            }
        }
        fprintf(f,"host_interval\t%u\t%llu\t%llu\n",id,(unsigned long long)b->host_begin,(unsigned long long)b->host_end);
        for(y=0;y<2;y++) for(i=0;i<18;i++)
            fprintf(f,"memory_hash\t%u\t%s\tregion=%u\t%08X\n",id,y?"draw_end":"cpu_begin",i,b->memory_hash[y][i]);
        for (i=0;i<b->events;i++) { RcEvent *e=&b->event[i];
            fprintf(f,"event\t%u\t%llu\t%llu\t%u\t%u\t%d\t%u\t%u\t%u\t%04X\t%u\n",id,
                (unsigned long long)e->cpu,(unsigned long long)e->beam,e->v,e->h,(int)e->line-1,e->phase,e->source,e->boundary,e->reg,e->value); }
        for (y=0;y<224;y++) { RcLine *r=&b->line[y];
            fprintf(f,"row\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%u\t%u\t%08X\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%04X\t%u\t",id,y,r->valid,r->width,r->pitch,
                (unsigned long long)r->cpu,(unsigned long long)r->beam,r->v,r->h,r->hash,r->nonblack,r->journal,r->dropped,r->cursor,r->completed,
                (unsigned long long)r->gsu_clock,(unsigned long long)r->instructions,r->sfr,r->irq);
            for(i=0;i<sizeof(r->regs);i++) fprintf(f,"%02X",r->regs[i]);
            fprintf(f,"\t%u\n",r->renderer);
            if(rc_box_enabled && y>=rc_box_y && y-rc_box_y<rc_box_h) {
                RcBoxRow *q=&r->box;
                const uint8_t *s=r->regs;
#define RC_REG_OFF(member) (offsetof(Ppu,member)-offsetof(Ppu,inidisp))
                unsigned hs=RC_REG_OFF(hScroll), vs=RC_REG_OFF(vScroll), wi=RC_REG_OFF(window1left);
                fprintf(f,"box_row\t%u\ty=%u\tvalid=%u\tx=%u\twidth=%u\thashRGB=%08X\tnonblack=%u\tfirst=%u\tlast=%u\tsum_x=%u\tflat_x=%u\tflat_len=%u\tflat_color_hash=%08X\tblack_x=%u\tblack_len=%u",id,y,q->valid,q->x,q->width,q->hash,q->nonblack,q->first,q->last,q->sum_x,q->flat_x,q->flat_len,q->flat_color_hash,q->black_x,q->black_len);
                for(i=0;i<4;i++) {
                    uint16_t h,v;
                    memcpy(&h,s+hs+i*2,sizeof(h));memcpy(&v,s+vs+i*2,sizeof(v));
                    fprintf(f,"\tBG%u_h=%u\tBG%u_v=%u",i+1,(unsigned)h,i+1,(unsigned)v);
                }
                fprintf(f,"\tW1=%u,%u\tW2=%u,%u\tINIDISP=%02X\tTM=%02X\tTS=%02X\n",s[wi],s[wi+1],s[wi+2],s[wi+3],s[0],s[RC_REG_OFF(screenEnabled)],s[RC_REG_OFF(screenEnabled)+1]);
#undef RC_REG_OFF
            }
            for(i=0;i<8;i++) { DmaChannel *d=&r->channels[i];
                fprintf(f,"hdma_sample\t%u\t%u\t%u\t%u\t%u\t%04X\t%u\t%u\t%u\t%02X\t%u\t%04X\t%02X\t%04X\t%02X\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",id,y,i,(unsigned)d->hdmaActive,(unsigned)d->terminated,d->tableAdr,d->repCount,(unsigned)d->doTransfer,(unsigned)d->indirect,d->bAdr,d->mode,d->size,d->aBank,d->aAdr,d->indBank,(unsigned)d->dmaActive,(unsigned)d->fixed,(unsigned)d->decrement,(unsigned)d->fromB,(unsigned)d->unusedBit,d->offIndex,b->line[y].hdma_pending_init); }
        }
        for(i=0;i<b->dma_count;i++) { PpuDmaInfo *d=&b->dma[i];
            fprintf(f,"dma\t%u\tframework_frame=%d\tline=%d\tphase=%u\tch=%u\tfromB=%u\tsrc=%02X:%04X\tb=%02X\tdest=%04X\tbytes=%u\n",id,d->frame,d->line,d->phase,d->channel,d->fromB,d->aBank,d->aAdr,d->bAdr,d->dest,d->size); }
        for(i=0;i<b->jobs;i++) fprintf(f,"job_recent\t%u\t%llu\t%llu\t%06X\n",id,(unsigned long long)b->job[i].start_master,(unsigned long long)b->job[i].stop_master,b->job[i].pc24);
        snprintf(name,sizeof(name),"native-%08u.ppm",id); image=rc_open(name);
        if (!image) { failures++; continue; }
        /* Only export complete, constant-width native images. */
        for(y=0;y<224;y++) if(!b->line[y].valid || b->line[y].width!=b->line[0].width) break;
        if(y==224) { fprintf(image,"P6\n%u 224\n255\n",b->line[0].width);
            for(y=0;y<224;y++) if(fwrite(b->rgb+y*RC_WIDTH*3,3,b->line[0].width,image)!=b->line[0].width) failures++; }
        else { fprintf(f,"image_invalid\t%u\n",id); failures++; }
        if(fclose(image)) failures++;
    }
    fprintf(f,"summary\tio_failures=%u\tpre_missing=%u\tgsu_evictions=unknown\tpresentation=unavailable\n",failures,rc_timed?0:(rc_mark-rc_armed_frame>=8?0:8-(rc_mark-rc_armed_frame)));
    { int failed=ferror(f); if(fclose(f)) failed=1;
      fprintf(stderr,"YI capture: batch exported; image failures=%u telemetry_error=%d\n",failures,failed);
      rc_status(failed || failures || (rc_timed && !rc_export.count) ? "capture.failed" : "capture.done",
          failed || failures || (rc_timed && !rc_export.count) ? "Export has errors or no complete frames; preserve all files and inspect capture.tsv\n" : "Export complete; inspect drops and missing frames in capture.tsv\n"); }
    /* Timed storage remains until process exit: an interrupted producer may
     * still own the excluded current slot. Reused only after writer_done. */
    if(!rc_timed) free(ring);
    return 0;
}
static int SDLCALL rc_timed_writer(void *unused) {
    unsigned count;
    while(!SDL_AtomicGet(&rc_timed_stop) && SDL_GetPerformanceCounter()<rc_deadline_counter) SDL_Delay(5);
    SDL_AtomicSet(&rc_timed_stop,1);
    /* Acquire only fully published slots. The current producer slot is never
     * read, even if guest execution is paused indefinitely at the deadline. */
    count=(unsigned)SDL_AtomicGet(&rc_timed_completed);
    rc_export.reset=(unsigned)SDL_AtomicGet(&rc_timed_reset);
    rc_export.ring=rc_ring; rc_export.count=count;
    rc_export.last=count?rc_ring[count-1].id:rc_mark-1;
    { int result=rc_flush(unused);
      SDL_AtomicSet(&rc_timed_writer_done,1);
      return result; }
}
static void rc_end(void) {
    uint64_t count, delta, back;
    if (!rc_current) return;
    rc_memory(1);
    count=ppudma_dma_count(); delta=count>=rc_current->dma_begin?count-rc_current->dma_begin:0;
    rc_current->dma_count=(unsigned)(delta>512?512:delta);
    rc_current->dma_lost=(unsigned)(delta-rc_current->dma_count);
    for(back=0;back<rc_current->dma_count;back++)
        if(!ppudma_dma_at(back,&rc_current->dma[rc_current->dma_count-1-(unsigned)back])) rc_current->dma_lost++;
    rc_current->jobs=(unsigned)superfx_job_log(rc_current->job,64);
    rc_current->host_end=SDL_GetPerformanceCounter();
    if(rc_timed) {
        if(!SDL_AtomicGet(&rc_timed_stop) && SDL_GetPerformanceCounter()<rc_deadline_counter) {
            rc_timed_used++;
            SDL_AtomicSet(&rc_timed_completed,(int)rc_timed_used);
        }
        rc_current=NULL;
        return;
    }
    if(rc_mark && trace_frame-rc_mark>=16) {
        SDL_Thread *writer;
        rc_export.ring=rc_ring; rc_export.last=trace_frame;
        rc_done=1; rc_ring=NULL; rc_current=NULL;
        writer=SDL_CreateThread(rc_flush,"yi-capture-writer",NULL);
        if(writer) SDL_DetachThread(writer);
        else { fputs("YI capture: writer thread failed; batch lost\n",stderr);
            rc_status("capture.failed", "Writer thread failed\n"); free(rc_export.ring); }
    }
    rc_current=NULL;
}

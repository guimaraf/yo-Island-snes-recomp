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
#define RC_EVENTS 8192u
#define RC_WIDTH 800u
typedef struct {
    uint64_t cpu, beam;
    unsigned v, h, line, phase, source, reg, value, boundary;
} RcEvent;
typedef struct {
    unsigned valid, pitch, width, hash, nonblack, journal, dropped, cursor, completed;
    uint64_t cpu, beam, gsu_clock, instructions;
    unsigned v, h, sfr, irq;
    uint8_t regs[PPU_SAVESTATE_REGS_SIZE];
    DmaChannel channels[8]; /* exported as metadata only */
} RcLine;
#define RC_PROBES 12u
typedef struct {
    unsigned stage, v, h;
    uint64_t cpu, beam;
    PpuRasterProbe state;
} RcProbe;
typedef struct {
    unsigned probes, probe_lost;
    RcProbe probe[RC_PROBES];
    unsigned id, events, lost, invalid_rows, dma_lost, dma_count, jobs;
    RcEvent event[RC_EVENTS];
    RcLine line[224];
    uint8_t rgb[224 * RC_WIDTH * 3];
    PpuDmaInfo dma[512];
    SuperFxJob job[64];
    uint64_t dma_begin;
    uint32_t memory_hash[2][18];
    uint64_t host_begin, host_end;
} RcFrame;
static RcFrame *rc_ring, *rc_current;
static unsigned rc_initialized, rc_mark, rc_schedule, rc_done, rc_resets, rc_armed_frame;
static const char *rc_dir;
static char rc_attached_dir[1024];
static unsigned rc_timed, rc_capacity = RC_FRAMES, rc_timed_used;
static SDL_atomic_t rc_timed_stop, rc_timed_completed, rc_timed_reset;
static SDL_atomic_t rc_timed_writer_done;
static uint64_t rc_deadline_counter;
static uint64_t rc_requested_deadline_utc;
static SDL_atomic_t rc_frame_cap_drops;
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
      if (fgets(mode, sizeof(mode), request) && !strncmp(mode, "timed5\n", 7)) rc_timed=1; }
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

static void rc_probe(unsigned stage) {
    RcProbe *p;
    if (!rc_current) return;
    if (rc_current->probes == RC_PROBES) { rc_current->probe_lost++; return; }
    p=&rc_current->probe[rc_current->probes++];
    p->stage=stage; p->cpu=g_cpu.master_cycles; p->beam=g_snes->beamMasterLast;
    p->v=g_snes->vPos; p->h=g_snes->hPos;
    ppu_rasterProbe(g_ppu, &p->state);
}
static void rc_event(int boundary, uint16_t reg, uint8_t value) {
    RcEvent *e;
    if (!rc_current) return;
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
        rc_requested_deadline_utc=0; rc_deadline_counter=0;
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
            rc_capacity=rc_timed?RC_TIMED_FRAMES:RC_FRAMES;
            if (sizeof(RcFrame) * rc_capacity > (rc_timed?512u:64u) * 1024u * 1024u) {
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
            rc_status("capture.armed", rc_timed ? "yi-render-timed-v1 seconds=5 frame_cap=360 pre=0\n" : "yi-render-attach-v1 pre=8 post=16 batch_cap=1\n");
            fprintf(stderr, "YI capture: armed, %zu bytes, one batch\n", sizeof(RcFrame)*rc_capacity);
        }
    }
    if (!rc_ring || rc_done) return;
    if (!rc_mark) {
        char path[1024]; FILE *signal;
        int n = snprintf(path, sizeof(path), "%s/trigger", rc_dir);
        signal = n > 0 && (size_t)n < sizeof(path) ? rc_fopen(path, "rb") : NULL;
        if (signal || (rc_schedule && trace_frame >= rc_schedule)) {
            rc_mark = trace_frame;
            if (rc_timed) {
                unsigned long long requested_deadline=0;
                uint64_t now=rc_utc_ms(), remaining;
                SDL_Thread *timer;
                if (!signal || fscanf(signal,"%llu",&requested_deadline)!=1) {
                    if(signal) fclose(signal);
                    rc_status("capture.failed","Invalid timed trigger\n"); rc_done=1;
                    SDL_AtomicSet(&rc_timed_writer_done,1); return;
                }
                remaining=requested_deadline>now?requested_deadline-now:0;
                rc_requested_deadline_utc=requested_deadline;
                if(remaining>5000) remaining=5000;
                rc_deadline_counter=SDL_GetPerformanceCounter()+remaining*SDL_GetPerformanceFrequency()/1000u;
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
static void rc_row(const Ppu *p, unsigned line) {
    unsigned x, c, width = 256u + 2u*p->extraLeftRight;
    RcLine *r;
    SuperFx *fx;
    if (!rc_current || !line || line > 224) return;
    r = &rc_current->line[line-1];
    r->cpu = g_cpu.master_cycles; r->beam = g_snes->beamMasterLast;
    r->v = g_snes->vPos; r->h = g_snes->hPos;
    r->pitch = p->renderPitch; r->width = width;
    memcpy(r->regs, &p->inidisp, sizeof(r->regs));
    memcpy(r->channels, g_snes->dma->channel, sizeof(r->channels));
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
    unsigned first = rc_timed ? rc_mark : (rc_mark > 8 ? rc_mark-8 : 1), id, y, i, failures = 0, slot;
    unsigned retained=rc_timed?rc_export.count:(last>=first?last-first+1:0);
    FILE *f = rc_open("capture.tsv");
    if (!f) { perror("YI capture.tsv"); rc_status("capture.failed", "Cannot create capture.tsv\n"); if(!rc_timed) free(ring); return 1; }
    fprintf(f, "# yi-render-v2 mark=%u first=%u last=%u pre=%u post=%s bytes=%zu event_cap=%u batch_cap=1 resets=%u frame_cap=%u\n",
        rc_mark, first, last, rc_timed?0:8, rc_timed?"5seconds":"16frames", sizeof(RcFrame)*rc_capacity, RC_EVENTS, rc_resets,rc_capacity);
    if(rc_timed) fprintf(f,"# timed completed=%u frame_cap_drops=%u deadline_utc_ms=%llu host_counter_frequency=%llu end=%s incomplete_current_frame=excluded\n",rc_export.count,(unsigned)SDL_AtomicGet(&rc_frame_cap_drops),(unsigned long long)rc_requested_deadline_utc,(unsigned long long)SDL_GetPerformanceFrequency(),rc_export.reset?"reset_partial":"monotonic_deadline");
    fputs("# yi-render-raster-probe-v1 cap=12 states=INIDISP,TM,TS,CGADSUB,CGWSEL stages=1:boundary_before,2:boundary_after,3:render_begin_before,4:render_begin_after,5:render_end_before,6:render_end_after,7:cpu_begin,8:cpu_end physical_slots=0,1 recording=current completed=current^1_when_have_completed\n", f);
    fputs("# counters=port-call; field=V225-journal-not-independent-ID; presentation=unavailable; beam during draw is parked; line=after-runLine-before-HDMA; clocks=master\n", f);
    fputs("# event: frame cpu beam V H drawLine phase(0cpu/1draw) source boundary reg value(256=redacted)\n", f);
    fputs("# row: frame y valid width pitch cpu beam V H hashBGR nonblack journal dropped cursor completed gsuClock instructions SFR IRQ regs64\n", f);
    fputs("# regs64 layout=local Ppu.inidisp..cgwsel; offsets follow\n", f);
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
        fprintf(f,"raster_probe_summary\t%u\tcount=%u\tlost=%u\n",id,b->probes,b->probe_lost);
        for(i=0;i<b->probes;i++) {
            RcProbe *p=&b->probe[i]; PpuRasterProbe *q=&p->state; unsigned k;
            fprintf(f,"raster_probe\t%u\tstage=%u\tcpu=%llu\tbeam=%llu\tV=%u\tH=%u\tcurrent=%u\tarmed=%u\thave_completed=%u\trendering=%u\tcursor=%u\tlive=",id,p->stage,(unsigned long long)p->cpu,(unsigned long long)p->beam,p->v,p->h,q->current,q->armed,q->have_completed,q->rendering,q->cursor);
            for(k=0;k<5;k++) fprintf(f,"%02X",q->live[k]);
            for(k=0;k<2;k++) fprintf(f,"\tslot%u=%02X%02X%02X%02X%02X,count=%u,dropped=%u",k,q->base[k][0],q->base[k][1],q->base[k][2],q->base[k][3],q->base[k][4],q->count[k],q->dropped[k]);
            fputc('\n',f);
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
            for(i=0;i<sizeof(r->regs);i++) fprintf(f,"%02X",r->regs[i]); fputc('\n',f);
            for(i=0;i<8;i++) { DmaChannel *d=&r->channels[i];
                fprintf(f,"hdma_sample\t%u\t%u\t%u\t%u\t%u\t%04X\t%u\t%u\t%u\t%02X\t%u\t%04X\n",id,y,i,d->hdmaActive,d->terminated,d->tableAdr,d->repCount,d->doTransfer,d->indirect,d->bAdr,d->mode,d->size); }
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

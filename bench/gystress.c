/* gystress - GUICy's stress and speed harness. Offscreen (gy_offscreen), no window, one thread.

     gystress              run every scene, print markdown tables
     gystress NAME...      run only the named scenes (startup typical text buttons anim tiling resize kernels memory)

   Times with QueryPerformanceCounter. Every timed frame is split in two: the UI pass (gy_begin and
   the widget calls) and gy_end (bookkeeping, the tile hash pass, clears and drawing). Each row gives
   the median and the 95th percentile of its frames, the dirty tiles and the instance count from
   gy_get_stats(). It measures only; it changes nothing in guicy.h. It reads a few of the context's
   fields (arena sizes) because it is the implementation file, for the memory row. */
#define GUICY_IMPLEMENTATION
#include "../guicy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psapi.h>

/* --- timing ------------------------------------------------------------------------------------ */
static double g_tick_ms;
static double g_t0;                             /* main's start */
static double now_ms(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return (double)t.QuadPart * g_tick_ms; }

#define MAXS 2048
static double s_tot[MAXS], s_ui[MAXS], s_end[MAXS];
static int s_dirty[MAXS], s_inst[MAXS], s_n;

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static int cmpi(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return x < y ? -1 : x > y; }
static double pct(double *v, int n, double p) {
    static double tmp[MAXS];
    int k;
    if (n <= 0) return 0;
    memcpy(tmp, v, sizeof(double) * (size_t)n);
    qsort(tmp, (size_t)n, sizeof(double), cmpd);
    k = (int)(p * (double)(n - 1) + 0.5);
    return tmp[k];
}
static int pcti(int *v, int n) {
    static int tmp[MAXS];
    if (n <= 0) return 0;
    memcpy(tmp, v, sizeof(int) * (size_t)n);
    qsort(tmp, (size_t)n, sizeof(int), cmpi);
    return tmp[(n - 1) / 2];
}

typedef void (*body_fn)(void);
typedef void (*step_fn)(int i);
static double f_ui, f_end;
static unsigned g_clock = 1000;

/* one frame: input first (step), then the timed UI pass and the timed gy_end */
static void frame(body_fn body) {
    double a, b, c;
    gy_input_time(g_clock);
    a = now_ms();
    gy_begin();
    body();
    b = now_ms();
    gy_end();
    c = now_ms();
    f_ui = b - a; f_end = c - b;
}

/* budget: 0 none; which: 't' the whole frame, 'u' the UI pass */
static void row(const char *name, double budget, int which, const char *bud_txt) {
    double mt = pct(s_tot, s_n, 0.5), pt = pct(s_tot, s_n, 0.95), mu = pct(s_ui, s_n, 0.5), me = pct(s_end, s_n, 0.5);
    double v = which == 'u' ? mu : mt;
    printf("| %s | %d | %.3f | %.3f | %.3f | %.3f | %d | %d | %s%s |\n", name, s_n, mt, pt, mu, me, pcti(s_dirty, s_n), pcti(s_inst, s_n),
           budget > 0 ? bud_txt : "", budget > 0 ? (v <= budget ? " **pass**" : " **MISS**") : "");
    fflush(stdout);
}
static void head(const char *title) {
    printf("\n### %s\n\n| case | frames | median ms | p95 ms | UI pass ms (median) | gy_end ms (median) | dirty tiles | instances | budget |\n|---|---|---|---|---|---|---|---|---|\n", title);
}

/* run warm untimed frames, then n timed ones; step(i) sets the input before frame i */
static void run(body_fn body, step_fn step, int warm, int n) {
    int i;
    s_n = 0;
    for (i = 0; i < warm + n; i++) {
        if (step) step(i);
        frame(body);
        if (i >= warm && s_n < MAXS) {
            gy_stats st = gy_get_stats();
            s_tot[s_n] = f_ui + f_end; s_ui[s_n] = f_ui; s_end[s_n] = f_end;
            s_dirty[s_n] = st.dirty_tiles; s_inst[s_n] = st.instances; s_n++;
        }
    }
}

static size_t private_bytes(void) {
    PROCESS_MEMORY_COUNTERS_EX m;
    memset(&m, 0, sizeof m); m.cb = sizeof m;
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m, sizeof m)) return 0;
    return m.PrivateUsage;
}

/* =================================================================================================
   1. The typical app screen: the step-4 v1 widget scene, plus more widgets in the right pane
   ================================================================================================= */
static struct {
    float sl, num, split, sl2, prog;
    int sli, numi, combo, chip, ef, img, chk[4], sel;
    char name[64];
    gy_rect r_btn, r_slider, r_edit, r_empty;
} V;

static void v_reset(void) {
    memset(&V, 0, sizeof V);
    V.sl = 32; V.num = 1.5f; V.sli = 3; V.numi = 7; V.combo = 1; V.split = 300; V.sl2 = 0.5f; V.prog = 0.62f;
    strcpy(V.name, "base");
}

static void make_image(void) {
    static unsigned char px[8 * 8 * 4];
    int x, y;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        unsigned char *p = px + (y * 8 + x) * 4;
        int on = (x + y) & 1;
        p[0] = on ? 0 : 230; p[1] = on ? 150 : 190; p[2] = on ? 150 : 90; p[3] = 255;
    }
    V.img = gy_image_make(8, 8, px, GY_FMT_RGBA);
}

static void typ_body(void) {
    static const float half[2] = { 0.5f, -1 }, three[3] = { -1, -1, -1 }, four[4] = { -1, -1, -1, -1 };
    static const char *const chips[] = { "Small", "Medium", "Large" };
    int i;
    gy_split("main", GY_H, &V.split, 300);
        gy_section("Numbers");
        gy_slider("Size", &V.sl, 8, 512); V.r_slider = gy_last_rect();
        gy_slider_i("Detail", &V.sli, 1, 4);
        gy_number("Scale", &V.num, 0.25f, 0, 10);
        gy_number_i("How many", &V.numi, 1, 20);
        gy_combo("Style", &V.combo, "Badlands\0Highland\0Mine\0");
        V.ef |= gy_edit("Name", V.name, sizeof V.name); V.r_edit = gy_last_rect();
        gy_row(3, three);
        gy_chips(chips, 3, &V.chip);
        gy_row(0, NULL);
        gy_progress(V.prog);
        gy_button_key("Build##k", 'B');
        gy_text_wrap("Wrapped text: the row grows to fit the lines, and the next widget goes below it.");
        gy_button_accent("Primary");
    gy_pane();
        if (gy_tree("Tree")) { gy_text("child one"); gy_text("child two"); gy_tree_end(); }
        gy_scroll_begin("list", 150);
        for (i = 0; i < 30; i++) gy_text("row %d", i + 1);
        gy_scroll_end();
        gy_row(2, half);
        gy_image(V.img, 64, 64);
        gy_image(V.img, 32, 32);
        gy_row(3, three);
        gy_button("Open"); V.r_btn = gy_last_rect();
        gy_button("Save"); gy_button("Export");
        gy_button("Undo"); gy_button("Redo"); gy_button("Reset");
        gy_row(4, four);
        gy_check("Pixel look", &V.chk[0]); gy_check("Textures", &V.chk[1]); gy_check("Shadows", &V.chk[2]); gy_check("Grid", &V.chk[3]);
        gy_row(3, three);
        if (gy_select("Low", V.sel == 0)) V.sel = 0;
        if (gy_select("Mid", V.sel == 1)) V.sel = 1;
        if (gy_select("High", V.sel == 2)) V.sel = 2;
        gy_row(0, NULL);
        gy_text("Status: %d bases ready, %.1f MB", 42, 12.5);
        gy_text_col(gy_theme_get()->text_dim, "Last build took %d ms", 812);
        gy_slider("Opacity", &V.sl2, 0, 1);
        gy_progress(0.3f);
        {
            gy_rect a = gy_avail();
            gy_canvas_line((float)a.x, (float)a.y + 10, (float)a.x + 120, (float)a.y + 40, 2, gy_theme_get()->good);
            gy_canvas_dash((float)a.x, (float)a.y + 50, (float)a.x + 200, (float)a.y + 50, 2, 6, gy_theme_get()->warn);
            gy_canvas_circle(a.x + 160, a.y + 20, 14, gy_theme_get()->accent);
            V.r_empty = (gy_rect){ a.x + a.w - 40, a.y + a.h - 40, 10, 10 };   /* empty background, bottom right */
        }
    gy_split_end();
}


/* what a full redraw touches: pixel area per instance kind, clipped to the frame */
static void overdraw(const char *what) {
    double area[8] = { 0 }, total = 0;
    int cnt[8] = { 0 }, i, x0, y0, x1, y1;
    static const char *const kn[8] = { "plain rect", "rounded or bordered box", "glyph", "line", "image", "", "", "" };
    for (i = 0; i < gy__c.ninst; i++) {
        const gy__inst *in = &gy__c.inst[i];
        int k = in->kind == GY__K_BOX ? ((in->radius > 0 || in->border > 0) ? 1 : 0) : in->kind == GY__K_GLYPH ? 2 : in->kind == GY__K_LINE ? 3 : 4;
        if (!gy__inst_box(in, gy__c.clips[in->clip], &x0, &y0, &x1, &y1)) { cnt[5]++; continue; }
        area[k] += (double)(x1 - x0) * (y1 - y0); cnt[k]++; total += (double)(x1 - x0) * (y1 - y0);
    }
    printf("\noverdraw, %s (%dx%d = %.2f Mpx): ", what, gy__c.w, gy__c.h, (double)gy__c.w * gy__c.h / 1e6);
    for (i = 0; i < 5; i++) printf("%s %d (%.2f Mpx); ", kn[i], cnt[i], area[i] / 1e6);
    printf("off screen or clipped away %d; total %.2f Mpx = %.2fx the frame\n", cnt[5], total / 1e6, total / ((double)gy__c.w * gy__c.h));
    fflush(stdout);
}

static int cx(gy_rect r) { return r.x + r.w / 2; }
static int cy(gy_rect r) { return r.y + r.h / 2; }
static void st_still(int i) { (void)i; gy_input_mouse(cx(V.r_empty), cy(V.r_empty)); }
static void st_hover(int i) { if (i & 1) gy_input_mouse(cx(V.r_btn), cy(V.r_btn)); else gy_input_mouse(cx(V.r_empty), cy(V.r_empty)); }
static void st_type(int i) {
    gy_input_key(GY_KEY_BACK, 0);
    if (i & 1) gy_input_key(GY_KEY_BACK, 1); else gy_input_char('a');
}
static void st_drag(int i) { gy_input_mouse(V.r_slider.x + 30 + (i % 60) * 3, cy(V.r_slider)); }
static void st_anim(int i) { V.prog = (float)(i % 100) / 100.0f; st_still(i); }

static int open_frame(int w, int h) {
    gy_shutdown();
    if (!gy_offscreen(w, h)) { printf("gy_offscreen(%d, %d) failed: %s\n", w, h, gy_last_error()); return 0; }
    gy_isa_set(GY_ISA_AVX2);
    return 1;
}

static void scene_typical(void) {
    static const int sizes[3][2] = { { 1540, 950 }, { 1920, 1080 }, { 2560, 1440 } };
    int k;
    for (k = 0; k < 3; k++) {
        int w = sizes[k][0], h = sizes[k][1];
        char title[96], nm[96];

        snprintf(title, sizeof title, "1. Typical app screen, %dx%d (kernels %s)", w, h, "avx2");
        head(title);
        v_reset();
        if (!open_frame(w, h)) return;
        make_image();
        gy_input_mouse(-100, -100);
        /* the very first frame: glyph misses, full redraw */
        s_n = 0; frame(typ_body);
        { gy_stats st = gy_get_stats(); s_tot[0] = f_ui + f_end; s_ui[0] = f_ui; s_end[0] = f_end; s_dirty[0] = st.dirty_tiles; s_inst[0] = st.instances; s_n = 1;
          snprintf(nm, sizeof nm, "first frame (cold: %d glyph misses)", st.glyph_misses); }
        row(nm, 0, 't', "");
        frame(typ_body);                        /* settles the layout (rects for the input) */
        if (w == 1920) overdraw("typical scene");
        run(typ_body, st_still, 5, 300);
        row("unchanged frame", 0.2, 'u', "UI pass <= 0.2");
        run(typ_body, st_hover, 4, 300);
        row("hover over one button (on/off each frame)", 0.3, 't', "<= 0.3 ms");
        /* full redraw of the same, warm: tiles off */
        gy_tiles(0);
        run(typ_body, st_still, 3, 200);
        snprintf(nm, sizeof nm, "full redraw, warm (gy_tiles(0)), %dx%d", w, h);
        row(nm, (w == 1920) ? 4.0 : 0, 't', "<= 4 ms at 1080p");
        gy_tiles(1);
        frame(typ_body);
        /* typing: click the edit box, then one char / one backspace a frame */
        gy_input_mouse(cx(V.r_edit), cy(V.r_edit)); frame(typ_body);
        gy_input_button(0, 1); frame(typ_body); gy_input_button(0, 0); frame(typ_body);
        run(typ_body, st_type, 4, 300);

        row("typing one char (char / backspace alternately)", 0.3, 't', "<= 0.3 ms (as a caret frame)");
        if (strncmp(V.name, "base", 4)) printf("(note: typing check: the edit box holds \"%s\")\n", V.name);
        gy_input_key(GY_KEY_BACK, 0);
        gy_input_mouse(cx(V.r_empty), cy(V.r_empty)); gy_input_button(0, 1); frame(typ_body); gy_input_button(0, 0); frame(typ_body);
        /* slider drag */
        { float before = V.sl;
          gy_input_mouse(V.r_slider.x + 30, cy(V.r_slider)); frame(typ_body);
          gy_input_button(0, 1); frame(typ_body);
          run(typ_body, st_drag, 2, 300);
          row("dragging a slider (3 px a frame)", 0, 't', "");
          gy_input_button(0, 0); frame(typ_body);
          if (V.sl == before) printf("(note: the slider did not move)\n"); }
        /* the animated progress bar (scene 4 at this size) */
        if (w == 1920) {
            printf("\n### 4. Animated scene at 1920x1080: a progress bar moves every frame\n\n| case | frames | median ms | p95 ms | UI pass ms (median) | gy_end ms (median) | dirty tiles | instances | budget |\n|---|---|---|---|---|---|---|---|---|\n");
            run(typ_body, st_anim, 4, 300);
            row("tiles on", 0.3, 't', "<= 0.3 ms (a few tiles)");
            gy_tiles(0);
            run(typ_body, st_anim, 4, 300);
            row("tiles off (gy_tiles(0), full redraw)", 4.0, 't', "<= 4 ms");
            gy_tiles(1);
            V.prog = 0.62f;
        }
        if (w == 2560) {
            printf("\nmemory at 2560x1440 after scene 1: perm arena committed %.1f MB, frame arena committed %.1f MB, "
                   "atlas pages %d (1 MB each), process private %.1f MB\n",
                   (double)gy__c.perm.committed / 1048576.0, (double)gy__c.frame.committed / 1048576.0, gy__c.npages, (double)private_bytes() / 1048576.0);
        }
    }
}

/* =================================================================================================
   2. Text heavy: long scroll lists
   ================================================================================================= */
static int g_rows, g_cols, g_small_font;
static void text_body(void) {
    int i;
    float w[16];
    if (g_small_font) gy_font(1);
    gy_scroll_begin("big", 0);
    if (g_cols > 1) { for (i = 0; i < g_cols; i++) w[i] = -1; gy_row(g_cols, w); }
    if (g_small_font) gy_row_h(13);
    for (i = 0; i < g_rows; i++) gy_text("Line %d of the list, abc", i + 1);
    gy_scroll_end();
}
static void st_scroll(int i) { gy_input_mouse(400, 400); gy_input_wheel((i & 1) ? 1.0f : -1.0f); }
static void st_mid(int i) { (void)i; gy_input_mouse(400, 400); }

static void text_case(int w, int h, int cols, int small, int rows) {
    char nm[128];
    g_rows = rows; g_cols = cols; g_small_font = small;
    if (!open_frame(w, h)) return;
    if (small) { gy_theme t = *gy_theme_get(); t.gap = 1; gy_theme_set(&t); gy_font_add("Segoe UI", 11, 0); }
    gy_input_mouse(400, 400);
    frame(text_body); frame(text_body);
    gy_input_wheel(-(float)rows * 0.5f / (float)cols * (small ? 14.0f : 34.0f) / 84.0f); frame(text_body);   /* to about the middle */
    run(text_body, st_mid, 2, rows >= 100000 ? 15 : 60);
    snprintf(nm, sizeof nm, "%d lines (%d cols), unchanged", rows, cols);
    row(nm, 0, 't', "");
    run(text_body, st_scroll, 2, rows >= 100000 ? 15 : 60);
    snprintf(nm, sizeof nm, "%d lines (%d cols), wheel one notch each frame", rows, cols);
    row(nm, 0, 't', "");
    gy_tiles(0);
    run(text_body, st_mid, 2, rows >= 100000 ? 10 : 30);
    snprintf(nm, sizeof nm, "%d lines (%d cols), full redraw", rows, cols);
    row(nm, 0, 't', "");
    gy_tiles(1);
}

static void scene_text(void) {
    head("2a. Text list at 1920x1080, 1 column, default font (about 30 lines visible): does cost grow with total rows?");
    text_case(1920, 1080, 1, 0, 1000);
    text_case(1920, 1080, 1, 0, 10000);
    text_case(1920, 1080, 1, 0, 100000);
    head("2b. About 2,000 visible lines: 3840x2160, 13 columns, 11 px font, 13 px rows, gap 1");
    text_case(3840, 2160, 13, 1, 2000);
    text_case(3840, 2160, 13, 1, 10000);
    text_case(3840, 2160, 13, 1, 100000);
}

/* =================================================================================================
   3. Many buttons in a gy_grid (in a scroll region, so the frame stays 1920x1080)
   ================================================================================================= */
static int g_nbtn;
static gy_rect g_btn0;
static char g_lab[20000][12];                  /* made once, so the UI pass times GUICy, not snprintf */
static void btn_body(void) {
    int i;
    if (!g_lab[0][0]) for (i = 0; i < 20000; i++) snprintf(g_lab[i], sizeof g_lab[i], "B %d", i);
    gy_scroll_begin("grid", 0);
    gy_grid(100);
    for (i = 0; i < g_nbtn; i++) { gy_button(g_lab[i]); if (i == 3) g_btn0 = gy_last_rect(); }
    gy_scroll_end();
}
static void st_btn_hover(int i) { if (i & 1) gy_input_mouse(cx(g_btn0), cy(g_btn0)); else gy_input_mouse(1900, 1070); }
static void st_btn_still(int i) { (void)i; gy_input_mouse(1900, 1070); }

static void scene_buttons(void) {
    static const int ns[3] = { 1000, 5000, 20000 };
    int k;
    head("3. Many buttons, gy_grid(100) at 1920x1080 (16 columns, in a scroll region; about 560 on screen)");
    for (k = 0; k < 3; k++) {
        char nm[96];
        g_nbtn = ns[k];
        if (!open_frame(1920, 1080)) return;
        gy_input_mouse(1900, 1070);
        frame(btn_body); frame(btn_body);
        run(btn_body, st_btn_still, 3, 100);
        snprintf(nm, sizeof nm, "%d buttons, unchanged", ns[k]); row(nm, 0.2, 'u', "UI pass <= 0.2");
        run(btn_body, st_btn_hover, 4, 100);
        snprintf(nm, sizeof nm, "%d buttons, hover on/off", ns[k]); row(nm, 0.3, 't', "<= 0.3 ms");
        gy_tiles(0);
        run(btn_body, st_btn_still, 2, 50);
        snprintf(nm, sizeof nm, "%d buttons, full redraw", ns[k]); row(nm, 4.0, 't', "<= 4 ms");
        gy_tiles(1);
    }
    /* all on screen: 1,000 buttons that fit (16 x 63 cells of 16 px rows) */
    head("3b. 1,000 buttons all on screen (gy_grid(100), rows 11 px, gap 1, at 1920x1080)");
    {
        gy_theme t;
        if (!open_frame(1920, 1080)) return;
        t = *gy_theme_get(); t.gap = 1; gy_theme_set(&t);
        g_nbtn = 1000;
        gy_input_mouse(1900, 1070);
        frame(btn_body); frame(btn_body);
        run(btn_body, st_btn_still, 3, 100); row("1,000 visible, unchanged", 0.2, 'u', "UI pass <= 0.2");
        run(btn_body, st_btn_hover, 4, 100); row("1,000 visible, hover on/off", 0.3, 't', "<= 0.3 ms");
        gy_tiles(0);
        run(btn_body, st_btn_still, 2, 50); row("1,000 visible, full redraw", 4.0, 't', "<= 4 ms");
        gy_tiles(1);
    }
}

/* =================================================================================================
   5. Tiling
   ================================================================================================= */
static int g_ntiles;
static float g_tsl[32];
static int g_tck[32];
static void tile_body(void) {
    int i;
    char nm[16];
    for (i = 0; i < g_ntiles; i++) {
        snprintf(nm, sizeof nm, "Tile%d", i);
        if (gy_tile(nm)) {
            gy_text("Tile %d", i);
            gy_button(nm);
            gy_check("On", &g_tck[i]);
            gy_slider("Amount", &g_tsl[i], 0, 1);
        }
    }
}
static void keyup_all(void) { gy_input_key(GY_KEY_ALT, 0); gy_input_key(GY_KEY_LEFT, 0); gy_input_key(GY_KEY_RIGHT, 0); gy_input_key(GY_KEY_PLUS, 0); gy_input_key(GY_KEY_MINUS, 0); gy_input_key('F', 0); }
static void st_tile_move(int i) { keyup_all(); gy_input_key(GY_KEY_ALT, 1); gy_input_key((i & 1) ? GY_KEY_LEFT : GY_KEY_RIGHT, 1); }
static void st_tile_grow(int i) { keyup_all(); gy_input_key(GY_KEY_ALT, 1); gy_input_key((i & 1) ? GY_KEY_MINUS : GY_KEY_PLUS, 1); }
static void st_tile_flip(int i) { (void)i; keyup_all(); gy_input_key(GY_KEY_ALT, 1); gy_input_key('F', 1); }
static void st_tile_still(int i) { (void)i; keyup_all(); gy_input_mouse(-100, -100); }

static void scene_tiling(void) {
    static const int ns[3] = { 4, 12, 32 };
    int k;
    head("5. Tiling at 1920x1080: n tiles, each with a text, a button, a check box and a slider");
    for (k = 0; k < 3; k++) {
        char nm[96], line[2048];
        int bars = 0, j;
        g_ntiles = ns[k];
        memset(g_tsl, 0, sizeof g_tsl); memset(g_tck, 0, sizeof g_tck);
        if (!open_frame(1920, 1080)) return;
        gy_input_mouse(-100, -100);
        frame(tile_body); frame(tile_body); frame(tile_body);
        gy_tiles_save(line, sizeof line);
        for (j = 0; line[j]; j++) if (line[j] == '|') bars++;
        run(tile_body, st_tile_still, 3, 200);
        snprintf(nm, sizeof nm, "%d tiles (%d stacked as tabs), unchanged", ns[k], bars); row(nm, 0.2, 'u', "UI pass <= 0.2");
        { int f0 = gy__c.tfocus, f1, f2;
          st_tile_move(0); frame(tile_body); f1 = gy__c.tfocus; st_tile_move(1); frame(tile_body); f2 = gy__c.tfocus;
          int leaves = 0, small = 0, mw = 99999, mh = 99999, n;
          for (n = 0; n < GY__TNODES; n++) if (gy__c.tn[n].used && gy__c.tn[n].leaf) {
              gy_rect lr = gy__c.tn[n].r;
              leaves++; if (lr.w < GY__TILE_MIN_W || lr.h < GY__TILE_MIN_H) small++;
              if (lr.w < mw) mw = lr.w;
              if (lr.h < mh) mh = lr.h; }
          printf("(%d tiles: %d leaves, %d under the 160x110 minimum, smallest w %d, smallest h %d; focus node %d -> Alt+Right %d -> Alt+Left %d; layout %.160s%s)\n",
                 ns[k], leaves, small, mw, mh, f0, f1, f2, line, strlen(line) > 160 ? "..." : ""); }
        run(tile_body, st_tile_move, 2, 100);
        snprintf(nm, sizeof nm, "%d tiles, focus move (Alt+Right / Alt+Left)", ns[k]); row(nm, 0.3, 't', "<= 0.3 ms");
        run(tile_body, st_tile_grow, 2, 100);
        snprintf(nm, sizeof nm, "%d tiles, layout change (Alt+Plus / Alt+Minus)", ns[k]); row(nm, 0, 't', "");
        run(tile_body, st_tile_flip, 2, 100);
        snprintf(nm, sizeof nm, "%d tiles, layout change (Alt+F flip)", ns[k]); row(nm, 0, 't', "");
        keyup_all();
    }
}

/* =================================================================================================
   6. Resize: 30 sizes in a row, each a full redraw
   ================================================================================================= */
static void scene_resize(void) {
    double alloc[30], fr[30], tot[30], pxs = 0, sum = 0;
    size_t p0, p1;
    int i;
    head("6. Resize: 30 sizes in a row, 800x500 up to 2560x1440 (typical scene; each frame a full redraw)");
    v_reset();
    if (!open_frame(1540, 950)) return;
    make_image();
    gy_input_mouse(-100, -100);
    frame(typ_body); frame(typ_body);
    p0 = private_bytes();
    s_n = 0;
    for (i = 0; i < 30; i++) {
        int w = 800 + (2560 - 800) * i / 29 + (i & 1) * 7, h = 500 + (1440 - 500) * i / 29 + (i % 3);
        double a = now_ms(), b;
        gy_offscreen(w, h);                    /* the offscreen way to change size: a new frame */
        b = now_ms();
        frame(typ_body);
        alloc[i] = b - a; fr[i] = f_ui + f_end; tot[i] = alloc[i] + fr[i];
        s_tot[s_n] = tot[i]; s_ui[s_n] = f_ui; s_end[s_n] = f_end; s_dirty[s_n] = gy_get_stats().dirty_tiles; s_inst[s_n] = gy_get_stats().instances; s_n++;
        pxs += (double)w * h; sum += fr[i];
    }
    p1 = private_bytes();
    row("resize frame (gy_offscreen + frame)", 0, 't', "");
    printf("\nresize: gy_offscreen median %.3f ms; frame alone median %.3f ms, max %.3f ms; %.2f ns per pixel; "
           "process private grew %.1f MB over the 30 sizes (perm arena %.1f MB committed)\n",
           pct(alloc, 30, 0.5), pct(fr, 30, 0.5), pct(fr, 30, 1.0), sum * 1e6 / pxs, (double)(p1 - p0) / 1048576.0,
           (double)gy__c.perm.committed / 1048576.0);
}

/* =================================================================================================
   7. Kernels at frame scale (full-redraw path, minus an empty frame)
   ================================================================================================= */
static int g_kimg;
static int g_kind;
static void kern_body(void) {
    int i;
    if (g_kind == 1) for (i = 0; i < 1000; i++)
        gy_canvas_box((gy_rect){ (i % 20) * 96, (i / 20) * 21, 92, 24 }, GY_RGB(56, 58, 64), GY_RGB(0, 150, 150), 4, 1);
    if (g_kind == 2) {
        static const char line[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN";
        for (i = 0; i < 100; i++) gy_canvas_text((i / 50) * 960, (i % 50) * 21, line, GY_RGB(224, 226, 230));
    }
    if (g_kind == 3) gy_canvas_image(g_kimg, (gy_rect){ 0, 0, 1024, 1024 }, 1);
    if (g_kind == 4) gy_canvas_image(g_kimg, (gy_rect){ 0, 0, 1024, 1024 }, 0);
    if (g_kind == 5) for (i = 0; i < 1000; i++)
        gy_canvas_rect((gy_rect){ (i % 20) * 96, (i / 20) * 21, 92, 24 }, GY_RGB(56, 58, 64), 0);
    if (g_kind == 6) for (i = 0; i < 200; i++)
        gy_canvas_line((float)(i * 9), 0, (float)(i * 9 + 300), 1000, 2, GY_RGB(90, 190, 110));
}

static void scene_kernels(void) {
    static unsigned char *img;
    static const char *const names[] = { "", "1,000 rounded boxes 92x24, r 4, border 1", "10,000 glyphs (100 lines of 100)",
                                         "512x512 image to 1024x1024, bilinear", "512x512 image to 1024x1024, nearest",
                                         "1,000 plain rects 92x24 (for comparison)", "200 lines 2 px wide, ~1,040 px long" };
    int isa, k, x, y;
    if (!img) img = (unsigned char *)malloc(512 * 512 * 4);
    for (y = 0; y < 512; y++) for (x = 0; x < 512; x++) {
        unsigned char *p = img + (y * 512 + x) * 4;
        p[0] = (unsigned char)x; p[1] = (unsigned char)y; p[2] = (unsigned char)(x ^ y); p[3] = 255;
    }
    printf("\n### 7. Kernels at frame scale, 1920x1080, full-redraw path (gy_tiles(0)); draw ms = gy_end minus an empty frame's gy_end\n\n"
           "| case | kernels | gy_clear ms or draw ms (median) | p95 | UI pass ms | instances | ns per pixel or per item |\n|---|---|---|---|---|---|---|\n");
    for (isa = GY_ISA_AVX2; isa >= GY_ISA_SCALAR; isa--) {
        double empty, c[200];
        const char *iname;
        if (!open_frame(1920, 1080)) return;
        gy_isa_set(isa); iname = gy_isa_name();
        g_kimg = gy_image_make(512, 512, img, GY_FMT_RGBA);
        gy_tiles(0);
        for (k = 0; k < 200; k++) { double a = now_ms(); gy_clear(); c[k] = now_ms() - a; }
        printf("| full 1920x1080 clear (gy_clear) | %s | %.3f | %.3f | - | - | %.3f ns/px |\n", iname, pct(c, 200, 0.5), pct(c, 200, 0.95), pct(c, 200, 0.5) * 1e6 / (1920.0 * 1080.0));
        g_kind = 0; run(kern_body, NULL, 3, 100); empty = pct(s_end, s_n, 0.5);
        printf("| empty frame, full redraw (clear + hash pass) | %s | %.3f | %.3f | %.3f | 0 | - |\n", iname, empty, pct(s_end, s_n, 0.95), pct(s_ui, s_n, 0.5));
        for (k = 1; k <= 6; k++) {
            double d, per, items;
            g_kind = k;
            run(kern_body, NULL, 3, k >= 3 && k <= 4 ? 60 : 100);
            d = pct(s_end, s_n, 0.5) - empty;
            items = k == 1 || k == 5 ? 1000 : k == 2 ? s_inst[0] : k == 6 ? 200 : 1024.0 * 1024.0;
            per = d * 1e6 / items;
            printf("| %s | %s | %.3f | %.3f | %.3f | %d | %.1f ns per %s |\n", names[k], iname, d, pct(s_end, s_n, 0.95) - empty, pct(s_ui, s_n, 0.5), s_inst[0], per,
                   k == 3 || k == 4 ? "pixel" : k == 2 ? "glyph" : k == 6 ? "line" : "box");
        }
        fflush(stdout);
    }
}

/* =================================================================================================
   8. First-frame costs (run first, while the process is cold)
   ================================================================================================= */
static void ext_body(void) {
    static char buf[4096];
    int n = 0, c, line = 0;
    for (c = 0x100; c < 0x500; c++) {           /* 1,024 new code points: Latin Extended, IPA, Greek, Cyrillic */
        n += gy__utf8_put(buf + n, (uint32_t)c);
        if ((c & 63) == 63) { buf[n] = 0; gy_canvas_text(10, 10 + line * 20, buf, GY_RGB(224, 226, 230)); n = 0; line++; }
    }
}
static void scene_startup(void) {
    double t_main = now_ms(), a, b, init, font2, off, first, second, ext;
    int m0, m1;
    FILETIME cr, ex, k, u, nowf;
    ULARGE_INTEGER c64, n64;
    GetProcessTimes(GetCurrentProcess(), &cr, &ex, &k, &u);
    GetSystemTimePreciseAsFileTime(&nowf);
    c64.LowPart = cr.dwLowDateTime; c64.HighPart = cr.dwHighDateTime; n64.LowPart = nowf.dwLowDateTime; n64.HighPart = nowf.dwHighDateTime;
    a = now_ms(); gy_theme_get(); b = now_ms(); init = b - a;              /* context: arenas, glyph table, DC, Segoe UI */
    a = now_ms(); gy_font_add("Consolas", 15, 0); b = now_ms(); font2 = b - a;
    a = now_ms(); gy_offscreen(1540, 950); b = now_ms(); off = b - a;
    v_reset(); make_image();
    gy_input_mouse(-100, -100);
    m0 = gy_get_stats().glyph_misses;
    frame(typ_body); first = f_ui + f_end;
    m1 = gy_get_stats().glyph_misses;
    { double fu = f_ui, fe = f_end;
      frame(typ_body); second = f_ui + f_end;
      printf("\n### 8. First-frame costs (cold process, 1540x950)\n\n| step | ms | note |\n|---|---|---|\n");
      printf("| process start to main | %.2f | from GetProcessTimes creation time (loader, CRT) |\n", (double)(n64.QuadPart - c64.QuadPart) / 10000.0 - (now_ms() - t_main));
      printf("| context init (first gy_theme_get: arenas, glyph table, DC, CreateFont Segoe UI 15) | %.3f | |\n", init);
      printf("| a second font (gy_font_add Consolas 15) | %.3f | |\n", font2);
      printf("| gy_offscreen(1540, 950) (frame memory + clear) | %.3f | |\n", off);
      printf("| first frame of the typical scene | %.3f | UI pass %.3f (glyph misses: %d), gy_end %.3f |\n", first, fu, m1 - m0, fe);
      printf("| second frame (warm glyphs, unchanged) | %.3f | |\n", second);
      printf("| **library startup to first frame** (init + offscreen + first frame) | **%.2f** | budget <= 30 ms: **%s** |\n", init + off + first, init + off + first <= 30 ? "pass" : "MISS"); }
    m0 = gy_get_stats().glyph_misses;
    a = now_ms(); gy_begin(); ext_body(); b = now_ms(); gy_end(); ext = now_ms() - a;
    m1 = gy_get_stats().glyph_misses;
    printf("| 1,024 new glyphs in one frame (atlas fill) | %.3f | %d misses, %.1f us per miss (UI pass %.3f); atlas pages %d |\n", ext, m1 - m0, (b - a) * 1000.0 / (m1 - m0 > 0 ? m1 - m0 : 1), b - a, gy_get_stats().atlas_pages);
    a = now_ms(); gy_begin(); ext_body(); gy_end(); b = now_ms();
    printf("| the same frame again (all hits) | %.3f | |\n", b - a);
    fflush(stdout);
}

static int want(int argc, char **argv, const char *n) {
    int a;
    if (argc < 2) return 1;
    for (a = 1; a < argc; a++) if (!strcmp(argv[a], n)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    LARGE_INTEGER f;
    char cpu[64] = "";
    unsigned r[12];
    QueryPerformanceFrequency(&f);
    g_tick_ms = 1000.0 / (double)f.QuadPart;
    g_t0 = now_ms();
    if (want(argc, argv, "startup")) scene_startup();
    __get_cpuid(0x80000002, &r[0], &r[1], &r[2], &r[3]); __get_cpuid(0x80000003, &r[4], &r[5], &r[6], &r[7]); __get_cpuid(0x80000004, &r[8], &r[9], &r[10], &r[11]);
    memcpy(cpu, r, 48); cpu[48] = 0;
    printf("\ngystress: GUICy %s, CPU %s, best kernels %s, QPC %lld Hz\n", GUICY_VERSION, cpu, gy_isa_name(), (long long)f.QuadPart);
    if (want(argc, argv, "typical")) scene_typical();
    if (want(argc, argv, "text")) scene_text();
    if (want(argc, argv, "buttons")) scene_buttons();
    if (want(argc, argv, "tiling")) scene_tiling();
    if (want(argc, argv, "resize")) scene_resize();
    if (want(argc, argv, "kernels")) scene_kernels();
    gy_shutdown();
    printf("\ngystress: done in %.1f s\n", (now_ms() - g_t0) / 1000.0);
    return 0;
}

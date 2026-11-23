/* gytest - GUICy's own test host. Runs hidden: everything is drawn off screen, no window, no focus.

     gytest              run every test; print PASS/FAIL per test; exit code = number of failures
     gytest --update     take the current pictures as the new goldens (only after looking at them)
     gytest --shots      also write every picture to work\out (they are written anyway when a test fails)
     gytest NAME...      run only the named tests

   Every test is drawn three times, with the scalar, SSE2 and AVX2 kernels, and the three frames
   must be equal byte for byte. The frame is written to work\out\NAME.bmp, and its hash must match
   the line for NAME in tests\golden\hashes.txt. tools\golden_png.py makes the PNGs beside it for
   people to look at. */
#define GUICY_IMPLEMENTATION
#include "../guicy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_isa;                              /* the kernels the current run uses */
static char g_log[16384];                       /* what a test reports (shown with its result) */

static int frame_open(int w, int h) {
    if (!gy_offscreen(w, h)) return 0;
    return gy_isa_set(g_isa) == g_isa;
}

/* --- the widget scene, drawn the same way by every widget test ------------------------------- */
static int s_check_a = 0, s_check_b = 1, s_sel = 1;

static void scene(void) {
    static const float two[2] = { 0.5f, -1 }, three[3] = { 110, 110, -1 };
    gy_begin();
    gy_text("GUICy: widgets, step %d", 2);
    gy_row(3, three);
    if (gy_button("Build")) strcat(g_log, "Build clicked; ");
    gy_button("Stop##1");
    gy_button("Open folder");
    gy_row(2, two);
    if (gy_check("Pixel look", &s_check_a)) { char b[64]; sprintf(b, "check -> %d; ", s_check_a); strcat(g_log, b); }
    gy_check("Textures", &s_check_b);
    gy_row(3, three);
    if (gy_select("Small", s_sel == 0)) s_sel = 0;
    if (gy_select("Medium", s_sel == 1)) s_sel = 1;
    if (gy_select("Large", s_sel == 2)) s_sel = 2;
    gy_row(0, NULL);
    gy_text_col(gy_theme_get()->heading, "Disabled:");
    gy_row(3, three);
    gy_disable_begin();
    gy_button("Stop##2");
    gy_check("Off", &s_check_a);
    gy_select("Chosen", 1);
    gy_disable_end();
    gy_row(0, NULL);
    gy_text_col(gy_theme_get()->text_dim, "Unicode: caf\xc3\xa9, na\xc3\xafve, \xe2\x82\xac 12.50, %.2f", 3.14159);
    gy_end();
}

static int scene_at(int mx, int my, int press) {
    s_check_a = 0; s_check_b = 1; s_sel = 1;
    if (!frame_open(420, 250)) return 0;
    gy_input_mouse(mx, my);
    scene();                                   /* first frame: hover settles */
    if (press) { gy_input_button(0, 1); scene(); }
    scene();
    return 1;
}

static int t_blank(void) { if (!frame_open(256, 160)) return 0; gy_begin(); gy_end(); return 1; }
static int t_widgets(void) { return scene_at(-100, -100, 0); }
static int t_button_hot(void) { return scene_at(60, 56, 0); }
static int t_button_down(void) { return scene_at(60, 56, 1); }
static int t_check_hot(void) { return scene_at(40, 90, 0); }
static int t_select_hot(void) { return scene_at(60, 124, 0); }
static int t_disabled_hot(void) { return scene_at(60, 192, 1); }   /* pressing a disabled button does nothing */

/* a scripted click: move, press, release over the check box; the log must say it turned on */
static int t_click_check(void) {
    s_check_a = 0; s_check_b = 1; s_sel = 1;
    if (!frame_open(420, 250)) return 0;
    gy_input_mouse(40, 90); scene();
    gy_input_button(0, 1); scene();
    gy_input_button(0, 0); scene();
    gy_input_mouse(-100, -100); scene();
    if (s_check_a != 1) { strcat(g_log, "the check did not turn on; "); return 0; }
    gy_input_mouse(60, 124); scene();          /* and a select click moves the choice to Small */
    gy_input_button(0, 1); scene();
    gy_input_button(0, 0); scene();
    if (s_sel != 0) { strcat(g_log, "the select did not move; "); return 0; }
    gy_input_mouse(60, 192); scene();          /* a disabled button ignores the click */
    gy_input_button(0, 1); scene();
    gy_input_button(0, 0); scene();
    gy_input_mouse(-100, -100); scene();
    return 1;
}

/* dirty tiles: the same frames with tiles on and off must be equal at every frame, an unchanged
   frame draws no tile, and a hover over one button draws at most 4 tiles */
static uint64_t frame_hash(void);
static int run_tiles(int on, uint64_t *hs, int *dirty) {
    static const int path[][3] = { { -100, -100, 0 }, { -100, -100, 0 }, { 60, 56, 0 }, { 60, 56, 1 }, { 60, 56, 0 },
                                   { 40, 90, 1 }, { 40, 90, 0 }, { 300, 124, 1 }, { 300, 124, 0 }, { -100, -100, 0 } };
    int i;
    s_check_a = 0; s_check_b = 1; s_sel = 1;
    if (!frame_open(420, 250)) return 0;
    gy_tiles(on);
    for (i = 0; i < 10; i++) {
        gy_input_mouse(path[i][0], path[i][1]);
        gy_input_button(0, path[i][2]);
        scene();
        hs[i] = frame_hash();
        dirty[i] = gy_get_stats().dirty_tiles;
    }
    return 1;
}
static int t_tiles(void) {
    uint64_t on[10], off[10];
    int don[10], doff[10], i;
    char b[160];
    if (!run_tiles(0, off, doff)) return 0;
    gy_shutdown();
    if (!run_tiles(1, on, don)) return 0;
    for (i = 0; i < 10; i++) if (on[i] != off[i]) { sprintf(b, "frame %d differs from a full redraw; ", i); strcat(g_log, b); return 0; }
    sprintf(b, "dirty tiles per frame (of %d): %d %d %d %d %d %d %d %d %d %d; ", gy_get_stats().tiles,
            don[0], don[1], don[2], don[3], don[4], don[5], don[6], don[7], don[8], don[9]);
    strcat(g_log, b);
    if (don[1] != 0) { strcat(g_log, "an unchanged frame drew tiles; "); return 0; }
    if (don[2] > 4) { strcat(g_log, "a hover drew more than 4 tiles; "); return 0; }
    return 1;
}

#include "v1_tests.inc"

typedef int (*test_fn)(void);
static const struct { const char *name; test_fn fn; } TESTS[] = {
    { "blank", t_blank },
    { "widgets", t_widgets },
    { "button_hot", t_button_hot },
    { "button_down", t_button_down },
    { "check_hot", t_check_hot },
    { "select_hot", t_select_hot },
    { "disabled_hot", t_disabled_hot },
    { "click_check", t_click_check },
    { "tiles", t_tiles },
    { "v1_widgets", t_v1_widgets },
    { "v1_combo_open", t_v1_combo_open },
    { "v1_combo_pick", t_v1_combo_pick },
    { "v1_slider", t_v1_slider },
    { "v1_edit", t_v1_edit },
    { "v1_caret", t_v1_caret },
    { "v1_tip", t_v1_tip },
    { "v1_scroll", t_v1_scroll },
    { "v1_split", t_v1_split },
    { "v1_keys", t_v1_keys },
    { "v1_popup_trap", t_v1_popup_trap },
    { "tile_layout", t_tile_layout },
    { "tile_keys", t_tile_keys },
    { "tile_zoom", t_tile_zoom },
    { "tile_stack", t_tile_stack },
    { "tile_restore", t_tile_restore },
    { "tile_online", t_tile_online },
    { "tile_clash", t_tile_clash },
    { "tile_small", t_tile_small },
    { "fix_offscreen_leak", t_fix_offscreen_leak },
    { "fix_tile_many", t_fix_tile_many },
    { "f1_cull", t_f1_cull },
    { "f1_corner_cache", t_f1_corner_cache },
};
#define NTESTS ((int)(sizeof TESTS / sizeof TESTS[0]))

static char g_root[MAX_PATH];                  /* the repo folder: the parent of the exe's build\ */

static uint64_t fnv64(const uint32_t *px, int n, int w, int h) {
    uint64_t x = 1469598103934665603ull;
    const unsigned char *b = (const unsigned char *)px;
    size_t i, len = (size_t)n * 4;
    x = (x ^ (uint64_t)w) * 1099511628211ull;
    x = (x ^ (uint64_t)h) * 1099511628211ull;
    for (i = 0; i < len; i++) x = (x ^ b[i]) * 1099511628211ull;
    return x;
}

static char g_names[128][32];
static uint64_t g_hash[128];
static int g_nh;

static void load_hashes(const char *path) {
    FILE *f = fopen(path, "r");
    char name[64];
    unsigned long long h;
    if (!f) return;
    while (g_nh < 128 && fscanf(f, "%63s %llx", name, &h) == 2) { strncpy(g_names[g_nh], name, 31); g_hash[g_nh++] = h; }
    fclose(f);
}

static int find_hash(const char *name) { int i; for (i = 0; i < g_nh; i++) if (!strcmp(g_names[i], name)) return i; return -1; }

static void save_hashes(const char *path) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) { printf("FAIL could not write %s\n", path); return; }
    for (i = 0; i < g_nh; i++) fprintf(f, "%s %016llx\n", g_names[i], (unsigned long long)g_hash[i]);
    fclose(f);
}

static uint64_t frame_hash(void) { return fnv64(gy_pixels(), gy_width() * gy_height(), gy_width(), gy_height()); }

int main(int argc, char **argv) {
    char out[MAX_PATH], hashes[MAX_PATH], *s;
    int update = 0, fails = 0, ran = 0, i, a, named = 0, shots = 0;
    GetModuleFileNameA(NULL, g_root, MAX_PATH);
    if ((s = strrchr(g_root, '\\'))) *s = 0;   /* ...\build */
    if ((s = strrchr(g_root, '\\'))) *s = 0;   /* ...\guicy */
    for (a = 1; a < argc; a++) { if (!strcmp(argv[a], "--update")) update = 1; else if (!strcmp(argv[a], "--shots")) shots = 1; else named = 1; }
    snprintf(out, sizeof out, "%s\\work", g_root); CreateDirectoryA(out, NULL);
    snprintf(out, sizeof out, "%s\\work\\out", g_root); CreateDirectoryA(out, NULL);
    snprintf(hashes, sizeof hashes, "%s\\tests\\golden\\hashes.txt", g_root);
    load_hashes(hashes);

    for (i = 0; i < NTESTS; i++) {
        uint64_t h[3] = { 0, 0, 0 };
        int want = !named, isa, ok = 1, k;
        for (a = 1; a < argc && !want; a++) if (!strcmp(argv[a], TESTS[i].name)) want = 1;
        if (!want) continue;
        ran++;
        g_log[0] = 0;
        for (isa = GY_ISA_AVX2; isa >= GY_ISA_SCALAR; isa--) {
            g_isa = isa;
            if (!TESTS[i].fn()) { ok = 0; break; }
            h[isa] = fnv64(gy_pixels(), gy_width() * gy_height(), gy_width(), gy_height());
            if (isa == GY_ISA_AVX2) {          /* the picture: kept when asked, or when something is wrong */
                int k2 = find_hash(TESTS[i].name);
                snprintf(out, sizeof out, "%s\\work\\out\\%s.bmp", g_root, TESTS[i].name);
                if ((update || shots || k2 < 0 || g_hash[k2] != fnv64(gy_pixels(), gy_width() * gy_height(), gy_width(), gy_height()))
                    && !gy_shot(out)) { ok = 0; break; }
            }
            gy_shutdown();
        }
        if (!ok) { printf("FAIL %s: %s%s\n", TESTS[i].name, g_log, gy_last_error()); fails++; gy_shutdown(); continue; }
        if (h[0] != h[1] || h[1] != h[2]) {
            printf("FAIL %s: the kernels disagree: scalar %016llx sse2 %016llx avx2 %016llx\n", TESTS[i].name,
                   (unsigned long long)h[0], (unsigned long long)h[1], (unsigned long long)h[2]);
            fails++; continue;
        }
        k = find_hash(TESTS[i].name);
        if (update) {
            if (k < 0) { k = g_nh++; strncpy(g_names[k], TESTS[i].name, 31); }
            g_hash[k] = h[2];
            printf("UPDATED %s %016llx %s\n", TESTS[i].name, (unsigned long long)h[2], g_log);
        } else if (k < 0) {
            printf("FAIL %s: no golden (look at work\\out\\%s.bmp, then run --update)\n", TESTS[i].name, TESTS[i].name); fails++;
        } else if (g_hash[k] != h[2]) {
            printf("FAIL %s: differs from the golden (%016llx, golden %016llx)\n", TESTS[i].name, (unsigned long long)h[2], (unsigned long long)g_hash[k]); fails++;
        } else printf("PASS %s (3 kernels equal) %s\n", TESTS[i].name, g_log);
    }
    if (update) save_hashes(hashes);
    printf("%d test(s), %d failed\n", ran, fails);
    return fails;
}

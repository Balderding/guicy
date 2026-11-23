/* h1_tests.inc - H1: an app's own window (gy_attach / gy_event), the desktop guard, the text-record
   hook and the synthetic test font. Included into gytest.c after v1_tests.inc.

   Tests named w_* need real windows. They run only under "gytest --windowed", which relaunches
   gytest on a desktop of its own (never the user's), the way tools/baseline.py does. */

/* --- the guard: a test build never opens a window on the user's desktop ------------------------ */
static int t_h1_guard(void) {
    HWND msgwin;
    int got;
    if (!open_at(64, 64)) return 0;
    gy_shutdown();
    got = gy_open(&(gy_desc){ .title = "must not open", .w = 320, .h = 200 });
    CHECK(!got, "gy_open opened a window on the user's desktop in a test build");
    CHECK(strstr(gy_last_error(), "test desktop") != NULL, "no plain error: %s", gy_last_error());
    gy_shutdown();
    /* a message-only window (never visible) for gy_attach to refuse */
    msgwin = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 10, 10, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    got = gy_attach(msgwin);
    if (msgwin) DestroyWindow(msgwin);
    CHECK(!got, "gy_attach took a window on the user's desktop in a test build");
    gy_shutdown();
    LOG("gy_open and gy_attach refuse on the Default desktop; ");
    return open_at(64, 64) && (gy_begin(), gy_end(), 1);
}

/* --- the text-record hook: every text with its colour and the colour under it --------------------- */
static double lum(gy_color c) {
    double ch[3];
    int i;
    for (i = 0; i < 3; i++) {
        double v = (double)((c >> (16 - 8 * i)) & 255) / 255.0;
        ch[i] = v <= 0.04045 ? v / 12.92 : __builtin_pow((v + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
}
static double contrast(gy_color a, gy_color b) { double x = lum(a), y = lum(b); return x > y ? (x + 0.05) / (y + 0.05) : (y + 0.05) / (x + 0.05); }

static int t_h1_texts(void) {
    const gy_test_text *t;
    int n, i, found_build = 0, found_name = 0;
    double worst = 99;
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    n = gy_test_texts(&t);
    for (i = 0; i < n; i++) {
        gy_color bg = gy_test_text_bg(i);
        double c = contrast(t[i].fg, bg);
        if (c < worst) worst = c;
        if (!strcmp(t[i].s, "Build##k") || !strncmp(t[i].s, "Build", 5)) {
            found_build = 1;
            CHECK(bg == gy_theme_get()->button, "the Build label's background is %08x, not the button colour", (unsigned)bg);
            CHECK(t[i].fg == gy_theme_get()->text, "the Build label's colour is %08x", (unsigned)t[i].fg);
        }
        if (!strncmp(t[i].s, "Name", 4)) { found_name = 1; CHECK(bg == gy_theme_get()->bg, "the Name label's background is %08x", (unsigned)bg); }
    }
    LOG("%d texts recorded, worst contrast %.2f:1 (texts on a disabled or dim colour included); ", n, worst);
    CHECK(n > 20, "only %d texts recorded", n);
    CHECK(found_build && found_name, "a label is missing from the record");
    return 1;
}

/* --- the synthetic test font: metrics and glyphs that are exact multiples of the size --------------
   px/2 advance, a px/4 x px/2 box, ascent 3/4 px, descent 1/4 px. Swapped in through gy__os, so
   every structural check can scale by exact factors (D1). */
static int tf_px(HDC dc) {
    LOGFONTW lf;
    HGDIOBJ f = GetCurrentObject(dc, OBJ_FONT);
    if (!f || !GetObjectW(f, sizeof lf, &lf)) return 16;
    return lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight ? lf.lfHeight : 16;
}
static BOOL WINAPI tf_metrics(HDC dc, LPTEXTMETRICW tm) {
    int px = tf_px(dc);
    memset(tm, 0, sizeof *tm);
    tm->tmAscent = px * 3 / 4; tm->tmDescent = px / 4; tm->tmHeight = px;
    return TRUE;
}
static DWORD WINAPI tf_glyph(HDC dc, UINT ch, UINT fmt, LPGLYPHMETRICS gm, DWORD n, LPVOID buf, const MAT2 *m) {
    int px = tf_px(dc), w = px / 4, h = px / 2, pitch = (w + 3) & ~3, y, x;
    (void)fmt; (void)m;
    memset(gm, 0, sizeof *gm);
    gm->gmCellIncX = (short)(px / 2);
    if (ch == ' ') return 0;
    gm->gmBlackBoxX = (UINT)w; gm->gmBlackBoxY = (UINT)h;
    gm->gmptGlyphOrigin.x = px / 8; gm->gmptGlyphOrigin.y = px / 2;
    if (!buf || n < (DWORD)(pitch * h)) return (DWORD)(pitch * h);
    for (y = 0; y < h; y++) for (x = 0; x < pitch; x++) ((unsigned char *)buf)[y * pitch + x] = x < w ? 64 : 0;
    return (DWORD)(pitch * h);
}
static gy__os_t tf_saved;
static void tf_on(void) { tf_saved = gy__os; gy__os.glyph_outline = tf_glyph; gy__os.text_metrics = tf_metrics; }
static void tf_off(void) { gy__os = tf_saved; }

static int t_h1_test_font(void) {
    int f16, f32, f48, i;
    tf_on();
    if (!open_at(320, 120)) { tf_off(); return 0; }
    f16 = gy_font_add("Segoe UI", 16, 0); f32 = gy_font_add("Segoe UI", 32, 0); f48 = gy_font_add("Segoe UI", 48, 0);
    for (i = 0; i < 3; i++) {
        int k = i == 0 ? f16 : i == 1 ? f32 : f48, px = 16 * (i + 1);
        gy_font(k);
        if (gy_text_width("abc") != 3 * px / 2 || gy_line_height() != px) {
            LOG("at %d px: width %.1f (want %d), line %.1f (want %d); ", px, (double)gy_text_width("abc"), 3 * px / 2, (double)gy_line_height(), px);
            tf_off(); return 0;
        }
    }
    gy_begin();
    gy_font(f16); gy_canvas_text(10, 10, "abc", gy_theme_get()->text);
    gy_font(f32); gy_canvas_text(10, 40, "abc", gy_theme_get()->text);
    gy_end();
    /* the glyph boxes at 32 px are exactly twice the ones at 16 px, measured from the pen */
    CHECK(gy__c.inst[3].x0 - 10 == 2 * (gy__c.inst[0].x0 - 10) && gy__c.inst[3].x1 - gy__c.inst[3].x0 == 2 * (gy__c.inst[0].x1 - gy__c.inst[0].x0)
          && gy__c.inst[3].y1 - gy__c.inst[3].y0 == 2 * (gy__c.inst[0].y1 - gy__c.inst[0].y0), "the 32 px glyph is not twice the 16 px one");
    LOG("16/32/48 px: widths 24/48/72, lines 16/32/48, glyph boxes exactly 2x; ");
    tf_off();
    return 1;
}

/* --- windowed (gytest --windowed only) -------------------------------------------------------------- */
static int w_chk;                              /* the check box's state: reset at the start of every run */
static void w_body(void) {
    static const float three[3] = { 110, 110, -1 };
    gy_text("GUICy in an app's own window");
    gy_row(3, three);
    gy_button("Build"); gy_button("Stop"); gy_button("Open folder");
    gy_row(0, NULL);
    gy_check("Pixel look", &w_chk);
    gy_progress(0.4f);
}
static const int w_path[][3] = { { 200, 200, 0 }, { 60, 56, 0 }, { 60, 56, 1 }, { 60, 56, 0 }, { 40, 90, 1 }, { 40, 90, 0 }, { 300, 300, 0 } };
#define W_STEPS ((int)(sizeof w_path / sizeof w_path[0]))

static uint64_t inst_hash(void) {
    uint64_t x = 1469598103934665603ull;
    const unsigned char *b = (const unsigned char *)gy__c.inst;
    size_t i, n = sizeof(gy__inst) * (size_t)gy__c.ninst;
    for (i = 0; i < n; i++) x = (x ^ b[i]) * 1099511628211ull;
    return x ^ (uint64_t)gy__c.ninst;
}

/* the same steps off screen, for the reference */
static int w_offscreen(uint64_t *ih, uint64_t *ph) {
    int i;
    if (!open_at(420, 200)) return 0;
    w_chk = 0;
    for (i = 0; i < W_STEPS; i++) {
        gy_input_mouse(w_path[i][0], w_path[i][1]); gy_input_button(0, w_path[i][2]);
        gy_begin(); w_body(); gy_end();
        gy_begin(); w_body(); gy_end();
    }
    *ih = inst_hash(); *ph = frame_hash();
    LOG("off screen %d instances, hot %u, mouse %d,%d; ", gy__c.ninst, (unsigned)gy__c.active, gy__c.mx, gy__c.my);
    gy_shutdown();
    return 1;
}

static LRESULT CALLBACK w_app_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (gy_event(h, m, (uintptr_t)w, (intptr_t)l)) return 0;   /* the app hands its messages to GUICy */
    return DefWindowProcW(h, m, w, l);
}

static HWND w_app_window(int cw, int ch) {
    WNDCLASSW wc;
    RECT r = { 0, 0, cw, ch };
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = w_app_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"gytest_app";
    RegisterClassW(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowExW(0, L"gytest_app", L"gytest app", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, GetModuleHandleW(NULL), NULL);
}

static void w_send(HWND h, int i) {
    SendMessageW(h, WM_MOUSEMOVE, 0, MAKELPARAM(w_path[i][0], w_path[i][1]));
    if (w_path[i][2] && !gy__c.down[0]) SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(w_path[i][0], w_path[i][1]));
    if (!w_path[i][2] && gy__c.down[0]) SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(w_path[i][0], w_path[i][1]));
}

/* an app's own window and loop: the same instance list and pixels as off screen */
static int t_w_attach(void) {
    uint64_t ih0, ph0, ih, ph;
    HWND h;
    int i;
    if (!w_offscreen(&ih0, &ph0)) return 0;
    if (!open_at(1, 1)) return 0;
    gy_shutdown();
    h = w_app_window(420, 200);
    CHECK(h != NULL, "no app window");
    if (!gy_attach(h)) { LOG("gy_attach: %s; ", gy_last_error()); DestroyWindow(h); return 0; }
    gy_isa_set(g_isa);
    w_chk = 0;
    for (i = 0; i < W_STEPS; i++) {
        w_send(h, i);
        gy_begin(); w_body(); gy_end();
        gy_begin(); w_body(); gy_end();
    }
    ih = inst_hash(); ph = frame_hash();
    LOG("attached %dx%d, %d instances, mouse %d,%d, check %d; ", gy_width(), gy_height(), gy__c.ninst, gy__c.mx, gy__c.my, gy__c.down[0]);
    if (ih != ih0 || ph != ph0) { LOG("differs from off screen (instances %s, pixels %s); ", ih == ih0 ? "same" : "differ", ph == ph0 ? "same" : "differ"); DestroyWindow(h); return 0; }
    /* keep the frame for the golden; the window goes with gy_shutdown's caller */
    g_win_to_destroy = h;
    return 1;
}

/* GUICy's own window and gy_frame loop: the same again */
static int t_w_open(void) {
    uint64_t ih0, ph0, ih, ph;
    int i, frames_run = 0;
    if (!w_offscreen(&ih0, &ph0)) return 0;
    if (!gy_open(&(gy_desc){ .title = "gytest w_open", .w = 420, .h = 200 })) { LOG("gy_open: %s; ", gy_last_error()); return 0; }
    gy_isa_set(g_isa);
    w_chk = 0;
    for (i = 0; i < W_STEPS; i++) {
        PostMessageW(gy__c.hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(w_path[i][0], w_path[i][1]));
        if (w_path[i][2] && !gy__c.down[0]) PostMessageW(gy__c.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(w_path[i][0], w_path[i][1]));
        if (!w_path[i][2] && gy__c.down[0]) PostMessageW(gy__c.hwnd, WM_LBUTTONUP, 0, MAKELPARAM(w_path[i][0], w_path[i][1]));
        if (!gy_frame()) break;
        w_body(); frames_run++;
        if (!gy_frame()) break;
        w_body(); frames_run++;
    }
    gy_end();
    ih = inst_hash(); ph = frame_hash();
    LOG("gy_open %dx%d, %d frames, %d instances, mouse %d,%d, check %d, inst %s, px %s; ", gy_width(), gy_height(), frames_run, gy__c.ninst, gy__c.mx, gy__c.my, w_chk, ih == ih0 ? "same" : "differ", ph == ph0 ? "same" : "differ");
    CHECK(frames_run == 2 * W_STEPS, "gy_frame ran %d frames, not %d", frames_run, 2 * W_STEPS);
    CHECK(ih == ih0 && ph == ph0, "gy_open's frames differ from off screen");
    return 1;
}

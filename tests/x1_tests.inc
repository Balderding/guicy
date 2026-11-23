/* X1 tests: fallback glyphs, keyboard focus, IME (included by gytest.c) */

/* --- fallback glyphs: a mixed-script list, each line's non-Latin characters from a fallback font --- */
static const char *const x1_lines[] = {
    "Latin: Pixel art from anywhere",
    "Japanese: \xe3\x83\x89""\xe3\x83\x83""\xe3\x83\x88""\xe7\xb5\xb5"" \xe6\x97\xa5""\xe6\x9c\xac""\xe8\xaa\x9e""",
    "Chinese: \xe5\x83\x8f""\xe7\xb4\xa0""\xe7\x94\xbb"" \xe4\xb8\xad""\xe6\x96\x87""",
    "Korean: \xed\x94\xbd""\xec\x85\x80"" \xec\x95\x84""\xed\x8a\xb8""",
    "Symbols: \xe2\x98\x85"" \xe2\x9c\x93"" \xe2\x8c\x98"" \xe2\x86\x92""",
    "Emoji: \xf0\x9f\x98\x80"" \xf0\x9f\x8e\xa8"" \xf0\x9f\x93\x81"""
};
#define X1_NLINES ((int)(sizeof x1_lines / sizeof x1_lines[0]))

/* for every character >= 128 in the list: is it drawn by a fallback font (not the font's box)? */
static int x1_check_fallback(void) {
    int i, missing = 0, own = 0, fb = 0;
    for (i = 0; i < X1_NLINES; i++) {
        const char *s = x1_lines[i];
        while (*s) {
            uint32_t cp = gy__utf8_next(&s);
            UINT code = cp, how = GGO_GRAY8_BITMAP;
            if (cp < 128) continue;
            gy__fallback(&gy__c.fonts[0], cp, &code, &how);
            if (gy__c.dc_font != gy__c.fonts[0].hf) fb++;
            else if (gy__cmap_gid(&gy__c.fonts[0].cmap, cp)) own++;
            else { missing++; LOG("U+%04X has no font; ", (unsigned)cp); }
        }
    }
    LOG("%d characters from fallback fonts, %d the font's own, %d with no font; ", fb, own, missing);
    return missing == 0 && fb > 0;
}

static void x1_scene(void) {
    int i;
    gy_begin();
    for (i = 0; i < X1_NLINES; i++) gy_text("%s", x1_lines[i]);
    gy_end();
}

static int x1_fallback_at(float k) {
    LARGE_INTEGER f, a, b;
    int m0;
    gy_shutdown();
    if (!open_at((int)(420 * k), (int)(230 * k))) return 0;
    gy_scale_set(k);
    QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
    x1_scene();
    QueryPerformanceCounter(&b);
    LOG("first frame with the fallbacks %.1f ms; ", (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart);
    m0 = gy_get_stats().glyph_misses;
    x1_scene();
    if (gy_get_stats().glyph_misses != m0) { LOG("%d glyph misses after the first frame; ", gy_get_stats().glyph_misses - m0); return 0; }
    return x1_check_fallback();
}
static int t_x1_fallback_100(void) { return x1_fallback_at(1.0f); }
static int t_x1_fallback_125(void) { return x1_fallback_at(1.25f); }

/* --- keyboard focus: a scripted Tab walk over the v1 scene visits every widget once, in order ---- */
static void x1_tab(int back) { key(v_scene, back ? GY_MOD_SHIFT : 0, GY_KEY_TAB); }

static int t_x1_focus_walk(void) {
    uint32_t seen[64];
    int i, j, n;
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    n = gy__c.nkf;
    if (n != 12) { LOG("%d widgets in the walk, want 12; ", n); return 0; }
    for (i = 0; i < n; i++) {
        x1_tab(0);
        seen[i] = gy__c.kfocus;
        if (seen[i] != gy__c.kf[i]) { LOG("Tab %d reached widget %08x, not the %d-th; ", i + 1, (unsigned)seen[i], i + 1); return 0; }
        for (j = 0; j < i; j++) if (seen[j] == seen[i]) { LOG("Tab %d visited a widget twice; ", i + 1); return 0; }
    }
    x1_tab(0);
    if (gy__c.kfocus != seen[0]) { LOG("Tab after the last does not wrap to the first; "); return 0; }
    x1_tab(1);
    if (gy__c.kfocus != seen[n - 1]) { LOG("Shift+Tab from the first does not go to the last; "); return 0; }
    for (i = n - 2; i >= 0; i--) { x1_tab(1); if (gy__c.kfocus != seen[i]) { LOG("Shift+Tab step %d off; ", n - 1 - i); return 0; } }
    LOG("%d widgets, Tab and Shift+Tab each visit all once, in order, and wrap; ", n);
    return 1;
}

/* Enter / Space / arrows on the focused widget, by counts */
static int x1_tab_to(int k) { int i; x1_tab(0); for (i = 0; i < k; i++) x1_tab(0); return gy__c.kfocus == gy__c.kf[k]; }

static int t_x1_focus_keys(void) {
    float sl;
    int ok = 1;
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    /* the slider: Right steps it by a twentieth */
    if (!x1_tab_to(0)) return 0;
    sl = V.sl; key(v_scene, 0, GY_KEY_RIGHT);
    if (V.sl <= sl) { LOG("Right did not move the slider (%.2f); ", (double)V.sl); ok = 0; }
    /* the number: Left steps it down, Enter starts typing */
    gy_shutdown(); if (!v_open()) return 0; gy_input_mouse(-100, -100); frames(v_scene, 2);
    if (!x1_tab_to(2)) return 0;
    key(v_scene, 0, GY_KEY_LEFT);
    if (V.num != 1.25f) { LOG("Left on the number gave %.2f, want 1.25; ", (double)V.num); ok = 0; }
    key(v_scene, 0, GY_KEY_ENTER);
    if (!gy__c.focus) { LOG("Enter on the number did not start typing; "); ok = 0; }
    x1_tab(0);                                 /* Tab out takes the typed value back */
    if (gy__c.focus) { LOG("Tab out of the number kept the keys; "); ok = 0; }
    /* the combo: Space opens it */
    gy_shutdown(); if (!v_open()) return 0; gy_input_mouse(-100, -100); frames(v_scene, 2);
    if (!x1_tab_to(4)) return 0;
    key(v_scene, 0, GY_KEY_SPACE);
    if (!gy__c.popup_id) { LOG("Space did not open the combo; "); ok = 0; }
    /* the edit box: a Tab gives it the keys with all selected; typing replaces */
    gy_shutdown(); if (!v_open()) return 0; gy_input_mouse(-100, -100); frames(v_scene, 2);
    if (!x1_tab_to(5)) return 0;
    type(v_scene, "x");
    if (strcmp(V.name, "x")) { LOG("typing after Tab gave \"%s\", want \"x\"; ", V.name); ok = 0; }
    /* the button: Enter presses it once */
    gy_shutdown(); if (!v_open()) return 0; gy_input_mouse(-100, -100); frames(v_scene, 2);
    g_log[0] = 0;
    if (!x1_tab_to(9)) return 0;
    key(v_scene, 0, GY_KEY_ENTER);
    if (!strstr(g_log, "Build key; ") || strstr(strstr(g_log, "Build key; ") + 1, "Build key; ")) { LOG("Enter on Build: not exactly one press; "); ok = 0; }
    /* a click hides the outline and moves the focus quietly */
    click_at(v_scene, AT_SLIDER);
    if (gy__c.kvis || gy__c.kfocus != gy__c.kf[0]) { LOG("a click did not move the focus quietly; "); ok = 0; }
    if (ok) LOG("slider Right, number Left/Enter/Tab out, combo Space, edit Tab+type, button Enter once, click quiet; ");
    return ok;
}

/* the outline, for eyes: Tab to the slider */
static int t_x1_focus_ring(void) {
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    x1_tab(0); x1_tab(0); x1_tab(0); x1_tab(0); x1_tab(0); x1_tab(0);   /* to the edit box */
    frames(v_scene, 2);
    return 1;
}

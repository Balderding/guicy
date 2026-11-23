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

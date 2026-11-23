/* d1_tests.inc - D1: everything scales. Included into gytest.c after h1_tests.inc. */

/* the theme at scale 1 resolves to exactly today's integers, so today's goldens stay byte-equal */
static int t_d1_resolved(void) {
    if (!open_at(64, 64)) return 0;
    CHECK(GY__S.pad == 8 && GY__S.gap == 6 && GY__S.row_h == 28 && GY__S.radius == 4, "pad/gap/row/radius %d/%d/%d/%d", GY__S.pad, GY__S.gap, GY__S.row_h, GY__S.radius);
    CHECK(GY__S.check == 16 && GY__S.scroll == 10 && GY__S.split == 6 && GY__S.title == 24, "check/scroll/split/title %d/%d/%d/%d", GY__S.check, GY__S.scroll, GY__S.split, GY__S.title);
    CHECK(GY__S.tile_w == 160 && GY__S.tile_h == 110 && GY__S.track == 6 && GY__S.knob == 12, "tile/track/knob %d/%d/%d/%d", GY__S.tile_w, GY__S.tile_h, GY__S.track, GY__S.knob);
    CHECK(GY__S.button_w == 22 && GY__S.inset == 6 && GY__S.line == 1 && GY__S.font_px == 15, "button/inset/line/font %d/%d/%d/%d", GY__S.button_w, GY__S.inset, GY__S.line, GY__S.font_px);
    gy_scale_set(1.25f);
    LOG("at 125 %%: row %d, pad %d, gap %d, check %d, font %d px, line %d; ", GY__S.row_h, GY__S.pad, GY__S.gap, GY__S.check, GY__S.font_px, GY__S.line);
    CHECK(GY__S.row_h == 35 && GY__S.gap == 8 && GY__S.line == 1, "125 %% did not resolve as planned (row 35, gap 8, line 1)");
    gy_scale_set(1);
    gy_begin(); gy_end();
    return 1;
}

/* exact scaling: the same scene at factors 1, 2, 3 and 4 with the synthetic font and a theme of even
   sizes; every instance rect is the factor-1 rect times k, within the rounding of one halving
   (k / 2 px). An unscaled constant shows up as a far bigger difference. */
static void scale_scene(void) {
    static const float three[3] = { 110, 110, -1 };
    static int c1 = 1, c2 = 0;
    static float sl = 30;
    static char nm[32] = "base";
    gy_begin();
    gy_section("Section");
    gy_text("Plain text");
    gy_row(3, three);
    gy_button("One"); gy_button("Two"); gy_select("Three", 1);
    gy_row(0, NULL);
    gy_check("Check on", &c1); gy_check("Check off", &c2);
    gy_slider("Slide", &sl, 0, 100);
    gy_edit("Name", nm, sizeof nm);
    gy_progress(0.5f);
    gy_combo("Pick", &c2, "a\0b\0");
    gy_end();
}
static gy_theme even_theme(void) {
    gy_theme t = *gy_theme_get();
    t.font_px = 16;
    t.pad = 8.0f / 16; t.gap = 6.0f / 16; t.row_h = 32.0f / 16; t.radius = 4.0f / 16; t.check = 16.0f / 16; t.scroll = 10.0f / 16;
    t.split = 6.0f / 16; t.title = 24.0f / 16; t.tile_w = 160.0f / 16; t.tile_h = 112.0f / 16; t.track = 6.0f / 16; t.knob = 12.0f / 16;
    t.button_w = 22.0f / 16; t.inset = 6.0f / 16;
    return t;
}
static int t_d1_exact(void) {
    static gy__inst base[2048];
    int nbase = 0, k, i, worst = 0, worst_k = 0, worst_i = -1;
    tf_on();
    for (k = 1; k <= 4; k++) {
        gy_theme t;
        if (!open_at(420 * k, 360 * k)) { tf_off(); return 0; }
        t = even_theme(); gy_theme_set(&t);
        gy_scale_set((float)k);
        gy_input_mouse(-100, -100); scale_scene(); scale_scene();
        if (k == 1) { nbase = gy__c.ninst; memcpy(base, gy__c.inst, sizeof(gy__inst) * (size_t)nbase); }
        else {
            if (gy__c.ninst != nbase) { LOG("at %dx: %d instances, at 1x %d; ", k, gy__c.ninst, nbase); tf_off(); return 0; }
            for (i = 0; i < nbase; i++) {
                const gy__inst *a = &base[i], *b = &gy__c.inst[i];
                int d;
                if (a->kind == GY__K_LINE) continue;   /* a line's box carries its width's +1 */
                d = abs(b->x0 - k * a->x0); if (abs(b->y0 - k * a->y0) > d) d = abs(b->y0 - k * a->y0);
                if (abs(b->x1 - k * a->x1) > d) d = abs(b->x1 - k * a->x1);
                if (abs(b->y1 - k * a->y1) > d) d = abs(b->y1 - k * a->y1);
                if (d - k / 2 > worst) { worst = d - k / 2; worst_k = k; worst_i = i; }
            }
        }
        if (k < 4) gy_shutdown();
    }
    tf_off();
    if (worst > 0) {
        const gy__inst *a = &base[worst_i];
        LOG("instance %d (kind %d, at 1x %d,%d-%d,%d) is %d px off k x at %dx; ", worst_i, a->kind, a->x0, a->y0, a->x1, a->y1, worst + worst_k / 2, worst_k);
        return 0;
    }
    LOG("%d instances, every rect k x the 1x one at k = 2, 3, 4 (within k/2 px); ", nbase);
    return 1;
}

/* the look set: the real font at 125 %, 150 % and 200 % (120, 144 and 192 DPI) */
static int look_at(float k) {
    if (!v_open()) return 0;
    gy_shutdown();
    if (!open_at((int)(640 * k), (int)(520 * k))) return 0;
    gy_scale_set(k);
    make_images();
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}
static int t_d1_look_125(void) { return look_at(1.25f); }
static int t_d1_look_150(void) { return look_at(1.5f); }
static int t_d1_look_200(void) { return look_at(2.0f); }

/* no seams: at 125 % times every zoom step down to 0.75, pixel -> DIP -> pixel is the identity, and a
   1-DIP canvas line is exactly one pixel wide */
static int t_d1_seams(void) {
    int z, x, w;
    if (!open_at(400, 200)) return 0;
    for (z = 100; z >= 75; z -= 5) {
        gy_scale_set(1.25f * (float)z / 100.0f);
        for (x = 0; x < 300; x += 7)
            for (w = 1; w < 60; w += 5) {
                gy__ir r = { x, x / 2, w, w + 3 }, b = gy__from_dip(gy__to_dip(r));
                if (b.x != r.x || b.y != r.y || b.w != r.w || b.h != r.h) {
                    LOG("at %.4f: %d,%d %dx%d came back %d,%d %dx%d; ", (double)gy__c.scale, r.x, r.y, r.w, r.h, b.x, b.y, b.w, b.h);
                    return 0;
                }
            }
        {   /* a 1-DIP line at a quarter-pixel offset: exactly one row of pixels */
            int y, rows = 0;
            gy_begin();
            gy_canvas_line(10, 20.25f, 200, 20.25f, 1, GY_RGB(255, 255, 255));
            gy_end();
            for (y = 0; y < gy_height(); y++) if (gy_pixels()[y * gy_width() + 100] != gy_theme_get()->bg) rows++;
            if (rows != 1) { LOG("at %.4f a 1-DIP line covers %d rows; ", (double)gy__c.scale, rows); return 0; }
        }
    }
    LOG("125 %% x 1.00..0.75: rects round-trip exactly, a 1-DIP line is 1 px; ");
    gy_scale_set(1.25f);
    gy_begin(); gy_canvas_line(10, 20.25f, 200, 20.25f, 1, GY_RGB(255, 255, 255)); gy_end();
    return 1;
}

/* zoom, VS Code style */
static int t_d1_zoom(void) {
    char line[64];
    int i, owned[1] = { GY_KEYCODE(GY_MOD_CTRL, GY_KEY_PLUS) };
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    key(v_scene, GY_MOD_CTRL, GY_KEY_PLUS);
    CHECK(gy_zoom() > 1.09f && gy_zoom() < 1.11f && gy_scale() > 1.09f, "Ctrl+= gave zoom %.2f", (double)gy_zoom());
    for (i = 0; i < 30; i++) key(v_scene, GY_MOD_CTRL, GY_KEY_PLUS);
    CHECK(gy_zoom() == 3.0f, "the zoom went past 3.0 (%.2f)", (double)gy_zoom());
    for (i = 0; i < 40; i++) key(v_scene, GY_MOD_CTRL, GY_KEY_SUBTRACT);
    CHECK(gy_zoom() == 0.75f, "the zoom went under 0.75 (%.2f)", (double)gy_zoom());
    key(v_scene, GY_MOD_CTRL, '0');
    CHECK(gy_zoom() == 1.0f, "Ctrl+0 gave %.2f", (double)gy_zoom());
    gy_input_mouse(AT_LIST);                   /* Ctrl+wheel over the list zooms, and doesn't scroll it */
    gy_input_key(GY_KEY_CTRL, 1); gy_input_wheel(1); v_scene(); gy_input_key(GY_KEY_CTRL, 0); v_scene();
    CHECK(gy_zoom() > 1.09f && gy_zoom() < 1.11f, "Ctrl+wheel gave %.2f", (double)gy_zoom());
    /* after a zoom: the next frame is drawn at the new size, then nothing more */
    gy_input_mouse(-100, -100); v_scene();
    v_scene(); CHECK(gy_get_stats().dirty_tiles == 0, "a quiet frame after a zoom drew %d tiles", gy_get_stats().dirty_tiles);
    gy_view_save(line, sizeof line);
    LOG("saved \"%s\"; ", line);
    gy_shutdown();
    if (!v_open()) return 0;
    CHECK(gy_view_load(line) && gy_zoom() > 1.09f && gy_zoom() < 1.11f, "the zoom did not come back from \"%s\"", line);
    /* an app that owns Ctrl+= keeps it: that zoom key is off, a banner says so, numpad + still zooms */
    gy_keys_owned(owned, 1);
    CHECK(strstr(gy_last_error(), "zoom_in") != NULL, "no clash message: %s", gy_last_error());
    gy_zoom_set(1);
    V.key_d = 0;
    key(v_scene, GY_MOD_CTRL, GY_KEY_PLUS);
    CHECK(gy_zoom() == 1.0f, "the app's own Ctrl+= zoomed");
    key(v_scene, GY_MOD_CTRL, GY_KEY_ADD);
    CHECK(gy_zoom() > 1.09f, "Ctrl+numpad+ did not zoom");
    gy_zoom_set(1);
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

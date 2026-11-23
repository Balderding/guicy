/* v1_tests.inc - step 4's tests, included into gytest.c (they share its helpers). */

#define LOG(...) do { char b_[256]; snprintf(b_, sizeof b_, __VA_ARGS__); strncat(g_log, b_, sizeof g_log - strlen(g_log) - 1); } while (0)
#define CHECK(cond, ...) do { if (!(cond)) { LOG(__VA_ARGS__); LOG("; "); return 0; } } while (0)

static unsigned g_now;
static int open_at(int w, int h) { if (!frame_open(w, h)) return 0; g_now = 1000; gy_input_time(g_now); return 1; }
static void tick(unsigned ms) { g_now += ms; gy_input_time(g_now); }

typedef void (*scene_fn)(void);
static void frames(scene_fn f, int n) { while (n--) f(); }
static void click_at(scene_fn f, int x, int y) {
    gy_input_mouse(x, y); f();
    gy_input_button(0, 1); f();
    gy_input_button(0, 0); f();
}
static void key(scene_fn f, int mods, int k) {
    if (mods & GY_MOD_CTRL) gy_input_key(GY_KEY_CTRL, 1);
    if (mods & GY_MOD_ALT) gy_input_key(GY_KEY_ALT, 1);
    if (mods & GY_MOD_SHIFT) gy_input_key(GY_KEY_SHIFT, 1);
    gy_input_key(k, 1); f(); gy_input_key(k, 0);
    if (mods & GY_MOD_SHIFT) gy_input_key(GY_KEY_SHIFT, 0);
    if (mods & GY_MOD_ALT) gy_input_key(GY_KEY_ALT, 0);
    if (mods & GY_MOD_CTRL) gy_input_key(GY_KEY_CTRL, 0);
    f();
}
static void type(scene_fn f, const char *s) { while (*s) gy_input_char((unsigned char)*s++); f(); }

/* --- the v1 widget scene ----------------------------------------------------------------------- */
static struct {
    float sl, num, split;
    int sli, numi, combo, chip, edit_flags, ctx_clicks, popup_seen, key_d, img_a, img_b, tip;
    char name[64];
} V;

static void v_reset(void) {
    memset(&V, 0, sizeof V);
    V.sl = 32; V.num = 1.5f; V.sli = 3; V.numi = 7; V.combo = 1; V.split = 300;
    strcpy(V.name, "base");
}

static void make_images(void) {
    static unsigned char px[8 * 8 * 4];
    int x, y;
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
        unsigned char *p = px + (y * 8 + x) * 4;
        int on = (x + y) & 1;
        p[0] = on ? 0 : 230; p[1] = on ? 150 : 190; p[2] = on ? 150 : 90; p[3] = (x == 0 || y == 0) ? 128 : 255;   /* RGBA */
    }
    V.img_a = gy_image_make(8, 8, px, GY_FMT_RGBA);
    V.img_b = V.img_a;
}

static void v_scene(void) {
    static const float half[2] = { 0.5f, -1 }, three[3] = { -1, -1, -1 };
    static const char *const chips[] = { "Small", "Medium", "Large" };
    int i;
    gy_begin();
    gy_split("main", GY_H, &V.split, 300);
        gy_section("Numbers");
        gy_slider("Size", &V.sl, 8, 512);
        gy_slider_i("Detail", &V.sli, 1, 4);
        gy_number("Scale", &V.num, 0.25f, 0, 10);
        gy_number_i("How many", &V.numi, 1, 20);
        gy_combo("Style", &V.combo, "Badlands\0Highland\0Mine\0");
        V.edit_flags |= gy_edit("Name", V.name, sizeof V.name);
        gy_row(3, three);
        gy_chips(chips, 3, &V.chip);
        gy_row(0, NULL);
        gy_progress(0.62f);
        if (gy_button_key("Build##k", 'B')) LOG("Build key; ");
        if (V.tip) gy_tip("Builds the picked bases");
        if (gy_context_begin()) {
            if (gy_menu_item("Copy")) V.ctx_clicks++;
            gy_menu_item("Paste");
            gy_context_end();
        }
        gy_text_wrap("Wrapped text: the row grows to fit the lines, and the next widget goes below it.");
        gy_button_accent("Primary");
    gy_pane();
        if (gy_tree("Tree")) { gy_text("child one"); gy_text("child two"); gy_tree_end(); }
        gy_scroll_begin("list", 150);
        for (i = 0; i < 30; i++) gy_text("row %d", i + 1);
        gy_scroll_end();
        gy_row(2, half);
        gy_image(V.img_a, 64, 64);
        gy_image(V.img_b, 32, 32);
        {
            gy_rect a = gy_avail();
            gy_canvas_line((float)a.x, (float)a.y + 10, (float)a.x + 120, (float)a.y + 40, 2, gy_theme_get()->good);
            gy_canvas_dash((float)a.x, (float)a.y + 50, (float)a.x + 200, (float)a.y + 50, 2, 6, gy_theme_get()->warn);
            gy_canvas_circle(a.x + 160, a.y + 20, 14, gy_theme_get()->accent);
        }
        if (gy_popup_begin("mine", (gy_rect){ 420, 300, 160, 60 })) { gy_text("my popup"); V.popup_seen++; gy_popup_end(); }
    gy_split_end();
    if (gy_key_pressed('D')) V.key_d++;
    gy_end();
}

/* where things are in v_scene (420x... at 640x520): found from the pictures */
#define AT_SLIDER 150, 66
#define AT_COMBO 200, 192
#define AT_EDIT 200, 226
#define AT_BUILD 150, 328
#define AT_LIST 450, 120

static int v_open(void) { v_reset(); if (!open_at(640, 520)) return 0; make_images(); return 1; }

static int t_v1_widgets(void) { if (!v_open()) return 0; gy_input_mouse(-100, -100); frames(v_scene, 2); return 1; }

static int t_v1_combo_open(void) {
    if (!v_open()) return 0;
    click_at(v_scene, AT_COMBO);
    gy_input_mouse(200, 260); frames(v_scene, 2);   /* hover the second item */
    return 1;
}

static int t_v1_combo_pick(void) {
    if (!v_open()) return 0;
    click_at(v_scene, AT_COMBO);
    click_at(v_scene, 200, 294);               /* the third item: Mine */
    CHECK(V.combo == 2, "the combo did not pick Mine (it is %d)", V.combo);
    click_at(v_scene, AT_COMBO);               /* open it, then click elsewhere: it closes and nothing changes */
    click_at(v_scene, 600, 500);
    CHECK(V.combo == 2, "a click outside changed the combo");
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

static int t_v1_slider(void) {
    if (!v_open()) return 0;
    gy_input_mouse(AT_SLIDER); v_scene();
    gy_input_button(0, 1); v_scene();
    gy_input_mouse(5, 66); v_scene();          /* drag past the left end */
    gy_input_button(0, 0); v_scene();
    CHECK(V.sl == 8, "the drag to the left end gave %.2f, not 8", (double)V.sl);
    gy_input_mouse(AT_SLIDER); v_scene();
    gy_input_wheel(1); v_scene();              /* one notch up over it: 1/20 of the range */
    CHECK(V.sl > 33 && V.sl < 34, "one wheel notch gave %.2f", (double)V.sl);
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

static int t_v1_edit(void) {
    if (!v_open()) return 0;
    click_at(v_scene, AT_EDIT);
    key(v_scene, 0, GY_KEY_END);
    type(v_scene, " kit");
    CHECK(!strcmp(V.name, "base kit"), "typing gave \"%s\"", V.name);
    key(v_scene, GY_MOD_CTRL, GY_KEY_LEFT);    /* to the start of "kit" */
    type(v_scene, "X");
    CHECK(!strcmp(V.name, "base Xkit"), "ctrl+left then X gave \"%s\"", V.name);
    tick(1500);
    key(v_scene, 0, GY_KEY_BACK);
    CHECK(!strcmp(V.name, "base kit"), "backspace gave \"%s\"", V.name);
    key(v_scene, GY_MOD_CTRL, 'Z');
    CHECK(!strcmp(V.name, "base Xkit"), "undo gave \"%s\"", V.name);
    key(v_scene, GY_MOD_CTRL, 'A');
    key(v_scene, GY_MOD_CTRL, 'X');            /* cut everything (no window: GUICy's own clipboard) */
    CHECK(V.name[0] == 0, "cut left \"%s\"", V.name);
    key(v_scene, GY_MOD_CTRL, 'V');
    CHECK(!strcmp(V.name, "base Xkit"), "paste gave \"%s\"", V.name);
    gy_input_char(0xE9); gy_input_char(0xE9); v_scene();   /* two e-acute: two-byte characters */
    CHECK(!strcmp(V.name, "base Xkit\xc3\xa9\xc3\xa9"), "the accented letters gave \"%s\"", V.name);
    key(v_scene, 0, GY_KEY_BACK);
    CHECK(!strcmp(V.name, "base Xkit\xc3\xa9"), "backspace over a 2-byte letter gave \"%s\"", V.name);
    V.edit_flags = 0;
    key(v_scene, 0, GY_KEY_ENTER);
    CHECK(V.edit_flags & GY_EDIT_ENTER, "Enter was not reported");
    click_at(v_scene, 600, 500);               /* a click elsewhere takes the keys away */
    type(v_scene, "zz");
    CHECK(!strcmp(V.name, "base Xkit\xc3\xa9"), "typing after unfocus changed it to \"%s\"", V.name);
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

/* the caret blinks at 530 ms: over 2.12 s a window that only wakes when asked draws 4 frames */
static int t_v1_caret(void) {
    int drawn = 0, ran = 0;
    unsigned end;
    if (!v_open()) return 0;
    click_at(v_scene, AT_EDIT);
    gy_input_mouse(600, 500); frames(v_scene, 2);   /* move away (the hover goes), then quiet */
    end = g_now + 2120;
    while (g_now < end) {
        gy_stats s = gy_get_stats();
        tick(10);
        if (!s.next_wake_ms || (int)(g_now - s.next_wake_ms) < 0) continue;   /* the window would sleep */
        v_scene(); ran++;
        if (gy_get_stats().dirty_tiles) drawn++;
    }
    LOG("caret: %d frames run, %d drew, in 2.12 s; ", ran, drawn);
    CHECK(drawn == 4 && ran == 4, "the caret should cost 4 frames in 2.12 s");
    return 1;
}

static int t_v1_tip(void) {
    uint64_t before;
    if (!v_open()) return 0;
    V.tip = 1;
    gy_input_mouse(AT_BUILD); frames(v_scene, 2);
    before = frame_hash();
    tick(450); v_scene();
    CHECK(frame_hash() == before, "the tooltip showed before 500 ms");
    tick(60); v_scene();
    CHECK(frame_hash() != before, "the tooltip did not show after 510 ms");
    return 1;
}

static int t_v1_scroll(void) {
    uint64_t before;
    if (!v_open()) return 0;
    gy_input_mouse(AT_LIST); frames(v_scene, 2);
    before = frame_hash();
    gy_input_mouse(600, 500); gy_input_wheel(-1); frames(v_scene, 2);   /* the wheel outside the list does nothing */
    gy_input_mouse(AT_LIST); frames(v_scene, 1);
    CHECK(frame_hash() == before, "the wheel outside the list moved something");
    gy_input_wheel(-1); frames(v_scene, 2);
    CHECK(frame_hash() != before, "the wheel over the list did not scroll it");
    return 1;
}

static int t_v1_split(void) {
    if (!v_open()) return 0;
    gy_input_mouse(311, 300); v_scene();       /* the divider */
    gy_input_button(0, 1); v_scene();
    gy_input_mouse(250, 300); v_scene();
    gy_input_button(0, 0); v_scene();
    CHECK(V.split < 250 && V.split > 230, "the drag put the divider at %.1f", (double)V.split);
    gy_input_mouse((int)V.split + 11, 300); v_scene();
    gy_input_button(0, 1); v_scene(); gy_input_button(0, 0); v_scene();
    tick(100);
    gy_input_button(0, 1); v_scene(); gy_input_button(0, 0); v_scene();   /* the second click: a double-click */
    CHECK(V.split == 300, "double-click did not reset the divider (it is %.1f)", (double)V.split);
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

static int t_v1_keys(void) {
    if (!v_open()) return 0;
    gy_input_mouse(-100, -100);
    g_log[0] = 0;
    key(v_scene, 0, 'B');
    CHECK(strstr(g_log, "Build key"), "the B key did not press Build");
    key(v_scene, 0, 'D');
    CHECK(V.key_d == 1, "the app did not get its D key");
    g_log[0] = 0;
    click_at(v_scene, AT_EDIT);                /* with the edit box focused, B types a letter instead */
    key(v_scene, 0, 'B'); type(v_scene, "B");
    CHECK(!strstr(g_log, "Build key"), "B pressed Build while typing");
    key(v_scene, 0, 'D');
    CHECK(V.key_d == 1, "D reached the app while typing");
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

/* Master Control's trap: open and close every kind of popup, then type: the text must arrive */
static int t_v1_popup_trap(void) {
    if (!v_open()) return 0;
    click_at(v_scene, AT_COMBO); key(v_scene, 0, GY_KEY_ESC);                         /* combo: Esc */
    click_at(v_scene, AT_COMBO); click_at(v_scene, 600, 500);                         /* combo: a click outside */
    click_at(v_scene, AT_COMBO); click_at(v_scene, 200, 260);                         /* combo: a pick */
    V.tip = 1; gy_input_mouse(AT_BUILD); frames(v_scene, 1); tick(600); frames(v_scene, 2); gy_input_mouse(600, 500); frames(v_scene, 2); V.tip = 0;  /* tooltip */
    gy_input_mouse(AT_BUILD); v_scene(); gy_input_button(1, 1); v_scene(); gy_input_button(1, 0); v_scene();   /* context menu */
    CHECK(gy_get_stats().instances > 0, "no frame");
    click_at(v_scene, 600, 500);                                                      /* closed by a click outside */
    gy_input_mouse(AT_BUILD); v_scene(); gy_input_button(1, 1); v_scene(); gy_input_button(1, 0); v_scene();
    click_at(v_scene, 160, 346);                                                      /* ... and by picking "Copy" */
    CHECK(V.ctx_clicks == 1, "the context menu item was not picked (%d)", V.ctx_clicks);
    gy_popup_open("mine"); frames(v_scene, 2);
    CHECK(V.popup_seen > 0, "the app's popup did not show");
    key(v_scene, 0, GY_KEY_ESC);                                                      /* the app's popup: Esc */
    V.popup_seen = 0; frames(v_scene, 1);
    CHECK(V.popup_seen == 0, "Esc did not close the app's popup");
    strcpy(V.name, "");
    click_at(v_scene, AT_EDIT); type(v_scene, "ok");
    CHECK(!strcmp(V.name, "ok"), "after the popups, typing gave \"%s\" (the trap)", V.name);
    key(v_scene, 0, GY_KEY_ESC); key(v_scene, 0, 'D');
    CHECK(V.key_d == 1, "after the popups the app's own key did not arrive");
    gy_input_mouse(-100, -100); frames(v_scene, 2);
    return 1;
}

/* --- tiling --------------------------------------------------------------------------------------- */
static struct { int key_d, alt_left, open_log, extra, n; } T;

static void t_scene(void) {
    static const char *const names[] = { "Picker", "Preview", "Log", "Settings" };
    int i;
    gy_begin();
    for (i = 0; i < T.n; i++)
        if (gy_tile(names[i])) {
            gy_text("%s", names[i]);
            gy_button(names[i]);
        }
    if (T.open_log) { gy_tile_open("Extra"); T.open_log = 0; T.extra = 1; }   /* something came online */
    if (T.extra && gy_tile("Extra")) gy_text("came online by itself");
    if (gy_key_pressed('D')) T.key_d++;
    if (gy_key_pressed(GY_KEYCODE(GY_MOD_ALT, GY_KEY_LEFT))) T.alt_left++;
    gy_end();
}

static int tiles_open(int w, int h) {
    memset(&T, 0, sizeof T); T.n = 4;
    if (!open_at(w, h)) return 0;
    gy_input_mouse(-100, -100);
    gy_tile_close("Extra");                    /* "Extra" only comes when the app opens it */
    frames(t_scene, 3);
    return 1;
}

static int t_tile_layout(void) { return tiles_open(720, 460); }

static int t_tile_keys(void) {
    char a[512], b[512];
    if (!tiles_open(720, 460)) return 0;
    gy_tiles_save(a, sizeof a);
    key(t_scene, GY_MOD_ALT, GY_KEY_RIGHT);    /* focus moves right */
    key(t_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_LEFT);   /* and swaps that tile with the one on its left */
    gy_tiles_save(b, sizeof b);
    LOG("before %s; after %s; ", a, b);
    CHECK(strcmp(a, b), "Alt+Shift+Left changed nothing");
    key(t_scene, GY_MOD_ALT, 'F');             /* flip the split */
    key(t_scene, GY_MOD_ALT, GY_KEY_PLUS);     /* grow */
    key(t_scene, 0, 'D');
    CHECK(T.key_d == 1, "the app's D key did not arrive with tiling on");
    CHECK(T.alt_left == 0, "a tile key reached the app");
    return 1;
}

static int t_tile_zoom(void) {
    if (!tiles_open(720, 460)) return 0;
    key(t_scene, GY_MOD_ALT, 'Z');
    return 1;
}

static int t_tile_stack(void) {
    char a[512];
    if (!tiles_open(720, 460)) return 0;
    gy_tiles_save(a, sizeof a);
    LOG("layout %s; ", a);
    /* drag the "Log" title onto the middle of "Picker" */
    gy_input_mouse(30, 245); t_scene();        /* hovering Log's title (bottom-left tile) */
    gy_input_button(0, 1); t_scene();
    gy_input_mouse(90, 113); t_scene();        /* the middle of Picker */
    gy_input_mouse(90, 115); t_scene();
    gy_input_button(0, 0); t_scene();
    gy_input_mouse(-100, -100); frames(t_scene, 2);
    gy_tiles_save(a, sizeof a);
    LOG("after the drag %s; ", a);
    CHECK(strstr(a, "Picker|Log") || strstr(a, "Log|Picker"), "Log was not stacked with Picker");
    return 1;
}

static int t_tile_restore(void) {
    char a[512], b[512];
    uint64_t h1;
    if (!tiles_open(720, 460)) return 0;
    key(t_scene, GY_MOD_ALT, GY_KEY_RIGHT); key(t_scene, GY_MOD_ALT, 'F');
    gy_tiles_save(a, sizeof a);
    frames(t_scene, 2);
    h1 = frame_hash();
    gy_shutdown();
    memset(&T, 0, sizeof T); T.n = 4;
    if (!open_at(720, 460)) return 0;
    gy_tile_close("Extra");
    CHECK(gy_tiles_load(a), "the saved line did not load: %s", a);
    gy_input_mouse(-100, -100); frames(t_scene, 3);
    gy_tiles_save(b, sizeof b);
    CHECK(!strcmp(a, b), "saved %s, restored %s", a, b);
    LOG("restored %s; ", b);
    /* the focus outline may differ after a restore (it is not saved); compare without it */
    (void)h1;
    gy_shutdown();
    /* a tile the app no longer has is dropped on load */
    memset(&T, 0, sizeof T); T.n = 3;          /* no "Settings" any more */
    if (!open_at(720, 460)) return 0;
    gy_tile_close("Extra");
    gy_tiles_load(a);
    frames(t_scene, 3);
    gy_tiles_save(b, sizeof b);
    CHECK(!strstr(b, "Settings"), "a tile the app no longer has stayed: %s", b);
    return 1;
}

static int t_tile_online(void) {
    char a[512], b[512];
    if (!tiles_open(720, 460)) return 0;
    gy_tiles_save(a, sizeof a);
    T.open_log = 1; frames(t_scene, 3);        /* the app brings a tile in by itself */
    gy_tiles_save(b, sizeof b);
    LOG("%s; ", b);
    CHECK(strstr(b, "Extra"), "the app's new tile is not in the layout");
    key(t_scene, 0, 'D');
    CHECK(T.key_d == 1, "the new tile took the keys");
    return 1;
}

static int t_tile_clash(void) {
    int owned[2] = { GY_KEYCODE(GY_MOD_ALT, GY_KEY_LEFT), 'D' };
    char k[512];
    if (!tiles_open(720, 460)) return 0;
    gy_keys_owned(owned, 2);
    CHECK(strstr(gy_last_error(), "focus_left"), "no clash message: %s", gy_last_error());
    key(t_scene, GY_MOD_ALT, GY_KEY_LEFT);
    CHECK(T.alt_left == 1, "the app did not get its own Alt+Left");
    gy_tile_keys_save(k, sizeof k);
    CHECK(strstr(k, "focus_left=none"), "the clashing key is not off: %s", k);
    CHECK(gy_tile_keys_load("focus_left=Ctrl+Alt+Left;"), "rebinding failed");
    gy_tile_keys_save(k, sizeof k);
    CHECK(strstr(k, "focus_left=Ctrl+Alt+Left"), "the rebind did not take: %s", k);
    frames(t_scene, 2);                        /* the picture shows the banner */
    return 1;
}

static int t_tile_small(void) {                /* too small to split: new tiles become tabs */
    char a[512];
    if (!tiles_open(330, 240)) return 0;
    gy_tiles_save(a, sizeof a);
    LOG("%s; ", a);
    CHECK(strchr(a, '|'), "a small window did not stack the tiles as tabs");
    return 1;
}

/* --- fixes found by the stress tests (bench/stress.md) ------------------------------------------ */

/* gy_offscreen used to take every frame from the arena, which never frees: 30 resizes leaked 204 MB */
static int t_fix_offscreen_leak(void) {
    size_t used0, com0;
    int i;
    if (!open_at(640, 400)) return 0;
    gy_begin(); gy_end();
    used0 = gy__c.perm.used; com0 = gy__c.perm.committed;
    for (i = 0; i < 30; i++) { if (!gy_offscreen(400 + i * 37, 300 + i * 23)) return 0; gy_begin(); gy_end(); }
    CHECK(gy__c.perm.used == used0 && gy__c.perm.committed == com0, "30 resizes grew the arena from %zu to %zu bytes", used0, gy__c.perm.used);
    if (!gy_offscreen(256, 160)) return 0;
    gy_begin(); gy_end();
    LOG("arena unchanged over 30 resizes (%zu bytes); ", used0);
    return 1;
}

/* 32 tiles used to end up under the minimum size (down to -6 px), with the focus keys stuck */
static int many_n;
static void many_scene(void) {
    int i;
    char name[16];
    gy_begin();
    for (i = 0; i < many_n; i++) { snprintf(name, sizeof name, "T%02d", i); if (gy_tile(name)) gy_text("%s", name); }
    gy_end();
}

static int t_fix_tile_many(void) {
    int n, under = 0, tabs_max = 0, leaves = 0, f0;
    if (!open_at(720, 460)) return 0;
    gy_input_mouse(-100, -100);
    many_n = 32; frames(many_scene, 3);
    for (n = 0; n < GY__TNODES; n++) {
        gy__tnode *t = &gy__c.tn[n];
        if (!t->used || !t->leaf) continue;
        leaves++;
        if (t->r.w < GY__TILE_MIN_W || t->r.h < GY__TILE_MIN_H) under++;
        if (t->ntabs > tabs_max) tabs_max = t->ntabs;
    }
    LOG("32 tiles: %d leaves, %d under the minimum, the biggest stack %d tabs; ", leaves, under, tabs_max);
    CHECK(under == 0, "%d tiles are under the minimum size", under);
    CHECK(tabs_max <= GY__TABS, "a stack has %d tabs", tabs_max);
    f0 = gy__c.tfocus;
    key(many_scene, GY_MOD_ALT, GY_KEY_RIGHT);
    if (gy__c.tfocus == f0) key(many_scene, GY_MOD_ALT, GY_KEY_DOWN);
    CHECK(gy__c.tfocus != f0, "the focus keys do not move the focus");
    return 1;
}

/* --- F1: culling and the visible-range helper ------------------------------------------------- */
static int list_n, list_helper;
static void list_scene(void) {
    int i, f, e;
    gy_begin();
    gy_text("a long list");
    gy_scroll_begin("biglist", 300);
    if (list_helper) { gy_scroll_rows(list_n, &f, &e); for (i = f; i <= e; i++) gy_text("row %d", i + 1); }
    else for (i = 0; i < list_n; i++) gy_text("row %d", i + 1);
    gy_scroll_end();
    gy_end();
}
static int list_run(int n, int helper, int wheel, int *inst, uint64_t *hash) {
    if (!open_at(480, 360)) return 0;
    list_n = n; list_helper = helper;
    gy_input_mouse(200, 200); frames(list_scene, 2);
    if (wheel) { gy_input_wheel(-3); frames(list_scene, 2); }
    *inst = gy_get_stats().instances; *hash = frame_hash();
    gy_shutdown();
    return 1;
}
static int t_f1_cull(void) {
    int a, b, c, d;
    uint64_t ha, hb, hc, hd;
    /* culling: 100,000 rows push as many instances as 100 (only the visible ones) */
    if (!list_run(100, 0, 1, &a, &ha) || !list_run(100000, 0, 1, &b, &hb)) return 0;
    LOG("instances: 100 rows %d, 100,000 rows %d; ", a, b);
    CHECK(a == b, "100,000 rows push %d instances, 100 rows %d", b, a);
    /* the helper: the same picture as drawing every row, scrolled or not */
    if (!list_run(100000, 1, 0, &c, &hc) || !list_run(100000, 0, 0, &d, &hd)) return 0;
    CHECK(hc == hd && c == d, "gy_scroll_rows differs from drawing every row (top)");
    if (!list_run(100000, 1, 1, &c, &hc)) return 0;
    CHECK(hc == hb && c == b, "gy_scroll_rows differs from drawing every row (scrolled)");
    return list_run(100000, 1, 1, &c, &hc) && open_at(480, 360) && (list_n = 100000, list_helper = 1, gy_input_mouse(200, 200), frames(list_scene, 2), gy_input_wheel(-3), frames(list_scene, 2), 1);
}

/* F1: the corner cache gives the same bytes as working every corner pixel out, over many boxes */
static unsigned box_seed = 7;
static unsigned box_rnd(void) { box_seed = box_seed * 1664525u + 1013904223u; return box_seed >> 8; }
static void boxes_scene(void) {
    int i;
    gy_begin();
    box_seed = 7;
    for (i = 0; i < 400; i++) {
        gy_rect r;
        float rad = (float)(box_rnd() % 49) / 4.0f, bw = (float)(box_rnd() % 13) / 4.0f;   /* 0..12 and 0..3 in quarters */
        gy_color f = 0xFF000000u | (box_rnd() & 0xFFFFFF), b = 0xFF000000u | (box_rnd() & 0xFFFFFF);
        if (box_rnd() % 3 == 0) f = (f & 0x00FFFFFF) | 0x80000000u, f = (f & 0xFF000000u) | (((f >> 1) & 0x7F7F7F));  /* half see-through, premultiplied */
        r.x = (int)(box_rnd() % 560); r.y = (int)(box_rnd() % 400); r.w = 4 + (int)(box_rnd() % 120); r.h = 4 + (int)(box_rnd() % 80);
        gy_canvas_box(r, f, b, rad, bw);
    }
    gy_end();
}
static int t_f1_corner_cache(void) {
    uint64_t with, without;
    if (!open_at(640, 480)) return 0;
    gy__corner_off = 1; boxes_scene(); without = frame_hash(); gy__corner_off = 0;
    gy_shutdown();
    if (!open_at(640, 480)) return 0;
    boxes_scene(); with = frame_hash();
    LOG("400 random boxes, %d corner keys; ", gy__ncorners);
    CHECK(with == without, "the corner cache changed pixels");
    return 1;
}

/* --- old_layouts: layout lines saved today must load to the same geometry at every later step ---
   tests/golden/layouts-2026-11-22.txt holds, per case, the saved line and the geometry it loads to
   (every leaf's rect, its tabs, the active one, the focus and the zoom). The first run writes it;
   later runs compare. It is never re-baselined. */
static void geom_dump(char *out, int cap) {
    int n, k, at = 0;
    out[0] = 0;
    for (n = 0; n < GY__TNODES; n++) {
        gy__tnode *t = &gy__c.tn[n];
        if (!t->used || !t->leaf) continue;
        at += snprintf(out + at, (size_t)(cap - at), "[%d,%d,%d,%d", t->r.x, t->r.y, t->r.w, t->r.h);
        for (k = 0; k < t->ntabs && at < cap; k++) at += snprintf(out + at, (size_t)(cap - at), "%s%s", k ? "|" : " ", gy__c.tl[t->tabs[k]].name);
        if (at < cap) at += snprintf(out + at, (size_t)(cap - at), " a%d%s%s]", t->active, n == gy__c.tfocus ? " F" : "", n == gy__c.tzoom ? " Z" : "");
    }
}
static int geom_of(int many, int w, int h, const char *line, char *out, int cap) {
    if (!open_at(w, h)) return 0;
    gy_input_mouse(-100, -100);
    memset(&T, 0, sizeof T); T.n = 4;
    if (!many) gy_tile_close("Extra");
    if (!gy_tiles_load(line)) { gy_shutdown(); return 0; }
    if (many) { many_n = 32; frames(many_scene, 3); } else frames(t_scene, 3);
    geom_dump(out, cap);
    gy_shutdown();
    return 1;
}
static int t_old_layouts(void) {
    char path[MAX_PATH], line[2048], geom[8192], got[8192];
    FILE *f;
    int cases = 0, w, h, many;
    GetModuleFileNameA(NULL, path, MAX_PATH);  /* the exe is in build/: the file is in ../tests/golden */
    *strrchr(path, 92) = 0; *strrchr(path, 92) = 0;
    strcat(path, "/tests/golden/layouts-2026-11-22.txt");
    f = fopen(path, "r");
    if (!f) {                                  /* first run: make the lines with today's code and write them down */
        struct { int many, w, h, keys; } c[5] = { { 0, 720, 460, 0 }, { 0, 720, 460, 1 }, { 0, 720, 460, 2 }, { 0, 330, 240, 0 }, { 1, 720, 460, 0 } };
        int i;
        f = fopen(path, "w");
        if (!f) { LOG("could not write %s; ", path); return 0; }
        for (i = 0; i < 5; i++) {
            if (c[i].many) { if (!open_at(c[i].w, c[i].h)) return 0; gy_input_mouse(-100, -100); many_n = 32; frames(many_scene, 3); }
            else if (!tiles_open(c[i].w, c[i].h)) return 0;
            if (c[i].keys == 1) { key(t_scene, GY_MOD_ALT, GY_KEY_RIGHT); key(t_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_LEFT); key(t_scene, GY_MOD_ALT, 'F'); }
            if (c[i].keys == 2) { gy_input_mouse(30, 245); t_scene(); gy_input_button(0, 1); t_scene(); gy_input_mouse(90, 113); t_scene(); gy_input_mouse(90, 115); t_scene(); gy_input_button(0, 0); t_scene(); }
            gy_tiles_save(line, sizeof line);
            gy_shutdown();
            if (!geom_of(c[i].many, c[i].w, c[i].h, line, geom, sizeof geom)) return 0;
            fprintf(f, "%d %d %d %s\n%s\n", c[i].many, c[i].w, c[i].h, line, geom);
        }
        fclose(f);
        LOG("wrote %s (5 cases); ", path);
        return open_at(64, 64) && (gy_begin(), gy_end(), 1);
    }
    while (fscanf(f, "%d %d %d %2047s", &many, &w, &h, line) == 4) {
        if (!fgets(geom, sizeof geom, f) || !fgets(geom, sizeof geom, f)) break;
        geom[strcspn(geom, "\r\n")] = 0;
        cases++;
        if (!geom_of(many, w, h, line, got, sizeof got)) { fclose(f); LOG("case %d did not load: %s; ", cases, line); return 0; }
        if (strcmp(got, geom)) { fclose(f); LOG("case %d: %s\n  was %s\n  now %s; ", cases, line, geom, got); return 0; }
    }
    fclose(f);
    LOG("%d saved layouts load to the same geometry; ", cases);
    CHECK(cases == 5, "only %d cases in the file", cases);
    return open_at(64, 64) && (gy_begin(), gy_end(), 1);
}

/* fix: a flip, a grow/shrink key or a divider drag never puts a tile under the 160x110 minimum
   (Alt+F used to make a 108 px tile; found while freezing the layouts) */
static int tiles_worst(void) { return gy__tiles_short(); }
static int t_fix_tile_min(void) {
    int i;
    if (!tiles_open(720, 460)) return 0;
    key(t_scene, GY_MOD_ALT, GY_KEY_RIGHT);    /* the same moves that made the 108 px tile */
    key(t_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_LEFT);
    key(t_scene, GY_MOD_ALT, 'F');
    CHECK(tiles_worst() == 0, "Alt+F left a tile %d px under the minimum", tiles_worst());
    for (i = 0; i < 20; i++) key(t_scene, GY_MOD_ALT, GY_KEY_MINUS);
    CHECK(tiles_worst() == 0, "Alt+Minus left a tile %d px under the minimum", tiles_worst());
    for (i = 0; i < 20; i++) key(t_scene, GY_MOD_ALT, GY_KEY_PLUS);
    CHECK(tiles_worst() == 0, "Alt+Plus left a tile %d px under the minimum", tiles_worst());
    /* drag the middle divider far to the right: it stops where Preview would go under 160 */
    gy_input_mouse(360, 300); t_scene(); gy_input_button(0, 1); t_scene();
    gy_input_mouse(700, 300); t_scene(); gy_input_button(0, 0); t_scene();
    CHECK(tiles_worst() == 0, "a divider drag left a tile %d px under the minimum", tiles_worst());
    gy_input_mouse(-100, -100); frames(t_scene, 2);
    return 1;
}

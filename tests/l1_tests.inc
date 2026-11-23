/* L1 tests: developer locks (included by gytest.c). The scene: "Side" (a rule: the left side, 200 DIPs,
   with the lock under test), then "Main" and "Log". */

static void l1_scene(void) {
    gy_begin();
    if (gy_tile("Side")) gy_text("side panel");
    if (gy_tile("Main")) gy_text("main");
    if (gy_tile("Log")) gy_text("log");
    gy_end();
}
static int l1_open_w(int lock, float side_w, int w, int h) {
    gy_shutdown();
    if (!open_at(w, h)) return 0;
    gy_scale_set(1);
    gy_tile_rule("Side", (gy_rule){ .side = GY_SIDE_LEFT, .w = side_w, .lock = lock });
    gy_input_mouse(-100, -100);
    frames(l1_scene, 3);
    return 1;
}
static int l1_tall;
static int l1_open(int lock) { return l1_open_w(lock, 200, 900, l1_tall ? 800 : 500); }
static int l1_leaf(const char *nm) { int t = gy__tile_find(gy__hash(nm, 0x711E)); return t >= 0 ? gy__tile_leaf(t) : -1; }
static gy__ir l1_r(const char *nm) { int l = l1_leaf(nm); gy__ir z = { 0, 0, 0, 0 }; return l >= 0 ? gy__c.tn[l].r : z; }
static void l1_focus(const char *nm) { gy_tile_focus(nm); l1_scene(); }
static void l1_drag(int x0, int y0, int x1, int y1) {
    gy_input_mouse(x0, y0); l1_scene();
    gy_input_button(0, 1); l1_scene();
    gy_input_mouse((x0 + x1) / 2, (y0 + y1) / 2); l1_scene();
    gy_input_mouse(x1, y1); l1_scene(); l1_scene();
    gy_input_button(0, 0); l1_scene();
    gy_input_mouse(-100, -100); frames(l1_scene, 2);
}
/* the user's actions, each guarded by one flag */
static void a_swap(void) { l1_focus("Side"); key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_RIGHT); }
static void a_grow(void) { l1_focus("Side"); key(l1_scene, GY_MOD_ALT, GY_KEY_PLUS); }
static void a_divider(void) { gy__ir r = l1_r("Side"); int x = r.x + r.w + gy__c.rs.split / 2; l1_drag(x, r.y + r.h / 2, x + 90, r.y + r.h / 2); }
static void a_flip(void) { l1_focus("Side"); key(l1_scene, GY_MOD_ALT, 'F'); }
static void a_close(void) { l1_focus("Side"); key(l1_scene, GY_MOD_ALT, 'W'); }
static void a_stack(void) {                    /* Main's title dropped on the middle of Side */
    gy__ir m = l1_r("Main"), s = l1_r("Side");
    l1_drag(m.x + 20, m.y + gy__c.rs.title / 2, s.x + s.w / 2, s.y + s.h / 2);
}
static void a_drag_out(void) {                 /* Side's title dropped on Main (a swap) */
    gy__ir m = l1_r("Main"), s = l1_r("Side");
    l1_drag(s.x + 20, s.y + gy__c.rs.title / 2, m.x + 10, m.y + m.h - 10);
}

static int l1_case(const char *what, int flag, void (*act)(void)) {
    char a[512], b[512];
    int refused, changed;
    if (!l1_open(flag)) return 0;
    gy_tiles_save(a, sizeof a); act(); gy_tiles_save(b, sizeof b);
    refused = !strcmp(a, b) && strstr(gy__c.lock_msg, "Locked:") != NULL;
    if (!l1_open(0)) return 0;
    gy_tiles_save(a, sizeof a); act(); gy_tiles_save(b, sizeof b);
    changed = strcmp(a, b) != 0;
    if (!refused || !changed) { LOG("%s: locked %s, unlocked %s; ", what, refused ? "refused" : "NOT refused", changed ? "changed" : "NOT changed"); return 0; }
    return 1;
}
static int t_l1_flags(void) {
    int ok = 1;
    ok &= l1_case("swap key (MOVE)", GY_LOCK_MOVE, a_swap);
    ok &= l1_case("drag its title (MOVE)", GY_LOCK_MOVE, a_drag_out);
    ok &= l1_case("grow key (SIZE)", GY_LOCK_SIZE, a_grow);
    ok &= l1_case("divider drag (SIZE)", GY_LOCK_SIZE, a_divider);
    l1_tall = 1;                               /* flipped, the tiles need the height */
    ok &= l1_case("flip key (SIZE)", GY_LOCK_SIZE, a_flip);
    l1_tall = 0;
    ok &= l1_case("close key (CLOSE)", GY_LOCK_CLOSE, a_close);
    ok &= l1_case("a tab dropped in (TABS)", GY_LOCK_TABS, a_stack);
    ok &= l1_case("a tab dropped in (SPLIT)", GY_LOCK_SPLIT, a_stack);
    if (ok) LOG("8 actions: each refused (line byte-equal, \"Locked\" shown) on the locked tile, each done on the unlocked one; ");
    /* a refused tile key never reaches the app */
    if (!l1_open(GY_LOCK_MOVE)) return 0;
    l1_focus("Side");
    gy_input_key(GY_KEY_ALT, 1); gy_input_key(GY_KEY_SHIFT, 1); gy_input_key(GY_KEY_RIGHT, 1);
    gy_begin();
    if (gy_tile("Side")) {}
    if (gy_key_pressed(GY_KEY_RIGHT | (GY_MOD_ALT | GY_MOD_SHIFT) << 16)) { LOG("a refused tile key reached the app; "); ok = 0; }
    gy_end();
    gy_input_key(GY_KEY_RIGHT, 0); gy_input_key(GY_KEY_SHIFT, 0); gy_input_key(GY_KEY_ALT, 0);
    l1_scene();
    return ok;
}

/* SIZE: the width in DIPs holds while the window grows and shrinks; unlocked, it follows the window */
static int t_l1_size_resize(void) {
    int w0, w1, u0, u1;
    if (!l1_open(GY_LOCK_SIZE)) return 0;
    w0 = l1_r("Side").w;
    gy_offscreen(1300, 500); frames(l1_scene, 3); w1 = l1_r("Side").w;
    if (!l1_open(0)) return 0;
    u0 = l1_r("Side").w;
    gy_offscreen(1300, 500); frames(l1_scene, 3); u1 = l1_r("Side").w;
    if (w0 != 200 || w1 != 200) { LOG("locked width %d then %d px, want 200 both; ", w0, w1); return 0; }
    if (u0 == u1) { LOG("unlocked width did not follow the window (%d); ", u0); return 0; }
    LOG("locked 200 px at 900 and 1300 wide; unlocked %d -> %d; ", u0, u1);
    gy_offscreen(900, 500); frames(l1_scene, 2);
    return 1;
}

/* the whole layout locked: nothing the user does changes it; off again, the tile's own lock stays */
static int t_l1_layout_lock(void) {
    char a[512], b[512];
    if (!l1_open(GY_LOCK_MOVE)) return 0;
    gy_layout_lock(1);
    gy_tiles_save(a, sizeof a);
    l1_focus("Main"); key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_RIGHT);
    l1_focus("Log"); key(l1_scene, GY_MOD_ALT, GY_KEY_PLUS);
    l1_focus("Log"); key(l1_scene, GY_MOD_ALT, 'W');
    l1_focus("Main"); key(l1_scene, GY_MOD_ALT, 'F');
    gy_tiles_save(b, sizeof b);
    if (strcmp(a, b)) { LOG("with the layout locked the line changed: %s -> %s; ", a, b); return 0; }
    gy_layout_lock(0);
    a_swap(); gy_tiles_save(b, sizeof b);
    if (strcmp(a, b)) { LOG("the layout lock off took the tile's own lock with it; "); return 0; }
    l1_focus("Main"); key(l1_scene, GY_MOD_ALT, GY_KEY_PLUS); gy_tiles_save(b, sizeof b);
    if (!strcmp(a, b)) { LOG("the layout lock off: an unlocked tile still can't change; "); return 0; }
    LOG("locked layout: 4 actions refused, line byte-equal; off: the tile's own lock stays, others move; ");
    return 1;
}

/* a hostile hand-edited line: the locked tile comes back to its rule place and size */
static int t_l1_hostile_load(void) {
    gy__ir r0, r1;
    char b[512];
    if (!l1_open(GY_LOCK_MOVE | GY_LOCK_SIZE)) return 0;
    r0 = l1_r("Side");
    gy_tiles_load("H0.800([Main|Side]*1,V0.100([Log],[Side]))");
    frames(l1_scene, 3);
    r1 = l1_r("Side");
    gy_tiles_save(b, sizeof b);
    if (gy__c.tn[l1_leaf("Side")].ntabs != 1 || r1.x != r0.x || r1.y != r0.y || r1.w != r0.w || r1.h != r0.h) {
        LOG("after the hostile line Side is at %d,%d %dx%d (want %d,%d %dx%d), line %s; ", r1.x, r1.y, r1.w, r1.h, r0.x, r0.y, r0.w, r0.h, b);
        return 0;
    }
    LOG("hostile line: Side back at %d,%d %dx%d alone; line %s; ", r1.x, r1.y, r1.w, r1.h, b);
    return 1;
}

/* reset: back to the code layout, byte for byte */
static int t_l1_reset(void) {
    char c[512], b[512];
    if (!l1_open(GY_LOCK_MOVE)) return 0;
    gy_tiles_save(c, sizeof c);
    l1_focus("Main"); key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_RIGHT);
    l1_focus("Log"); key(l1_scene, GY_MOD_ALT, GY_KEY_PLUS);
    l1_focus("Log"); key(l1_scene, GY_MOD_ALT, 'W');
    gy_tiles_save(b, sizeof b);
    if (!strcmp(c, b)) { LOG("the user's changes did nothing; "); return 0; }
    key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_BACK);
    frames(l1_scene, 2);
    gy_tiles_save(b, sizeof b);
    if (strcmp(c, b)) { LOG("after reset %s, the code layout is %s; ", b, c); return 0; }
    LOG("swap, grow and close undone by Alt+Shift+Backspace: %s; ", b);
    return 1;
}

/* too small: fits at 100 %; fits by GUICy's own zoom; below 75 % the locked size gives way; grown back,
   both return exactly. Side is SIZE-locked at 400 DIPs. */
static int l1_side_dip(void) { return (int)((float)l1_r("Side").w / gy__c.scale + 0.5f); }
static int t_l1_too_small(void) {
    int ok = 1, path0;
    float z;
    if (!l1_open_w(GY_LOCK_SIZE | GY_LOCK_MOVE, 400, 1000, 500)) return 0;
    path0 = gy__c.tn[l1_leaf("Side")].parent;
    if (gy_scale() != 1 || l1_side_dip() != 400) { LOG("at 1000 wide: scale %.2f, side %d DIPs; ", (double)gy_scale(), l1_side_dip()); ok = 0; }
    gy_offscreen(700, 500); frames(l1_scene, 4);
    z = gy_scale();
    if (!(z < 1 && z >= 0.75f) || l1_side_dip() < 399 || l1_side_dip() > 401 || gy__c.giveway < 1) { LOG("at 700 wide: scale %.2f, side %d DIPs, give way %.2f; ", (double)z, l1_side_dip(), (double)gy__c.giveway); ok = 0; }
    else LOG("700 wide: zoom %.0f %%, side still 400 DIPs; ", (double)(z * 100));
    gy_offscreen(380, 300); frames(l1_scene, 4);
    if (gy_scale() != 0.75f || !(gy__c.giveway < 1) || l1_side_dip() >= 400 || gy__c.tn[l1_leaf("Side")].parent != path0) {
        LOG("at 380 wide: scale %.2f, give way %.2f, side %d DIPs; ", (double)gy_scale(), (double)gy__c.giveway, l1_side_dip()); ok = 0;
    } else LOG("380 wide: 75 %%, sizes give way (%.2f), side %d DIPs, same place; ", (double)gy__c.giveway, l1_side_dip());
    gy_offscreen(1000, 500); frames(l1_scene, 4);
    if (gy_scale() != 1 || l1_side_dip() != 400 || gy__c.giveway < 1) { LOG("grown back: scale %.2f, side %d DIPs; ", (double)gy_scale(), l1_side_dip()); ok = 0; }
    else LOG("grown back: 100 %%, side 400 DIPs; ");
    gy_offscreen(380, 300); frames(l1_scene, 4);   /* the picture: the gold note while sizes give way */
    return ok;
}

/* a locked gy_split divider: no drag, no double-click reset */
static float sp_pos;
static void sp_scene(void) { gy_begin(); gy_split("pane", GY_H | GY_SPLIT_LOCK, &sp_pos, 200); gy_text("left"); gy_pane(); gy_text("right"); gy_split_end(); gy_end(); }
static int t_l1_split_lock(void) {
    float p0;
    gy_shutdown();
    if (!open_at(600, 200)) return 0;
    gy_scale_set(1);
    sp_pos = 0; frames(sp_scene, 2); p0 = sp_pos;
    gy_input_mouse((int)p0 + 10, 60); sp_scene(); gy_input_button(0, 1); sp_scene();
    gy_input_mouse((int)p0 + 110, 60); sp_scene(); gy_input_button(0, 0); sp_scene();
    if (sp_pos != p0) { LOG("the locked divider moved %.0f -> %.0f; ", (double)p0, (double)sp_pos); return 0; }
    LOG("locked divider stayed at %.0f under a drag; ", (double)p0);
    return 1;
}

/* the lock fuzz: 10,000 random keys, drags, loads, resets and resizes; after each, the locked tile is
   where and as big as its rule says; over the run the unlocked tiles moved and resized */
static unsigned l1_rng = 12345;
static unsigned l1_rand(void) { l1_rng = l1_rng * 1103515245u + 12345u; return l1_rng >> 16; }
static int t_l1_fuzz(void) {
    static const int keys[] = { GY_KEY_LEFT, GY_KEY_RIGHT, GY_KEY_UP, GY_KEY_DOWN, GY_KEY_PLUS, GY_KEY_MINUS, 'F', 'W', 'T', 'Z' };
    static const char *const lines[] = { "H0.300([Side],[Main|Log])", "V0.500([Log],H0.700([Main],[Side]))", "[Side|Main|Log]", "H0.9([Main],[Log])", "garbage(" };
    static const char *const names[] = { "Side", "Main", "Log" };
    char before[512], now[512];
    int i, bad = 0, moved = 0, resized = 0, mw0, sx0;
    if (!l1_open(GY_LOCK_MOVE | GY_LOCK_SIZE | GY_LOCK_CLOSE)) return 0;
    sx0 = l1_r("Side").x; mw0 = l1_r("Main").w;
    for (i = 0; i < 10000 && !bad; i++) {
        unsigned r = l1_rand() % 100;
        gy_tiles_save(before, sizeof before);
        if (r < 50) { gy_tile_focus(names[l1_rand() % 3]); key(l1_scene, (l1_rand() & 1 ? GY_MOD_SHIFT : 0) | GY_MOD_ALT, keys[l1_rand() % 10]); }
        else if (r < 75) { int x0 = (int)(l1_rand() % 900), y0 = (int)(l1_rand() % 500); l1_drag(x0, y0, (int)(l1_rand() % 900), (int)(l1_rand() % 500)); }
        else if (r < 82) { gy_tiles_load(lines[l1_rand() % 5]); frames(l1_scene, 2); }
        else if (r < 86) { key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_BACK); }
        else if (r < 94) { gy_offscreen(700 + (int)(l1_rand() % 600), 400 + (int)(l1_rand() % 300)); frames(l1_scene, 3); }
        else { gy_tile_open(names[1 + l1_rand() % 2]); frames(l1_scene, 2); }
        gy_tiles_save(now, sizeof now);
        {   gy__ir s = l1_r("Side");
            int alone = l1_leaf("Main") < 0 && l1_leaf("Log") < 0;   /* alone it fills the window: tiles always do */
            if ((!alone && s.w != 200) || s.x != sx0 || gy__c.tn[l1_leaf("Side")].ntabs != 1) { LOG("event %d: Side at x %d, %d px, %d tabs (line %s, before %s); ", i, s.x, s.w, gy__c.tn[l1_leaf("Side")].ntabs, now, before); bad = 1; }
            if (l1_leaf("Main") >= 0 && l1_r("Main").w != mw0) resized = 1;
            if (l1_leaf("Main") >= 0 && l1_leaf("Log") >= 0 && l1_r("Main").x > l1_r("Log").x) moved = 1;   /* Main went right of Log */
        }
    }
    gy_offscreen(900, 500); key(l1_scene, GY_MOD_ALT | GY_MOD_SHIFT, GY_KEY_BACK); frames(l1_scene, 2);
    if (bad) return 0;
    if (!moved || !resized) { LOG("over the run the unlocked tiles never %s; ", !moved ? "moved" : "resized"); return 0; }
    LOG("10,000 events: Side always at x %d, 200 px, alone; the unlocked tiles moved and resized; ", sx0);
    return 1;
}

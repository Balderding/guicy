/* A1 gap tests (bbb_kit's port): filled polygons, ellipses (filled, outlined, dashed), cut labels.
   Included by gytest.c. */

/* one shape on its own frame; its mask is mask 0 until the next gy_begin */
static const struct gy__mask *a1_one(void (*draw)(void)) {
    gy_begin(); draw(); gy_end();
    return gy__c.nmasks ? &gy__c.masks[0] : NULL;
}
static long a1_sum(const struct gy__mask *m, int h) {
    long t = 0;
    int i;
    for (i = 0; i < m->w * h; i++) t += m->cov[i];
    return t;
}
static int a1_h(void) { return gy__c.inst[gy__c.ninst - 1].y1 - gy__c.inst[gy__c.ninst - 1].y0; }

static const float a1_square[] = { 10, 10, 50, 10, 50, 50, 10, 50 };
static const float a1_half[] = { 10.5f, 10, 50, 10, 50, 50, 10.5f, 50 };
static const float a1_star[] = { 100, 20, 124, 90, 64, 46, 136, 46, 76, 90 };   /* a pentagram: it crosses itself */
static const float a1_ell[] = { 0 };
static void a1_d_square(void) { gy_canvas_poly(a1_square, 4, GY_RGB(255, 255, 255)); }
static void a1_d_half(void) { gy_canvas_poly(a1_half, 4, GY_RGB(255, 255, 255)); }
static void a1_d_star(void) { gy_canvas_poly(a1_star, 5, GY_RGB(255, 255, 255)); }
static void a1_d_ell(void) { (void)a1_ell; gy_canvas_ellipse(100, 80, 60, 40, GY_RGB(255, 255, 255), 0, 0, 0); }
static void a1_d_ring(void) { gy_canvas_ellipse(100, 80, 60, 40, 0, GY_RGB(255, 255, 255), 4, 0); }
static void a1_d_dash(void) { gy_canvas_ellipse(100, 80, 60, 40, 0, GY_RGB(255, 255, 255), 4, 10); }

static int t_a1_shapes_exact(void) {
    const struct gy__mask *m;
    long sum;
    int ok = 1, i;
    double area, want;
    gy_shutdown();
    if (!open_at(260, 180)) return 0;
    gy_scale_set(1);
    /* a whole-pixel square: every pixel full, exactly */
    m = a1_one(a1_d_square);
    if (!m || m->w != 40 || a1_h() != 40 || a1_sum(m, 40) != 40L * 40 * 255) { LOG("the 40x40 square is not exactly full; "); ok = 0; }
    /* half a pixel in: the first column is half covered (128), the rest full */
    m = a1_one(a1_d_half);
    if (!m || m->w != 40) { LOG("no mask for the half square; "); return 0; }
    for (i = 0; i < 40; i++) if (m->cov[i * 40] != 128 || m->cov[i * 40 + 1] != 255) { LOG("half-pixel edge: row %d gives %d,%d, want 128,255; ", i, m->cov[i * 40], m->cov[i * 40 + 1]); ok = 0; break; }
    /* the pentagram: its middle is wound twice; non-zero fills it (even-odd would leave it empty) */
    m = a1_one(a1_d_star);
    if (!m) return 0;
    {   int bx = gy__c.inst[gy__c.ninst - 1].x0, by = gy__c.inst[gy__c.ninst - 1].y0, c = m->cov[(62 - by) * m->w + (100 - bx)];
        if (c != 255) { LOG("the pentagram's middle has coverage %d, want 255 (non-zero); ", c); ok = 0; }
    }
    /* an ellipse: its covered area is pi rx ry within 0.2 % */
    m = a1_one(a1_d_ell);
    if (!m) return 0;
    sum = a1_sum(m, a1_h()); area = (double)sum / 255.0; want = 3.14159265 * 60 * 40;
    if (area < want * 0.998 || area > want * 1.002) { LOG("ellipse area %.1f, want %.1f; ", area, want); ok = 0; }
    else LOG("ellipse area %.1f of %.1f; ", area, want);
    /* the ring: its area is the outer minus the inner, its middle empty */
    m = a1_one(a1_d_ring);
    if (!m) return 0;
    area = (double)a1_sum(m, a1_h()) / 255.0; want = 3.14159265 * (60.0 * 40 - 56.0 * 36);
    if (area < want * 0.99 || area > want * 1.01) { LOG("ring area %.1f, want %.1f; ", area, want); ok = 0; }
    {   int bx = gy__c.inst[gy__c.ninst - 1].x0, by = gy__c.inst[gy__c.ninst - 1].y0;
        if (m->cov[(80 - by) * m->w + (100 - bx)] != 0) { LOG("the ring's middle is not empty; "); ok = 0; }
    }
    /* dashes: about half the ring */
    m = a1_one(a1_d_dash);
    if (!m) return 0;
    {   double d = (double)a1_sum(m, a1_h()) / 255.0;
        if (d < want * 0.42 || d > want * 0.58) { LOG("dashed ring area %.1f, want about %.1f; ", d, want / 2); ok = 0; }
        else LOG("dashes %.0f%% of the ring; ", d * 100 / want);
    }
    if (ok) LOG("square exact, half-pixel edge 128, pentagram middle full (non-zero), areas right; ");
    return ok;
}

/* for eyes, and the three kernels: every shape at 125 % */
static const float a1_l[] = { 160, 20, 230, 20, 230, 45, 185, 45, 185, 110, 160, 110 };   /* concave: an L */
static const float a1_bow[] = { 20, 120, 80, 160, 80, 120, 20, 160 };                     /* crosses itself: a bow tie */
static const float a1_touch[] = { 100, 120, 140, 120, 120, 140, 140, 160, 100, 160, 120, 140 };   /* two corners touch */
static void a1_shapes_scene(void) {
    gy_begin();
    gy_canvas_poly(a1_star, 5, gy_theme_get()->accent);
    gy_canvas_poly(a1_l, 6, gy_theme_get()->good);
    gy_canvas_poly(a1_bow, 4, gy_theme_get()->warn);
    gy_canvas_poly(a1_touch, 6, gy_theme_get()->text);
    gy_canvas_ellipse(40, 60, 30, 20, gy_theme_get()->accent, 0, 0, 0);
    gy_canvas_ellipse(270, 60, 30, 40, 0, gy_theme_get()->text, 2, 0);
    gy_canvas_ellipse(270, 140, 40, 25, 0, gy_theme_get()->warn, 2, 6);
    gy_canvas_ellipse(200, 140, 25, 25, gy_theme_get()->field, gy_theme_get()->good, 3, 0);
    gy_end();
}
static int t_a1_shapes(void) {
    gy_shutdown();
    if (!open_at(400, 225)) return 0;
    gy_scale_set(1.25f);
    a1_shapes_scene(); a1_shapes_scene();
    return 1;
}

/* a shape that moves: the dirty-tile frame equals a full redraw */
static float a1_dx;
static void a1_move_scene(void) {
    float p[10];
    int i;
    for (i = 0; i < 10; i++) p[i] = a1_star[i] + (i % 2 ? 0 : a1_dx);
    gy_begin();
    gy_canvas_poly(p, 5, gy_theme_get()->accent);
    gy_canvas_ellipse(60 + a1_dx, 140, 30, 20, 0, gy_theme_get()->warn, 2, 6);
    gy_end();
}
static int t_a1_shapes_dirty(void) {
    uint32_t *keep;
    int n, same;
    gy_shutdown();
    if (!open_at(300, 200)) return 0;
    a1_dx = 0; a1_move_scene(); a1_move_scene();
    a1_dx = 37.3f; a1_move_scene();
    n = gy__c.w * gy__c.h;
    keep = (uint32_t *)malloc((size_t)n * 4);
    if (!keep) return 0;
    memcpy(keep, gy__c.px, (size_t)n * 4);
    gy_shutdown();
    if (!open_at(300, 200)) { free(keep); return 0; }
    a1_move_scene();                           /* the same frame, drawn whole */
    same = !memcmp(keep, gy__c.px, (size_t)n * 4);
    free(keep);
    if (!same) { LOG("after a move the dirty-tile frame differs from a full redraw; "); return 0; }
    LOG("moved shapes: the dirty-tile frame equals a full redraw; ");
    return 1;
}

/* --- cut labels: "..." when too long, the whole text as the tip on hover, the app's tip wins ---- */
static int a1_own_tip;
static void a1_cut_scene(void) {
    static const float w1[1] = { 130 };
    int on = 1;
    gy_begin();
    gy_row(1, w1);
    gy_button("Open the folder with every base in it");
    if (a1_own_tip) gy_tip("my own tip");
    gy_check("Build the overhangs as separate parts", &on);
    gy_select("Highland ruins with a long name", 0);
    gy_button("Fits");
    gy_end();
}
static int a1_text_is(const char *want) {
    const gy_test_text *t;
    int i, n = gy_test_texts(&t);
    for (i = 0; i < n; i++) if (!strcmp(t[i].s, want)) return 1;
    return 0;
}
static int t_a1_cut(void) {
    const gy_test_text *t;
    int i, n, cuts = 0, ok = 1;
    gy_shutdown();
    if (!open_at(300, 180)) return 0;
    a1_own_tip = 0;
    gy_input_mouse(-100, -100); a1_cut_scene(); a1_cut_scene();
    n = gy_test_texts(&t);
    for (i = 0; i < n; i++) if (strstr(t[i].s, "\xe2\x80\xa6")) cuts++;
    if (cuts != 3) { LOG("%d labels cut, want 3; ", cuts); ok = 0; }
    if (!a1_text_is("Fits")) { LOG("a label that fits was changed; "); ok = 0; }
    /* hover the cut button, wait: its whole text is the tip */
    gy_input_mouse(40, 22); a1_cut_scene();
    g_now += 600; gy_input_time(g_now); a1_cut_scene(); a1_cut_scene();
    if (!a1_text_is("Open the folder with every base in it")) { LOG("no tip with the whole label; "); ok = 0; }
    /* the app's own tip for that widget wins */
    a1_own_tip = 1;
    a1_cut_scene(); a1_cut_scene();
    if (!a1_text_is("my own tip") || a1_text_is("Open the folder with every base in it")) { LOG("the app's own tip did not win; "); ok = 0; }
    a1_own_tip = 0;
    gy_input_mouse(40, 22); a1_cut_scene(); a1_cut_scene();
    if (ok) LOG("3 cut with an ellipsis, Fits untouched, the whole text as the tip, the app's tip wins; ");
    return ok;
}

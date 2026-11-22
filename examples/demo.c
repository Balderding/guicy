/* demo - a small GUICy window. It sleeps until there is input, and redraws only what changed.
   Used by tools/baseline.py to measure GUICy the same way as the Nuklear apps (hidden desktop). */
#define GUICY_IMPLEMENTATION
#include "../guicy.h"

int main(void) {
    static const float three[3] = { 160, 160, -1 }, two[2] = { 0.5f, -1 };
    int clicks = 0, pixel = 0, tex = 1, size = 1, i;
    if (!gy_open(&(gy_desc){ .title = "GUICy demo", .w = 1540, .h = 950, .min_w = 640, .min_h = 400 })) return 1;
    while (gy_frame()) {
        gy_text("GUICy %s, kernels: %s.  Clicks: %d", GUICY_VERSION, gy_isa_name(), clicks);
        gy_row(3, three);
        if (gy_button("Build")) clicks++;
        if (gy_button("Stop")) clicks = 0;
        gy_button("Open folder");
        gy_row(2, two);
        gy_check("Pixel look", &pixel);
        gy_check("Textures", &tex);
        gy_row(3, three);
        if (gy_select("Small", size == 0)) size = 0;
        if (gy_select("Medium", size == 1)) size = 1;
        if (gy_select("Large", size == 2)) size = 2;
        gy_row(0, NULL);
        for (i = 0; i < 20; i++) gy_text("Line %d of a longer panel of text, to give the renderer something to skip.", i + 1);
    }
    gy_shutdown();
    return 0;
}

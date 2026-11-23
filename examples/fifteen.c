/* fifteen.c - the 15-line example from PLAN.md, written against the real API */
#define GUICY_IMPLEMENTATION
#include "guicy.h"

int main(void) {
    int count = 3, pixel = 1; float size = 32, split = 280; char name[64] = "base";
    if (!gy_open(&(gy_desc){ .title = "Demo", .w = 900, .h = 600, .min_w = 640, .min_h = 400 })) return 1;
    while (gy_frame()) {                                   /* sleeps until something happens */
        gy_split("main", GY_H, &split, 280);               /* drag; double-click resets to 280 */
            gy_text("Bases: %d", count);
            if (gy_button_key("Build", 'B')) count++;      /* the B key does it too */
            gy_check("Pixel look", &pixel); gy_slider("Size", &size, 8, 512);
            gy_edit("Name", name, sizeof name); gy_tip("Saved with the model");
        gy_pane(); gy_canvas_rect(gy_avail(), gy_theme_get()->card, 6);
        gy_split_end();
    }
    return 0;
}

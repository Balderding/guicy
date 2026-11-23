/* hello.c - the smallest GUICy program. Build: clang -O2 hello.c -lgdi32 -luser32 (or gcc). */
#define GUICY_IMPLEMENTATION
#include "guicy.h"

int main(void) {
    int clicks = 0;
    if (!gy_open(&(gy_desc){ .title = "Hello", .w = 320, .h = 120 })) return 1;
    while (gy_frame()) {
        gy_text("Clicked %d times", clicks);
        if (gy_button("Click me")) clicks++;
    }
    gy_shutdown();
    return 0;
}

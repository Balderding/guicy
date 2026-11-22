/* guicy.h - GUICy, an immediate-mode GUI library in plain C.  https://github.com/Balderding/guicy (private)

   Use it in ONE .c file of your program:
       #define GUICY_IMPLEMENTATION
       #include "guicy.h"
   and include it without the define anywhere else.

   Options (define before the include, in the implementation file):
       GUICY_USE_CRT   use the C library's memset/memcpy instead of GUICy's own (for bisecting bugs)

   Pixels are premultiplied BGRA8 in a uint32_t: 0xAARRGGBB, so the bytes in memory are B, G, R, A.
   Colours are in sRGB space; blending is not linear (it matches what GDI shows).

   Status: step 1 of PLAN.md (the skeleton): context, arena, off-screen frame, clear, BMP shot.
*/
#ifndef GUICY_H
#define GUICY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GUICY_VERSION "0.1.0"

typedef uint32_t gy_color;                     /* premultiplied 0xAARRGGBB */
#define GY_RGB(r, g, b) ((gy_color)(0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

/* The theme: a flat table of named colours. No default colour is red (the user can't read red text). */
typedef struct gy_theme {
    gy_color bg;          /* the window behind everything */
    gy_color panel;       /* a panel or tile body */
    gy_color card;        /* a raised area inside a panel */
    gy_color border;      /* thin lines around things */
    gy_color text;        /* normal text */
    gy_color text_dim;    /* secondary text */
    gy_color text_off;    /* disabled text */
    gy_color heading;     /* section headings (gold) */
    gy_color button;      /* a button at rest */
    gy_color button_hot;  /* under the mouse */
    gy_color button_down; /* pressed */
    gy_color accent;      /* the chosen / primary thing (teal) */
    gy_color accent_hot;
    gy_color accent_text; /* text on the accent */
    gy_color field;       /* edit boxes, slider tracks */
    gy_color field_hot;
    gy_color select;      /* selected text background */
    gy_color good;        /* green: done, OK */
    gy_color warn;        /* gold: needs a look */
    gy_color bad;         /* problems: navy-violet, never red */
    gy_color divider;     /* splitter lines */
    gy_color divider_hot;
    gy_color tip;         /* tooltip background */
    gy_color shadow;      /* drop shadows (with alpha) */
} gy_theme;

/* --- the frame -------------------------------------------------------------------------------- */
int  gy_offscreen(int w, int h);              /* draw into memory, no window; 1 on success */
void gy_clear(void);                          /* fill the frame with the theme's bg */
int  gy_width(void);
int  gy_height(void);
const uint32_t *gy_pixels(void);              /* the frame, w*h, rows top to bottom */
int  gy_shot(const char *path);               /* write the frame as a 32-bit BMP; 1 on success */
void gy_shutdown(void);

/* --- theme -------------------------------------------------------------------------------------- */
const gy_theme *gy_theme_get(void);
void gy_theme_set(const gy_theme *t);

/* --- errors: text for the app's window and the log, never a message box ------------------------- */
const char *gy_last_error(void);              /* "" when there is none */

#ifdef __cplusplus
}
#endif
#endif /* GUICY_H */

/* ============================================================================================== */
#ifdef GUICY_IMPLEMENTATION
#ifndef GUICY_IMPLEMENTED
#define GUICY_IMPLEMENTED

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* --- no C library inside GUICy: its own small helpers ----------------------------------------- */
/* internal helpers some builds don't call yet */
#define GY__FN static __attribute__((unused))

#ifdef GUICY_USE_CRT
#include <string.h>
#define gy__memset memset
#define gy__memcpy memcpy
#else
__attribute__((no_builtin("memset"))) GY__FN void *gy__memset(void *d, int c, size_t n) {
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
__attribute__((no_builtin("memcpy"))) GY__FN void *gy__memcpy(void *d, const void *s, size_t n) {
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}
#endif

/* --- the arena: one reserved range, committed as it grows, never moved, never drops anything ---- */
typedef struct gy__arena {
    unsigned char *base;
    size_t reserved, committed, used;
} gy__arena;

#define GY__RESERVE ((size_t)1 << 30)          /* 1 GB of address space; only what is used is committed */
#define GY__COMMIT_STEP ((size_t)1 << 20)

static int gy__arena_init(gy__arena *a) {
    a->base = (unsigned char *)VirtualAlloc(NULL, GY__RESERVE, MEM_RESERVE, PAGE_NOACCESS);
    a->reserved = a->base ? GY__RESERVE : 0;
    a->committed = a->used = 0;
    return a->base != NULL;
}

static void *gy__arena_push(gy__arena *a, size_t n) {
    size_t at = (a->used + 63) & ~(size_t)63;  /* 64-byte aligned: whole cache lines, room for AVX */
    if (at + n > a->reserved) return NULL;
    if (at + n > a->committed) {
        size_t want = ((at + n - a->committed) + GY__COMMIT_STEP - 1) & ~(GY__COMMIT_STEP - 1);
        if (!VirtualAlloc(a->base + a->committed, want, MEM_COMMIT, PAGE_READWRITE)) return NULL;
        a->committed += want;
    }
    a->used = at + n;
    return a->base + at;
}

/* --- the context: one, implicit ---------------------------------------------------------------- */
typedef struct gy__ctx {
    int ready;
    gy__arena perm;                            /* lives as long as the context */
    int w, h;
    uint32_t *px;                              /* the off-screen frame */
    gy_theme theme;
    char error[512];
} gy__ctx;

static gy__ctx gy__c;

static const gy_theme gy__theme_dark = {
    /* bg          panel         card          border        text          text_dim      text_off */
    GY_RGB(30,31,34), GY_RGB(40,41,45), GY_RGB(50,52,57), GY_RGB(64,66,72), GY_RGB(224,226,230), GY_RGB(160,164,172), GY_RGB(100,103,110),
    /* heading     button        button_hot    button_down */
    GY_RGB(230,190,90), GY_RGB(56,58,64), GY_RGB(70,73,80), GY_RGB(44,46,51),
    /* accent      accent_hot    accent_text */
    GY_RGB(0,150,150), GY_RGB(0,175,175), GY_RGB(255,255,255),
    /* field       field_hot     select        good          warn          bad */
    GY_RGB(24,25,28), GY_RGB(32,33,37), GY_RGB(0,95,110), GY_RGB(90,190,110), GY_RGB(230,190,90), GY_RGB(120,120,220),
    /* divider     divider_hot   tip           shadow (black, 40 % alpha, premultiplied) */
    GY_RGB(52,54,59), GY_RGB(0,150,150), GY_RGB(58,60,66), 0x66000000u,
};

static void gy__set_error(const char *what, const char *detail) {
    size_t n = 0, i;
    for (i = 0; what[i] && n < sizeof gy__c.error - 1; i++) gy__c.error[n++] = what[i];
    if (detail) {
        if (n < sizeof gy__c.error - 3) { gy__c.error[n++] = ':'; gy__c.error[n++] = ' '; }
        for (i = 0; detail[i] && n < sizeof gy__c.error - 1; i++) gy__c.error[n++] = detail[i];
    }
    gy__c.error[n] = 0;
}

static int gy__init(void) {
    if (gy__c.ready) return 1;
    gy__memset(&gy__c, 0, sizeof gy__c);
    if (!gy__arena_init(&gy__c.perm)) { gy__set_error("GUICy could not reserve its memory", NULL); return 0; }
    gy__c.theme = gy__theme_dark;
    gy__c.ready = 1;
    return 1;
}

int gy_offscreen(int w, int h) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { gy__set_error("gy_offscreen", "the size must be 1..16384"); return 0; }
    if (!gy__init()) return 0;
    gy__c.px = (uint32_t *)gy__arena_push(&gy__c.perm, (size_t)w * (size_t)h * 4);
    if (!gy__c.px) { gy__set_error("gy_offscreen", "out of memory for the frame"); return 0; }
    gy__c.w = w; gy__c.h = h;
    gy_clear();
    return 1;
}

void gy_clear(void) {
    size_t i, n = (size_t)gy__c.w * (size_t)gy__c.h;
    gy_color c = gy__c.theme.bg;
    for (i = 0; i < n; i++) gy__c.px[i] = c;
}

int gy_width(void) { return gy__c.w; }
int gy_height(void) { return gy__c.h; }
const uint32_t *gy_pixels(void) { return gy__c.px; }
const gy_theme *gy_theme_get(void) { gy__init(); return &gy__c.theme; }
void gy_theme_set(const gy_theme *t) { if (gy__init()) gy__c.theme = *t; }
const char *gy_last_error(void) { return gy__c.error; }

static void gy__put32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

int gy_shot(const char *path) {
    unsigned char hdr[54];
    uint32_t img = (uint32_t)gy__c.w * (uint32_t)gy__c.h * 4;
    HANDLE f;
    DWORD wrote = 0;
    BOOL ok;
    if (!gy__c.px) { gy__set_error("gy_shot", "there is no frame yet"); return 0; }
    gy__memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    gy__put32(hdr + 2, 54 + img);
    gy__put32(hdr + 10, 54);
    gy__put32(hdr + 14, 40);                   /* BITMAPINFOHEADER */
    gy__put32(hdr + 18, (uint32_t)gy__c.w);
    gy__put32(hdr + 22, (uint32_t)-gy__c.h);   /* negative height: rows top to bottom, as in memory */
    hdr[26] = 1; hdr[28] = 32;                 /* 1 plane, 32 bits per pixel, BI_RGB */
    gy__put32(hdr + 34, img);
    gy__put32(hdr + 38, 2835); gy__put32(hdr + 42, 2835);  /* 72 dpi */
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { gy__set_error("gy_shot could not create the file", path); return 0; }
    ok = WriteFile(f, hdr, sizeof hdr, &wrote, NULL) && wrote == sizeof hdr
      && WriteFile(f, gy__c.px, img, &wrote, NULL) && wrote == img;
    CloseHandle(f);
    if (!ok) { gy__set_error("gy_shot could not write the file", path); return 0; }
    return 1;
}

void gy_shutdown(void) {
    if (gy__c.perm.base) VirtualFree(gy__c.perm.base, 0, MEM_RELEASE);
    gy__memset(&gy__c, 0, sizeof gy__c);
}

#endif /* GUICY_IMPLEMENTED */
#endif /* GUICY_IMPLEMENTATION */

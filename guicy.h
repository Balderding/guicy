/* guicy.h - GUICy, an immediate-mode GUI library in plain C.  https://github.com/Balderding/guicy (private)

   Use it in ONE .c file of your program:
       #define GUICY_IMPLEMENTATION
       #include "guicy.h"
   and include it without the define anywhere else.

   Options (define before the include, in the implementation file):
       GUICY_USE_CRT   use the C library's memset/memcpy instead of GUICy's own (for bisecting bugs)

   Pixels are premultiplied BGRA8 in a uint32_t: 0xAARRGGBB, so the bytes in memory are B, G, R, A.
   Colours are in sRGB space; blending is not linear (it matches what GDI shows).

   A frame:
       gy_begin();                     input since the last frame is applied
       gy_row(2, (float[]){ 120, -1 }); one row: 120 px, then the rest
       if (gy_button("Save")) save();
       gy_check("Pixel look", &pixel);
       gy_end();                       the frame is drawn

   Status: step 3 of PLAN.md: core, row layout, draw list, CPU renderer (scalar/SSE2/AVX2),
   GDI glyph atlas, widgets text/button/check/select/disable, a window that sleeps until input,
   and 64x64 dirty tiles (only changed tiles are drawn and copied to the screen).
*/
#ifndef GUICY_H
#define GUICY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GUICY_VERSION "0.3.0"

typedef uint32_t gy_color;                     /* premultiplied 0xAARRGGBB */
#define GY_RGB(r, g, b) ((gy_color)(0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

typedef struct gy_rect { int x, y, w, h; } gy_rect;

/* The theme: named colours and a few sizes. No default colour is red (the user can't read red text). */
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
    gy_color field;       /* edit boxes, slider tracks, check boxes */
    gy_color field_hot;
    gy_color select;      /* selected text background */
    gy_color good;        /* green: done, OK */
    gy_color warn;        /* gold: needs a look */
    gy_color bad;         /* problems: violet, never red */
    gy_color divider;     /* splitter lines */
    gy_color divider_hot;
    gy_color tip;         /* tooltip background */
    gy_color shadow;      /* drop shadows (with alpha) */
    int pad;              /* space inside the frame or panel edge, px */
    int gap;              /* space between widgets, px */
    int row_h;            /* the height of one row of widgets, px */
    int radius;           /* corner radius of buttons and fields, px */
    int font_px;          /* the default font's height, px */
} gy_theme;

/* --- the frame -------------------------------------------------------------------------------- */
int  gy_offscreen(int w, int h);              /* draw into memory, no window; 1 on success */
void gy_begin(void);                          /* start a frame */
void gy_end(void);                            /* lay out and draw the frame */
void gy_clear(void);                          /* fill the frame with the theme's bg */
int  gy_width(void);
int  gy_height(void);
const uint32_t *gy_pixels(void);              /* the frame, w*h, rows top to bottom */
int  gy_shot(const char *path);               /* write the frame as a 32-bit BMP; 1 on success */
void gy_shutdown(void);

/* --- a window ------------------------------------------------------------------------------------ */
typedef struct gy_desc {
    const char *title;                        /* UTF-8 */
    int w, h;                                 /* the client size at start */
    int min_w, min_h;                         /* the smallest the window may be dragged to */
} gy_desc;
int  gy_open(const gy_desc *d);               /* make the window; 1 on success */
int  gy_frame(void);                          /* draw the last frame, sleep until something happens,
                                                 start the next; 0 when the window was closed */
void gy_animate(int ms);                      /* ask for a frame in ms even with no input */
void gy_wake(void);                           /* from any thread: run a frame soon */
void gy_tiles(int on);                        /* 0: redraw every pixel every frame (to test the tiles) */

/* --- input (a window feeds these itself; tests and scripts call them) ------------------------- */
void gy_input_mouse(int x, int y);
void gy_input_button(int button, int down);   /* 0 left, 1 right, 2 middle */

/* --- layout ------------------------------------------------------------------------------------- */
/* The next widgets go into n columns. A width > 1 is pixels, 0 < w <= 1 a share of the row,
   w <= 0 an equal part of what is left. gy_row(0, NULL) or no call at all: one full-width column. */
void gy_row(int n, const float *widths);
gy_rect gy_last_rect(void);                   /* where the last widget went */

/* --- widgets ------------------------------------------------------------------------------------ */
void gy_text(const char *fmt, ...);           /* %d %i %u %x %c %s %f %.Nf %% */
void gy_text_col(gy_color c, const char *fmt, ...);
int  gy_button(const char *label);            /* 1 on the frame it was clicked */
int  gy_check(const char *label, int *on);    /* 1 when it changed *on */
int  gy_select(const char *label, int on);    /* drawn chosen when on; 1 when clicked */
void gy_disable_begin(void);                  /* the widgets until _end are greyed and ignore input */
void gy_disable_end(void);
void gy_id_push(int i);                       /* labels repeat in loops: push the index */
void gy_id_pop(void);
/* A label's "##suffix" is part of its ID but not shown: "Save##top" and "Save##bottom". */

/* --- drawing (the canvas) ---------------------------------------------------------------------- */
void gy_draw_box(gy_rect r, gy_color fill, gy_color border, float radius, float border_w);
void gy_draw_text(int x, int y, const char *s, gy_color c);   /* x, y = the top-left of the line */
int  gy_text_width(const char *s);
int  gy_line_height(void);

/* --- fonts ------------------------------------------------------------------------------------- */
int  gy_font_add(const char *face, int px, int bold);   /* returns its index; 0 is the theme's font */
void gy_font(int i);                                  /* text from here on uses font i */

/* --- the CPU kernels ----------------------------------------------------------------------------- */
enum { GY_ISA_SCALAR, GY_ISA_SSE2, GY_ISA_AVX2 };
int  gy_isa_set(int isa);                     /* returns the one in use (lower if the CPU can't) */
const char *gy_isa_name(void);

typedef struct gy_stats {
    int instances, glyph_misses, fonts, atlas_pages;
    int tiles, dirty_tiles;                   /* this frame: all tiles, tiles drawn again */
    int frames, frames_drawn;                 /* since start: frames, frames that changed a pixel */
} gy_stats;
gy_stats gy_get_stats(void);

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
#include <stdarg.h>
#include <immintrin.h>
#include <cpuid.h>

/* internal helpers some builds don't call yet */
#define GY__FN static __attribute__((unused))

/* --- no C library inside GUICy: its own small helpers ----------------------------------------- */
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

static int gy__mini(int a, int b) { return a < b ? a : b; }
static int gy__maxi(int a, int b) { return a > b ? a : b; }
static float gy__minf(float a, float b) { return a < b ? a : b; }
static float gy__maxf(float a, float b) { return a > b ? a : b; }
static float gy__absf(float a) { return a < 0 ? -a : a; }
static float gy__sqrtf(float a) { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(a))); }

/* --- the arena: one reserved range, committed as it grows, never moved, never drops anything ---- */
typedef struct gy__arena {
    unsigned char *base;
    size_t reserved, committed, used;
} gy__arena;

#define GY__COMMIT_STEP ((size_t)1 << 20)

static int gy__arena_init(gy__arena *a, size_t reserve) {
    a->base = (unsigned char *)VirtualAlloc(NULL, reserve, MEM_RESERVE, PAGE_NOACCESS);
    a->reserved = a->base ? reserve : 0;
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

/* --- the draw list: one 48-byte instance for everything that is drawn --------------------------- */
enum { GY__K_BOX = 1, GY__K_GLYPH = 2 };

typedef struct gy__inst {
    int16_t x0, y0, x1, y1;                    /* pixel-snapped rect, x1/y1 exclusive */
    uint16_t u, v;                             /* atlas position of a glyph or icon */
    uint16_t clip;                             /* index into the clip table */
    uint8_t kind, page;                        /* GY__K_*, atlas page */
    gy_color col0, col1;                       /* fill (or glyph colour), border */
    float radius, border, soft;                /* corner radius, border width, edge softness */
    uint32_t reserved[3];
} gy__inst;
typedef char gy__inst_is_48_bytes[sizeof(gy__inst) == 48 ? 1 : -1];

/* --- fonts and the glyph atlas ------------------------------------------------------------------ */
#define GY__MAX_FONTS 16
#define GY__ATLAS 1024                         /* one page is 1024 x 1024 coverage bytes */
#define GY__MAX_PAGES 8
#define GY__GLYPH_SLOTS 8192                   /* hash table size, power of two */
#define GY__ICON_FONT 31                       /* font number used for baked icons */

typedef struct gy__glyph {
    uint32_t key;                              /* font << 21 | codepoint; 0 = empty slot */
    uint16_t u, v, w, h;
    int16_t ox, oy;                            /* from the pen: left, and up from the baseline */
    int16_t adv;
    uint8_t page, ok;
} gy__glyph;

typedef struct gy__font {
    HFONT hf;
    int px, ascent, descent, height;
    int ascii[128];                            /* glyph slot + 1 for codepoints < 128, 0 = not yet */
} gy__font;

/* --- the context: one, implicit ---------------------------------------------------------------- */
typedef struct gy__ctx {
    int ready;
    gy__arena perm;                            /* lives as long as the context */
    gy__arena frame;                           /* emptied at every gy_begin */
    int w, h;
    uint32_t *px;                              /* the frame */
    gy_theme theme;
    char error[512];

    /* draw list */
    gy__inst *inst;
    int ninst, cap_inst;
    gy_rect clips[64];
    int nclips;

    /* input */
    int mx, my;
    int down[3], pressed[3], released[3];

    /* ids and widget state */
    uint32_t id_stack[32];
    int id_depth;
    uint32_t active;                           /* the widget the mouse is pressing */
    int disabled;

    /* row layout */
    gy_rect area;                              /* where widgets go */
    int cur_y;                                 /* the top of the current row */
    int ncols, col;
    float widths[16];
    int col_x[17];
    gy_rect last;

    /* text */
    HDC dc;
    gy__font fonts[GY__MAX_FONTS];
    int nfonts, font;
    gy__glyph *glyphs;
    unsigned char *pages[GY__MAX_PAGES];
    int npages, shelf_x, shelf_y, shelf_h;
    unsigned char *scratch;                    /* GetGlyphOutline output */
    int glyph_misses;

    /* dirty tiles: 64 x 64, each with the hash of what is drawn on it */
    int tiles_off, tiles_x, tiles_y, want_full;
    uint64_t *tile_prev, *tile_cur;
    unsigned char *tile_dirty;
    int dirty_tiles, frames, frames_drawn;

    /* the window */
    HWND hwnd;
    HBITMAP dib;
    HDC memdc;
    int closed, passes, in_frame, resize_w, resize_h, min_w, min_h, tracking;
    DWORD anim_at;
    int anim_on;

    /* kernels */
    int isa;
    void (*k_fill)(uint32_t *d, int n, uint32_t s);
    void (*k_row)(uint32_t *d, const uint32_t *s, int n);
    void (*k_glyph)(uint32_t *d, const unsigned char *cov, int n, uint32_t col);
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
    /* pad gap row_h radius font_px */
    8, 6, 28, 4, 15,
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

/* ================================================================================================
   The pixel kernels. All three versions give the same bytes: integer maths, exact rounded /255.
   ================================================================================================ */

static uint32_t gy__div255(uint32_t x) { x += 128; return (x + (x >> 8)) >> 8; }

static uint32_t gy__scale(uint32_t c, uint32_t k) {   /* every channel * k / 255, rounded */
    return gy__div255((c & 255) * k) | gy__div255(((c >> 8) & 255) * k) << 8
         | gy__div255(((c >> 16) & 255) * k) << 16 | gy__div255((c >> 24) * k) << 24;
}

static uint32_t gy__over(uint32_t d, uint32_t s) {     /* premultiplied s over d */
    uint32_t ia = 255 - (s >> 24);
    return ((s & 255) + gy__div255((d & 255) * ia))
         | (((s >> 8) & 255) + gy__div255(((d >> 8) & 255) * ia)) << 8
         | (((s >> 16) & 255) + gy__div255(((d >> 16) & 255) * ia)) << 16
         | ((s >> 24) + gy__div255((d >> 24) * ia)) << 24;
}

/* scalar */
static void gy__fill_scalar(uint32_t *d, int n, uint32_t s) {
    int i;
    if ((s >> 24) == 255) { for (i = 0; i < n; i++) d[i] = s; return; }
    if (!s) return;
    for (i = 0; i < n; i++) d[i] = gy__over(d[i], s);
}
static void gy__row_scalar(uint32_t *d, const uint32_t *s, int n) {
    int i;
    for (i = 0; i < n; i++) d[i] = gy__over(d[i], s[i]);
}
static void gy__glyph_scalar(uint32_t *d, const unsigned char *cov, int n, uint32_t col) {
    int i;
    for (i = 0; i < n; i++) if (cov[i]) d[i] = gy__over(d[i], gy__scale(col, cov[i]));
}

/* SSE2: 4 pixels at a time in 16-bit lanes */
static __m128i gy__sse_div255(__m128i x) {
    x = _mm_add_epi16(x, _mm_set1_epi16(128));
    return _mm_srli_epi16(_mm_add_epi16(x, _mm_srli_epi16(x, 8)), 8);
}
static __m128i gy__sse_over16(__m128i d, __m128i s) {   /* 2 pixels, 16-bit lanes */
    __m128i a = _mm_shufflehi_epi16(_mm_shufflelo_epi16(s, 0xFF), 0xFF);
    __m128i ia = _mm_sub_epi16(_mm_set1_epi16(255), a);
    return _mm_add_epi16(s, gy__sse_div255(_mm_mullo_epi16(d, ia)));
}
static __m128i gy__sse_over4(__m128i d, __m128i s) {
    __m128i z = _mm_setzero_si128();
    __m128i lo = gy__sse_over16(_mm_unpacklo_epi8(d, z), _mm_unpacklo_epi8(s, z));
    __m128i hi = gy__sse_over16(_mm_unpackhi_epi8(d, z), _mm_unpackhi_epi8(s, z));
    return _mm_packus_epi16(lo, hi);
}
static void gy__fill_sse2(uint32_t *d, int n, uint32_t s) {
    int i = 0;
    __m128i sv = _mm_set1_epi32((int)s);
    if ((s >> 24) == 255) { for (; i + 4 <= n; i += 4) _mm_storeu_si128((__m128i *)(d + i), sv); }
    else if (!s) return;
    else for (; i + 4 <= n; i += 4) _mm_storeu_si128((__m128i *)(d + i), gy__sse_over4(_mm_loadu_si128((const __m128i *)(d + i)), sv));
    gy__fill_scalar(d + i, n - i, s);
}
static void gy__row_sse2(uint32_t *d, const uint32_t *s, int n) {
    int i = 0;
    for (; i + 4 <= n; i += 4)
        _mm_storeu_si128((__m128i *)(d + i), gy__sse_over4(_mm_loadu_si128((const __m128i *)(d + i)), _mm_loadu_si128((const __m128i *)(s + i))));
    gy__row_scalar(d + i, s + i, n - i);
}
static void gy__glyph_sse2(uint32_t *d, const unsigned char *cov, int n, uint32_t col) {
    int i = 0;
    __m128i z = _mm_setzero_si128();
    __m128i c16 = _mm_unpacklo_epi8(_mm_set1_epi32((int)col), z);   /* the colour, 2 pixels in 16-bit lanes */
    for (; i + 4 <= n; i += 4) {
        int32_t k4;
        __m128i k, dv, lo, hi;
        gy__memcpy(&k4, cov + i, 4);
        if (!k4) continue;
        k = _mm_cvtsi32_si128(k4);
        k = _mm_unpacklo_epi8(k, k);
        k = _mm_unpacklo_epi16(k, k);                                /* each coverage byte 4 times */
        dv = _mm_loadu_si128((const __m128i *)(d + i));
        lo = gy__sse_over16(_mm_unpacklo_epi8(dv, z), gy__sse_div255(_mm_mullo_epi16(c16, _mm_unpacklo_epi8(k, z))));
        hi = gy__sse_over16(_mm_unpackhi_epi8(dv, z), gy__sse_div255(_mm_mullo_epi16(c16, _mm_unpackhi_epi8(k, z))));
        _mm_storeu_si128((__m128i *)(d + i), _mm_packus_epi16(lo, hi));
    }
    gy__glyph_scalar(d + i, cov + i, n - i, col);
}

/* AVX2: 8 pixels at a time */
#define GY__AVX2 __attribute__((target("avx2")))
GY__AVX2 static __m256i gy__avx_div255(__m256i x) {
    x = _mm256_add_epi16(x, _mm256_set1_epi16(128));
    return _mm256_srli_epi16(_mm256_add_epi16(x, _mm256_srli_epi16(x, 8)), 8);
}
GY__AVX2 static __m256i gy__avx_over16(__m256i d, __m256i s) {
    __m256i a = _mm256_shufflehi_epi16(_mm256_shufflelo_epi16(s, 0xFF), 0xFF);
    __m256i ia = _mm256_sub_epi16(_mm256_set1_epi16(255), a);
    return _mm256_add_epi16(s, gy__avx_div255(_mm256_mullo_epi16(d, ia)));
}
GY__AVX2 static __m256i gy__avx_over8(__m256i d, __m256i s) {
    __m256i z = _mm256_setzero_si256();
    __m256i lo = gy__avx_over16(_mm256_unpacklo_epi8(d, z), _mm256_unpacklo_epi8(s, z));
    __m256i hi = gy__avx_over16(_mm256_unpackhi_epi8(d, z), _mm256_unpackhi_epi8(s, z));
    return _mm256_packus_epi16(lo, hi);
}
GY__AVX2 static void gy__fill_avx2(uint32_t *d, int n, uint32_t s) {
    int i = 0;
    __m256i sv = _mm256_set1_epi32((int)s);
    if ((s >> 24) == 255) { for (; i + 8 <= n; i += 8) _mm256_storeu_si256((__m256i *)(d + i), sv); }
    else if (!s) return;
    else for (; i + 8 <= n; i += 8) _mm256_storeu_si256((__m256i *)(d + i), gy__avx_over8(_mm256_loadu_si256((const __m256i *)(d + i)), sv));
    gy__fill_scalar(d + i, n - i, s);
}
GY__AVX2 static void gy__row_avx2(uint32_t *d, const uint32_t *s, int n) {
    int i = 0;
    for (; i + 8 <= n; i += 8)
        _mm256_storeu_si256((__m256i *)(d + i), gy__avx_over8(_mm256_loadu_si256((const __m256i *)(d + i)), _mm256_loadu_si256((const __m256i *)(s + i))));
    gy__row_scalar(d + i, s + i, n - i);
}
GY__AVX2 static void gy__glyph_avx2(uint32_t *d, const unsigned char *cov, int n, uint32_t col) {
    int i = 0;
    __m256i z = _mm256_setzero_si256();
    __m256i c16 = _mm256_unpacklo_epi8(_mm256_set1_epi32((int)col), z);
    for (; i + 8 <= n; i += 8) {
        int64_t k8;
        __m128i k, klo, khi;
        __m256i kk, dv, lo, hi;
        gy__memcpy(&k8, cov + i, 8);
        if (!k8) continue;
        k = _mm_unpacklo_epi8(_mm_cvtsi64_si128(k8), _mm_cvtsi64_si128(k8));
        klo = _mm_unpacklo_epi16(k, k);                              /* pixels 0-3, each byte 4 times */
        khi = _mm_unpackhi_epi16(k, k);                              /* pixels 4-7 */
        kk = _mm256_set_m128i(khi, klo);
        dv = _mm256_loadu_si256((const __m256i *)(d + i));
        lo = gy__avx_over16(_mm256_unpacklo_epi8(dv, z), gy__avx_div255(_mm256_mullo_epi16(c16, _mm256_unpacklo_epi8(kk, z))));
        hi = gy__avx_over16(_mm256_unpackhi_epi8(dv, z), gy__avx_div255(_mm256_mullo_epi16(c16, _mm256_unpackhi_epi8(kk, z))));
        _mm256_storeu_si256((__m256i *)(d + i), _mm256_packus_epi16(lo, hi));
    }
    gy__glyph_scalar(d + i, cov + i, n - i, col);
}

static unsigned long long gy__xgetbv(unsigned i) {
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(i));
    return ((unsigned long long)hi << 32) | lo;
}

static int gy__cpu_best(void) {
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return GY_ISA_SSE2;          /* x86-64 always has SSE2 */
    if ((c & (1u << 27)) && (c & (1u << 28)) && (gy__xgetbv(0) & 6) == 6
        && __get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & (1u << 5)))
        return GY_ISA_AVX2;
    return GY_ISA_SSE2;
}

int gy_isa_set(int isa) {
    int best = gy__cpu_best();
    if (isa > best) isa = best;
    if (isa < GY_ISA_SCALAR) isa = GY_ISA_SCALAR;
    gy__c.isa = isa;
    if (isa == GY_ISA_AVX2) { gy__c.k_fill = gy__fill_avx2; gy__c.k_row = gy__row_avx2; gy__c.k_glyph = gy__glyph_avx2; }
    else if (isa == GY_ISA_SSE2) { gy__c.k_fill = gy__fill_sse2; gy__c.k_row = gy__row_sse2; gy__c.k_glyph = gy__glyph_sse2; }
    else { gy__c.k_fill = gy__fill_scalar; gy__c.k_row = gy__row_scalar; gy__c.k_glyph = gy__glyph_scalar; }
    return isa;
}

const char *gy_isa_name(void) {
    return gy__c.isa == GY_ISA_AVX2 ? "avx2" : gy__c.isa == GY_ISA_SSE2 ? "sse2" : "scalar";
}

/* ================================================================================================
   Context, frame
   ================================================================================================ */

static int gy__font_make(const char *face, int px, int bold);

static int gy__init(void) {
    char isa[16];
    DWORD n;
    if (gy__c.ready) return 1;
    gy__memset(&gy__c, 0, sizeof gy__c);
    if (!gy__arena_init(&gy__c.perm, (size_t)1 << 30) || !gy__arena_init(&gy__c.frame, (size_t)1 << 30)) {
        gy__set_error("GUICy could not reserve its memory", NULL); return 0;
    }
    gy__c.theme = gy__theme_dark;
    gy__c.glyphs = (gy__glyph *)gy__arena_push(&gy__c.perm, sizeof(gy__glyph) * GY__GLYPH_SLOTS);
    gy__c.scratch = (unsigned char *)gy__arena_push(&gy__c.perm, 1 << 20);
    gy__c.dc = CreateCompatibleDC(NULL);
    if (!gy__c.glyphs || !gy__c.scratch || !gy__c.dc) { gy__set_error("GUICy could not start", "no memory or no device context"); return 0; }
    gy__memset(gy__c.glyphs, 0, sizeof(gy__glyph) * GY__GLYPH_SLOTS);
    /* GUICY_ISA=scalar|sse2|avx2 picks the kernels (tests use it); default: the best the CPU has */
    n = GetEnvironmentVariableA("GUICY_ISA", isa, sizeof isa);
    if (n && n < sizeof isa) gy_isa_set(isa[0] == 's' && isa[1] == 'c' ? GY_ISA_SCALAR : isa[0] == 's' ? GY_ISA_SSE2 : GY_ISA_AVX2);
    else gy_isa_set(GY_ISA_AVX2);
    gy__c.ready = 1;
    if (gy__font_make("Segoe UI", gy__c.theme.font_px, 0) < 0) return 0;
    return 1;
}

int gy_offscreen(int w, int h) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { gy__set_error("gy_offscreen", "the size must be 1..16384"); return 0; }
    if (!gy__init()) return 0;
    gy__c.px = (uint32_t *)gy__arena_push(&gy__c.perm, (size_t)w * (size_t)h * 4);
    if (!gy__c.px) { gy__set_error("gy_offscreen", "out of memory for the frame"); return 0; }
    gy__c.w = w; gy__c.h = h;
    gy__c.want_full = 1;
    gy_clear();
    return 1;
}

void gy_clear(void) {
    int y;
    for (y = 0; y < gy__c.h; y++) gy__c.k_fill(gy__c.px + (size_t)y * gy__c.w, gy__c.w, gy__c.theme.bg);
}

int gy_width(void) { return gy__c.w; }
int gy_height(void) { return gy__c.h; }
const uint32_t *gy_pixels(void) { return gy__c.px; }
const gy_theme *gy_theme_get(void) { gy__init(); return &gy__c.theme; }
void gy_theme_set(const gy_theme *t) { if (gy__init()) gy__c.theme = *t; }
const char *gy_last_error(void) { return gy__c.error; }

gy_stats gy_get_stats(void) {
    gy_stats s;
    s.instances = gy__c.ninst; s.glyph_misses = gy__c.glyph_misses; s.fonts = gy__c.nfonts; s.atlas_pages = gy__c.npages;
    s.tiles = gy__c.tiles_x * gy__c.tiles_y; s.dirty_tiles = gy__c.dirty_tiles; s.frames = gy__c.frames; s.frames_drawn = gy__c.frames_drawn;
    return s;
}

/* --- input ---------------------------------------------------------------------------------------- */
void gy_input_mouse(int x, int y) { gy__c.mx = x; gy__c.my = y; }
void gy_input_button(int b, int down) {
    if (b < 0 || b > 2) return;
    if (down && !gy__c.down[b]) gy__c.pressed[b] = 1;
    if (!down && gy__c.down[b]) gy__c.released[b] = 1;
    gy__c.down[b] = down;
}

/* --- the draw list -------------------------------------------------------------------------------- */
static gy__inst *gy__push(int kind, int x0, int y0, int x1, int y1) {
    gy__inst *in;
    if (gy__c.ninst == gy__c.cap_inst) {      /* grow: never drop a command */
        int cap = gy__c.cap_inst ? gy__c.cap_inst * 2 : 4096;
        gy__inst *n = (gy__inst *)gy__arena_push(&gy__c.frame, sizeof(gy__inst) * (size_t)cap);
        if (!n) { gy__set_error("GUICy ran out of memory for the draw list", NULL); return NULL; }
        if (gy__c.ninst) gy__memcpy(n, gy__c.inst, sizeof(gy__inst) * (size_t)gy__c.ninst);
        gy__c.inst = n; gy__c.cap_inst = cap;
    }
    in = &gy__c.inst[gy__c.ninst++];
    gy__memset(in, 0, sizeof *in);
    in->kind = (uint8_t)kind;
    in->x0 = (int16_t)x0; in->y0 = (int16_t)y0; in->x1 = (int16_t)x1; in->y1 = (int16_t)y1;
    return in;
}

void gy_draw_box(gy_rect r, gy_color fill, gy_color border, float radius, float border_w) {
    gy__inst *in;
    if (r.w <= 0 || r.h <= 0 || (!fill && (!border || border_w <= 0))) return;
    in = gy__push(GY__K_BOX, r.x, r.y, r.x + r.w, r.y + r.h);
    if (!in) return;
    in->col0 = fill; in->col1 = border; in->radius = radius; in->border = border_w;
}

/* ================================================================================================
   Text: GDI glyphs cached in coverage pages
   ================================================================================================ */

static int gy__utf16_from_utf8(const char *s, WCHAR *out, int cap) {
    int n = 0;
    while (*s && n < cap - 1) out[n++] = (WCHAR)(unsigned char)*s++;   /* face names are ASCII */
    out[n] = 0;
    return n;
}

static int gy__font_make(const char *face, int px, int bold) {
    WCHAR wface[64];
    TEXTMETRICW tm;
    gy__font *f;
    if (gy__c.nfonts >= GY__MAX_FONTS) { gy__set_error("gy_font_add", "16 fonts at most"); return -1; }
    f = &gy__c.fonts[gy__c.nfonts];
    gy__memset(f, 0, sizeof *f);
    gy__utf16_from_utf8(face, wface, 64);
    f->hf = CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wface);
    if (!f->hf) { gy__set_error("gy_font_add could not make the font", face); return -1; }
    SelectObject(gy__c.dc, f->hf);
    GetTextMetricsW(gy__c.dc, &tm);
    f->px = px; f->ascent = tm.tmAscent; f->descent = tm.tmDescent; f->height = tm.tmHeight;
    return gy__c.nfonts++;
}

int gy_font_add(const char *face, int px, int bold) { if (!gy__init()) return -1; return gy__font_make(face, px, bold); }
void gy_font(int i) { if (i >= 0 && i < gy__c.nfonts) gy__c.font = i; }
int gy_line_height(void) { return gy__c.fonts[gy__c.font].height; }

/* a place in the atlas for a w x h coverage block; shelves, 1 px apart */
static int gy__atlas_alloc(int w, int h, int *page, int *u, int *v) {
    if (w > GY__ATLAS || h > GY__ATLAS) return 0;
    if (!gy__c.npages || gy__c.shelf_x + w + 1 > GY__ATLAS) {
        gy__c.shelf_y += gy__c.shelf_h + 1; gy__c.shelf_x = 0; gy__c.shelf_h = 0;
    }
    if (!gy__c.npages || gy__c.shelf_y + h + 1 > GY__ATLAS) {
        if (gy__c.npages == GY__MAX_PAGES) { gy__set_error("GUICy's glyph atlas is full", "8 pages of 1024x1024"); return 0; }
        gy__c.pages[gy__c.npages] = (unsigned char *)gy__arena_push(&gy__c.perm, GY__ATLAS * GY__ATLAS);
        if (!gy__c.pages[gy__c.npages]) { gy__set_error("GUICy ran out of memory for glyphs", NULL); return 0; }
        gy__memset(gy__c.pages[gy__c.npages], 0, GY__ATLAS * GY__ATLAS);
        gy__c.npages++; gy__c.shelf_x = 0; gy__c.shelf_y = 0; gy__c.shelf_h = 0;
    }
    *page = gy__c.npages - 1; *u = gy__c.shelf_x; *v = gy__c.shelf_y;
    gy__c.shelf_x += w + 1;
    if (h > gy__c.shelf_h) gy__c.shelf_h = h;
    return 1;
}

static gy__glyph *gy__slot(uint32_t key) {
    uint32_t i = (key * 2654435761u) & (GY__GLYPH_SLOTS - 1);
    for (;;) {
        gy__glyph *g = &gy__c.glyphs[i];
        if (g->key == key || g->key == 0) return g;
        i = (i + 1) & (GY__GLYPH_SLOTS - 1);
    }
}

static gy__glyph *gy__glyph_get(int font, uint32_t cp) {
    static const MAT2 ident = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
    uint32_t key = (uint32_t)(font + 1) << 21 | cp;
    gy__glyph *g;
    GLYPHMETRICS gm;
    DWORD need;
    int page = 0, u = 0, v = 0, x, y, pitch;
    if (cp < 128 && gy__c.fonts[font].ascii[cp]) return &gy__c.glyphs[gy__c.fonts[font].ascii[cp] - 1];
    g = gy__slot(key);
    if (g->key == key) return g;
    gy__c.glyph_misses++;
    g->key = key;
    SelectObject(gy__c.dc, gy__c.fonts[font].hf);
    need = GetGlyphOutlineW(gy__c.dc, cp > 0xFFFF ? 0xFFFD : cp, GGO_GRAY8_BITMAP, &gm, 0, NULL, &ident);
    if (need == GDI_ERROR) { gy__memset(&gm, 0, sizeof gm); need = 0; }
    g->adv = (int16_t)gm.gmCellIncX;
    if (need && need <= (1u << 20) && GetGlyphOutlineW(gy__c.dc, cp > 0xFFFF ? 0xFFFD : cp, GGO_GRAY8_BITMAP, &gm, need, gy__c.scratch, &ident) != GDI_ERROR
        && gy__atlas_alloc((int)gm.gmBlackBoxX, (int)gm.gmBlackBoxY, &page, &u, &v)) {
        pitch = ((int)gm.gmBlackBoxX + 3) & ~3;  /* GDI rows are DWORD-aligned */
        for (y = 0; y < (int)gm.gmBlackBoxY; y++)
            for (x = 0; x < (int)gm.gmBlackBoxX; x++) {
                unsigned k = gy__c.scratch[y * pitch + x];                 /* 0..64 */
                gy__c.pages[page][(v + y) * GY__ATLAS + u + x] = (unsigned char)((k * 255 + 32) / 64);
            }
        g->u = (uint16_t)u; g->v = (uint16_t)v; g->w = (uint16_t)gm.gmBlackBoxX; g->h = (uint16_t)gm.gmBlackBoxY;
        g->ox = (int16_t)gm.gmptGlyphOrigin.x; g->oy = (int16_t)gm.gmptGlyphOrigin.y;
        g->page = (uint8_t)page; g->ok = 1;
    }
    if (cp < 128) gy__c.fonts[font].ascii[cp] = (int)(g - gy__c.glyphs) + 1;
    return g;
}

static uint32_t gy__utf8_next(const char **ps) {  /* bad bytes become U+FFFD */
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t c = *s++;
    int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (c >= 0x80 && c < 0xC0) { *ps = (const char *)s; return 0xFFFD; }
    c &= more == 3 ? 0x07 : more == 2 ? 0x0F : more == 1 ? 0x1F : 0x7F;
    while (more--) {
        if ((*s & 0xC0) != 0x80) { *ps = (const char *)s; return 0xFFFD; }
        c = c << 6 | (*s++ & 0x3F);
    }
    *ps = (const char *)s;
    return c;
}

static const char *gy__label_end(const char *s) {   /* the shown part of a label stops at "##" */
    while (*s && !(s[0] == '#' && s[1] == '#')) s++;
    return s;
}

static int gy__text_w(const char *s, const char *end) {
    int w = 0;
    if (!gy__init()) return 0;
    while (s < end && *s) w += gy__glyph_get(gy__c.font, gy__utf8_next(&s))->adv;
    return w;
}

int gy_text_width(const char *s) { return gy__text_w(s, s + 0x7FFFFFFF); }

static void gy__draw_text_n(int x, int y, const char *s, const char *end, gy_color col) {
    int base = y + gy__c.fonts[gy__c.font].ascent;
    while (s < end && *s) {
        gy__glyph *g = gy__glyph_get(gy__c.font, gy__utf8_next(&s));
        if (g->ok && g->w) {
            gy__inst *in = gy__push(GY__K_GLYPH, x + g->ox, base - g->oy, x + g->ox + g->w, base - g->oy + g->h);
            if (!in) return;
            in->u = g->u; in->v = g->v; in->page = g->page; in->col0 = col;
        }
        x += g->adv;
    }
}

void gy_draw_text(int x, int y, const char *s, gy_color c) { if (gy__init()) gy__draw_text_n(x, y, s, s + 0x7FFFFFFF, c); }

/* --- a baked icon: the check mark, drawn by distance to its two strokes ------------------------- */
static float gy__seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float t = (wx * vx + wy * vy) / (vx * vx + vy * vy);
    t = gy__maxf(0, gy__minf(1, t));
    wx -= vx * t; wy -= vy * t;
    return gy__sqrtf(wx * wx + wy * wy);
}

static gy__glyph *gy__icon_check(int size) {
    uint32_t key = (uint32_t)GY__ICON_FONT << 21 | (uint32_t)size;
    gy__glyph *g = gy__slot(key);
    int page, u, v, x, y;
    float s = (float)size / 16.0f;
    if (g->key == key) return g;
    g->key = key;
    if (!gy__atlas_alloc(size, size, &page, &u, &v)) return g;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float d = gy__minf(gy__seg_dist(px, py, 3.5f * s, 8.5f * s, 6.5f * s, 11.5f * s),
                               gy__seg_dist(px, py, 6.5f * s, 11.5f * s, 12.5f * s, 4.5f * s));
            float cov = gy__maxf(0, gy__minf(1, 1.1f * s + 0.5f - d));
            gy__c.pages[page][(v + y) * GY__ATLAS + u + x] = (unsigned char)(cov * 255.0f + 0.5f);
        }
    g->u = (uint16_t)u; g->v = (uint16_t)v; g->w = g->h = (uint16_t)size; g->page = (uint8_t)page; g->ok = 1;
    return g;
}

static void gy__draw_icon(gy__glyph *g, int x, int y, gy_color col) {
    gy__inst *in;
    if (!g->ok) return;
    in = gy__push(GY__K_GLYPH, x, y, x + g->w, y + g->h);
    if (!in) return;
    in->u = g->u; in->v = g->v; in->page = g->page; in->col0 = col;
}

/* --- a tiny printf: %d %i %u %x %c %s %f %.Nf %% ------------------------------------------------- */
static int gy__vfmt(char *out, int cap, const char *f, va_list ap) {
    int n = 0;
#define GY__PUT(ch) do { if (n < cap - 1) out[n] = (char)(ch); n++; } while (0)
    for (; *f; f++) {
        int prec = -1;
        char tmp[32];
        int t = 0;
        if (*f != '%') { GY__PUT(*f); continue; }
        f++;
        if (*f == '.') { prec = 0; f++; while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0'); }
        if (*f == 'l') f++;
        switch (*f) {
        case 'd': case 'i': {
            long v = va_arg(ap, int);
            unsigned long u = v < 0 ? (unsigned long)-v : (unsigned long)v;
            do tmp[t++] = (char)('0' + u % 10); while (u /= 10);
            if (v < 0) GY__PUT('-');
            while (t) GY__PUT(tmp[--t]);
            break;
        }
        case 'u': case 'x': {
            unsigned u = va_arg(ap, unsigned), base = *f == 'x' ? 16 : 10;
            do tmp[t++] = "0123456789abcdef"[u % base]; while (u /= base);
            while (t) GY__PUT(tmp[--t]);
            break;
        }
        case 'c': GY__PUT(va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(null)"; while (*s) GY__PUT(*s++); break; }
        case 'f': {
            double v = va_arg(ap, double), r = 0.5;
            unsigned long long ip;
            int i;
            if (prec < 0) prec = 6;
            if (prec > 9) prec = 9;
            if (v < 0) { GY__PUT('-'); v = -v; }
            for (i = 0; i < prec; i++) r /= 10;
            v += r;
            ip = (unsigned long long)v;
            v -= (double)ip;
            do tmp[t++] = (char)('0' + ip % 10); while (ip /= 10);
            while (t) GY__PUT(tmp[--t]);
            if (prec) GY__PUT('.');
            for (i = 0; i < prec; i++) { int dgt; v *= 10; dgt = (int)v; GY__PUT('0' + dgt); v -= dgt; }
            break;
        }
        case '%': GY__PUT('%'); break;
        case 0: f--; break;
        default: GY__PUT('%'); GY__PUT(*f); break;
        }
    }
#undef GY__PUT
    out[n < cap ? n : cap - 1] = 0;
    return n;
}

/* ================================================================================================
   IDs, layout, widgets
   ================================================================================================ */

static uint32_t gy__hash(const char *s, uint32_t seed) {   /* FNV-1a over the whole label */
    uint32_t h = seed ^ 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h ? h : 1;
}
static uint32_t gy__id(const char *label) { return gy__hash(label, gy__c.id_depth ? gy__c.id_stack[gy__c.id_depth - 1] : 0); }

void gy_id_push(int i) {
    uint32_t seed = gy__c.id_depth ? gy__c.id_stack[gy__c.id_depth - 1] : 0, h = (seed ^ (uint32_t)i) * 16777619u + 0x9E3779B9u;
    if (gy__c.id_depth < 32) gy__c.id_stack[gy__c.id_depth++] = h;
}
void gy_id_pop(void) { if (gy__c.id_depth) gy__c.id_depth--; }
void gy_disable_begin(void) { gy__c.disabled++; }
void gy_disable_end(void) { if (gy__c.disabled) gy__c.disabled--; }

static void gy__row_layout(void) {
    int i, n = gy__c.ncols, avail = gy__c.area.w - gy__c.theme.gap * (n - 1), fixed = 0, grow = 0;
    float x;
    int px[16];
    for (i = 0; i < n; i++) {
        float w = gy__c.widths[i];
        if (w > 1) px[i] = (int)w;
        else if (w > 0) px[i] = (int)(w * (float)avail);
        else { px[i] = -1; grow++; continue; }
        fixed += px[i];
    }
    x = (float)gy__c.area.x;
    for (i = 0; i < n; i++) {
        float w = px[i] >= 0 ? (float)px[i] : (float)gy__maxi(0, avail - fixed) / (float)grow;
        gy__c.col_x[i] = (int)x;
        x += w + (float)gy__c.theme.gap;
    }
    /* the end of the last column; with a growing column it is exactly the area's right edge */
    gy__c.col_x[n] = grow ? gy__c.area.x + gy__c.area.w + gy__c.theme.gap : (int)x;
}

void gy_row(int n, const float *widths) {
    int i;
    if (n <= 0 || !widths) { n = 1; gy__c.widths[0] = -1; }
    else for (i = 0; i < n && i < 16; i++) gy__c.widths[i] = widths[i];
    if (gy__c.col) gy__c.cur_y += gy__c.theme.row_h + gy__c.theme.gap;  /* a row already started: go below it */
    gy__c.ncols = n > 16 ? 16 : n;
    gy__c.col = 0;
    gy__row_layout();
}

static gy_rect gy__next(void) {
    gy_rect r;
    if (gy__c.col >= gy__c.ncols) { gy__c.cur_y += gy__c.theme.row_h + gy__c.theme.gap; gy__c.col = 0; }
    r.x = gy__c.col_x[gy__c.col];
    r.w = gy__c.col_x[gy__c.col + 1] - gy__c.theme.gap - r.x;
    r.y = gy__c.cur_y; r.h = gy__c.theme.row_h;
    gy__c.col++;
    gy__c.last = r;
    return r;
}

gy_rect gy_last_rect(void) { return gy__c.last; }

static int gy__inside(gy_rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

/* the press-and-release behaviour every clickable widget shares */
static int gy__button_logic(uint32_t id, gy_rect r, int *hot, int *held) {
    int over = !gy__c.disabled && gy__inside(r, gy__c.mx, gy__c.my) && (!gy__c.active || gy__c.active == id);
    if (over && gy__c.pressed[0]) gy__c.active = id;
    *hot = over;
    *held = gy__c.active == id && gy__c.down[0];
    return gy__c.active == id && gy__c.released[0] && over;
}

void gy_begin(void) {
    if (!gy__init()) return;
    gy__c.frame.used = 0;
    gy__c.inst = NULL; gy__c.ninst = gy__c.cap_inst = 0;
    gy__c.nclips = 1;
    gy__c.clips[0].x = 0; gy__c.clips[0].y = 0; gy__c.clips[0].w = gy__c.w; gy__c.clips[0].h = gy__c.h;
    gy__c.area.x = gy__c.theme.pad; gy__c.area.y = gy__c.theme.pad;
    gy__c.area.w = gy__c.w - 2 * gy__c.theme.pad; gy__c.area.h = gy__c.h - 2 * gy__c.theme.pad;
    gy__c.cur_y = gy__c.area.y;
    gy__c.ncols = 1; gy__c.col = 0; gy__c.widths[0] = -1;
    gy__row_layout();
    gy__c.id_depth = 0; gy__c.disabled = 0; gy__c.font = 0;
    gy__c.in_frame = 1;
}

static void gy__text_v(gy_color col, const char *fmt, va_list ap) {
    char buf[1024];
    gy_rect r = gy__next();
    gy__vfmt(buf, sizeof buf, fmt, ap);
    gy__draw_text_n(r.x, r.y + (r.h - gy_line_height()) / 2, buf, buf + sizeof buf, gy__c.disabled ? gy__c.theme.text_off : col);
}

void gy_text(const char *fmt, ...) { va_list ap; va_start(ap, fmt); gy__text_v(gy__c.theme.text, fmt, ap); va_end(ap); }
void gy_text_col(gy_color c, const char *fmt, ...) { va_list ap; va_start(ap, fmt); gy__text_v(c, fmt, ap); va_end(ap); }

int gy_button(const char *label) {
    const gy_theme *t = &gy__c.theme;
    gy_rect r = gy__next();
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held);
    gy_color bg = gy__c.disabled ? t->button : held && hot ? t->button_down : hot ? t->button_hot : t->button;
    gy_draw_box(r, bg, hot ? t->accent : t->border, (float)t->radius, 1);
    gy__draw_text_n(r.x + (r.w - gy__text_w(label, end)) / 2, r.y + (r.h - gy_line_height()) / 2, label, end,
                    gy__c.disabled ? t->text_off : t->text);
    return clicked;
}

int gy_check(const char *label, int *on) {
    const gy_theme *t = &gy__c.theme;
    gy_rect r = gy__next(), box;
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held), size = 16;
    (void)held;
    if (clicked) *on = !*on;
    box.x = r.x; box.y = r.y + (r.h - size) / 2; box.w = box.h = size;
    if (*on) {
        gy_draw_box(box, gy__c.disabled ? t->button : hot ? t->accent_hot : t->accent, 0, 3, 0);
        gy__draw_icon(gy__icon_check(size), box.x, box.y, gy__c.disabled ? t->text_off : t->accent_text);
    } else gy_draw_box(box, hot ? t->field_hot : t->field, hot ? t->accent : t->border, 3, 1);
    gy__draw_text_n(r.x + size + t->gap, r.y + (r.h - gy_line_height()) / 2, label, end, gy__c.disabled ? t->text_off : t->text);
    return clicked;
}

int gy_select(const char *label, int on) {
    const gy_theme *t = &gy__c.theme;
    gy_rect r = gy__next();
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held);
    gy_color bg = on ? (gy__c.disabled ? t->button : hot ? t->accent_hot : t->accent) : held && hot ? t->button_down : hot ? t->button_hot : 0;
    if (bg) gy_draw_box(r, bg, 0, (float)t->radius, 0);
    gy__draw_text_n(r.x + t->gap, r.y + (r.h - gy_line_height()) / 2, label, end,
                    gy__c.disabled ? t->text_off : on ? t->accent_text : t->text);
    return clicked;
}

/* ================================================================================================
   The CPU renderer
   ================================================================================================ */

/* coverage of the pixel at (px, py) (its centre) by a rounded box; 0..255 */
static uint32_t gy__box_cov(float px, float py, float cx, float cy, float hx, float hy, float r) {
    float qx, qy, d, ox, oy;
    if (hx <= 0 || hy <= 0) return 0;
    r = gy__minf(r, gy__minf(hx, hy));
    qx = gy__absf(px - cx) - (hx - r);
    qy = gy__absf(py - cy) - (hy - r);
    ox = gy__maxf(qx, 0); oy = gy__maxf(qy, 0);
    d = gy__sqrtf(ox * ox + oy * oy) + gy__minf(gy__maxf(qx, qy), 0) - r;
    d = 0.5f - d;
    return d <= 0 ? 0 : d >= 1 ? 255 : (uint32_t)(d * 255.0f + 0.5f);
}

static uint32_t gy__box_px(const gy__inst *in, float px, float py, float cx, float cy, float hx, float hy) {
    uint32_t co = gy__box_cov(px, py, cx, cy, hx, hy, in->radius), ci, s;
    if (in->border <= 0) return gy__scale(in->col0, co);
    ci = gy__box_cov(px, py, cx, cy, hx - in->border, hy - in->border, gy__maxf(in->radius - in->border, 0));
    s = gy__scale(in->col0, ci);
    if (co > ci) {                              /* add the ring's share of the border colour */
        uint32_t b = gy__scale(in->col1, co - ci), o = 0;
        int sh;
        for (sh = 0; sh < 32; sh += 8) o |= (uint32_t)gy__mini((int)(((s >> sh) & 255) + ((b >> sh) & 255)), 255) << sh;
        s = o;
    }
    return s;
}

static void gy__draw_box_cpu(const gy__inst *in, gy_rect clip) {
    int x0 = gy__maxi(in->x0, clip.x), y0 = gy__maxi(in->y0, clip.y);
    int x1 = gy__mini(in->x1, clip.x + clip.w), y1 = gy__mini(in->y1, clip.y + clip.h);
    int n = x1 - x0, x, y, band;
    float cx = (float)(in->x0 + in->x1) * 0.5f, cy = (float)(in->y0 + in->y1) * 0.5f;
    float hx = (float)(in->x1 - in->x0) * 0.5f, hy = (float)(in->y1 - in->y0) * 0.5f;
    uint32_t *row;
    if (n <= 0 || y1 <= y0) return;
    if (in->radius <= 0 && in->border <= 0) {  /* a plain rectangle: whole rows, the fast path */
        for (y = y0; y < y1; y++) gy__c.k_fill(gy__c.px + (size_t)y * gy__c.w + x0, n, in->col0);
        return;
    }
    /* rows away from the top and bottom edge bands depend only on x: work one row out, reuse it */
    band = (int)(gy__maxf(in->radius, in->border) + 1.0f);
    row = (uint32_t *)gy__arena_push(&gy__c.frame, sizeof(uint32_t) * (size_t)n);
    if (!row) return;
    for (x = 0; x < n; x++) row[x] = gy__box_px(in, (float)(x0 + x) + 0.5f, cy, cx, cy, hx, hy);
    for (y = y0; y < y1; y++) {
        uint32_t *d = gy__c.px + (size_t)y * gy__c.w + x0;
        if (y >= in->y0 + band && y < in->y1 - band) { gy__c.k_row(d, row, n); continue; }
        for (x = 0; x < n; x++) {
            uint32_t s = gy__box_px(in, (float)(x0 + x) + 0.5f, (float)y + 0.5f, cx, cy, hx, hy);
            if (s) d[x] = gy__over(d[x], s);
        }
    }
}

static void gy__draw_glyph_cpu(const gy__inst *in, gy_rect clip) {
    int x0 = gy__maxi(in->x0, clip.x), y0 = gy__maxi(in->y0, clip.y);
    int x1 = gy__mini(in->x1, clip.x + clip.w), y1 = gy__mini(in->y1, clip.y + clip.h), y;
    const unsigned char *page = gy__c.pages[in->page];
    if (x1 <= x0 || y1 <= y0) return;
    for (y = y0; y < y1; y++)
        gy__c.k_glyph(gy__c.px + (size_t)y * gy__c.w + x0,
                      page + (size_t)(in->v + y - in->y0) * GY__ATLAS + in->u + (x0 - in->x0), x1 - x0, in->col0);
}

/* --- dirty tiles ---------------------------------------------------------------------------------
   Every instance's 48 bytes (and its clip) are hashed into each 64x64 tile it touches, in order.
   A tile whose hash equals last frame's already shows the right pixels and is not touched. A
   changed tile is cleared and every instance over it is drawn again, clipped to the tile, so the
   result is byte for byte what a full redraw gives (gytest checks that). */
#define GY__TILE 64
#define GY__MAX_TILES ((16384 / GY__TILE) * (16384 / GY__TILE))

static uint64_t gy__mix(uint64_t h, uint64_t v) { h ^= v; h *= 0x9E3779B97F4A7C15ull; return h ^ (h >> 29); }

static uint64_t gy__inst_hash(const gy__inst *in, gy_rect clip) {
    const uint64_t *q = (const uint64_t *)(const void *)in;
    uint64_t h = 0x51ED27D3A9C47B1Full;
    int i;
    for (i = 0; i < 6; i++) h = gy__mix(h, q[i]);
    return gy__mix(h, (uint64_t)(uint32_t)clip.x << 48 ^ (uint64_t)(uint32_t)clip.y << 32 ^ (uint64_t)(uint32_t)clip.w << 16 ^ (uint32_t)clip.h);
}

static int gy__tiles_ready(void) {
    if (gy__c.tile_prev) return 1;
    gy__c.tile_prev = (uint64_t *)gy__arena_push(&gy__c.perm, sizeof(uint64_t) * GY__MAX_TILES);
    gy__c.tile_cur = (uint64_t *)gy__arena_push(&gy__c.perm, sizeof(uint64_t) * GY__MAX_TILES);
    gy__c.tile_dirty = (unsigned char *)gy__arena_push(&gy__c.perm, GY__MAX_TILES);
    if (!gy__c.tile_prev || !gy__c.tile_cur || !gy__c.tile_dirty) { gy__set_error("GUICy ran out of memory for tiles", NULL); gy__c.tile_prev = NULL; return 0; }
    return 1;
}

/* the instance's box on screen, inside its clip and the frame; 0 if nothing shows */
static int gy__inst_box(const gy__inst *in, gy_rect clip, int *x0, int *y0, int *x1, int *y1) {
    *x0 = gy__maxi(gy__maxi(in->x0, clip.x), 0); *y0 = gy__maxi(gy__maxi(in->y0, clip.y), 0);
    *x1 = gy__mini(gy__mini(in->x1, clip.x + clip.w), gy__c.w); *y1 = gy__mini(gy__mini(in->y1, clip.y + clip.h), gy__c.h);
    return *x1 > *x0 && *y1 > *y0;
}

static gy_rect gy__isect(gy_rect a, gy_rect b) {
    gy_rect r;
    r.x = gy__maxi(a.x, b.x); r.y = gy__maxi(a.y, b.y);
    r.w = gy__mini(a.x + a.w, b.x + b.w) - r.x; r.h = gy__mini(a.y + a.h, b.y + b.h) - r.y;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

static void gy__draw_inst(const gy__inst *in, gy_rect clip) {
    if (in->kind == GY__K_BOX) gy__draw_box_cpu(in, clip);
    else if (in->kind == GY__K_GLYPH) gy__draw_glyph_cpu(in, clip);
}

static void gy__render(void) {
    int i, t, tx, ty, nt, x0, y0, x1, y1, full;
    uint64_t seed;
    gy__c.tiles_x = (gy__c.w + GY__TILE - 1) / GY__TILE;
    gy__c.tiles_y = (gy__c.h + GY__TILE - 1) / GY__TILE;
    nt = gy__c.tiles_x * gy__c.tiles_y;
    full = !gy__tiles_ready() || gy__c.tiles_off || gy__c.want_full;   /* (tiles_ready first: it sets up the tables) */
    if (full) {                                /* everything, the plain way */
        gy_clear();
        for (i = 0; i < gy__c.ninst; i++) gy__draw_inst(&gy__c.inst[i], gy__c.clips[gy__c.inst[i].clip]);
        gy__c.dirty_tiles = nt;
        if (gy__c.tile_prev) gy__c.want_full = 0;   /* the next frame can compare */
    }
    if (!gy__c.tile_prev) return;
    seed = gy__mix(0xC0FFEEull, gy__c.theme.bg);
    for (t = 0; t < nt; t++) gy__c.tile_cur[t] = seed;
    for (i = 0; i < gy__c.ninst; i++) {
        const gy__inst *in = &gy__c.inst[i];
        gy_rect clip = gy__c.clips[in->clip];
        uint64_t h;
        if (!gy__inst_box(in, clip, &x0, &y0, &x1, &y1)) continue;
        h = gy__inst_hash(in, clip);
        for (ty = y0 / GY__TILE; ty <= (y1 - 1) / GY__TILE; ty++)
            for (tx = x0 / GY__TILE; tx <= (x1 - 1) / GY__TILE; tx++) {
                t = ty * gy__c.tiles_x + tx;
                gy__c.tile_cur[t] = gy__mix(gy__c.tile_cur[t], h);
            }
    }
    if (!full) {
        gy__c.dirty_tiles = 0;
        for (t = 0; t < nt; t++) {
            gy__c.tile_dirty[t] = gy__c.tile_cur[t] != gy__c.tile_prev[t];
            if (gy__c.tile_dirty[t]) {         /* clear the changed tile to the background */
                int yy, ax = (t % gy__c.tiles_x) * GY__TILE, ay = (t / gy__c.tiles_x) * GY__TILE;
                int aw = gy__mini(GY__TILE, gy__c.w - ax), ah = gy__mini(GY__TILE, gy__c.h - ay);
                for (yy = ay; yy < ay + ah; yy++) gy__c.k_fill(gy__c.px + (size_t)yy * gy__c.w + ax, aw, gy__c.theme.bg);
                gy__c.dirty_tiles++;
            }
        }
        if (gy__c.dirty_tiles)
            for (i = 0; i < gy__c.ninst; i++) {
                const gy__inst *in = &gy__c.inst[i];
                gy_rect clip = gy__c.clips[in->clip], tr;
                if (!gy__inst_box(in, clip, &x0, &y0, &x1, &y1)) continue;
                for (ty = y0 / GY__TILE; ty <= (y1 - 1) / GY__TILE; ty++)
                    for (tx = x0 / GY__TILE; tx <= (x1 - 1) / GY__TILE; tx++) {
                        if (!gy__c.tile_dirty[ty * gy__c.tiles_x + tx]) continue;
                        tr.x = tx * GY__TILE; tr.y = ty * GY__TILE; tr.w = GY__TILE; tr.h = GY__TILE;
                        gy__draw_inst(in, gy__isect(clip, tr));
                    }
            }
    } else for (t = 0; t < nt; t++) gy__c.tile_dirty[t] = 1;
    for (t = 0; t < nt; t++) gy__c.tile_prev[t] = gy__c.tile_cur[t];
}

void gy_tiles(int on) { gy__c.tiles_off = !on; gy__c.want_full = 1; }

static void gy__present(void);

void gy_end(void) {
    if (!gy__c.ready || !gy__c.px) return;
    gy__c.in_frame = 0;
    gy__render();
    gy__c.frames++;
    if (gy__c.dirty_tiles) { gy__c.frames_drawn++; if (gy__c.hwnd) gy__present(); }
    if (gy__c.released[0]) gy__c.active = 0;
    gy__memset(gy__c.pressed, 0, sizeof gy__c.pressed);
    gy__memset(gy__c.released, 0, sizeof gy__c.released);
}

/* ================================================================================================
   The window: a DIB section is the frame; dirty tiles are copied to the screen with BitBlt
   ================================================================================================ */

static void gy__present(void) {
    HDC wdc = GetDC(gy__c.hwnd);
    int ty, tx;
    if (!wdc) return;
    for (ty = 0; ty < gy__c.tiles_y; ty++)
        for (tx = 0; tx < gy__c.tiles_x; tx++) {
            int run;
            if (!gy__c.tile_dirty[ty * gy__c.tiles_x + tx]) continue;
            for (run = tx; run + 1 < gy__c.tiles_x && gy__c.tile_dirty[ty * gy__c.tiles_x + run + 1]; run++) {}
            BitBlt(wdc, tx * GY__TILE, ty * GY__TILE, gy__mini((run + 1) * GY__TILE, gy__c.w) - tx * GY__TILE,
                   gy__mini(GY__TILE, gy__c.h - ty * GY__TILE), gy__c.memdc, tx * GY__TILE, ty * GY__TILE, SRCCOPY);
            tx = run;
        }
    ReleaseDC(gy__c.hwnd, wdc);
}

static int gy__dib(int w, int h) {
    BITMAPINFO bi;
    void *bits = NULL;
    HBITMAP nb;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    gy__memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;  /* top-down, like the frame */
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    nb = CreateDIBSection(gy__c.memdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!nb) { gy__set_error("GUICy could not make the window's frame", "CreateDIBSection failed"); return 0; }
    SelectObject(gy__c.memdc, nb);
    if (gy__c.dib) DeleteObject(gy__c.dib);
    gy__c.dib = nb; gy__c.px = (uint32_t *)bits; gy__c.w = w; gy__c.h = h;
    gy__c.want_full = 1;
    return 1;
}

static LRESULT CALLBACK gy__wndproc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!gy__c.tracking) {
            TRACKMOUSEEVENT te;
            te.cbSize = sizeof te; te.dwFlags = TME_LEAVE; te.hwndTrack = hw; te.dwHoverTime = 0;
            gy__c.tracking = TrackMouseEvent(&te);
        }
        gy_input_mouse((short)LOWORD(lp), (short)HIWORD(lp)); gy__c.passes = 2; return 0;
    case WM_MOUSELEAVE: gy__c.tracking = 0; gy_input_mouse(-32768, -32768); gy__c.passes = 2; return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        SetCapture(hw);
        gy_input_button(msg == WM_LBUTTONDOWN ? 0 : msg == WM_RBUTTONDOWN ? 1 : 2, 1); gy__c.passes = 2; return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        ReleaseCapture();
        gy_input_button(msg == WM_LBUTTONUP ? 0 : msg == WM_RBUTTONUP ? 1 : 2, 0); gy__c.passes = 2; return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) { gy__c.resize_w = LOWORD(lp); gy__c.resize_h = HIWORD(lp); gy__c.passes = 2; }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        RECT r = { 0, 0, gy__c.min_w, gy__c.min_h };
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        if (gy__c.min_w) { mm->ptMinTrackSize.x = r.right - r.left; mm->ptMinTrackSize.y = r.bottom - r.top; }
        return 0;
    }
    case WM_ERASEBKGND: return 1;              /* the frame covers everything */
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        if (gy__c.memdc) BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
                                ps.rcPaint.bottom - ps.rcPaint.top, gy__c.memdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_CLOSE: gy__c.closed = 1; return 0;
    case WM_APP + 1: gy__c.passes = gy__maxi(gy__c.passes, 1); return 0;   /* gy_wake */
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int gy_open(const gy_desc *d) {
    WNDCLASSEXW wc;
    WCHAR title[256];
    RECT r;
    if (!gy__init()) return 0;
    gy__memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = gy__wndproc; wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW); wc.lpszClassName = L"GUICy";
    wc.style = CS_DBLCLKS;
    RegisterClassExW(&wc);
    if (!MultiByteToWideChar(CP_UTF8, 0, d->title ? d->title : "GUICy", -1, title, 256)) title[0] = 0;
    gy__c.min_w = d->min_w; gy__c.min_h = d->min_h;
    r.left = 0; r.top = 0; r.right = d->w > 0 ? d->w : 900; r.bottom = d->h > 0 ? d->h : 600;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    gy__c.memdc = CreateCompatibleDC(NULL);
    if (!gy__c.memdc || !gy__dib(d->w > 0 ? d->w : 900, d->h > 0 ? d->h : 600)) return 0;
    gy__c.hwnd = CreateWindowExW(0, L"GUICy", title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    if (!gy__c.hwnd) { gy__set_error("GUICy could not make its window", "CreateWindowExW failed"); return 0; }
    ShowWindow(gy__c.hwnd, SW_SHOWDEFAULT);
    gy__c.passes = 2;
    return 1;
}

void gy_animate(int ms) {
    DWORD at = GetTickCount() + (DWORD)(ms < 0 ? 0 : ms);
    if (!gy__c.anim_on || (int)(at - gy__c.anim_at) < 0) { gy__c.anim_at = at; gy__c.anim_on = 1; }
}

void gy_wake(void) { if (gy__c.hwnd) PostMessageW(gy__c.hwnd, WM_APP + 1, 0, 0); }

int gy_frame(void) {
    MSG m;
    if (!gy__c.hwnd) return 0;
    if (gy__c.in_frame) gy_end();
    for (;;) {                                 /* sleep until there is something to do */
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) gy__c.closed = 1;
            TranslateMessage(&m); DispatchMessageW(&m);
        }
        if (gy__c.closed) return 0;
        if (gy__c.anim_on && (int)(GetTickCount() - gy__c.anim_at) >= 0) { gy__c.anim_on = 0; gy__c.passes = gy__maxi(gy__c.passes, 1); }
        if (gy__c.passes > 0) break;
        MsgWaitForMultipleObjectsEx(0, NULL, gy__c.anim_on ? gy__maxi(0, (int)(gy__c.anim_at - GetTickCount())) : INFINITE,
                                    QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    gy__c.passes--;                            /* an input event gets two passes: the second settles */
    if (gy__c.resize_w && (gy__c.resize_w != gy__c.w || gy__c.resize_h != gy__c.h)) gy__dib(gy__c.resize_w, gy__c.resize_h);
    gy__c.resize_w = gy__c.resize_h = 0;
    gy_begin();
    return 1;
}

/* ================================================================================================
   Output
   ================================================================================================ */

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
    int i;
    for (i = 0; i < gy__c.nfonts; i++) if (gy__c.fonts[i].hf) DeleteObject(gy__c.fonts[i].hf);
    if (gy__c.dc) DeleteDC(gy__c.dc);
    if (gy__c.hwnd) DestroyWindow(gy__c.hwnd);
    if (gy__c.dib) DeleteObject(gy__c.dib);
    if (gy__c.memdc) DeleteDC(gy__c.memdc);
    if (gy__c.perm.base) VirtualFree(gy__c.perm.base, 0, MEM_RELEASE);
    if (gy__c.frame.base) VirtualFree(gy__c.frame.base, 0, MEM_RELEASE);
    gy__memset(&gy__c, 0, sizeof gy__c);
}

#endif /* GUICY_IMPLEMENTED */
#endif /* GUICY_IMPLEMENTATION */

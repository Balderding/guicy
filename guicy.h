/* guicy.h - GUICy, an immediate-mode GUI library in plain C.  https://github.com/Balderding/guicy (private)

   Use it in ONE .c file of your program:
       #define GUICY_IMPLEMENTATION
       #include "guicy.h"
   and include it without the define anywhere else. Link with -lgdi32 -luser32.

   Options (define before the include, in the implementation file):
       GUICY_USE_CRT   use the C library's memset/memcpy instead of GUICy's own (for bisecting bugs)
       GUICY_NO_ASM    no hand-written assembly (the one kernel that won its race, see BENCH.md)
       GUICY_NO_CRT    the program has no C library: GUICy defines memcpy, memset and memmove, which
                       the compiler calls for struct copies (GUICy itself calls no C library function)

   Pixels are premultiplied BGRA8 in a uint32_t: 0xAARRGGBB, so the bytes in memory are B, G, R, A.
   Colours are in sRGB space; blending is not linear (it matches what GDI shows).

   A program:
       gy_open(&(gy_desc){ .title = "Demo", .w = 900, .h = 600 });
       while (gy_frame()) {                         sleeps until something happens
           gy_row(2, (float[]){ 120, -1 });         one row: 120 px, then the rest
           if (gy_button("Save")) save();
           gy_check("Pixel look", &pixel);
       }

   Status: step 4 of PLAN.md: every v1 widget, layers (popups, menus, tooltips), split panes,
   scroll regions, images, the canvas, and tiling (each part of a tool in its own tile).
*/
#ifndef GUICY_H
#define GUICY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GUICY_VERSION "0.4.0"

typedef uint32_t gy_color;                     /* premultiplied 0xAARRGGBB */
#define GY_RGB(r, g, b) ((gy_color)(0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

typedef struct gy_rect { int x, y, w, h; } gy_rect;

/* The theme: named colours and a few sizes. No default colour is red (the user can't read red text). */
typedef struct gy_theme {
    gy_color bg;          /* the window behind everything */
    gy_color panel;       /* a panel or tile body */
    gy_color card;        /* a raised area inside a panel; tile title bars */
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
    gy_color tip;         /* tooltip and popup background */
    gy_color shadow;      /* drop shadows (with alpha) */
    int pad;              /* space inside the frame or panel edge, px */
    int gap;              /* space between widgets, px */
    int row_h;            /* the height of one row of widgets, px */
    int radius;           /* corner radius of buttons and fields, px */
    int font_px;          /* the default font's height, px */
} gy_theme;
enum { GY_COL_BG, GY_COL_PANEL, GY_COL_CARD, GY_COL_BORDER, GY_COL_TEXT, GY_COL_TEXT_DIM, GY_COL_TEXT_OFF,
       GY_COL_HEADING, GY_COL_BUTTON, GY_COL_BUTTON_HOT, GY_COL_BUTTON_DOWN, GY_COL_ACCENT, GY_COL_ACCENT_HOT,
       GY_COL_ACCENT_TEXT, GY_COL_FIELD, GY_COL_FIELD_HOT, GY_COL_SELECT, GY_COL_GOOD, GY_COL_WARN, GY_COL_BAD,
       GY_COL_DIVIDER, GY_COL_DIVIDER_HOT, GY_COL_TIP, GY_COL_SHADOW, GY_COL_COUNT };

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

/* --- an app's own window and loop -------------------------------------------------------------- */
int  gy_attach(void *hwnd);                   /* draw into the app's window (an HWND); 1 on success */
int  gy_event(void *hwnd, unsigned msg, uintptr_t wp, intptr_t lp);   /* from the window procedure:
                                                 1 = GUICy took it (return 0 from the procedure) */
int  gy_wants_frame(void);                    /* 1: run gy_begin ... gy_end now (input, a timer, gy_wake) */

/* --- the frame without a window (tests, pictures) ---------------------------------------------- */
int  gy_offscreen(int w, int h);              /* draw into memory; 1 on success */
void gy_begin(void);                          /* start a frame */
void gy_end(void);                            /* lay out and draw the frame */
void gy_clear(void);
int  gy_width(void);
int  gy_height(void);
const uint32_t *gy_pixels(void);              /* the frame, w*h, rows top to bottom */
int  gy_shot(const char *path);               /* write the frame as a 32-bit BMP; 1 on success */
void gy_shutdown(void);

/* --- input (a window feeds these itself; tests and scripts call them) ------------------------- */
void gy_input_mouse(int x, int y);
void gy_input_button(int button, int down);   /* 0 left, 1 right, 2 middle */
void gy_input_wheel(float notches);           /* + is away from the user (scroll up) */
void gy_input_key(int key, int down);         /* GY_KEY_* or 'A'..'Z', '0'..'9' (Windows key codes) */
void gy_input_char(uint32_t codepoint);       /* typed text */
void gy_input_time(unsigned ms);              /* the clock, for tests; a window uses GetTickCount */
int  gy_key_pressed(int key);                 /* the app's own keys: 1 if pressed this frame and GUICy
                                                 didn't use it (an edit box has focus, or a tile key) */
int  gy_mods(void);                           /* GY_MOD_* held now */
enum { GY_KEY_BACK = 0x08, GY_KEY_TAB = 0x09, GY_KEY_ENTER = 0x0D, GY_KEY_SHIFT = 0x10, GY_KEY_CTRL = 0x11,
       GY_KEY_ALT = 0x12, GY_KEY_ESC = 0x1B, GY_KEY_SPACE = 0x20, GY_KEY_PGUP = 0x21, GY_KEY_PGDN = 0x22,
       GY_KEY_END = 0x23, GY_KEY_HOME = 0x24, GY_KEY_LEFT = 0x25, GY_KEY_UP = 0x26, GY_KEY_RIGHT = 0x27,
       GY_KEY_DOWN = 0x28, GY_KEY_DELETE = 0x2E, GY_KEY_F1 = 0x70, GY_KEY_PLUS = 0xBB, GY_KEY_MINUS = 0xBD };
enum { GY_MOD_SHIFT = 1, GY_MOD_CTRL = 2, GY_MOD_ALT = 4 };
#define GY_KEYCODE(mods, key) ((mods) << 16 | (key))   /* a key with its modifiers, for gy_keys_owned */

/* --- layout ------------------------------------------------------------------------------------- */
/* The next widgets go into n columns. A width > 1 is pixels, 0 < w <= 1 a share of the row,
   w <= 0 an equal part of what is left. gy_row(0, NULL) or no call at all: one full-width column. */
void gy_row(int n, const float *widths);
void gy_row_h(int h);                         /* the height of the rows from here (0: the theme's) */
void gy_grid(int cell_w);                     /* as many columns of about cell_w as the width holds */
void gy_spacer(void);                         /* skip one cell */
gy_rect gy_last_rect(void);                   /* where the last widget went */
gy_rect gy_avail(void);                       /* what is left of the area below the current row */

/* --- widgets ------------------------------------------------------------------------------------ */
/* Labels are IDs: "Save##top" and "Save##bottom" show the same text and are two widgets. */
void gy_text(const char *fmt, ...);           /* %d %i %u %x %c %s %f %.Nf %% */
void gy_text_col(gy_color c, const char *fmt, ...);
void gy_text_wrap(const char *text);          /* wrapped to the cell's width; the row grows to fit */
void gy_section(const char *title);           /* a heading row */
int  gy_button(const char *label);            /* 1 on the frame it was clicked */
int  gy_button_key(const char *label, int key);   /* also when key is pressed (outside edit boxes) */
int  gy_button_accent(const char *label);     /* the primary-action look */
int  gy_check(const char *label, int *on);    /* 1 when it changed *on */
int  gy_select(const char *label, int on);    /* drawn chosen when on; 1 when clicked */
int  gy_chips(const char *const *items, int n, int *sel);   /* one choice of n, in the current columns */
int  gy_slider(const char *label, float *v, float lo, float hi);   /* drag, wheel; 1 when changed */
int  gy_slider_i(const char *label, int *v, int lo, int hi);
int  gy_number(const char *label, float *v, float step, float lo, float hi);  /* drag, wheel, -/+, double-click to type */
int  gy_number_i(const char *label, int *v, int lo, int hi);
int  gy_combo(const char *label, int *sel, const char *items);  /* items "a\0b\0c\0"; 1 when changed */
enum { GY_EDIT_CHANGED = 1, GY_EDIT_ENTER = 2 };
int  gy_edit(const char *label, char *buf, int cap);  /* one line; returns GY_EDIT_* flags */
void gy_unfocus(void);                        /* no edit box has the keys any more */
void gy_progress(float done);                 /* 0..1 */
void gy_tip(const char *text);                /* a tooltip for the last widget, after 500 ms */
void gy_disable_begin(void);                  /* the widgets until _end are greyed and ignore input */
void gy_disable_end(void);
void gy_id_push(int i);                       /* labels repeat in loops: push the index */
void gy_id_pop(void);
void gy_push_col(int which, gy_color c);      /* change one theme colour (GY_COL_*) until gy_pop_col */
void gy_pop_col(void);
void gy_error(const char *text);              /* a banner at the bottom of the window ("" clears it) */

/* --- containers ----------------------------------------------------------------------------------- */
void gy_scroll_begin(const char *id, int h);  /* a scrolled region h px high (h <= 0: the rest) */
void gy_scroll_end(void);
void gy_scroll_rows(int n, int *first, int *last);  /* in a scroll region: of n one-line rows, only
                                                 first..last are on screen; draw those, the rest take their space */
int  gy_tree(const char *label);              /* a fold-out row; 1 when open (then call gy_tree_end) */
void gy_tree_end(void);
void gy_panel_begin(void);                    /* a card around the widgets until _end */
void gy_panel_end(void);
enum { GY_H = 0, GY_V = 1 };                  /* side by side, stacked */
void gy_split(const char *id, int dir, float *pos, float def);  /* two panes; *pos = the first one's px */
void gy_pane(void);                           /* now the second pane */
void gy_split_end(void);

/* --- popups and menus (they sit on a layer above everything) ------------------------------------ */
int  gy_context_begin(void);                  /* after a widget: 1 while its right-click menu is open */
int  gy_menu_item(const char *label);         /* 1 when clicked (the menu closes) */
void gy_context_end(void);
int  gy_popup_begin(const char *id, gy_rect r);   /* a layer of your own at r, open until gy_popup_close */
void gy_popup_end(void);
void gy_popup_open(const char *id);
void gy_popup_close(void);

/* --- tiling: each part of a tool in its own tile; the tiles always fill the window -------------- */
int  gy_tile(const char *name);               /* 1 when shown: then put the tile's widgets here */
void gy_tile_open(const char *name);          /* bring it in (beside the focused tile, focus stays) */
void gy_tile_close(const char *name);
void gy_tile_focus(const char *name);
int  gy_tiles_save(char *buf, int cap);       /* the layout as one line, for the app's ini */
int  gy_tiles_load(const char *line);
int  gy_tile_keys_save(char *buf, int cap);   /* the tile keys as one line, for the app's ini */
int  gy_tile_keys_load(const char *line);
void gy_keys_owned(const int *keys, int n);   /* the app's own keys (GY_KEYCODE): tile keys never take them */

/* --- images and the canvas ---------------------------------------------------------------------- */
enum { GY_FMT_RGBA, GY_FMT_BGRA, GY_FMT_BGRA_PM };   /* straight RGBA (stb_image), straight BGRA, premultiplied */
int  gy_image_make(int w, int h, const void *px, int fmt);   /* returns an image id (> 0), 0 on failure */
int  gy_image_update(int img, const void *px, int fmt);
void gy_image_free(int img);
void gy_image(int img, int w, int h);         /* an image widget */
void gy_canvas_rect(gy_rect r, gy_color fill, float radius);
void gy_canvas_box(gy_rect r, gy_color fill, gy_color border, float radius, float border_w);
void gy_canvas_circle(int cx, int cy, int r, gy_color c);
void gy_canvas_line(float x0, float y0, float x1, float y1, float width, gy_color c);
void gy_canvas_dash(float x0, float y0, float x1, float y1, float width, float dash, gy_color c);
void gy_canvas_text(int x, int y, const char *s, gy_color c);    /* x, y = the top-left of the line */
void gy_canvas_image(int img, gy_rect r, int smooth);            /* smooth 0: nearest, 1: bilinear */
int  gy_hovered(void);                        /* the mouse is over the last widget */
int  gy_clicked(int button);                  /* the last widget was clicked with that button */
int  gy_text_width(const char *s);
int  gy_line_height(void);
/* older names */
#define gy_draw_box gy_canvas_box
#define gy_draw_text gy_canvas_text

/* --- test builds only (GUICY_TESTS): every text drawn in the last frame, for contrast checks ----- */
#ifdef GUICY_TESTS
typedef struct gy_test_text { gy_rect r; gy_color fg; int inst; char s[48]; } gy_test_text;
int gy_test_texts(const gy_test_text **out);  /* the last frame's texts, in drawing order */
gy_color gy_test_text_bg(int i);              /* the opaque colour under text i (the box drawn under it) */
#endif

/* --- fonts, theme, kernels, stats, errors ------------------------------------------------------ */
int  gy_font_add(const char *face, int px, int bold);   /* returns its index; 0 is the theme's font */
void gy_font(int i);                                  /* text from here on uses font i */
const gy_theme *gy_theme_get(void);
void gy_theme_set(const gy_theme *t);
enum { GY_ISA_SCALAR, GY_ISA_SSE2, GY_ISA_AVX2 };
int  gy_isa_set(int isa);                     /* returns the one in use (lower if the CPU can't) */
const char *gy_isa_name(void);
typedef struct gy_stats {
    int instances, glyph_misses, fonts, atlas_pages;
    int tiles, dirty_tiles;                   /* this frame: all tiles, tiles drawn again */
    int frames, frames_drawn;                 /* since start: frames, frames that changed a pixel */
    unsigned next_wake_ms;                    /* when gy_animate wants the next frame (0: never) */
} gy_stats;
gy_stats gy_get_stats(void);
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

/* internal helpers some builds don't call */
#define GY__FN static __attribute__((unused))
/* keep the compiler from turning a helper's own loop into a call to the C library's version */
#if defined(__clang__)
#define GY__NO_BUILTIN(f) __attribute__((no_builtin(f)))
#else
#define GY__NO_BUILTIN(f) __attribute__((optimize("no-tree-loop-distribute-patterns")))
#endif

/* --- no C library inside GUICy: its own small helpers ----------------------------------------- */
#ifdef GUICY_USE_CRT
#include <string.h>
#define gy__memset memset
#define gy__memcpy memcpy
#define gy__memmove memmove
#else
GY__NO_BUILTIN("memset") GY__FN void *gy__memset(void *d, int c, size_t n) {
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
GY__NO_BUILTIN("memcpy") GY__FN void *gy__memcpy(void *d, const void *s, size_t n) {
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}
GY__NO_BUILTIN("memmove") GY__FN void *gy__memmove(void *d, const void *s, size_t n) {
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    if (p < q) while (n--) *p++ = *q++;
    else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
#endif

#ifdef GUICY_NO_CRT
GY__NO_BUILTIN("memcpy") void *memcpy(void *d, const void *s, size_t n) { return gy__memcpy(d, s, n); }
GY__NO_BUILTIN("memset") void *memset(void *d, int c, size_t n) { return gy__memset(d, c, n); }
GY__NO_BUILTIN("memmove") void *memmove(void *d, const void *s, size_t n) { return gy__memmove(d, s, n); }
#endif

static int gy__mini(int a, int b) { return a < b ? a : b; }
static int gy__maxi(int a, int b) { return a > b ? a : b; }
static int gy__clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float gy__minf(float a, float b) { return a < b ? a : b; }
static float gy__maxf(float a, float b) { return a > b ? a : b; }
static float gy__clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float gy__absf(float a) { return a < 0 ? -a : a; }
static float gy__sqrtf(float a) { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(a))); }
GY__NO_BUILTIN("strlen") static int gy__strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int gy__streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void gy__strcpy(char *d, const char *s, int cap) { int i = 0; while (s[i] && i < cap - 1) { d[i] = s[i]; i++; } d[i] = 0; }

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
enum { GY__K_BOX = 1, GY__K_GLYPH = 2, GY__K_LINE = 3, GY__K_IMAGE = 4 };
enum { GY__LAYERS = 3 };                       /* 0 the window, 1 popups and menus, 2 tooltips and banners */

typedef struct gy__inst {
    int16_t x0, y0, x1, y1;                    /* pixel-snapped rect, x1/y1 exclusive */
    uint16_t u, v;                             /* atlas position of a glyph or icon */
    uint16_t clip;                             /* index into the clip table */
    uint8_t kind, page;                        /* GY__K_*, atlas page */
    gy_color col0, col1;                       /* fill (or glyph colour), border */
    float radius, border, soft;                /* corner radius, border width; LINE: half width, ax, ay */
    uint32_t a, b, c;                          /* LINE: bx, by as float bits; IMAGE: id, generation, smooth */
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

/* --- per-widget state that lives between frames (scroll, edit, tree, ...) ------------------------ */
#define GY__STATES 1024
typedef struct gy__st {
    uint32_t id;
    int seen;                                  /* the frame it was last used */
    float f0, f1, f2;
    int i0, i1, i2;
} gy__st;

/* --- layout: a stack of areas (scroll regions, panes, tiles, trees) ----------------------------- */
typedef struct gy__lay {
    gy_rect area;                              /* where widgets go */
    int cur_y, row_h, row_extra;               /* the current row's top, its height, wrapped-text growth */
    int ncols, col;
    float widths[16];
    int col_x[17];
    int clip;                                  /* clip index for this area */
    int kind;                                  /* GY__L_* */
    uint32_t id;
    int top;                                   /* where the container started (scroll: content top) */
    int tail;                                  /* scroll: the height of rows gy_scroll_rows skipped at the end */
    int bg_inst;                               /* a background instance patched at the end (-1: none) */
    float split_pos;
    gy_rect pane_b;                            /* a split's second pane, waiting for gy_pane */
} gy__lay;
enum { GY__L_ROOT, GY__L_SCROLL, GY__L_TREE, GY__L_PANEL, GY__L_PANE_A, GY__L_PANE_B, GY__L_TILE, GY__L_POPUP };

/* --- tiling --------------------------------------------------------------------------------------- */
#define GY__TNODES 64
#define GY__TILEMAX 32
#define GY__TABS 8
typedef struct gy__tnode {
    int used, leaf, parent, a, b, dir;         /* a split has children a and b; dir GY_H / GY_V */
    float ratio;
    int ntabs, active;
    int tabs[GY__TABS];                        /* indexes into the tile list */
    gy_rect r;                                 /* from the last layout */
} gy__tnode;
typedef struct gy__tile {
    uint32_t id;
    char name[40];
    int seen, hidden, from_load;
} gy__tile;
enum { GY__TA_FOCUS_L, GY__TA_FOCUS_R, GY__TA_FOCUS_U, GY__TA_FOCUS_D, GY__TA_SWAP_L, GY__TA_SWAP_R, GY__TA_SWAP_U,
       GY__TA_SWAP_D, GY__TA_GROW, GY__TA_SHRINK, GY__TA_FLIP, GY__TA_ZOOM, GY__TA_CLOSE, GY__TA_TAB, GY__TA_COUNT };

/* --- images ------------------------------------------------------------------------------------- */
#define GY__IMAGES 256
typedef struct gy__image { int w, h; uint32_t gen; uint32_t *px; } gy__image;

/* --- the OS functions GUICy calls where a test may want a stand-in (the test font, the clock) ---- */
typedef struct gy__os_t {
    DWORD (WINAPI *glyph_outline)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2 *);
    BOOL (WINAPI *text_metrics)(HDC, LPTEXTMETRICW);
    DWORD (WINAPI *ticks)(void);
} gy__os_t;
static gy__os_t gy__os = { GetGlyphOutlineW, GetTextMetricsW, GetTickCount };

/* --- the context: one, implicit ---------------------------------------------------------------- */
typedef struct gy__ctx {
    int ready;
    gy__arena perm;                            /* lives as long as the context */
    gy__arena frame;                           /* emptied at every gy_begin */
    int w, h;
    uint32_t *px;                              /* the frame */
    uint32_t *off_px;                          /* the off-screen frame's own allocation */
    size_t off_cap;                            /* its size in pixels */
    gy_theme theme;
    char error[512];
    char banner[256];

    /* draw list */
    gy__inst *inst;
    int ninst, cap_inst, layer;
    int no_cull, culled;                       /* culling: off for a placeholder; how many were skipped */
#ifdef GUICY_TESTS
    gy_test_text texts[1024];
    int ntexts;
#endif
    gy__inst cull_scratch;                     /* where a culled push writes */
    gy_rect clips[256];
    int nclips;

    /* input */
    int mx, my, down[3], pressed[3], released[3], dbl[3];
    unsigned now, last_press_t[3];
    int last_press_x[3], last_press_y[3];
    int test_clock;
    float wheel;
    int keys_down[256];
    struct { int key, mods, used; } kev[64];
    int nkev;
    uint32_t chars[64];
    int nchars;
    int mods;

    /* ids and widget state */
    uint32_t id_stack[32];
    int id_depth;
    uint32_t active, focus, last_id, focus_claimed;
    int disabled, drag_x, drag_y;
    float drag_v;
    gy__st states[GY__STATES];
    struct { int which; gy_color old; } colstack[32];
    int ncolstack;

    /* layers: open popups and the rects that block the window under them */
    uint32_t popup_id, popup_owner;
    int popup_x, popup_y, popup_kind;          /* 1 combo, 2 context menu, 3 user popup */
    gy_rect block_prev[4], block_cur[4];
    int nblock_prev, nblock_cur;
    int popup_used;                            /* a popup was drawn this frame */
    uint32_t tip_id;
    unsigned tip_since;
    int tip_shown;

    /* layout */
    gy__lay lay[24];
    int nlay;
    gy_rect last;

    /* text */
    HDC dc;
    HFONT dc_font;                             /* the font selected into dc now */
    gy__font fonts[GY__MAX_FONTS];
    int nfonts, font;
    gy__glyph *glyphs;
    unsigned char *pages[GY__MAX_PAGES];
    int npages, shelf_x, shelf_y, shelf_h;
    unsigned char *scratch;                    /* GetGlyphOutline output */
    int glyph_misses;

    /* edit box */
    char undo[16][256];
    int undo_n, undo_kind;
    unsigned undo_t;
    char clip_text[1024];                      /* the clipboard when there is no window */

    /* tiling */
    gy__tnode tn[GY__TNODES];
    gy__tile tl[GY__TILEMAX];
    int ntl, troot, tfocus, tzoom, tlaid, tdrag, in_tile, tloaded;
    int tkeys[GY__TA_COUNT];
    int owned[64];
    int nowned;

    /* images */
    gy__image img[GY__IMAGES];

    /* dirty tiles: 64 x 64, each with the hash of what is drawn on it */
    int tiles_off, tiles_x, tiles_y, want_full;
    uint64_t *tile_prev, *tile_cur;
    unsigned char *tile_dirty;
    int dirty_tiles, frames, frames_drawn;

    /* the window */
    HWND hwnd;
    HBITMAP dib;
    HDC memdc;
    int closed, passes, in_frame, attached, leave_pending, resize_w, resize_h, min_w, min_h, tracking;
    unsigned anim_at;
    int anim_on;
    WCHAR surrogate;

    /* kernels */
    int isa;
    void (*k_fill)(uint32_t *d, int n, uint32_t s);
    void (*k_row)(uint32_t *d, const uint32_t *s, int n);
    void (*k_glyph)(uint32_t *d, const unsigned char *cov, int n, uint32_t col);
} gy__ctx;

static gy__ctx gy__c;
#define GY__T (gy__c.theme)
#define GY__L (gy__c.lay[gy__c.nlay - 1])

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
    int n = 0, i;
    for (i = 0; what[i] && n < (int)sizeof gy__c.error - 1; i++) gy__c.error[n++] = what[i];
    if (detail) {
        if (n < (int)sizeof gy__c.error - 3) { gy__c.error[n++] = ':'; gy__c.error[n++] = ' '; }
        for (i = 0; detail[i] && n < (int)sizeof gy__c.error - 1; i++) gy__c.error[n++] = detail[i];
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

/* unaligned loads that are plain loads (not a call to GUICy's byte-by-byte copy) */
typedef int32_t gy__u32u __attribute__((aligned(1), may_alias));
typedef int64_t gy__u64u __attribute__((aligned(1), may_alias));

/* Assembly that won its race (bench/gybench.c, BENCH.md step 5): `rep stosd` fills an opaque run of
   512+ pixels 55-68 % faster than the AVX2 store loop on this PC (fast strings, ERMSB), with the
   same bytes. Shorter runs are faster with the loop, so the kernels below pick by length. */
#ifndef GUICY_NO_ASM
/* void gy__asm_stosd(uint32_t *d, int n, uint32_t s), Windows x64: d in rcx, n in edx, s in r8d;
   rdi is callee-saved, so it is kept in r9 */
__asm__(".text\n"
        ".globl gy__asm_stosd\n"
        "gy__asm_stosd:\n"
        "    movq %rdi, %r9\n"
        "    movq %rcx, %rdi\n"
        "    movl %edx, %ecx\n"
        "    movl %r8d, %eax\n"
        "    rep stosl\n"
        "    movq %r9, %rdi\n"
        "    ret\n");
void gy__asm_stosd(uint32_t *d, int n, uint32_t s);
#define GY__STOSD_MIN 512
#endif

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
#ifdef GY__STOSD_MIN
    if ((s >> 24) == 255 && n >= GY__STOSD_MIN) { gy__asm_stosd(d, n, s); return; }
#endif
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
        int32_t k4 = *(const gy__u32u *)(const void *)(cov + i);
        __m128i k, dv, lo, hi;
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
#ifdef GY__STOSD_MIN
    if ((s >> 24) == 255 && n >= GY__STOSD_MIN) { gy__asm_stosd(d, n, s); return; }
#endif
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
    if (n < 16) { gy__glyph_sse2(d, cov, n, col); return; }   /* short glyph rows: SSE2 is faster (stress.md) */
    __m256i z = _mm256_setzero_si256();
    __m256i c16 = _mm256_unpacklo_epi8(_mm256_set1_epi32((int)col), z);
    for (; i + 8 <= n; i += 8) {
        int64_t k8 = *(const gy__u64u *)(const void *)(cov + i);
        __m128i k, klo, khi;
        __m256i kk, dv, lo, hi;
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
static void gy__tile_keys_default(void);

static int gy__ncorners, gy__corner_off;       /* the corner cache (below), whose pages live in the arena */

static int gy__init(void) {
    char isa[16];
    DWORD n;
    if (gy__c.ready) return 1;
    gy__memset(&gy__c, 0, sizeof gy__c);
    gy__ncorners = 0;                          /* its pages went with the last context's arena */
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
    gy__c.troot = gy__c.tfocus = gy__c.tzoom = gy__c.tdrag = -1;
    gy__tile_keys_default();
    gy__c.ready = 1;
    if (gy__font_make("Segoe UI", gy__c.theme.font_px, 0) < 0) return 0;
    return 1;
}

int gy_offscreen(int w, int h) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { gy__set_error("gy_offscreen", "the size must be 1..16384"); return 0; }
    if (!gy__init()) return 0;
    /* the frame has its own allocation, freed when the size changes (it used to come from the
       arena, which never frees, so every resize leaked a frame) */
    if (gy__c.off_px && (size_t)w * (size_t)h > gy__c.off_cap) { HeapFree(GetProcessHeap(), 0, gy__c.off_px); gy__c.off_px = NULL; }
    if (!gy__c.off_px) {
        gy__c.off_px = (uint32_t *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * (size_t)h * 4);
        gy__c.off_cap = gy__c.off_px ? (size_t)w * (size_t)h : 0;
    }
    gy__c.px = gy__c.off_px;
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
void gy_theme_set(const gy_theme *t) { if (gy__init()) { gy__c.theme = *t; gy__c.want_full = 1; } }
const char *gy_last_error(void) { return gy__c.error; }
void gy_error(const char *text) { gy__strcpy(gy__c.banner, text ? text : "", sizeof gy__c.banner); }

gy_stats gy_get_stats(void) {
    gy_stats s;
    s.instances = gy__c.ninst; s.glyph_misses = gy__c.glyph_misses; s.fonts = gy__c.nfonts; s.atlas_pages = gy__c.npages;
    s.tiles = gy__c.tiles_x * gy__c.tiles_y; s.dirty_tiles = gy__c.dirty_tiles; s.frames = gy__c.frames; s.frames_drawn = gy__c.frames_drawn;
    s.next_wake_ms = gy__c.anim_on ? gy__c.anim_at : 0;
    return s;
}

/* --- input ---------------------------------------------------------------------------------------- */
void gy_input_time(unsigned ms) { gy__c.now = ms; gy__c.test_clock = 1; }
void gy_input_mouse(int x, int y) { gy__c.mx = x; gy__c.my = y; }
void gy_input_wheel(float notches) { gy__c.wheel += notches; }
void gy_input_button(int b, int down) {
    if (b < 0 || b > 2) return;
    if (down && !gy__c.down[b]) {
        int dx = gy__c.mx - gy__c.last_press_x[b], dy = gy__c.my - gy__c.last_press_y[b];
        gy__c.pressed[b] = 1;
        if (gy__c.now - gy__c.last_press_t[b] < 400 && dx * dx + dy * dy < 25) gy__c.dbl[b] = 1;
        gy__c.last_press_t[b] = gy__c.now; gy__c.last_press_x[b] = gy__c.mx; gy__c.last_press_y[b] = gy__c.my;
    }
    if (!down && gy__c.down[b]) gy__c.released[b] = 1;
    gy__c.down[b] = down;
}
static void gy__mods_update(void) {
    gy__c.mods = (gy__c.keys_down[GY_KEY_SHIFT] ? GY_MOD_SHIFT : 0) | (gy__c.keys_down[GY_KEY_CTRL] ? GY_MOD_CTRL : 0)
               | (gy__c.keys_down[GY_KEY_ALT] ? GY_MOD_ALT : 0);
}
void gy_input_key(int key, int down) {
    if (key < 0 || key > 255) return;
    gy__c.keys_down[key] = down;
    gy__mods_update();
    if (down && key != GY_KEY_SHIFT && key != GY_KEY_CTRL && key != GY_KEY_ALT && gy__c.nkev < 64) {
        gy__c.kev[gy__c.nkev].key = key; gy__c.kev[gy__c.nkev].mods = gy__c.mods; gy__c.kev[gy__c.nkev].used = 0; gy__c.nkev++;
    }
}
void gy_input_char(uint32_t cp) { if (cp >= 32 && cp != 127 && gy__c.nchars < 64) gy__c.chars[gy__c.nchars++] = cp; }
int gy_mods(void) { return gy__c.mods; }

/* take a key event (key with exactly these modifiers) if nobody used it yet */
static int gy__take_key(int key, int mods) {
    int i;
    for (i = 0; i < gy__c.nkev; i++)
        if (!gy__c.kev[i].used && gy__c.kev[i].key == key && gy__c.kev[i].mods == mods) { gy__c.kev[i].used = 1; return 1; }
    return 0;
}
int gy_key_pressed(int key) {
    int i;
    if (gy__c.focus) return 0;                 /* an edit box has the keys */
    for (i = 0; i < gy__c.nkev; i++) if (!gy__c.kev[i].used && gy__c.kev[i].key == (key & 0xFFFF) && gy__c.kev[i].mods == (key >> 16)) return 1;
    return 0;
}

/* --- per-widget state ----------------------------------------------------------------------------- */
static gy__st *gy__state(uint32_t id) {
    uint32_t i = (id * 2654435761u) & (GY__STATES - 1), n;
    gy__st *oldest = NULL;
    for (n = 0; n < GY__STATES; n++, i = (i + 1) & (GY__STATES - 1)) {
        gy__st *s = &gy__c.states[i];
        if (s->id == id) { s->seen = gy__c.frames; return s; }
        if (!s->id) { gy__memset(s, 0, sizeof *s); s->id = id; s->seen = gy__c.frames; return s; }
        if (!oldest || s->seen < oldest->seen) oldest = s;
    }
    gy__memset(oldest, 0, sizeof *oldest);     /* full: reuse the one unused the longest */
    oldest->id = id; oldest->seen = gy__c.frames;
    return oldest;
}

/* --- the draw list -------------------------------------------------------------------------------- */
static gy__inst *gy__push(int kind, int x0, int y0, int x1, int y1) {
    gy__inst *in;
    if (!gy__c.no_cull) {                      /* nothing of it would show: it is not kept (it would add
                                                  no pixel and no hash to any tile, so the frame is the same) */
        gy_rect cl = gy__c.clips[gy__c.nlay ? GY__L.clip : 0];
        if (x1 <= cl.x || y1 <= cl.y || x0 >= cl.x + cl.w || y0 >= cl.y + cl.h || x1 <= x0 || y1 <= y0) {
            gy__c.culled++;
            gy__memset(&gy__c.cull_scratch, 0, sizeof gy__c.cull_scratch);
            return &gy__c.cull_scratch;        /* callers may write to it; it is never drawn */
        }
    }
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
    in->x0 = (int16_t)gy__clampi(x0, -32000, 32000); in->y0 = (int16_t)gy__clampi(y0, -32000, 32000);
    in->x1 = (int16_t)gy__clampi(x1, -32000, 32000); in->y1 = (int16_t)gy__clampi(y1, -32000, 32000);
    in->clip = (uint16_t)(gy__c.nlay ? GY__L.clip : 0);
    in->page = 0;
    in->c = (uint32_t)gy__c.layer << 24;       /* the layer rides in the top byte of c */
    return in;
}

static int gy__clip_push(gy_rect r) {
    gy_rect p = gy__c.clips[gy__c.nlay ? GY__L.clip : 0];
    gy_rect q;
    q.x = gy__maxi(p.x, r.x); q.y = gy__maxi(p.y, r.y);
    q.w = gy__maxi(0, gy__mini(p.x + p.w, r.x + r.w) - q.x); q.h = gy__maxi(0, gy__mini(p.y + p.h, r.y + r.h) - q.y);
    if (gy__c.nclips >= 256) return gy__c.nlay ? GY__L.clip : 0;
    gy__c.clips[gy__c.nclips] = q;
    return gy__c.nclips++;
}

void gy_canvas_box(gy_rect r, gy_color fill, gy_color border, float radius, float border_w) {
    gy__inst *in;
    if (r.w <= 0 || r.h <= 0 || (!fill && (!border || border_w <= 0))) return;
    in = gy__push(GY__K_BOX, r.x, r.y, r.x + r.w, r.y + r.h);
    if (!in) return;
    in->col0 = fill; in->col1 = border; in->radius = radius; in->border = border > 0 ? border_w : 0;
}
void gy_canvas_rect(gy_rect r, gy_color fill, float radius) { gy_canvas_box(r, fill, 0, radius, 0); }
void gy_canvas_circle(int cx, int cy, int r, gy_color c) { gy_rect b = { cx - r, cy - r, 2 * r, 2 * r }; gy_canvas_box(b, c, 0, (float)r, 0); }

static uint32_t gy__fbits(float f) { uint32_t u; gy__memcpy(&u, &f, 4); return u; }
static float gy__bitsf(uint32_t u) { float f; gy__memcpy(&f, &u, 4); return f; }

void gy_canvas_line(float x0, float y0, float x1, float y1, float width, gy_color c) {
    gy__inst *in;
    float hw = width * 0.5f + 1;
    if (!gy__init()) return;
    in = gy__push(GY__K_LINE, (int)(gy__minf(x0, x1) - hw), (int)(gy__minf(y0, y1) - hw), (int)(gy__maxf(x0, x1) + hw) + 1, (int)(gy__maxf(y0, y1) + hw) + 1);
    if (!in) return;
    in->col0 = c; in->radius = width * 0.5f; in->border = x0; in->soft = y0;
    in->a = gy__fbits(x1); in->b = gy__fbits(y1);
}

void gy_canvas_dash(float x0, float y0, float x1, float y1, float width, float dash, gy_color c) {
    float dx = x1 - x0, dy = y1 - y0, len = gy__sqrtf(dx * dx + dy * dy), t;
    if (len <= 0 || dash <= 0) return;
    for (t = 0; t < len; t += dash * 2) {
        float t1 = gy__minf(t + dash, len);
        gy_canvas_line(x0 + dx * t / len, y0 + dy * t / len, x0 + dx * t1 / len, y0 + dy * t1 / len, width, c);
    }
}

/* ================================================================================================
   Text: GDI glyphs cached in coverage pages
   ================================================================================================ */

static int gy__font_make(const char *face, int px, int bold) {
    WCHAR wface[64];
    TEXTMETRICW tm;
    gy__font *f;
    if (gy__c.nfonts >= GY__MAX_FONTS) { gy__set_error("gy_font_add", "16 fonts at most"); return -1; }
    f = &gy__c.fonts[gy__c.nfonts];
    gy__memset(f, 0, sizeof *f);
    if (!MultiByteToWideChar(CP_UTF8, 0, face, -1, wface, 64)) wface[0] = 0;
    f->hf = CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wface);
    if (!f->hf) { gy__set_error("gy_font_add could not make the font", face); return -1; }
    SelectObject(gy__c.dc, f->hf);
    gy__c.dc_font = f->hf;
    gy__os.text_metrics(gy__c.dc, &tm);
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
    if (gy__c.dc_font != gy__c.fonts[font].hf) { SelectObject(gy__c.dc, gy__c.fonts[font].hf); gy__c.dc_font = gy__c.fonts[font].hf; }
    /* two calls: the size, then the bitmap. (One call with a 1 MB buffer was tried in F1: GDI then
       takes 11 times as long and returns different pixels, so the size query stays.) */
    need = gy__os.glyph_outline(gy__c.dc, cp > 0xFFFF ? 0xFFFD : cp, GGO_GRAY8_BITMAP, &gm, 0, NULL, &ident);
    if (need == GDI_ERROR) { gy__memset(&gm, 0, sizeof gm); need = 0; }
    g->adv = (int16_t)gm.gmCellIncX;
    if (need && need <= (1u << 20) && gy__os.glyph_outline(gy__c.dc, cp > 0xFFFF ? 0xFFFD : cp, GGO_GRAY8_BITMAP, &gm, need, gy__c.scratch, &ident) != GDI_ERROR
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

static int gy__utf8_put(char *o, uint32_t c) {
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | c >> 6); o[1] = (char)(0x80 | (c & 63)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xE0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 63)); o[2] = (char)(0x80 | (c & 63)); return 3; }
    o[0] = (char)(0xF0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 63)); o[2] = (char)(0x80 | ((c >> 6) & 63)); o[3] = (char)(0x80 | (c & 63));
    return 4;
}

static const char *gy__label_end(const char *s) {   /* the shown part of a label stops at "##" */
    while (*s && !(s[0] == '#' && s[1] == '#')) s++;
    return s;
}

static int gy__text_w(const char *s, const char *end) {
    int w = 0;
    if (!gy__init()) return 0;
    while ((!end || s < end) && *s) w += gy__glyph_get(gy__c.font, gy__utf8_next(&s))->adv;
    return w;
}

int gy_text_width(const char *s) { return gy__text_w(s, NULL); }

static void gy__draw_text_n(int x, int y, const char *s, const char *end, gy_color col) {
    int base = y + gy__c.fonts[gy__c.font].ascent;
    gy_rect cl = gy__c.clips[gy__c.nlay ? GY__L.clip : 0];
    if (y >= cl.y + cl.h || y + gy__c.fonts[gy__c.font].height + 8 <= cl.y) { gy__c.culled++; return; }   /* the line is outside the clip */
#ifdef GUICY_TESTS
    if (gy__c.ntexts < 1024 && (!end || s < end) && *s) {
        gy_test_text *t = &gy__c.texts[gy__c.ntexts++];
        int k = 0;
        const char *q = s;
        while ((!end || q < end) && *q && k < 47) t->s[k++] = *q++;
        t->s[k] = 0;
        t->r.x = x; t->r.y = y; t->r.w = gy__text_w(s, end); t->r.h = gy__c.fonts[gy__c.font].height;
        t->fg = col; t->inst = gy__c.ninst;
    }
#endif
    while ((!end || s < end) && *s) {   /* end NULL: to the terminating zero */
        if (x >= cl.x + cl.w + 8) break;       /* the rest of the line is right of the clip (8: overhangs) */
        gy__glyph *g = gy__glyph_get(gy__c.font, gy__utf8_next(&s));
        if (g->ok && g->w) {
            gy__inst *in = gy__push(GY__K_GLYPH, x + g->ox, base - g->oy, x + g->ox + g->w, base - g->oy + g->h);
            if (!in) return;
            in->u = g->u; in->v = g->v; in->page = g->page; in->col0 = col;
        }
        x += g->adv;
    }
}

void gy_canvas_text(int x, int y, const char *s, gy_color c) { if (gy__init()) gy__draw_text_n(x, y, s, NULL, c); }

/* --- baked icons: drawn once into the atlas by distance to their strokes ------------------------ */
static float gy__seg_dist2(float px, float py, float ax, float ay, float bx, float by) {   /* squared */
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float t = (wx * vx + wy * vy) / (vx * vx + vy * vy);
    t = gy__maxf(0, gy__minf(1, t));
    wx -= vx * t; wy -= vy * t;
    return wx * wx + wy * wy;
}
static float gy__seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    return gy__sqrtf(gy__seg_dist2(px, py, ax, ay, bx, by));
}

/* one pixel of a line: the coverage is hw + 0.5 - distance; a pixel at R or more gets none, so its
   square root is skipped (a pixel just inside R gets under 1/510 coverage, which rounds to 0 too) */
static void gy__line_px(uint32_t *d, float px, float py, const float *g, gy_color col) {
    float d2 = gy__seg_dist2(px, py, g[0], g[1], g[2], g[3]), cov;
    if (d2 >= g[5]) return;
    cov = gy__clampf(g[4] + 0.5f - gy__sqrtf(d2), 0, 1);
    if (cov > 0) *d = gy__over(*d, gy__scale(col, (uint32_t)(cov * 255.0f + 0.5f)));
}

enum { GY__I_CHECK, GY__I_DOWN, GY__I_RIGHT, GY__I_MINUS, GY__I_PLUS, GY__I_CLOSE };
static const float gy__icon_pts[][7] = {       /* npoints-1 segments as a polyline on a 16 grid: x0 y0 x1 y1 x2 y2 (+ -1) */
    { 3.5f, 8.5f, 6.5f, 11.5f, 12.5f, 4.5f, 0 },
    { 4.5f, 6.5f, 8.0f, 10.0f, 11.5f, 6.5f, 0 },
    { 6.5f, 4.5f, 10.0f, 8.0f, 6.5f, 11.5f, 0 },
    { 4.0f, 8.0f, 12.0f, 8.0f, -1, -1, 0 },
    { 4.0f, 8.0f, 12.0f, 8.0f, -1, -1, 1 },    /* the plus adds a vertical stroke */
    { 4.5f, 4.5f, 11.5f, 11.5f, -1, -1, 2 },   /* the cross adds the other diagonal */
};

static gy__glyph *gy__icon(int kind, int size) {
    uint32_t key = (uint32_t)GY__ICON_FONT << 21 | (uint32_t)kind << 10 | (uint32_t)size;
    gy__glyph *g = gy__slot(key);
    const float *p = gy__icon_pts[kind];
    int page, u, v, x, y;
    float s = (float)size / 16.0f;
    if (g->key == key) return g;
    g->key = key;
    if (!gy__atlas_alloc(size, size, &page, &u, &v)) return g;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            float px = ((float)x + 0.5f) / s, py = ((float)y + 0.5f) / s, d = gy__seg_dist(px, py, p[0], p[1], p[2], p[3]), cov;
            if (p[4] >= 0) d = gy__minf(d, gy__seg_dist(px, py, p[2], p[3], p[4], p[5]));
            if (p[6] == 1) d = gy__minf(d, gy__seg_dist(px, py, 8, 4, 8, 12));
            if (p[6] == 2) d = gy__minf(d, gy__seg_dist(px, py, 11.5f, 4.5f, 4.5f, 11.5f));
            cov = gy__maxf(0, gy__minf(1, (1.1f - d) * s + 0.5f));
            gy__c.pages[page][(v + y) * GY__ATLAS + u + x] = (unsigned char)(cov * 255.0f + 0.5f);
        }
    g->u = (uint16_t)u; g->v = (uint16_t)v; g->w = g->h = (uint16_t)size; g->page = (uint8_t)page; g->ok = 1;
    return g;
}

static void gy__draw_icon(int kind, int x, int y, int size, gy_color col) {
    gy__glyph *g = gy__icon(kind, size);
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

static void gy__fmt(char *out, int cap, const char *f, ...) { va_list ap; va_start(ap, f); gy__vfmt(out, cap, f, ap); va_end(ap); }

/* ================================================================================================
   IDs and layout
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

void gy_push_col(int which, gy_color c) {
    gy_color *t = (gy_color *)(void *)&gy__c.theme;
    if (which < 0 || which >= GY_COL_COUNT || gy__c.ncolstack >= 32) return;
    gy__c.colstack[gy__c.ncolstack].which = which; gy__c.colstack[gy__c.ncolstack].old = t[which];
    gy__c.ncolstack++;
    t[which] = c;
}
void gy_pop_col(void) {
    gy_color *t = (gy_color *)(void *)&gy__c.theme;
    if (!gy__c.ncolstack) return;
    gy__c.ncolstack--;
    t[gy__c.colstack[gy__c.ncolstack].which] = gy__c.colstack[gy__c.ncolstack].old;
}

static void gy__row_layout(gy__lay *l) {
    int i, n = l->ncols, avail = l->area.w - GY__T.gap * (n - 1), fixed = 0, grow = 0;
    float x;
    int px[16];
    for (i = 0; i < n; i++) {
        float w = l->widths[i];
        if (w > 1) px[i] = (int)w;
        else if (w > 0) px[i] = (int)(w * (float)avail);
        else { px[i] = -1; grow++; continue; }
        fixed += px[i];
    }
    x = (float)l->area.x;
    for (i = 0; i < n; i++) {
        float w = px[i] >= 0 ? (float)px[i] : (float)gy__maxi(0, avail - fixed) / (float)grow;
        l->col_x[i] = (int)x;
        x += w + (float)GY__T.gap;
    }
    /* the end of the last column; with a growing column it is exactly the area's right edge */
    l->col_x[n] = grow ? l->area.x + l->area.w + GY__T.gap : (int)x;
}

static int gy__rowh(const gy__lay *l) { return l->row_h > 0 ? l->row_h : GY__T.row_h; }

static void gy__newline(gy__lay *l) {
    l->cur_y += gy__rowh(l) + l->row_extra + GY__T.gap;
    l->row_extra = 0;
    l->col = 0;
}

void gy_row(int n, const float *widths) {
    gy__lay *l = &GY__L;
    int i;
    if (n <= 0 || !widths) { n = 1; l->widths[0] = -1; }
    else for (i = 0; i < n && i < 16; i++) l->widths[i] = widths[i];
    if (l->col) gy__newline(l);                /* a row already started: go below it */
    l->ncols = n > 16 ? 16 : n;
    l->col = 0;
    gy__row_layout(l);
}

void gy_row_h(int h) { GY__L.row_h = h; }

void gy_grid(int cell_w) {
    float w[16];
    int n = gy__clampi((GY__L.area.w + GY__T.gap) / gy__maxi(1, cell_w + GY__T.gap), 1, 16), i;
    for (i = 0; i < n; i++) w[i] = -1;
    gy_row(n, w);
}

static gy_rect gy__next(void) {
    gy__lay *l = &GY__L;
    gy_rect r;
    if (l->col >= l->ncols) gy__newline(l);
    r.x = l->col_x[l->col];
    r.w = l->col_x[l->col + 1] - GY__T.gap - r.x;
    r.y = l->cur_y; r.h = gy__rowh(l);
    l->col++;
    gy__c.last = r;
    return r;
}

void gy_spacer(void) { gy__next(); }
gy_rect gy_last_rect(void) { return gy__c.last; }

gy_rect gy_avail(void) {
    gy__lay *l = &GY__L;
    gy_rect r;
    int y = l->col ? l->cur_y + gy__rowh(l) + l->row_extra + GY__T.gap : l->cur_y;
    r.x = l->area.x; r.y = y; r.w = l->area.w; r.h = gy__maxi(0, l->area.y + l->area.h - y);
    return r;
}

/* a whole row below the current one, h px high (h <= 0: the rest of the area) */
static gy_rect gy__take_rows(int h) {
    gy__lay *l = &GY__L;
    gy_rect r;
    if (l->col) gy__newline(l);
    r.x = l->area.x; r.y = l->cur_y; r.w = l->area.w;
    r.h = h > 0 ? h : gy__maxi(GY__T.row_h, l->area.y + l->area.h - l->cur_y);
    l->cur_y += r.h + GY__T.gap;
    gy__c.last = r;
    return r;
}

static gy__lay *gy__lay_push(int kind, gy_rect area, int clip) {
    gy__lay *l;
    if (gy__c.nlay >= 24) return &GY__L;
    l = &gy__c.lay[gy__c.nlay++];
    gy__memset(l, 0, sizeof *l);
    l->area = area; l->cur_y = area.y; l->clip = clip; l->kind = kind; l->bg_inst = -1;
    l->ncols = 1; l->widths[0] = -1;
    gy__row_layout(l);
    return l;
}

/* the bottom of what was laid out in l */
static int gy__lay_bottom(const gy__lay *l) { return l->col ? l->cur_y + gy__rowh(l) + l->row_extra : l->cur_y - GY__T.gap; }

static int gy__inside(gy_rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

/* the mouse is over r, inside the current clip, and no popup from a higher layer covers it */
static int gy__mouse_in(gy_rect r) {
    int i;
    if (!gy__inside(r, gy__c.mx, gy__c.my) || !gy__inside(gy__c.clips[GY__L.clip], gy__c.mx, gy__c.my)) return 0;
    if (gy__c.layer == 0)
        for (i = 0; i < gy__c.nblock_prev; i++) if (gy__inside(gy__c.block_prev[i], gy__c.mx, gy__c.my)) return 0;
    return 1;
}

/* the press-and-release behaviour every clickable widget shares */
static int gy__button_logic(uint32_t id, gy_rect r, int *hot, int *held) {
    int over = !gy__c.disabled && gy__mouse_in(r) && (!gy__c.active || gy__c.active == id);
    if (over && gy__c.pressed[0]) gy__c.active = id;
    *hot = over;
    *held = gy__c.active == id && gy__c.down[0];
    gy__c.last_id = id;
    return gy__c.active == id && gy__c.released[0] && over;
}

int gy_hovered(void) { return gy__mouse_in(gy__c.last); }
int gy_clicked(int b) { return b >= 0 && b < 3 && gy__c.released[b] && gy__mouse_in(gy__c.last) && !gy__c.disabled; }

/* ================================================================================================
   Widgets
   ================================================================================================ */

static int gy__text_y(gy_rect r) { return r.y + (r.h - gy_line_height()) / 2; }

static void gy__text_v(gy_color col, const char *fmt, va_list ap) {
    char buf[1024];
    gy_rect r = gy__next();
    gy__vfmt(buf, sizeof buf, fmt, ap);
    gy__draw_text_n(r.x, gy__text_y(r), buf, NULL, gy__c.disabled ? GY__T.text_off : col);
}

void gy_text(const char *fmt, ...) { va_list ap; va_start(ap, fmt); gy__text_v(GY__T.text, fmt, ap); va_end(ap); }
void gy_text_col(gy_color c, const char *fmt, ...) { va_list ap; va_start(ap, fmt); gy__text_v(c, fmt, ap); va_end(ap); }

void gy_section(const char *title) {
    gy_rect r;
    gy_row(0, NULL);
    r = gy__next();
    gy__draw_text_n(r.x, gy__text_y(r), title, gy__label_end(title), GY__T.heading);
    gy_canvas_rect((gy_rect){ r.x, r.y + r.h - 2, r.w, 1 }, GY__T.border, 0);
}

void gy_text_wrap(const char *text) {
    gy_rect r = gy__next();
    gy__lay *l = &GY__L;
    int lh = gy_line_height(), y = r.y + (gy__rowh(l) - lh) / 2, lines = 0;
    const char *s = text;
    gy_color col = gy__c.disabled ? GY__T.text_off : GY__T.text;
    while (*s) {                               /* each line: as many words as fit */
        const char *e = s, *brk = NULL;
        int w = 0;
        while (*e && *e != '\n') {
            const char *p = e;
            w += gy__glyph_get(gy__c.font, gy__utf8_next(&p))->adv;
            if (w > r.w && brk) break;
            if (*e == ' ') brk = e;
            e = p;
        }
        if (*e && *e != '\n' && brk) e = brk;
        gy__draw_text_n(r.x, y, s, e, col);
        y += lh; lines++;
        s = *e ? e + 1 : e;
    }
    if (lines > 1) l->row_extra = gy__maxi(l->row_extra, (lines - 1) * lh);   /* the row grows to fit */
    gy__c.last.h = gy__rowh(l) + l->row_extra;
}

static int gy__button_impl(const char *label, int accent, int key) {
    gy_rect r = gy__next();
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held);
    gy_color bg;
    if (key && !gy__c.disabled && !gy__c.focus && gy__take_key(key, 0)) clicked = 1;
    if (accent) bg = gy__c.disabled ? GY__T.button : held && hot ? GY__T.accent : hot ? GY__T.accent_hot : GY__T.accent;
    else bg = gy__c.disabled ? GY__T.button : held && hot ? GY__T.button_down : hot ? GY__T.button_hot : GY__T.button;
    gy_canvas_box(r, bg, hot ? GY__T.accent : GY__T.border, (float)GY__T.radius, 1);
    gy__draw_text_n(r.x + (r.w - gy__text_w(label, end)) / 2, gy__text_y(r), label, end,
                    gy__c.disabled ? GY__T.text_off : accent ? GY__T.accent_text : GY__T.text);
    return clicked;
}
int gy_button(const char *label) { return gy__button_impl(label, 0, 0); }
int gy_button_accent(const char *label) { return gy__button_impl(label, 1, 0); }
int gy_button_key(const char *label, int key) { return gy__button_impl(label, 0, key); }

int gy_check(const char *label, int *on) {
    gy_rect r = gy__next(), box;
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held), size = 16;
    (void)held;
    if (clicked) *on = !*on;
    box.x = r.x; box.y = r.y + (r.h - size) / 2; box.w = box.h = size;
    if (*on) {
        gy_canvas_box(box, gy__c.disabled ? GY__T.button : hot ? GY__T.accent_hot : GY__T.accent, 0, 3, 0);
        gy__draw_icon(GY__I_CHECK, box.x, box.y, size, gy__c.disabled ? GY__T.text_off : GY__T.accent_text);
    } else gy_canvas_box(box, hot ? GY__T.field_hot : GY__T.field, hot ? GY__T.accent : GY__T.border, 3, 1);
    gy__draw_text_n(r.x + size + GY__T.gap, gy__text_y(r), label, end, gy__c.disabled ? GY__T.text_off : GY__T.text);
    return clicked;
}

int gy_select(const char *label, int on) {
    gy_rect r = gy__next();
    const char *end = gy__label_end(label);
    int hot, held, clicked = gy__button_logic(gy__id(label), r, &hot, &held);
    gy_color bg = on ? (gy__c.disabled ? GY__T.button : hot ? GY__T.accent_hot : GY__T.accent) : held && hot ? GY__T.button_down : hot ? GY__T.button_hot : 0;
    if (bg) gy_canvas_rect(r, bg, (float)GY__T.radius);
    gy__draw_text_n(r.x + GY__T.gap, gy__text_y(r), label, end,
                    gy__c.disabled ? GY__T.text_off : on ? GY__T.accent_text : GY__T.text);
    return clicked;
}

int gy_chips(const char *const *items, int n, int *sel) {
    int i, changed = 0;
    for (i = 0; i < n; i++) {
        gy_id_push(i);
        if (gy_select(items[i], *sel == i) && *sel != i) { *sel = i; changed = 1; }
        gy_id_pop();
    }
    return changed;
}

void gy_progress(float done) {
    gy_rect r = gy__next(), f;
    int h = gy__mini(r.h, 10);
    r.y += (r.h - h) / 2; r.h = h;
    gy_canvas_rect(r, GY__T.field, (float)h / 2);
    f = r; f.w = (int)((float)r.w * gy__clampf(done, 0, 1) + 0.5f);
    if (f.w > 0) gy_canvas_rect(f, gy__c.disabled ? GY__T.text_off : GY__T.accent, (float)h / 2);
}

/* --- sliders and numbers ------------------------------------------------------------------------- */
static int gy__slider_core(const char *label, float *v, float lo, float hi, float step, char *shown) {
    gy_rect r = gy__next(), tr, knob, fill;
    uint32_t id = gy__id(label);
    const char *end = gy__label_end(label);
    int hot, held, changed = 0, tw;
    float old = *v, t;
    (void)gy__button_logic(id, r, &hot, &held);
    if (held && hi > lo) {
        t = gy__clampf((float)(gy__c.mx - r.x - 6) / (float)gy__maxi(1, r.w - 12), 0, 1);
        *v = lo + (hi - lo) * t;
    }
    if (hot && gy__c.wheel != 0 && !gy__c.disabled) { *v += (step > 0 ? step : (hi - lo) / 20) * gy__c.wheel; gy__c.wheel = 0; }
    if (step > 0) { float k = (*v - lo) / step; *v = lo + step * (float)(int)(k + (k >= 0 ? 0.5f : -0.5f)); }
    *v = gy__clampf(*v, lo, hi);
    changed = *v != old;
    t = hi > lo ? (*v - lo) / (hi - lo) : 0;
    tr.x = r.x; tr.w = r.w; tr.h = 6; tr.y = r.y + r.h - 8;
    gy_canvas_rect(tr, GY__T.field, 3);
    fill = tr; fill.w = (int)((float)tr.w * t);
    if (fill.w > 0) gy_canvas_rect(fill, gy__c.disabled ? GY__T.text_off : GY__T.accent, 3);
    knob.w = knob.h = 12; knob.x = tr.x + (int)((float)(tr.w - 12) * t); knob.y = tr.y - 3;
    gy_canvas_box(knob, gy__c.disabled ? GY__T.button : hot || held ? GY__T.accent_hot : GY__T.text, GY__T.border, 6, 1);
    gy__draw_text_n(r.x, r.y + 1, label, end, gy__c.disabled ? GY__T.text_off : GY__T.text_dim);
    tw = gy_text_width(shown);
    gy__draw_text_n(r.x + r.w - tw, r.y + 1, shown, NULL, gy__c.disabled ? GY__T.text_off : GY__T.text);
    return changed;
}

int gy_slider(const char *label, float *v, float lo, float hi) {
    char s[32];
    gy__fmt(s, sizeof s, "%.2f", (double)*v);
    return gy__slider_core(label, v, lo, hi, 0, s);
}

int gy_slider_i(const char *label, int *v, int lo, int hi) {
    char s[32];
    float f = (float)*v;
    int ch;
    gy__fmt(s, sizeof s, "%d", *v);
    ch = gy__slider_core(label, &f, (float)lo, (float)hi, 1, s);
    *v = (int)(f + (f >= 0 ? 0.5f : -0.5f));
    return ch;
}

static int gy__edit_core(uint32_t id, gy_rect r, char *buf, int cap, int numeric);

static double gy__atof(const char *s, int *ok) {
    double v = 0, sc = 1;
    int neg = 0, dig = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); dig = 1; }
    if (*s == '.' || *s == ',') { s++; while (*s >= '0' && *s <= '9') { sc /= 10; v += (*s++ - '0') * sc; dig = 1; } }
    while (*s == ' ') s++;
    *ok = dig && !*s;
    return neg ? -v : v;
}

static int gy__number_core(const char *label, float *v, float step, float lo, float hi, int is_int) {
    gy_rect r = gy__next(), mid, bl, br;
    uint32_t id = gy__id(label), tid = id ^ 0x5bd1e995u;
    const char *end = gy__label_end(label);
    gy__st *st = gy__state(id);
    int hot, held, hl, hr2, dl, dr, lw = gy__text_w(label, end), changed = 0;
    float old = *v;
    char s[64];
    if (lw) lw += GY__T.gap;
    gy__draw_text_n(r.x, gy__text_y(r), label, end, gy__c.disabled ? GY__T.text_off : GY__T.text);
    r.x += lw; r.w -= lw;
    bl = r; bl.w = 22; br = r; br.x = r.x + r.w - 22; br.w = 22;
    mid = r; mid.x += 22; mid.w -= 44;
    if (st->i0) {                              /* typing a value; Enter or a click elsewhere takes it */
        int f = gy__c.focus == tid ? gy__edit_core(tid, r, gy__c.undo[15], 64, 1) : 0;
        if ((f & GY_EDIT_ENTER) || gy__c.focus != tid) {
            int ok;
            double d = gy__atof(gy__c.undo[15], &ok);
            if (ok) *v = gy__clampf((float)d, lo, hi);
            st->i0 = 0;
            if (gy__c.focus == tid) gy__c.focus = 0;
        }
    } else {
        if (gy__button_logic(id ^ 1, bl, &hl, &dl)) *v -= step;
        if (gy__button_logic(id ^ 2, br, &hr2, &dr)) *v += step;
        (void)gy__button_logic(id, mid, &hot, &held);
        if (gy__c.pressed[0] && hot) { gy__c.drag_x = gy__c.mx; gy__c.drag_v = *v; }
        if (held && gy__c.mx != gy__c.drag_x) *v = gy__c.drag_v + (float)(gy__c.mx - gy__c.drag_x) / 4.0f * step;
        if (hot && gy__c.dbl[0]) {             /* double-click: type the value */
            st->i0 = 1; gy__c.focus = tid; gy__c.focus_claimed = tid; gy__c.active = 0;
            if (is_int) gy__fmt(gy__c.undo[15], 64, "%d", (int)*v); else gy__fmt(gy__c.undo[15], 64, "%.3f", (double)*v);
            gy__state(tid)->i0 = gy__strlen(gy__c.undo[15]); gy__state(tid)->i1 = 0;
        }
        if ((hot || hl || hr2) && gy__c.wheel != 0 && !gy__c.disabled) { *v += step * gy__c.wheel; gy__c.wheel = 0; }
        if (is_int) *v = (float)(int)(*v + (*v >= 0 ? 0.5f : -0.5f));
        *v = gy__clampf(*v, lo, hi);
        gy_canvas_box(r, hot || held ? GY__T.field_hot : GY__T.field, hot || hl || hr2 ? GY__T.accent : GY__T.border, (float)GY__T.radius, 1);
        gy__draw_icon(GY__I_MINUS, bl.x + 3, bl.y + (bl.h - 16) / 2, 16, gy__c.disabled ? GY__T.text_off : hl ? GY__T.accent_hot : GY__T.text_dim);
        gy__draw_icon(GY__I_PLUS, br.x + 3, br.y + (br.h - 16) / 2, 16, gy__c.disabled ? GY__T.text_off : hr2 ? GY__T.accent_hot : GY__T.text_dim);
        if (is_int) gy__fmt(s, sizeof s, "%d", (int)*v); else gy__fmt(s, sizeof s, "%.2f", (double)*v);
        gy__draw_text_n(mid.x + (mid.w - gy_text_width(s)) / 2, gy__text_y(mid), s, NULL, gy__c.disabled ? GY__T.text_off : GY__T.text);
    }
    changed = *v != old;
    return changed;
}

int gy_number(const char *label, float *v, float step, float lo, float hi) { return gy__number_core(label, v, step, lo, hi, 0); }
int gy_number_i(const char *label, int *v, int lo, int hi) {
    float f = (float)*v;
    int ch = gy__number_core(label, &f, 1, (float)lo, (float)hi, 1);
    *v = (int)f;
    return ch;
}

/* --- the edit box --------------------------------------------------------------------------------- */
static int gy__prev_cp(const char *s, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--; return i; }
static int gy__next_cp(const char *s, int i) { if (!s[i]) return i; i++; while (s[i] && ((unsigned char)s[i] & 0xC0) == 0x80) i++; return i; }
static int gy__word_l(const char *s, int i) { while (i > 0 && s[i - 1] == ' ') i--; while (i > 0 && s[i - 1] != ' ') i--; return i; }
static int gy__word_r(const char *s, int i) { while (s[i] && s[i] != ' ') i++; while (s[i] == ' ') i++; return i; }

static void gy__undo_push(const char *buf, int kind) {
    if (gy__c.undo_n && gy__c.undo_kind == kind && gy__c.now - gy__c.undo_t < 1000) { gy__c.undo_t = gy__c.now; return; }
    if (gy__c.undo_n == 15) { gy__memmove(gy__c.undo[0], gy__c.undo[1], 14 * 256); gy__c.undo_n = 14; }  /* slot 15 is the number box's */
    gy__strcpy(gy__c.undo[gy__c.undo_n++], buf, 256);
    gy__c.undo_kind = kind; gy__c.undo_t = gy__c.now;
}

static void gy__clip_set(const char *s, int n) {
    if (gy__c.hwnd && OpenClipboard(gy__c.hwnd)) {
        int wn = MultiByteToWideChar(CP_UTF8, 0, s, n, NULL, 0);
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (size_t)(wn + 1) * 2);
        EmptyClipboard();
        if (g) {
            WCHAR *w = (WCHAR *)GlobalLock(g);
            MultiByteToWideChar(CP_UTF8, 0, s, n, w, wn); w[wn] = 0;
            GlobalUnlock(g);
            if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
        }
        CloseClipboard();
        return;
    }
    n = gy__mini(n, (int)sizeof gy__c.clip_text - 1);
    gy__memcpy(gy__c.clip_text, s, (size_t)n); gy__c.clip_text[n] = 0;
}

static const char *gy__clip_get(void) {
    if (gy__c.hwnd && OpenClipboard(gy__c.hwnd)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        gy__c.clip_text[0] = 0;
        if (h) {
            const WCHAR *w = (const WCHAR *)GlobalLock(h);
            if (w) { WideCharToMultiByte(CP_UTF8, 0, w, -1, gy__c.clip_text, sizeof gy__c.clip_text, NULL, NULL); GlobalUnlock(h); }
        }
        CloseClipboard();
    }
    return gy__c.clip_text;
}

/* replace [a, b) of buf with ins (n bytes); keeps within cap */
static int gy__edit_replace(char *buf, int cap, int a, int b, const char *ins, int n, int numeric) {
    int len = gy__strlen(buf), i, k = 0;
    char tmp[256];
    for (i = 0; i < n && k < 255; i++) {       /* one line; a number box takes only number characters */
        char ch = ins[i];
        if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        if (numeric && !((ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == ',' || ch == '+')) continue;
        tmp[k++] = ch;
    }
    if (len - (b - a) + k > cap - 1) k = gy__maxi(0, cap - 1 - (len - (b - a)));
    while (k > 0 && ((unsigned char)tmp[k] & 0xC0) == 0x80 && k < n) k--;   /* never cut a character in half */
    gy__memmove(buf + a + k, buf + b, (size_t)(len - b + 1));
    gy__memcpy(buf + a, tmp, (size_t)k);
    return a + k;
}

static int gy__edit_core(uint32_t id, gy_rect r, char *buf, int cap, int numeric) {
    gy__st *st = gy__state(id);               /* i0 caret, i1 mark (selection anchor), f0 scroll x */
    int hot, held, i, flags = 0, focused, len = gy__strlen(buf), x0, shift;
    gy_rect tr;
    (void)gy__button_logic(id, r, &hot, &held);
    if (hot && gy__c.pressed[0]) { gy__c.focus = id; gy__c.focus_claimed = id; }
    focused = gy__c.focus == id;
    st->i0 = gy__clampi(st->i0, 0, len); st->i1 = gy__clampi(st->i1, 0, len);
    tr = r; tr.x += 6; tr.w -= 12;
    x0 = tr.x - (int)st->f0;
    if (focused && held) {                     /* click and drag place the caret */
        int best = 0, bx = 1 << 30, p = 0;
        for (;;) {
            int px = x0 + gy__text_w(buf, buf + p), d = px > gy__c.mx ? px - gy__c.mx : gy__c.mx - px;
            if (d < bx) { bx = d; best = p; }
            if (!buf[p]) break;
            p = gy__next_cp(buf, p);
        }
        st->i0 = best;
        if (gy__c.pressed[0]) st->i1 = best;
        if (gy__c.dbl[0]) { st->i1 = gy__word_l(buf, best); st->i0 = gy__word_r(buf, best); }
        st->i2 = (int)gy__c.now;
    }
    if (focused) {
        int a, b, ctrl;
        for (i = 0; i < gy__c.nkev; i++) {
            int k = gy__c.kev[i].key, m = gy__c.kev[i].mods, car = st->i0;
            if (gy__c.kev[i].used) continue;
            shift = m & GY_MOD_SHIFT; ctrl = m & GY_MOD_CTRL;
            if (m & GY_MOD_ALT) continue;      /* Alt keys stay for the tiles and the app */
            a = gy__mini(st->i0, st->i1); b = gy__maxi(st->i0, st->i1);
            gy__c.kev[i].used = 1;
            if (k == GY_KEY_LEFT) car = a != b && !shift ? a : ctrl ? gy__word_l(buf, car) : gy__prev_cp(buf, car);
            else if (k == GY_KEY_RIGHT) car = a != b && !shift ? b : ctrl ? gy__word_r(buf, car) : gy__next_cp(buf, car);
            else if (k == GY_KEY_HOME) car = 0;
            else if (k == GY_KEY_END) car = gy__strlen(buf);
            else if (k == GY_KEY_BACK || k == GY_KEY_DELETE) {
                if (a == b) { if (k == GY_KEY_BACK) a = ctrl ? gy__word_l(buf, a) : gy__prev_cp(buf, a); else b = ctrl ? gy__word_r(buf, b) : gy__next_cp(buf, b); }
                if (a != b) { gy__undo_push(buf, 2); car = gy__edit_replace(buf, cap, a, b, "", 0, numeric); flags |= GY_EDIT_CHANGED; }
                st->i0 = st->i1 = car; continue;
            }
            else if (k == GY_KEY_ENTER) { flags |= GY_EDIT_ENTER; continue; }
            else if (k == GY_KEY_ESC) { gy__c.focus = 0; continue; }
            else if (ctrl && k == 'A') { st->i1 = 0; st->i0 = gy__strlen(buf); continue; }
            else if (ctrl && (k == 'C' || k == 'X')) {
                if (a != b) gy__clip_set(buf + a, b - a);
                if (k == 'X' && a != b) { gy__undo_push(buf, 3); car = gy__edit_replace(buf, cap, a, b, "", 0, numeric); flags |= GY_EDIT_CHANGED; st->i0 = st->i1 = car; }
                continue;
            }
            else if (ctrl && k == 'V') {
                const char *p = gy__clip_get();
                gy__undo_push(buf, 3);
                car = gy__edit_replace(buf, cap, a, b, p, gy__strlen(p), numeric);
                st->i0 = st->i1 = car; flags |= GY_EDIT_CHANGED; continue;
            }
            else if (ctrl && k == 'Z') {
                if (gy__c.undo_n) { gy__strcpy(buf, gy__c.undo[--gy__c.undo_n], cap); st->i0 = st->i1 = gy__strlen(buf); flags |= GY_EDIT_CHANGED; gy__c.undo_kind = 0; }
                continue;
            }
            else { gy__c.kev[i].used = ctrl ? 0 : 1; continue; }   /* other ctrl keys stay for the app */
            st->i0 = car;
            if (!shift) st->i1 = car;
            st->i2 = (int)gy__c.now;
        }
        for (i = 0; i < gy__c.nchars; i++) {
            char u[4];
            int n = gy__utf8_put(u, gy__c.chars[i]), car;
            a = gy__mini(st->i0, st->i1); b = gy__maxi(st->i0, st->i1);
            gy__undo_push(buf, 1);
            car = gy__edit_replace(buf, cap, a, b, u, n, numeric);
            if (car != a || a != b) flags |= GY_EDIT_CHANGED;
            st->i0 = st->i1 = car; st->i2 = (int)gy__c.now;
        }
        gy__c.nchars = 0;
    }
    /* draw: the box, the selection, the text (scrolled so the caret shows), the caret */
    {
        int cx = gy__text_w(buf, buf + st->i0), a = gy__mini(st->i0, st->i1), b = gy__maxi(st->i0, st->i1), clip, ty = gy__text_y(r);
        if (cx - st->f0 > tr.w) st->f0 = (float)(cx - tr.w);
        if (cx - st->f0 < 0) st->f0 = (float)cx;
        x0 = tr.x - (int)st->f0;
        gy_canvas_box(r, focused || hot ? GY__T.field_hot : GY__T.field, focused ? GY__T.accent : hot ? GY__T.accent : GY__T.border, (float)GY__T.radius, 1);
        clip = gy__clip_push(tr);
        {
            int saved = GY__L.clip;
            GY__L.clip = clip;
            if (focused && a != b) gy_canvas_rect((gy_rect){ x0 + gy__text_w(buf, buf + a), ty, gy__text_w(buf + a, buf + b), gy_line_height() }, GY__T.select, 0);
            gy__draw_text_n(x0, ty, buf, NULL, gy__c.disabled ? GY__T.text_off : GY__T.text);
            if (focused) {                     /* the caret blinks: on 530 ms, off 530 ms */
                unsigned t = gy__c.now - (unsigned)st->i2;
                if ((t / 530) % 2 == 0) gy_canvas_rect((gy_rect){ x0 + cx, ty, 1, gy_line_height() }, GY__T.text, 0);
                gy_animate((int)(530 - t % 530));
            }
            GY__L.clip = saved;
        }
    }
    return flags;
}

int gy_edit(const char *label, char *buf, int cap) {
    gy_rect r = gy__next();
    const char *end = gy__label_end(label);
    int lw = gy__text_w(label, end);
    if (lw) {
        gy__draw_text_n(r.x, gy__text_y(r), label, end, gy__c.disabled ? GY__T.text_off : GY__T.text);
        r.x += lw + GY__T.gap; r.w -= lw + GY__T.gap;
    }
    return gy__edit_core(gy__id(label), r, buf, cap, 0);
}

void gy_unfocus(void) { gy__c.focus = 0; }

/* --- tooltips ----------------------------------------------------------------------------------------- */
void gy_tip(const char *text) {
    int w, h, x, y, pad = 6;
    if (gy__c.disabled || !gy__mouse_in(gy__c.last) || gy__c.down[0]) return;
    if (gy__c.tip_id != gy__c.last_id) { gy__c.tip_id = gy__c.last_id; gy__c.tip_since = gy__c.now; }
    gy__c.tip_shown = 1;
    if (gy__c.now - gy__c.tip_since < 500) { gy_animate((int)(500 - (gy__c.now - gy__c.tip_since))); return; }
    w = gy_text_width(text) + 2 * pad; h = gy_line_height() + 2 * pad;
    x = gy__clampi(gy__c.mx + 12, 0, gy__maxi(0, gy__c.w - w)); y = gy__c.my + 20;
    if (y + h > gy__c.h) y = gy__c.my - h - 4;
    {
        int saved_layer = gy__c.layer, saved_clip = GY__L.clip;
        gy__c.layer = 2; GY__L.clip = 0;
        gy_canvas_box((gy_rect){ x + 2, y + 3, w, h }, GY__T.shadow, 0, 4, 0);
        gy_canvas_box((gy_rect){ x, y, w, h }, GY__T.tip, GY__T.border, 4, 1);
        gy_canvas_text(x + pad, y + pad, text, GY__T.text);
        gy__c.layer = saved_layer; GY__L.clip = saved_clip;
    }
}

/* --- popups: combo lists, context menus, the app's own layers ------------------------------------- */
static void gy__block(gy_rect r) { if (gy__c.nblock_cur < 4) gy__c.block_cur[gy__c.nblock_cur++] = r; }

static gy__lay *gy__popup_lay(gy_rect r) {
    gy__lay *l;
    gy__c.layer = 1;
    gy__block(r);
    gy_canvas_box((gy_rect){ r.x + 2, r.y + 3, r.w, r.h }, GY__T.shadow, 0, 4, 0);
    l = gy__lay_push(GY__L_POPUP, (gy_rect){ r.x + 4, r.y + 4, r.w - 8, r.h - 8 }, 0);
    l->clip = gy__clip_push(r);
    gy_canvas_box(r, GY__T.tip, GY__T.border, 4, 1);
    gy__c.popup_used = 1;
    return l;
}

static void gy__popup_lay_end(void) {
    if (gy__c.nlay > 1 && GY__L.kind == GY__L_POPUP) gy__c.nlay--;
    gy__c.layer = 0;
}

int gy_combo(const char *label, int *sel, const char *items) {
    gy_rect r = gy__next(), list;
    uint32_t id = gy__id(label);
    const char *end = gy__label_end(label), *it;
    int hot, held, n = 0, i, changed = 0, lw = gy__text_w(label, end), rowh = gy__rowh(&GY__L);
    for (it = items; *it; it += gy__strlen(it) + 1) n++;
    if (lw) {
        gy__draw_text_n(r.x, gy__text_y(r), label, end, gy__c.disabled ? GY__T.text_off : GY__T.text);
        r.x += lw + GY__T.gap; r.w -= lw + GY__T.gap;
    }
    if (gy__button_logic(id, r, &hot, &held)) {
        if (gy__c.popup_id == id) gy__c.popup_id = 0;
        else { gy__c.popup_id = id; gy__c.popup_kind = 1; }
    }
    gy_canvas_box(r, hot ? GY__T.button_hot : GY__T.button, hot || gy__c.popup_id == id ? GY__T.accent : GY__T.border, (float)GY__T.radius, 1);
    for (it = items, i = 0; *it && i < *sel; it += gy__strlen(it) + 1) i++;
    if (*it) gy__draw_text_n(r.x + GY__T.gap, gy__text_y(r), it, NULL, gy__c.disabled ? GY__T.text_off : GY__T.text);
    gy__draw_icon(GY__I_DOWN, r.x + r.w - 20, r.y + (r.h - 16) / 2, 16, gy__c.disabled ? GY__T.text_off : GY__T.text_dim);
    if (gy__c.popup_id == id) {                /* the open list, on the popup layer */
        int saved_row = GY__L.row_h;
        list.x = r.x; list.y = r.y + r.h + 2; list.w = r.w; list.h = n * rowh + (n - 1) * GY__T.gap + 8;
        if (list.y + list.h > gy__c.h) list.y = gy__maxi(0, r.y - list.h - 2);
        gy__popup_lay(list);
        GY__L.row_h = rowh;
        (void)saved_row;
        for (it = items, i = 0; *it; it += gy__strlen(it) + 1, i++) {
            gy_id_push(i);
            if (gy_select(it, i == *sel)) { if (*sel != i) changed = 1; *sel = i; gy__c.popup_id = 0; }
            gy_id_pop();
        }
        gy__popup_lay_end();
        if (gy__take_key(GY_KEY_ESC, 0)) gy__c.popup_id = 0;
    }
    gy__c.last = r; gy__c.last_id = id;
    return changed;
}

static uint32_t gy__ctx_owner;
int gy_context_begin(void) {
    uint32_t owner = gy__c.last_id ^ 0xC0E7u;
    gy_rect r;
    int n = 6;
    if (gy__c.released[1] && gy__mouse_in(gy__c.last) && !gy__c.disabled) {
        gy__c.popup_id = owner; gy__c.popup_kind = 2; gy__c.popup_x = gy__c.mx; gy__c.popup_y = gy__c.my;
    }
    if (gy__c.popup_id != owner) return 0;
    gy__ctx_owner = owner;
    r.x = gy__clampi(gy__c.popup_x, 0, gy__maxi(0, gy__c.w - 200)); r.y = gy__clampi(gy__c.popup_y, 0, gy__maxi(0, gy__c.h - n * GY__T.row_h));
    r.w = 200; r.h = gy__state(owner)->i0 ? gy__state(owner)->i0 : n * (GY__T.row_h + GY__T.gap);
    gy__popup_lay(r);
    return 1;
}
int gy_menu_item(const char *label) {
    if (gy_select(label, 0)) { gy__c.popup_id = 0; return 1; }
    return 0;
}
void gy_context_end(void) {
    if (gy__c.nlay > 1 && GY__L.kind == GY__L_POPUP) {   /* remember the height the items needed, for the next frame */
        gy__state(gy__ctx_owner)->i0 = gy__lay_bottom(&GY__L) - GY__L.area.y + 8;
    }
    gy__popup_lay_end();
    if (gy__take_key(GY_KEY_ESC, 0)) gy__c.popup_id = 0;
}

void gy_popup_open(const char *id) { gy__c.popup_id = gy__hash(id, 0x9090u); gy__c.popup_kind = 3; }
void gy_popup_close(void) { gy__c.popup_id = 0; }
int gy_popup_begin(const char *id, gy_rect r) {
    if (gy__c.popup_id != gy__hash(id, 0x9090u)) return 0;
    gy__popup_lay(r);
    return 1;
}
void gy_popup_end(void) { gy__popup_lay_end(); if (gy__take_key(GY_KEY_ESC, 0)) gy__c.popup_id = 0; }

/* --- images --------------------------------------------------------------------------------------- */
static void gy__image_convert(uint32_t *d, const void *src, int n, int fmt) {
    const unsigned char *s = (const unsigned char *)src;
    int i;
    for (i = 0; i < n; i++, s += 4) {
        uint32_t r, g, b, a = s[3];
        if (fmt == GY_FMT_BGRA_PM) { d[i] = (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | a << 24; continue; }
        if (fmt == GY_FMT_RGBA) { r = s[0]; g = s[1]; b = s[2]; } else { b = s[0]; g = s[1]; r = s[2]; }
        d[i] = gy__div255(b * a) | gy__div255(g * a) << 8 | gy__div255(r * a) << 16 | a << 24;
    }
}

int gy_image_make(int w, int h, const void *px, int fmt) {
    int i;
    if (!gy__init() || w <= 0 || h <= 0 || w > 16384 || h > 16384 || !px) { gy__set_error("gy_image_make", "bad size or no pixels"); return 0; }
    for (i = 1; i < GY__IMAGES; i++) if (!gy__c.img[i].px) break;
    if (i == GY__IMAGES) { gy__set_error("gy_image_make", "255 images at most"); return 0; }
    gy__c.img[i].px = (uint32_t *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * (size_t)h * 4);
    if (!gy__c.img[i].px) { gy__set_error("gy_image_make", "out of memory"); return 0; }
    gy__c.img[i].w = w; gy__c.img[i].h = h; gy__c.img[i].gen++;
    gy__image_convert(gy__c.img[i].px, px, w * h, fmt);
    return i;
}

int gy_image_update(int img, const void *px, int fmt) {
    if (img <= 0 || img >= GY__IMAGES || !gy__c.img[img].px || !px) return 0;
    gy__image_convert(gy__c.img[img].px, px, gy__c.img[img].w * gy__c.img[img].h, fmt);
    gy__c.img[img].gen++;                      /* the tiles see a new generation and redraw */
    return 1;
}

void gy_image_free(int img) {
    if (img <= 0 || img >= GY__IMAGES || !gy__c.img[img].px) return;
    HeapFree(GetProcessHeap(), 0, gy__c.img[img].px);
    gy__c.img[img].px = NULL; gy__c.img[img].gen++;
}

void gy_canvas_image(int img, gy_rect r, int smooth) {
    gy__inst *in;
    if (img <= 0 || img >= GY__IMAGES || !gy__c.img[img].px || r.w <= 0 || r.h <= 0) return;
    in = gy__push(GY__K_IMAGE, r.x, r.y, r.x + r.w, r.y + r.h);
    if (!in) return;
    in->a = (uint32_t)img; in->b = gy__c.img[img].gen;
    in->c |= smooth ? 1u : 0u;
}

void gy_image(int img, int w, int h) {
    gy__lay *l = &GY__L;
    gy_rect r;
    if (img <= 0 || img >= GY__IMAGES || !gy__c.img[img].px) { gy__next(); return; }
    r = gy__next();
    if (h > gy__rowh(l)) l->row_extra = gy__maxi(l->row_extra, h - gy__rowh(l));   /* the row grows to the picture */
    r.w = gy__mini(w, r.w); r.h = h;
    gy_canvas_image(img, r, w != gy__c.img[img].w || h != gy__c.img[img].h);
    gy__c.last = r;
}

/* --- containers ------------------------------------------------------------------------------------ */
void gy_scroll_begin(const char *idl, int h) {
    uint32_t id = gy__id(idl);
    gy_rect r = gy__take_rows(h), in;
    gy__st *st = gy__state(id);                /* f0 scroll y, f1 content height, f2 view height */
    gy__lay *l;
    int clip = gy__clip_push(r);
    in = r; in.x += 4; in.w -= 16; in.y += 4; in.h -= 8;
    gy_canvas_box(r, GY__T.panel, GY__T.border, (float)GY__T.radius, 1);
    l = gy__lay_push(GY__L_SCROLL, in, clip);
    l->id = id;
    l->top = in.y;
    st->f0 = gy__clampf(st->f0, 0, gy__maxf(0, st->f1 - (float)in.h));
    l->cur_y = in.y - (int)st->f0;
    l->area.y = l->cur_y;
    st->f2 = (float)in.h;
    gy_id_push((int)id);
}

void gy_scroll_rows(int n, int *first, int *last) {
    gy__lay *l = &GY__L;
    gy_rect view;
    int pitch, f, e;
    *first = 0; *last = n - 1;
    if (n <= 0) { *last = -1; return; }
    if (gy__c.nlay < 2 || l->kind != GY__L_SCROLL) return;   /* outside a scroll region: every row */
    if (l->col) gy__newline(l);
    view = gy__c.clips[l->clip];
    pitch = gy__rowh(l) + GY__T.gap;
    f = gy__clampi((view.y - l->cur_y) / pitch, 0, n - 1);
    e = gy__clampi((view.y + view.h - l->cur_y) / pitch + 1, f, n - 1);
    l->cur_y += f * pitch;                     /* the rows above the view take their space */
    l->tail += (n - 1 - e) * pitch;            /* and the rows below it, counted at gy_scroll_end */
    *first = f; *last = e;
}

void gy_scroll_end(void) {
    gy__lay *l = &GY__L;
    gy__st *st;
    gy_rect view, bar, thumb;
    uint32_t id;
    int hot, held, content;
    if (gy__c.nlay < 2 || l->kind != GY__L_SCROLL) return;
    gy_id_pop();
    id = l->id;
    st = gy__state(id);
    content = gy__lay_bottom(l) - l->area.y + l->tail;
    st->f1 = (float)content;
    view = gy__c.clips[l->clip];
    gy__c.nlay--;
    if (gy__mouse_in(view) && gy__c.wheel != 0) {   /* the innermost region under the mouse takes the wheel */
        st->f0 -= gy__c.wheel * (float)(GY__T.row_h * 3);
        gy__c.wheel = 0;
        st->f0 = gy__clampf(st->f0, 0, gy__maxf(0, st->f1 - st->f2));
        gy__c.passes = gy__maxi(gy__c.passes, 1);
    }
    if (st->f1 > st->f2 + 0.5f) {              /* the bar and its thumb */
        bar.x = view.x + view.w - 10; bar.y = view.y + 3; bar.w = 7; bar.h = view.h - 6;
        thumb = bar;
        thumb.h = gy__maxi(20, (int)((float)bar.h * st->f2 / st->f1));
        thumb.y = bar.y + (int)((float)(bar.h - thumb.h) * st->f0 / gy__maxf(1, st->f1 - st->f2));
        (void)gy__button_logic(id ^ 0xBA5u, bar, &hot, &held);
        if (held && bar.h > thumb.h) {
            st->f0 = gy__clampf((float)(gy__c.my - bar.y - thumb.h / 2) / (float)(bar.h - thumb.h) * (st->f1 - st->f2), 0, st->f1 - st->f2);
        }
        gy_canvas_rect(thumb, hot || held ? GY__T.accent : GY__T.divider, 3);
    }
}

int gy_tree(const char *label) {
    gy_rect r = gy__next();
    uint32_t id = gy__id(label);
    gy__st *st = gy__state(id);
    const char *end = gy__label_end(label);
    int hot, held;
    gy__lay *l;
    if (gy__button_logic(id, r, &hot, &held)) st->i0 = !st->i0;
    if (hot) gy_canvas_rect(r, GY__T.button_hot, (float)GY__T.radius);
    gy__draw_icon(st->i0 ? GY__I_DOWN : GY__I_RIGHT, r.x, r.y + (r.h - 16) / 2, 16, gy__c.disabled ? GY__T.text_off : GY__T.text_dim);
    gy__draw_text_n(r.x + 20, gy__text_y(r), label, end, gy__c.disabled ? GY__T.text_off : GY__T.text);
    if (!st->i0) return 0;
    /* the children: an indented area below this row */
    {
        gy__lay *p = &GY__L;
        int y;
        gy__newline(p);
        y = p->cur_y;
        l = gy__lay_push(GY__L_TREE, (gy_rect){ p->area.x + 18, y, p->area.w - 18, p->area.y + p->area.h - y }, p->clip);
        l->id = id;
        gy_id_push((int)id);
    }
    return 1;
}

void gy_tree_end(void) {
    gy__lay *l = &GY__L;
    int bottom;
    if (gy__c.nlay < 2 || l->kind != GY__L_TREE) return;
    gy_id_pop();
    bottom = gy__lay_bottom(l);
    gy__c.nlay--;
    GY__L.cur_y = bottom + GY__T.gap; GY__L.col = 0; GY__L.row_extra = 0;
}

void gy_panel_begin(void) {
    gy__lay *p = &GY__L, *l;
    int y;
    if (p->col) gy__newline(p);
    y = p->cur_y;
    gy__c.no_cull = 1;                         /* the card grows at the end, so it must be kept even if this 1 px row is hidden */
    gy_canvas_box((gy_rect){ p->area.x, y, p->area.w, 1 }, GY__T.card, GY__T.border, (float)GY__T.radius, 1);
    gy__c.no_cull = 0;
    l = gy__lay_push(GY__L_PANEL, (gy_rect){ p->area.x + GY__T.pad, y + GY__T.pad, p->area.w - 2 * GY__T.pad, p->area.y + p->area.h - y - 2 * GY__T.pad }, p->clip);
    l->bg_inst = gy__c.ninst - 1;              /* the card is patched to the right height at the end */
    l->top = y;
}

void gy_panel_end(void) {
    gy__lay *l = &GY__L;
    int bottom;
    if (gy__c.nlay < 2 || l->kind != GY__L_PANEL) return;
    bottom = gy__lay_bottom(l) + GY__T.pad;
    if (l->bg_inst >= 0 && l->bg_inst < gy__c.ninst) gy__c.inst[l->bg_inst].y1 = (int16_t)bottom;
    gy__c.nlay--;
    GY__L.cur_y = bottom + GY__T.gap; GY__L.col = 0; GY__L.row_extra = 0;
}

void gy_split(const char *idl, int dir, float *pos, float def) {
    uint32_t id = gy__id(idl);
    gy_rect r = gy__take_rows(0), a, b, d;
    gy__lay *l;
    int hot, held, size = dir == GY_H ? r.w : r.h, dw = 6;
    float p;
    if (*pos <= 0) *pos = def;
    d = r;
    if (dir == GY_H) { d.x = r.x + (int)*pos; d.w = dw; } else { d.y = r.y + (int)*pos; d.h = dw; }
    (void)gy__button_logic(id, d, &hot, &held);
    if (held) *pos = (float)(dir == GY_H ? gy__c.mx - r.x : gy__c.my - r.y) - dw / 2;
    if (hot && gy__c.dbl[0]) { *pos = def; gy__c.active = 0; }   /* double-click: back to the default */
    p = gy__clampf(*pos, 40, (float)gy__maxi(40, size - 40 - dw));
    *pos = p;
    a = r; b = r;
    if (dir == GY_H) { a.w = (int)p; b.x = r.x + (int)p + dw; b.w = r.w - (int)p - dw; d.x = r.x + (int)p; }
    else { a.h = (int)p; b.y = r.y + (int)p + dw; b.h = r.h - (int)p - dw; d.y = r.y + (int)p; }
    gy_canvas_rect(dir == GY_H ? (gy_rect){ d.x + 2, d.y, 2, d.h } : (gy_rect){ d.x, d.y + 2, d.w, 2 }, hot || held ? GY__T.divider_hot : GY__T.divider, 1);
    if (hot || held) SetCursor(LoadCursorW(NULL, (LPCWSTR)(dir == GY_H ? IDC_SIZEWE : IDC_SIZENS)));
    l = gy__lay_push(GY__L_PANE_A, a, gy__clip_push(a));
    l->id = id; l->split_pos = p;
    l->pane_b = b;
}

void gy_pane(void) {
    gy__lay *l = &GY__L;
    gy_rect b;
    if (gy__c.nlay < 2 || l->kind != GY__L_PANE_A) return;
    b = l->pane_b;
    gy__c.nlay--;
    gy__lay_push(GY__L_PANE_B, b, gy__clip_push(b));
}

void gy_split_end(void) {
    if (gy__c.nlay > 1 && (GY__L.kind == GY__L_PANE_A || GY__L.kind == GY__L_PANE_B)) gy__c.nlay--;
}

/* ================================================================================================
   Tiling: a binary split tree of tiles that always fills the area; stacks of tabs at the leaves
   ================================================================================================ */

#define GY__TITLE_H 24
#define GY__TILE_MIN_W 160
#define GY__TILE_MIN_H 110

static void gy__tile_keys_default(void) {
    int A = GY_MOD_ALT << 16, AS = (GY_MOD_ALT | GY_MOD_SHIFT) << 16;
    int k[GY__TA_COUNT] = { A | GY_KEY_LEFT, A | GY_KEY_RIGHT, A | GY_KEY_UP, A | GY_KEY_DOWN,
                            AS | GY_KEY_LEFT, AS | GY_KEY_RIGHT, AS | GY_KEY_UP, AS | GY_KEY_DOWN,
                            A | GY_KEY_PLUS, A | GY_KEY_MINUS, A | 'F', A | 'Z', A | 'W', A | 'T' };
    gy__memcpy(gy__c.tkeys, k, sizeof k);
}

static const char *const gy__tkey_names[GY__TA_COUNT] = { "focus_left", "focus_right", "focus_up", "focus_down",
    "swap_left", "swap_right", "swap_up", "swap_down", "grow", "shrink", "flip", "zoom", "close", "next_tab" };

static void gy__tile_clash_check(void) {
    int a, i;
    char msg[200];
    for (a = 0; a < GY__TA_COUNT; a++)
        for (i = 0; i < gy__c.nowned; i++)
            if (gy__c.tkeys[a] && gy__c.tkeys[a] == gy__c.owned[i]) {
                gy__fmt(msg, sizeof msg, "The tile key for %s is one of the app's own keys, so it is off. Give it another key in the ini.", gy__tkey_names[a]);
                gy__set_error(msg, NULL);
                gy_error(msg);
                gy__c.tkeys[a] = 0;
            }
}

void gy_keys_owned(const int *keys, int n) {
    if (!gy__init()) return;
    gy__c.nowned = gy__mini(n, 64);
    gy__memcpy(gy__c.owned, keys, sizeof(int) * (size_t)gy__c.nowned);
    gy__tile_clash_check();
}

static int gy__tnode_new(void) {
    int i;
    for (i = 0; i < GY__TNODES; i++) if (!gy__c.tn[i].used) { gy__memset(&gy__c.tn[i], 0, sizeof gy__c.tn[i]); gy__c.tn[i].used = 1; gy__c.tn[i].parent = -1; return i; }
    return -1;
}

static int gy__tile_find(uint32_t id) { int i; for (i = 0; i < gy__c.ntl; i++) if (gy__c.tl[i].id == id) return i; return -1; }
static int gy__tile_leaf(int t) {
    int n, k;
    for (n = 0; n < GY__TNODES; n++) if (gy__c.tn[n].used && gy__c.tn[n].leaf) for (k = 0; k < gy__c.tn[n].ntabs; k++) if (gy__c.tn[n].tabs[k] == t) return n;
    return -1;
}
static int gy__first_leaf(int n) { while (n >= 0 && !gy__c.tn[n].leaf) n = gy__c.tn[n].a; return n; }

static void gy__tlayout(int n, gy_rect r) {
    gy__tnode *t = &gy__c.tn[n];
    t->r = r;
    if (t->leaf) return;
    if (t->dir == GY_H) {
        int aw = (int)((float)(r.w - 6) * t->ratio);
        gy__tlayout(t->a, (gy_rect){ r.x, r.y, aw, r.h });
        gy__tlayout(t->b, (gy_rect){ r.x + aw + 6, r.y, r.w - aw - 6, r.h });
    } else {
        int ah = (int)((float)(r.h - 6) * t->ratio);
        gy__tlayout(t->a, (gy_rect){ r.x, r.y, r.w, ah });
        gy__tlayout(t->b, (gy_rect){ r.x, r.y + ah + 6, r.w, r.h - ah - 6 });
    }
}

static gy_rect gy__tile_area(void) { return (gy_rect){ GY__T.pad / 2, GY__T.pad / 2, gy__c.w - GY__T.pad, gy__c.h - GY__T.pad }; }

/* how far the worst tile is under the minimum size, in px (0: every tile fits), after a fresh layout.
   A key, a flip or a divider move is refused when it would make this bigger (a tile never goes
   under its minimum by the user's hand; a window too small to start with doesn't lock the keys). */
static int gy__tiles_short(void) {
    int n, worst = 0;
    if (gy__c.troot < 0) return 0;
    gy__tlayout(gy__c.troot, gy__tile_area());
    for (n = 0; n < GY__TNODES; n++) {
        gy__tnode *t = &gy__c.tn[n];
        if (!t->used || !t->leaf) continue;
        worst = gy__maxi(worst, gy__maxi(GY__TILE_MIN_W - t->r.w, GY__TILE_MIN_H - t->r.h));
    }
    return worst;
}

/* a leaf can be split when both halves, on its longer side, keep the minimum tile size */
static int gy__tile_can_split(int n) {
    gy_rect r = gy__c.tn[n].r;
    if (r.w >= r.h) return (r.w - 6) / 2 >= GY__TILE_MIN_W && r.h >= GY__TILE_MIN_H;
    return (r.h - 6) / 2 >= GY__TILE_MIN_H && r.w >= GY__TILE_MIN_W;
}

/* put tile t next to the focused leaf (or as a tab when there is no room) */
static void gy__tile_insert(int t) {
    int leaf = gy__c.tfocus >= 0 && gy__c.tn[gy__c.tfocus].used ? gy__c.tfocus : gy__first_leaf(gy__c.troot), nl, sp;
    gy__tnode *L;
    if (gy__c.troot < 0) {
        gy__c.troot = gy__tnode_new();
        L = &gy__c.tn[gy__c.troot];
        L->leaf = 1; L->ntabs = 1; L->tabs[0] = t;
        gy__c.tfocus = gy__c.troot;
        gy__tlayout(gy__c.troot, gy__tile_area());
        return;
    }
    /* the focused leaf if it can be split, else a tab there if its stack has room, else the
       biggest leaf anywhere that can be split, else any stack with room; never a tile under
       the minimum size, never a ninth tab */
    if (!gy__tile_can_split(leaf)) {
        int n, best = -1, area = 0;
        if (gy__c.tn[leaf].ntabs < GY__TABS) { L = &gy__c.tn[leaf]; L->tabs[L->ntabs++] = t; return; }
        for (n = 0; n < GY__TNODES; n++)
            if (gy__c.tn[n].used && gy__c.tn[n].leaf && gy__tile_can_split(n) && gy__c.tn[n].r.w * gy__c.tn[n].r.h > area) { best = n; area = gy__c.tn[n].r.w * gy__c.tn[n].r.h; }
        if (best < 0) {
            for (n = 0; n < GY__TNODES; n++)
                if (gy__c.tn[n].used && gy__c.tn[n].leaf && gy__c.tn[n].ntabs < GY__TABS) { gy__c.tn[n].tabs[gy__c.tn[n].ntabs++] = t; return; }
            gy__set_error("GUICy: no room for another tile", gy__c.tl[t].name);
            return;
        }
        leaf = best;
    }
    L = &gy__c.tn[leaf];
    {
        int horiz = L->r.w >= L->r.h, i, freen = 0;
        for (i = 0; i < GY__TNODES; i++) freen += !gy__c.tn[i].used;
        if (freen < 2) { gy__set_error("GUICy: too many tiles", gy__c.tl[t].name); return; }
        nl = gy__tnode_new(); sp = gy__tnode_new();
        gy__c.tn[nl].leaf = 1; gy__c.tn[nl].ntabs = 1; gy__c.tn[nl].tabs[0] = t;
        gy__c.tn[sp].dir = horiz ? GY_H : GY_V; gy__c.tn[sp].ratio = 0.5f;
        gy__c.tn[sp].parent = gy__c.tn[leaf].parent;
        if (gy__c.tn[sp].parent < 0) gy__c.troot = sp;
        else if (gy__c.tn[gy__c.tn[sp].parent].a == leaf) gy__c.tn[gy__c.tn[sp].parent].a = sp;
        else gy__c.tn[gy__c.tn[sp].parent].b = sp;
        gy__c.tn[sp].a = leaf; gy__c.tn[sp].b = nl;
        gy__c.tn[leaf].parent = sp; gy__c.tn[nl].parent = sp;
    }
    gy__tlayout(gy__c.troot, gy__tile_area());
}

/* take tile t out of its stack; an empty leaf goes, and its sibling takes the parent's place */
static void gy__tile_remove(int t) {
    int leaf = gy__tile_leaf(t), k, p, sib, gp;
    gy__tnode *L;
    if (leaf < 0) return;
    L = &gy__c.tn[leaf];
    for (k = 0; k < L->ntabs && L->tabs[k] != t; k++) {}
    for (; k + 1 < L->ntabs; k++) L->tabs[k] = L->tabs[k + 1];
    L->ntabs--;
    if (L->active >= L->ntabs) L->active = gy__maxi(0, L->ntabs - 1);
    if (L->ntabs) return;
    p = L->parent;
    L->used = 0;
    if (gy__c.tzoom == leaf) gy__c.tzoom = -1;
    if (p < 0) { gy__c.troot = -1; gy__c.tfocus = -1; return; }
    sib = gy__c.tn[p].a == leaf ? gy__c.tn[p].b : gy__c.tn[p].a;
    gp = gy__c.tn[p].parent;
    gy__c.tn[sib].parent = gp;
    if (gp < 0) gy__c.troot = sib;
    else if (gy__c.tn[gp].a == p) gy__c.tn[gp].a = sib; else gy__c.tn[gp].b = sib;
    gy__c.tn[p].used = 0;
    if (gy__c.tfocus == leaf) gy__c.tfocus = gy__first_leaf(sib);
    gy__tlayout(gy__c.troot, gy__tile_area());
}

static int gy__tile_add(const char *name) {
    uint32_t id = gy__hash(name, 0x711E);
    int t = gy__tile_find(id);
    if (t >= 0) return t;
    if (gy__c.ntl >= GY__TILEMAX) { gy__set_error("GUICy: 32 tiles at most", name); return -1; }
    t = gy__c.ntl++;
    gy__memset(&gy__c.tl[t], 0, sizeof gy__c.tl[t]);
    gy__c.tl[t].id = id;
    {
        const char *e = gy__label_end(name);
        int n = gy__mini((int)(e - name), 39);
        gy__memcpy(gy__c.tl[t].name, name, (size_t)n); gy__c.tl[t].name[n] = 0;
    }
    gy__c.tl[t].seen = -1;
    return t;
}

static void gy__tile_ensure(int t) {
    if (gy__tile_leaf(t) < 0 && !gy__c.tl[t].hidden) gy__tile_insert(t);
}

void gy_tile_open(const char *name) {
    int t;
    if (!gy__init() || (t = gy__tile_add(name)) < 0) return;
    gy__c.tl[t].hidden = 0;
    gy__tile_ensure(t);
    gy__c.passes = gy__maxi(gy__c.passes, 1);
}
void gy_tile_close(const char *name) {
    int t;
    if (!gy__init() || (t = gy__tile_add(name)) < 0) return;   /* a tile not seen yet starts closed */
    gy__tile_remove(t);
    gy__c.tl[t].hidden = 1;
}
void gy_tile_focus(const char *name) {
    int t = gy__tile_find(gy__hash(name, 0x711E)), leaf = t >= 0 ? gy__tile_leaf(t) : -1, k;
    if (leaf < 0) return;
    gy__c.tfocus = leaf;
    for (k = 0; k < gy__c.tn[leaf].ntabs; k++) if (gy__c.tn[leaf].tabs[k] == t) gy__c.tn[leaf].active = k;
}

/* the leaf next to `from` in a direction: the nearest one whose edge faces it, most overlap first */
static int gy__tile_neighbour(int from, int dx, int dy) {
    gy_rect a = gy__c.tn[from].r;
    int n, best = -1, bestd = 1 << 30;
    for (n = 0; n < GY__TNODES; n++) {
        gy_rect b;
        int d, ov;
        if (!gy__c.tn[n].used || !gy__c.tn[n].leaf || n == from) continue;
        b = gy__c.tn[n].r;
        if (dx) {
            ov = gy__mini(a.y + a.h, b.y + b.h) - gy__maxi(a.y, b.y);
            d = dx > 0 ? b.x - (a.x + a.w) : a.x - (b.x + b.w);
        } else {
            ov = gy__mini(a.x + a.w, b.x + b.w) - gy__maxi(a.x, b.x);
            d = dy > 0 ? b.y - (a.y + a.h) : a.y - (b.y + b.h);
        }
        if (ov <= 0 || d < -2) continue;
        d = d * 4096 - ov;
        if (d < bestd) { bestd = d; best = n; }
    }
    return best;
}

static void gy__tile_swap_leaves(int a, int b) {
    gy__tnode t = gy__c.tn[a];
    gy__memcpy(gy__c.tn[a].tabs, gy__c.tn[b].tabs, sizeof t.tabs);
    gy__c.tn[a].ntabs = gy__c.tn[b].ntabs; gy__c.tn[a].active = gy__c.tn[b].active;
    gy__memcpy(gy__c.tn[b].tabs, t.tabs, sizeof t.tabs);
    gy__c.tn[b].ntabs = t.ntabs; gy__c.tn[b].active = t.active;
}

static void gy__tile_keys(void) {
    int f = gy__c.tfocus, a, k, n, p;
    static const int dirs[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    if (gy__c.focus || f < 0 || !gy__c.tn[f].used) return;
    for (a = 0; a < GY__TA_COUNT; a++) {
        k = gy__c.tkeys[a];
        if (!k || !gy__take_key(k & 0xFFFF, k >> 16)) continue;
        if (a <= GY__TA_FOCUS_D) { n = gy__tile_neighbour(f, dirs[a][0], dirs[a][1]); if (n >= 0) gy__c.tfocus = f = n; }
        else if (a <= GY__TA_SWAP_D) { n = gy__tile_neighbour(f, dirs[a - 4][0], dirs[a - 4][1]); if (n >= 0) { gy__tile_swap_leaves(f, n); gy__c.tfocus = f = n; } }
        else if (a == GY__TA_GROW || a == GY__TA_SHRINK) {
            p = gy__c.tn[f].parent;
            if (p >= 0) {
                float d = a == GY__TA_GROW ? 0.05f : -0.05f, old = gy__c.tn[p].ratio;
                int before = gy__tiles_short();
                gy__c.tn[p].ratio = gy__clampf(gy__c.tn[p].ratio + (gy__c.tn[p].a == f ? d : -d), 0.1f, 0.9f);
                if (gy__tiles_short() > before) gy__c.tn[p].ratio = old;   /* a tile would go under its minimum: refused */
            }
        }
        else if (a == GY__TA_FLIP) {
            p = gy__c.tn[f].parent;
            if (p >= 0) {
                int before = gy__tiles_short();
                gy__c.tn[p].dir = !gy__c.tn[p].dir;
                if (gy__tiles_short() > before) gy__c.tn[p].dir = !gy__c.tn[p].dir;   /* refused, as above */
            }
        }
        else if (a == GY__TA_ZOOM) gy__c.tzoom = gy__c.tzoom == f ? -1 : f;
        else if (a == GY__TA_CLOSE) {
            int t = gy__c.tn[f].tabs[gy__c.tn[f].active];
            gy__tile_remove(t); gy__c.tl[t].hidden = 1; f = gy__c.tfocus;
            if (f < 0) return;
        }
        else if (a == GY__TA_TAB && gy__c.tn[f].ntabs > 1) gy__c.tn[f].active = (gy__c.tn[f].active + 1) % gy__c.tn[f].ntabs;
        gy__tlayout(gy__c.troot, gy__tile_area());
    }
}

/* the dividers, title bars and tabs; called once a frame, at the first gy_tile */
static void gy__tile_chrome(void) {
    int n, k, hot, held;
    if (gy__c.troot < 0) return;
    gy__tlayout(gy__c.troot, gy__tile_area());
    gy__tile_keys();
    if (gy__c.troot < 0) return;
    gy__tlayout(gy__c.troot, gy__tile_area());
    for (n = 0; n < GY__TNODES; n++) {         /* dividers between the two sides of each split */
        gy__tnode *t = &gy__c.tn[n];
        gy_rect d;
        if (!t->used || t->leaf || gy__c.tzoom >= 0) continue;
        if (t->dir == GY_H) { d.x = gy__c.tn[t->a].r.x + gy__c.tn[t->a].r.w; d.y = t->r.y; d.w = 6; d.h = t->r.h; }
        else { d.x = t->r.x; d.y = gy__c.tn[t->a].r.y + gy__c.tn[t->a].r.h; d.w = t->r.w; d.h = 6; }
        (void)gy__button_logic(0x7D1Du + (uint32_t)n, d, &hot, &held);
        if (held || (hot && gy__c.dbl[0])) {   /* a drag, or a double-click back to the middle; never a tile under its minimum */
            float old = t->ratio;
            int before = gy__tiles_short();
            if (hot && gy__c.dbl[0]) { t->ratio = 0.5f; gy__c.active = 0; }
            else t->ratio = gy__clampf(t->dir == GY_H ? (float)(gy__c.mx - t->r.x) / (float)gy__maxi(1, t->r.w) : (float)(gy__c.my - t->r.y) / (float)gy__maxi(1, t->r.h), 0.1f, 0.9f);
            if (gy__tiles_short() > before) {  /* too far: the divider stops at the last place every tile fits */
                float ok = old, bad = t->ratio;
                int it;
                for (it = 0; it < 12; it++) {
                    t->ratio = (ok + bad) * 0.5f;
                    if (gy__tiles_short() > before) bad = t->ratio; else ok = t->ratio;
                }
                t->ratio = ok;
                gy__tlayout(gy__c.troot, gy__tile_area());
            }
        }
        if (hot || held) SetCursor(LoadCursorW(NULL, (LPCWSTR)(t->dir == GY_H ? IDC_SIZEWE : IDC_SIZENS)));
        if (hot || held) gy_canvas_rect(d.w == 6 ? (gy_rect){ d.x + 2, d.y, 2, d.h } : (gy_rect){ d.x, d.y + 2, d.w, 2 }, GY__T.divider_hot, 1);
    }
    gy__tlayout(gy__c.troot, gy__tile_area());
    for (n = 0; n < GY__TNODES; n++) {         /* each leaf: its card, its title bar with tabs */
        gy__tnode *t = &gy__c.tn[n];
        gy_rect r, bar;
        int x;
        if (!t->used || !t->leaf || (gy__c.tzoom >= 0 && n != gy__c.tzoom)) continue;
        r = gy__c.tzoom == n ? gy__tile_area() : t->r;
        gy_canvas_box(r, GY__T.panel, GY__T.border, (float)GY__T.radius, 1);
        bar = r; bar.h = GY__TITLE_H;
        gy_canvas_box((gy_rect){ bar.x + 1, bar.y + 1, bar.w - 2, bar.h - 1 }, GY__T.card, 0, (float)GY__T.radius, 0);
        if (n == gy__c.tfocus) {               /* the focus outline on its own: four thin sides and four
                                                  clipped corners, so a focus move redraws only the edge tiles */
            int cs = GY__T.radius + 2, c4, saved = GY__L.clip;
            gy_canvas_rect((gy_rect){ r.x + cs, r.y, r.w - 2 * cs, 1 }, GY__T.accent, 0);
            gy_canvas_rect((gy_rect){ r.x + cs, r.y + r.h - 1, r.w - 2 * cs, 1 }, GY__T.accent, 0);
            gy_canvas_rect((gy_rect){ r.x, r.y + cs, 1, r.h - 2 * cs }, GY__T.accent, 0);
            gy_canvas_rect((gy_rect){ r.x + r.w - 1, r.y + cs, 1, r.h - 2 * cs }, GY__T.accent, 0);
            for (c4 = 0; c4 < 4; c4++) {
                GY__L.clip = gy__clip_push((gy_rect){ c4 & 1 ? r.x + r.w - cs : r.x, c4 & 2 ? r.y + r.h - cs : r.y, cs, cs });
                gy_canvas_box(r, 0, GY__T.accent, (float)GY__T.radius, 1);
                GY__L.clip = saved;
            }
        }
        x = bar.x + 6;
        for (k = 0; k < t->ntabs; k++) {
            gy__tile *tl = &gy__c.tl[t->tabs[k]];
            int w = gy_text_width(tl->name) + 16, th, tdn;
            gy_rect tab = { x, bar.y + 2, w, bar.h - 3 };
            if (gy__button_logic(0x7AB0u ^ tl->id, tab, &th, &tdn)) { t->active = k; gy__c.tfocus = n; }
            if (tdn && gy__c.pressed[0]) gy__c.tdrag = t->tabs[k];
            if (k == t->active && t->ntabs > 1) gy_canvas_rect(tab, GY__T.button, 3);
            gy_canvas_text(x + 8, bar.y + (bar.h - gy_line_height()) / 2, tl->name, k == t->active ? (n == gy__c.tfocus ? GY__T.text : GY__T.text_dim) : GY__T.text_off);
            x += w + 2;
        }
        if (gy__c.pressed[0] && gy__inside(r, gy__c.mx, gy__c.my)) gy__c.tfocus = n;
    }
    /* dragging a title onto another tile: the middle stacks it as a tab there, the rest swaps the two */
    if (gy__c.tdrag >= 0) {
        int from = gy__tile_leaf(gy__c.tdrag);
        for (n = 0; n < GY__TNODES && from >= 0; n++) {
            gy__tnode *t = &gy__c.tn[n];
            gy_rect mid;
            if (!t->used || !t->leaf || n == from || !gy__inside(t->r, gy__c.mx, gy__c.my)) continue;
            mid.x = t->r.x + t->r.w * 3 / 10; mid.y = t->r.y + t->r.h * 3 / 10; mid.w = t->r.w * 4 / 10; mid.h = t->r.h * 4 / 10;
            gy__c.layer = 2;
            gy_canvas_box(gy__inside(mid, gy__c.mx, gy__c.my) ? mid : t->r, 0x40009696u, GY__T.accent, (float)GY__T.radius, 2);
            gy__c.layer = 0;
            if (gy__c.released[0]) {
                if (gy__inside(mid, gy__c.mx, gy__c.my) && t->ntabs < GY__TABS) {
                    int d = gy__c.tdrag;
                    gy__tile_remove(d);
                    n = gy__tile_leaf(t->tabs[0]);
                    if (n >= 0) { gy__c.tn[n].tabs[gy__c.tn[n].ntabs] = d; gy__c.tn[n].active = gy__c.tn[n].ntabs++; gy__c.tfocus = n; }
                } else { gy__tile_swap_leaves(from, n); gy__c.tfocus = n; }
                gy__tlayout(gy__c.troot, gy__tile_area());
            }
            break;
        }
        if (gy__c.released[0] || !gy__c.down[0]) gy__c.tdrag = -1;
    }
}

int gy_tile(const char *name) {
    int t, leaf, k;
    gy_rect r;
    gy__lay *l;
    if (!gy__init()) return 0;
    if (gy__c.in_tile && gy__c.nlay > 1 && GY__L.kind == GY__L_TILE) { while (gy__c.nlay > 1) gy__c.nlay--; }
    gy__c.in_tile = 0;
    t = gy__tile_add(name);
    if (t < 0) return 0;
    gy__c.tl[t].seen = gy__c.frames;
    gy__tile_ensure(t);
    if (gy__c.tlaid != gy__c.frames + 1) { gy__c.tlaid = gy__c.frames + 1; gy__tile_chrome(); }
    leaf = gy__tile_leaf(t);
    if (gy__c.tl[t].hidden || leaf < 0) return 0;
    if (gy__c.tzoom >= 0 && leaf != gy__c.tzoom) return 0;
    for (k = 0; k < gy__c.tn[leaf].ntabs; k++) if (gy__c.tn[leaf].tabs[k] == t && k != gy__c.tn[leaf].active) return 0;
    r = gy__c.tzoom == leaf ? gy__tile_area() : gy__c.tn[leaf].r;
    r.y += GY__TITLE_H; r.h -= GY__TITLE_H;
    r.x += GY__T.pad; r.w -= 2 * GY__T.pad; r.y += GY__T.pad / 2; r.h -= GY__T.pad;
    l = gy__lay_push(GY__L_TILE, r, 0);
    l->clip = gy__clip_push((gy_rect){ r.x - 2, r.y - 2, r.w + 4, r.h + 4 });
    l->id = gy__c.tl[t].id;
    gy__c.in_tile = 1;
    return 1;
}

/* the layout as one line: H0.50(V0.40([Picker],[Preview|Log]*1),[Settings]) */
static int gy__tsave(int n, char *b, int cap, int at) {
    gy__tnode *t = &gy__c.tn[n];
    int k;
    char num[16];
#define GY__OUT(ch) do { if (at < cap - 1) b[at] = (char)(ch); at++; } while (0)
    if (t->leaf) {
        GY__OUT('[');
        for (k = 0; k < t->ntabs; k++) { const char *s = gy__c.tl[t->tabs[k]].name; if (k) GY__OUT('|'); while (*s) GY__OUT(*s++); }
        GY__OUT(']');
        if (t->active) { GY__OUT('*'); GY__OUT('0' + t->active); }
        return at;
    }
    GY__OUT(t->dir == GY_H ? 'H' : 'V');
    gy__fmt(num, sizeof num, "%.3f", (double)t->ratio);
    for (k = 0; num[k]; k++) GY__OUT(num[k]);
    GY__OUT('(');
    at = gy__tsave(t->a, b, cap, at);
    GY__OUT(',');
    at = gy__tsave(t->b, b, cap, at);
    GY__OUT(')');
#undef GY__OUT
    return at;
}

int gy_tiles_save(char *buf, int cap) {
    int n;
    if (cap <= 0) return 0;
    if (gy__c.troot < 0) { buf[0] = 0; return 1; }
    n = gy__tsave(gy__c.troot, buf, cap, 0);
    if (n >= cap) { buf[cap - 1] = 0; return 0; }
    buf[n] = 0;
    return 1;
}

static int gy__tload(const char **ps, int parent) {
    const char *s = *ps;
    int n = gy__tnode_new();
    if (n < 0) return -1;
    gy__c.tn[n].parent = parent;
    if (*s == '[') {
        gy__c.tn[n].leaf = 1;
        s++;
        while (*s && *s != ']') {
            char name[40];
            int k = 0, t;
            while (*s && *s != '|' && *s != ']' && k < 39) name[k++] = *s++;
            name[k] = 0;
            while (*s && *s != '|' && *s != ']') s++;
            t = gy__tile_add(name);
            if (t >= 0 && gy__c.tn[n].ntabs < GY__TABS) { gy__c.tn[n].tabs[gy__c.tn[n].ntabs++] = t; gy__c.tl[t].from_load = 1; gy__c.tl[t].hidden = 0; }
            if (*s == '|') s++;
        }
        if (*s == ']') s++;
        if (*s == '*') { s++; gy__c.tn[n].active = *s >= '0' && *s <= '9' ? *s++ - '0' : 0; }
        if (gy__c.tn[n].active >= gy__c.tn[n].ntabs) gy__c.tn[n].active = 0;
    } else if (*s == 'H' || *s == 'V') {
        int ok;
        const char *e;
        char num[16];
        int k = 0;
        gy__c.tn[n].dir = *s == 'H' ? GY_H : GY_V;
        s++;
        for (e = s; *e && *e != '(' && k < 15; e++) num[k++] = *e;
        num[k] = 0;
        gy__c.tn[n].ratio = gy__clampf((float)gy__atof(num, &ok), 0.1f, 0.9f);
        if (!ok) gy__c.tn[n].ratio = 0.5f;
        s = e;
        if (*s != '(') return -1;
        s++;
        gy__c.tn[n].a = gy__tload(&s, n);
        if (*s != ',') return -1;
        s++;
        gy__c.tn[n].b = gy__tload(&s, n);
        if (*s != ')' || gy__c.tn[n].a < 0 || gy__c.tn[n].b < 0) return -1;
        s++;
    } else return -1;
    *ps = s;
    return n;
}

int gy_tiles_load(const char *line) {
    const char *s = line;
    int r, i;
    if (!gy__init() || !line || !*line) return 0;
    for (i = 0; i < GY__TNODES; i++) gy__c.tn[i].used = 0;
    gy__c.troot = gy__c.tfocus = gy__c.tzoom = -1;
    r = gy__tload(&s, -1);
    if (r < 0) {                               /* a broken line: start empty, the tiles come back as the app asks */
        for (i = 0; i < GY__TNODES; i++) gy__c.tn[i].used = 0;
        gy__set_error("gy_tiles_load: the saved layout could not be read", line);
        return 0;
    }
    gy__c.troot = r; gy__c.tfocus = gy__first_leaf(r);
    gy__c.tloaded = 1;
    return 1;
}

static const char *const gy__key_names[] = { "Left", "Right", "Up", "Down", "Plus", "Minus", "Enter", "Esc", "Space", "Tab", "Home", "End", "PgUp", "PgDn", "Delete" };
static const int gy__key_codes[] = { GY_KEY_LEFT, GY_KEY_RIGHT, GY_KEY_UP, GY_KEY_DOWN, GY_KEY_PLUS, GY_KEY_MINUS, GY_KEY_ENTER, GY_KEY_ESC, GY_KEY_SPACE, GY_KEY_TAB, GY_KEY_HOME, GY_KEY_END, GY_KEY_PGUP, GY_KEY_PGDN, GY_KEY_DELETE };

/* "focus_left=Alt+Left;swap_left=Alt+Shift+Left;..." ("none" = off) */
int gy_tile_keys_save(char *buf, int cap) {
    int a, at = 0, i;
    char part[64];
    if (cap <= 0) return 0;
    buf[0] = 0;
    for (a = 0; a < GY__TA_COUNT; a++) {
        int k = gy__c.tkeys[a], key = k & 0xFFFF, m = k >> 16, n = 0;
        char kn[16];
        kn[0] = 0;
        for (i = 0; i < (int)(sizeof gy__key_codes / sizeof gy__key_codes[0]); i++) if (gy__key_codes[i] == key) gy__strcpy(kn, gy__key_names[i], 16);
        if (!kn[0]) { kn[0] = (char)key; kn[1] = 0; }
        if (!k) n = (int)(gy__fmt(part, sizeof part, "%s=none;", gy__tkey_names[a]), gy__strlen(part));
        else n = (int)(gy__fmt(part, sizeof part, "%s=%s%s%s%s;", gy__tkey_names[a], m & GY_MOD_CTRL ? "Ctrl+" : "", m & GY_MOD_ALT ? "Alt+" : "",
                               m & GY_MOD_SHIFT ? "Shift+" : "", kn), gy__strlen(part));
        if (at + n >= cap) return 0;
        gy__memcpy(buf + at, part, (size_t)n + 1);
        at += n;
    }
    return 1;
}

int gy_tile_keys_load(const char *s) {
    int ok = 1;
    if (!gy__init() || !s) return 0;
    while (*s) {
        char name[32], val[32];
        int n = 0, v = 0, a, m = 0, key = 0, i;
        while (*s && *s != '=' && n < 31) name[n++] = *s++;
        name[n] = 0;
        if (*s == '=') s++;
        while (*s && *s != ';' && v < 31) val[v++] = *s++;
        val[v] = 0;
        if (*s == ';') s++;
        for (a = 0; a < GY__TA_COUNT && !gy__streq(name, gy__tkey_names[a]); a++) {}
        if (a == GY__TA_COUNT) { ok = 0; continue; }
        if (gy__streq(val, "none")) { gy__c.tkeys[a] = 0; continue; }
        {
            const char *p = val;
            for (;;) {
                if (p[0] == 'C' && p[1] == 't' && p[2] == 'r' && p[3] == 'l' && p[4] == '+') { m |= GY_MOD_CTRL; p += 5; }
                else if (p[0] == 'A' && p[1] == 'l' && p[2] == 't' && p[3] == '+') { m |= GY_MOD_ALT; p += 4; }
                else if (p[0] == 'S' && p[1] == 'h' && p[2] == 'i' && p[3] == 'f' && p[4] == 't' && p[5] == '+') { m |= GY_MOD_SHIFT; p += 6; }
                else break;
            }
            for (i = 0; i < (int)(sizeof gy__key_codes / sizeof gy__key_codes[0]); i++) if (gy__streq(p, gy__key_names[i])) key = gy__key_codes[i];
            if (!key && p[0] && !p[1]) key = p[0] >= 'a' && p[0] <= 'z' ? p[0] - 32 : p[0];
            if (!key) { ok = 0; continue; }
            gy__c.tkeys[a] = m << 16 | key;
        }
    }
    gy__tile_clash_check();
    return ok;
}

/* ================================================================================================
   The frame
   ================================================================================================ */

static int gy__dib(int w, int h);

#ifdef GUICY_TESTS
int gy_test_texts(const gy_test_text **out) { *out = gy__c.texts; return gy__c.ntexts; }

gy_color gy_test_text_bg(int i) {
    int k, cx, cy;
    if (i < 0 || i >= gy__c.ntexts) return gy__c.theme.bg;
    cx = gy__c.texts[i].r.x + gy__c.texts[i].r.w / 2; cy = gy__c.texts[i].r.y + gy__c.texts[i].r.h / 2;
    for (k = gy__mini(gy__c.texts[i].inst, gy__c.ninst) - 1; k >= 0; k--) {   /* the last opaque box under its middle */
        const gy__inst *in = &gy__c.inst[k];
        gy_rect cl = gy__c.clips[in->clip];
        if (in->kind == GY__K_BOX && (in->col0 >> 24) == 255 && cx >= in->x0 && cx < in->x1 && cy >= in->y0 && cy < in->y1
            && cx >= cl.x && cy >= cl.y && cx < cl.x + cl.w && cy < cl.y + cl.h) return in->col0;
    }
    return gy__c.theme.bg;
}
#endif

void gy_begin(void) {
    gy__lay *l;
    if (!gy__init()) return;
    if (!gy__c.test_clock) gy__c.now = gy__os.ticks();
    if (gy__c.hwnd && gy__c.resize_w && (gy__c.resize_w != gy__c.w || gy__c.resize_h != gy__c.h)) gy__dib(gy__c.resize_w, gy__c.resize_h);
    gy__c.resize_w = gy__c.resize_h = 0;
    if (gy__c.attached && gy__c.passes > 0) gy__c.passes--;   /* an app's own loop: this frame is one of the asked-for passes */
    if (gy__c.anim_on && (int)(gy__c.now - gy__c.anim_at) >= 0) gy__c.anim_on = 0;   /* this is the frame it asked for */
    gy__c.frame.used = 0;
    gy__c.inst = NULL; gy__c.ninst = gy__c.cap_inst = 0;
    gy__c.nclips = 1; gy__c.layer = 0;
    gy__c.clips[0].x = 0; gy__c.clips[0].y = 0; gy__c.clips[0].w = gy__c.w; gy__c.clips[0].h = gy__c.h;
    gy__c.nlay = 0;
    l = gy__lay_push(GY__L_ROOT, (gy_rect){ GY__T.pad, GY__T.pad, gy__c.w - 2 * GY__T.pad, gy__c.h - 2 * GY__T.pad }, 0);
    (void)l;
    gy__c.id_depth = 0; gy__c.disabled = 0; gy__c.font = 0; gy__c.ncolstack = 0;
    gy__c.nblock_cur = 0; gy__c.popup_used = 0; gy__c.tip_shown = 0; gy__c.focus_claimed = 0;
    gy__c.in_tile = 0;
    gy__c.in_frame = 1;
#ifdef GUICY_TESTS
    gy__c.ntexts = 0;
#endif
}

/* --- dirty tiles ---------------------------------------------------------------------------------
   Every instance's 48 bytes (and its clip) are hashed into each 64x64 tile it touches, in drawing
   order. A tile whose hash equals last frame's already shows the right pixels and is not touched.
   A changed tile is cleared and every instance over it is drawn again, clipped to the tile, so the
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

/* a box pixel's colour from its two coverages: the box (co) and the inside of its border (ci) */
static uint32_t gy__box_mix(const gy__inst *in, uint32_t co, uint32_t ci) {
    uint32_t s;
    if (in->border <= 0) return gy__scale(in->col0, co);
    s = gy__scale(in->col0, ci);
    if (co > ci) {                              /* add the ring's share of the border colour */
        uint32_t b = gy__scale(in->col1, co - ci), o = 0;
        int sh;
        for (sh = 0; sh < 32; sh += 8) o |= (uint32_t)gy__mini((int)(((s >> sh) & 255) + ((b >> sh) & 255)), 255) << sh;
        s = o;
    }
    return s;
}

static uint32_t gy__box_px(const gy__inst *in, float px, float py, float cx, float cy, float hx, float hy) {
    uint32_t co = gy__box_cov(px, py, cx, cy, hx, hy, in->radius), ci = 0;
    if (in->border > 0) ci = gy__box_cov(px, py, cx, cy, hx - in->border, hy - in->border, gy__maxf(in->radius - in->border, 0));
    return gy__box_mix(in, co, ci);
}

/* --- the corner cache --------------------------------------------------------------------------
   For a box whose radius and border are multiples of 1/4 px, and which is wider than its two corner
   column runs and taller than its two edge bands, the coverage of a corner pixel depends only on
   (radius, border) and the pixel's distance from the box's edges: every term in gy__box_cov is
   then an exact float (multiples of 1/4 under 2^22), so the same numbers come out for any box
   size. They are worked out once per (radius, border) and read back, so the same bytes. */
#define GY__CORNER_KEYS 32
typedef struct gy__corner {
    int r4, b4, band, cw;                      /* key (radius and border x 4), and the sizes */
    unsigned char *co, *ci;                    /* [band][cw]: a band row j from the edge, column k from the edge */
    unsigned char *mco, *mci;                  /* [cw]: a middle row, column k from the edge */
    unsigned char *rco, *rci;                  /* [band]: a band row's straight middle span */
} gy__corner;
static gy__corner gy__corners[GY__CORNER_KEYS];

static const gy__corner *gy__corner_get(const gy__inst *in, int band, int cw) {
    int r4 = (int)(in->radius * 4.0f), b4 = (int)(in->border * 4.0f), i, j, k;
    gy__corner *c;
    float hx, hy, rr, ri, bb;
    if ((float)r4 != in->radius * 4.0f || (float)b4 != in->border * 4.0f || in->radius > 64 || in->border > 16) return NULL;
    for (i = 0; i < gy__ncorners; i++) if (gy__corners[i].r4 == r4 && gy__corners[i].b4 == b4) return &gy__corners[i];
    if (gy__ncorners == GY__CORNER_KEYS) return NULL;
    c = &gy__corners[gy__ncorners];
    c->co = (unsigned char *)gy__arena_push(&gy__c.perm, (size_t)(band * cw * 2 + cw * 2 + band * 2));
    if (!c->co) return NULL;
    c->ci = c->co + band * cw; c->mco = c->ci + band * cw; c->mci = c->mco + cw; c->rco = c->mci + cw; c->rci = c->rco + band;
    c->r4 = r4; c->b4 = b4; c->band = band; c->cw = cw;
    /* a reference box big enough that its middle is far from every corner */
    hx = hy = 4096; rr = in->radius; bb = in->border; ri = gy__maxf(rr - bb, 0);
    for (j = 0; j < band; j++)
        for (k = 0; k < cw; k++) {
            float px = -hx + (float)k + 0.5f, py = -hy + (float)j + 0.5f;
            c->co[j * cw + k] = (unsigned char)gy__box_cov(px, py, 0, 0, hx, hy, rr);
            c->ci[j * cw + k] = bb > 0 ? (unsigned char)gy__box_cov(px, py, 0, 0, hx - bb, hy - bb, ri) : 0;
        }
    for (k = 0; k < cw; k++) {
        float px = -hx + (float)k + 0.5f;
        c->mco[k] = (unsigned char)gy__box_cov(px, 0, 0, 0, hx, hy, rr);
        c->mci[k] = bb > 0 ? (unsigned char)gy__box_cov(px, 0, 0, 0, hx - bb, hy - bb, ri) : 0;
    }
    for (j = 0; j < band; j++) {
        float py = -hy + (float)j + 0.5f;
        c->rco[j] = (unsigned char)gy__box_cov(0, py, 0, 0, hx, hy, rr);
        c->rci[j] = bb > 0 ? (unsigned char)gy__box_cov(0, py, 0, 0, hx - bb, hy - bb, ri) : 0;
    }
    gy__ncorners++;
    return c;
}

static void gy__draw_box_cpu(const gy__inst *in, gy_rect clip) {
    int x0 = gy__maxi(in->x0, clip.x), y0 = gy__maxi(in->y0, clip.y);
    int x1 = gy__mini(in->x1, clip.x + clip.w), y1 = gy__mini(in->y1, clip.y + clip.h);
    int n = x1 - x0, x, y, band, cw, lx1, rx0;
    const gy__corner *cc;
    float cx = (float)(in->x0 + in->x1) * 0.5f, cy = (float)(in->y0 + in->y1) * 0.5f;
    float hx = (float)(in->x1 - in->x0) * 0.5f, hy = (float)(in->y1 - in->y0) * 0.5f;
    uint32_t *row;
    size_t mark;
    if (n <= 0 || y1 <= y0) return;
    if (in->radius <= 0 && in->border <= 0) {  /* a plain rectangle: whole rows, the fast path */
        for (y = y0; y < y1; y++) gy__c.k_fill(gy__c.px + (size_t)y * gy__c.w + x0, n, in->col0);
        return;
    }
    /* Rows away from the top and bottom edge bands depend only on x: one row is worked out and
       reused. Across every row, the pixels more than cw from the left and right edges all get the
       same value (their distance to the box depends on y only: qx stays below qy, so max(qx, qy)
       is qy and the corner length has no x part), so only the two corner columns are done pixel
       by pixel and the span between them is one fill. Same bytes as doing every pixel (gytest). */
    band = (int)(gy__maxf(in->radius, in->border) + 1.0f);
    cw = (int)(in->radius + in->border) + 3;
    if (2 * cw >= in->x1 - in->x0) cw = in->x1 - in->x0;  /* narrow: every pixel by itself */
    lx1 = gy__clampi(in->x0 + cw, x0, x1);     /* [x0, lx1) the left corner columns (inside the clip) */
    rx0 = gy__clampi(in->x1 - cw, lx1, x1);    /* [rx0, x1) the right ones; between them the span */
    cc = !gy__corner_off && 2 * cw < in->x1 - in->x0 && 2 * band < in->y1 - in->y0 ? gy__corner_get(in, band, cw) : NULL;
    mark = gy__c.frame.used;
    row = (uint32_t *)gy__arena_push(&gy__c.frame, sizeof(uint32_t) * (size_t)n);
    if (!row) return;
    if (cc) {                                  /* the cached coverages, mirrored for the right side */
        for (x = x0; x < lx1; x++) { int k = x - in->x0; row[x - x0] = gy__box_mix(in, cc->mco[k], cc->mci[k]); }
        for (x = rx0; x < x1; x++) { int k = in->x1 - 1 - x; row[x - x0] = gy__box_mix(in, cc->mco[k], cc->mci[k]); }
        for (y = y0; y < y1; y++) {
            uint32_t *d = gy__c.px + (size_t)y * gy__c.w;
            int j;
            if (y >= in->y0 + band && y < in->y1 - band) {
                if (lx1 > x0) gy__c.k_row(d + x0, row, lx1 - x0);
                if (rx0 > lx1) gy__c.k_fill(d + lx1, rx0 - lx1, in->col0);
                if (x1 > rx0) gy__c.k_row(d + rx0, row + (rx0 - x0), x1 - rx0);
                continue;
            }
            j = y < in->y0 + band ? y - in->y0 : in->y1 - 1 - y;      /* the band row, from its edge */
            for (x = x0; x < lx1; x++) {
                int k = x - in->x0;
                uint32_t s = gy__box_mix(in, cc->co[j * cw + k], cc->ci[j * cw + k]);
                if (s) d[x] = gy__over(d[x], s);
            }
            if (rx0 > lx1) gy__c.k_fill(d + lx1, rx0 - lx1, gy__box_mix(in, cc->rco[j], cc->rci[j]));
            for (x = rx0; x < x1; x++) {
                int k = in->x1 - 1 - x;
                uint32_t s = gy__box_mix(in, cc->co[j * cw + k], cc->ci[j * cw + k]);
                if (s) d[x] = gy__over(d[x], s);
            }
        }
        gy__c.frame.used = mark;
        return;
    }
    for (x = x0; x < lx1; x++) row[x - x0] = gy__box_px(in, (float)x + 0.5f, cy, cx, cy, hx, hy);
    for (x = rx0; x < x1; x++) row[x - x0] = gy__box_px(in, (float)x + 0.5f, cy, cx, cy, hx, hy);
    for (y = y0; y < y1; y++) {
        uint32_t *d = gy__c.px + (size_t)y * gy__c.w;
        uint32_t mid;
        if (y >= in->y0 + band && y < in->y1 - band) {     /* a middle row: edges from the row, inside filled */
            if (lx1 > x0) gy__c.k_row(d + x0, row, lx1 - x0);
            if (rx0 > lx1) gy__c.k_fill(d + lx1, rx0 - lx1, in->col0);
            if (x1 > rx0) gy__c.k_row(d + rx0, row + (rx0 - x0), x1 - rx0);
            continue;
        }
        for (x = x0; x < lx1; x++) {
            uint32_t s = gy__box_px(in, (float)x + 0.5f, (float)y + 0.5f, cx, cy, hx, hy);
            if (s) d[x] = gy__over(d[x], s);
        }
        if (rx0 > lx1) {
            mid = gy__box_px(in, cx, (float)y + 0.5f, cx, cy, hx, hy);
            gy__c.k_fill(d + lx1, rx0 - lx1, mid);
        }
        for (x = rx0; x < x1; x++) {
            uint32_t s = gy__box_px(in, (float)x + 0.5f, (float)y + 0.5f, cx, cy, hx, hy);
            if (s) d[x] = gy__over(d[x], s);
        }
    }
    gy__c.frame.used = mark;                   /* the row was scratch */
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

static void gy__draw_line_cpu(const gy__inst *in, gy_rect clip) {
    int x0 = gy__maxi(in->x0, clip.x), y0 = gy__maxi(in->y0, clip.y);
    int x1 = gy__mini(in->x1, clip.x + clip.w), y1 = gy__mini(in->y1, clip.y + clip.h), x, y;
    float ax = in->border, ay = in->soft, bx = gy__bitsf(in->a), by = gy__bitsf(in->b), hw = in->radius;
    float dx = bx - ax, dy = by - ay, len = gy__sqrtf(dx * dx + dy * dy), R = hw + 0.5f;
    float minx = gy__minf(ax, bx) - R, maxx = gy__maxf(ax, bx) + R;
    float miny = gy__minf(ay, by) - R, maxy = gy__maxf(ay, by) + R;
    float g[6];
    g[0] = ax; g[1] = ay; g[2] = bx; g[3] = by; g[4] = hw; g[5] = R * R;
    if (gy__absf(dx) > gy__absf(dy)) {         /* a flat line: walk the columns, each needs only a few rows */
        for (x = x0; x < x1; x++) {
            float px = (float)x + 0.5f, xc_y = ay + (px - ax) * dy / dx, half = R * len / gy__absf(dx);
            int ys = gy__maxi(y0, (int)gy__maxf(miny, xc_y - half) - 2), ye = gy__mini(y1, (int)gy__minf(maxy, xc_y + half) + 2);
            for (y = ys; y < ye; y++) gy__line_px(gy__c.px + (size_t)y * gy__c.w + x, px, (float)y + 0.5f, g, in->col0);
        }
        return;
    }
    for (y = y0; y < y1; y++) {
        uint32_t *d = gy__c.px + (size_t)y * gy__c.w;
        float py = (float)y + 0.5f, lo = minx, hi = maxx;
        int xs, xe;
        /* only the pixels that can be within R of the line: within R of the infinite line means
           within R*len/|dy| of where it crosses this row (a superset, so the same bytes) */
        if (gy__absf(dy) > 1e-6f) {
            float xc = ax + (py - ay) * dx / dy, half = R * len / gy__absf(dy);
            lo = gy__maxf(lo, xc - half); hi = gy__minf(hi, xc + half);
        } else if (gy__absf(py - ay) >= R) continue;
        xs = gy__maxi(x0, (int)lo - 2); xe = gy__mini(x1, (int)hi + 2);
        for (x = xs; x < xe; x++) gy__line_px(d + x, (float)x + 0.5f, py, g, in->col0);
    }
}

static void gy__draw_image_cpu(const gy__inst *in, gy_rect clip) {
    int x0 = gy__maxi(in->x0, clip.x), y0 = gy__maxi(in->y0, clip.y);
    int x1 = gy__mini(in->x1, clip.x + clip.w), y1 = gy__mini(in->y1, clip.y + clip.h), x, y;
    const gy__image *im = &gy__c.img[in->a < GY__IMAGES ? in->a : 0];
    int dw = in->x1 - in->x0, dh = in->y1 - in->y0, smooth = (int)(in->c & 1u);
    uint32_t *row;
    int *colx;                                 /* per column: the source x (nearest) or x0, x1, weight */
    size_t mark;
    if (!im->px || x1 <= x0 || y1 <= y0 || dw <= 0 || dh <= 0) return;
    mark = gy__c.frame.used;
    row = (uint32_t *)gy__arena_push(&gy__c.frame, sizeof(uint32_t) * (size_t)(x1 - x0));
    colx = (int *)gy__arena_push(&gy__c.frame, sizeof(int) * 3 * (size_t)(x1 - x0));
    if (!row || !colx) { gy__c.frame.used = mark; return; }
    /* the source columns and weights are the same on every row: work them out once (the same
       integer maths as before, so the same bytes, without a 64-bit divide per pixel) */
    for (x = x0; x < x1; x++) {
        int *c3 = colx + 3 * (x - x0);
        if (!smooth) c3[0] = (int)(((int64_t)(x - in->x0) * 2 + 1) * im->w / (2 * dw));
        else {
            int64_t fx = (((int64_t)(x - in->x0) * 2 + 1) * im->w * 32768) / dw - 32768;
            if (fx < 0) fx = 0;
            c3[0] = (int)(fx >> 16); c3[2] = (int)((fx >> 8) & 255); c3[1] = gy__mini(c3[0] + 1, im->w - 1);
            c3[0] = gy__mini(c3[0], im->w - 1);
        }
    }
    for (y = y0; y < y1; y++) {
        if (!smooth) {                         /* nearest: the pixel whose area holds this pixel's centre */
            int sy = (int)(((int64_t)(y - in->y0) * 2 + 1) * im->h / (2 * dh));
            const uint32_t *s = im->px + (size_t)sy * im->w;
            for (x = x0; x < x1; x++) row[x - x0] = s[colx[3 * (x - x0)]];
        } else {                               /* bilinear in 16.16 fixed point, integer only */
            int64_t fy = (((int64_t)(y - in->y0) * 2 + 1) * im->h * 32768) / dh - 32768;
            int sy0, sy1, wy;
            const uint32_t *r0, *r1;
            if (fy < 0) fy = 0;
            sy0 = (int)(fy >> 16); wy = (int)((fy >> 8) & 255); sy1 = gy__mini(sy0 + 1, im->h - 1);
            sy0 = gy__mini(sy0, im->h - 1);
            r0 = im->px + (size_t)sy0 * im->w; r1 = im->px + (size_t)sy1 * im->w;
            for (x = x0; x < x1; x++) {
                const int *c3 = colx + 3 * (x - x0);
                uint32_t wxu = (uint32_t)c3[2], ix = 256 - wxu, iy = 256 - (uint32_t)wy;
                uint32_t a = r0[c3[0]], b = r0[c3[1]], c = r1[c3[0]], d = r1[c3[1]];
                /* two channels at a time: R and B (then A and G) in 16-bit lanes for the horizontal
                   step (at most 255 x 256 = 65280 each), widened to 32-bit lanes for the vertical one;
                   the same sums as one channel at a time, so the same bytes */
                uint32_t trb = (a & 0xFF00FFu) * ix + (b & 0xFF00FFu) * wxu, brb = (c & 0xFF00FFu) * ix + (d & 0xFF00FFu) * wxu;
                uint32_t tag = ((a >> 8) & 0xFF00FFu) * ix + ((b >> 8) & 0xFF00FFu) * wxu, bag = ((c >> 8) & 0xFF00FFu) * ix + ((d >> 8) & 0xFF00FFu) * wxu;
                uint64_t vrb = ((uint64_t)(trb & 0xFFFF) | (uint64_t)(trb >> 16) << 32) * iy
                             + ((uint64_t)(brb & 0xFFFF) | (uint64_t)(brb >> 16) << 32) * (uint64_t)wy + 0x0000800000008000ull;
                uint64_t vag = ((uint64_t)(tag & 0xFFFF) | (uint64_t)(tag >> 16) << 32) * iy
                             + ((uint64_t)(bag & 0xFFFF) | (uint64_t)(bag >> 16) << 32) * (uint64_t)wy + 0x0000800000008000ull;
                row[x - x0] = (uint32_t)((vrb >> 16) & 0xFF) | (uint32_t)((vrb >> 48) & 0xFF) << 16
                            | (uint32_t)((vag >> 16) & 0xFF) << 8 | (uint32_t)((vag >> 48) & 0xFF) << 24;
            }
        }
        gy__c.k_row(gy__c.px + (size_t)y * gy__c.w + x0, row, x1 - x0);
    }
    gy__c.frame.used = mark;
}

static void gy__draw_inst(const gy__inst *in, gy_rect clip) {
    if (in->kind == GY__K_BOX) gy__draw_box_cpu(in, clip);
    else if (in->kind == GY__K_GLYPH) gy__draw_glyph_cpu(in, clip);
    else if (in->kind == GY__K_LINE) gy__draw_line_cpu(in, clip);
    else if (in->kind == GY__K_IMAGE) gy__draw_image_cpu(in, clip);
}

static int gy__inst_layer(const gy__inst *in) { return (int)(in->c >> 24); }

static void gy__render(void) {
    int i, k, t, tx, ty, nt, x0, y0, x1, y1, full, L, cnt[GY__LAYERS + 1];
    int *ord;                                  /* the instances in drawing order: layer 0, then 1, then 2 */
    uint64_t seed;
    gy__c.tiles_x = (gy__c.w + GY__TILE - 1) / GY__TILE;
    gy__c.tiles_y = (gy__c.h + GY__TILE - 1) / GY__TILE;
    nt = gy__c.tiles_x * gy__c.tiles_y;
    /* one counting sort by layer, so every pass below walks the list once (not once per layer) */
    ord = (int *)gy__arena_push(&gy__c.frame, sizeof(int) * (size_t)(gy__c.ninst + 1));
    if (!ord) { gy__set_error("GUICy ran out of memory for the draw order", NULL); return; }
    for (L = 0; L <= GY__LAYERS; L++) cnt[L] = 0;
    for (i = 0; i < gy__c.ninst; i++) cnt[gy__mini(gy__inst_layer(&gy__c.inst[i]), GY__LAYERS - 1) + 1]++;
    for (L = 1; L <= GY__LAYERS; L++) cnt[L] += cnt[L - 1];
    for (i = 0; i < gy__c.ninst; i++) ord[cnt[gy__mini(gy__inst_layer(&gy__c.inst[i]), GY__LAYERS - 1)]++] = i;
    full = !gy__tiles_ready() || gy__c.tiles_off || gy__c.want_full;   /* (tiles_ready first: it sets up the tables) */
    if (full) {                                /* everything, the plain way */
        gy_clear();
        for (k = 0; k < gy__c.ninst; k++) gy__draw_inst(&gy__c.inst[ord[k]], gy__c.clips[gy__c.inst[ord[k]].clip]);
        gy__c.dirty_tiles = nt;
        if (gy__c.tile_prev) gy__c.want_full = 0;   /* the next frame can compare */
    }
    if (!gy__c.tile_prev) return;
    seed = gy__mix(0xC0FFEEull, gy__c.theme.bg);
    for (t = 0; t < nt; t++) gy__c.tile_cur[t] = seed;
    for (k = 0; k < gy__c.ninst; k++) {
            const gy__inst *in = &gy__c.inst[ord[k]];
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
        if (gy__c.dirty_tiles * 2 > nt) {      /* most tiles changed: one plain full redraw is cheaper (same pixels) */
            gy_clear();
            for (k = 0; k < gy__c.ninst; k++) gy__draw_inst(&gy__c.inst[ord[k]], gy__c.clips[gy__c.inst[ord[k]].clip]);
        } else if (gy__c.dirty_tiles)
            for (k = 0; k < gy__c.ninst; k++) {
                    const gy__inst *in = &gy__c.inst[ord[k]];
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
    int i;
    if (!gy__c.ready || !gy__c.px) return;
    while (gy__c.nlay > 1) gy__c.nlay--;       /* containers the app left open */
    if (gy__c.banner[0]) {                     /* an error banner, on the top layer */
        int h = gy_line_height() + 12;
        gy__c.layer = 2;
        gy_canvas_box((gy_rect){ 0, gy__c.h - h, gy__c.w, h }, GY__T.card, GY__T.warn, 0, 1);
        gy_canvas_text(GY__T.pad, gy__c.h - h + 6, gy__c.banner, GY__T.warn);
        gy__c.layer = 0;
    }
    /* a click outside an open popup closes it; a click that no edit box took takes the keys away */
    if (gy__c.popup_id && gy__c.pressed[0] | gy__c.pressed[1]) {
        int inside = 0;
        for (i = 0; i < gy__c.nblock_cur; i++) if (gy__inside(gy__c.block_cur[i], gy__c.mx, gy__c.my)) inside = 1;
        if (!inside && gy__c.active != gy__c.popup_id) gy__c.popup_id = 0;
    }
    if (!gy__c.popup_used && gy__c.popup_id) gy__c.popup_id = 0;   /* nobody drew it this frame: it is gone */
    if (gy__c.pressed[0] && gy__c.focus && gy__c.focus_claimed != gy__c.focus) gy__c.focus = 0;
    if (!gy__c.tip_shown) gy__c.tip_id = 0;
    gy__memcpy(gy__c.block_prev, gy__c.block_cur, sizeof gy__c.block_cur);
    gy__c.nblock_prev = gy__c.nblock_cur;
    /* tiles loaded from the ini that the app never asked for again are dropped */
    if (gy__c.tloaded && gy__c.ntl) {
        for (i = 0; i < gy__c.ntl; i++) if (gy__c.tl[i].from_load && gy__c.tl[i].seen < 0 && gy__tile_leaf(i) >= 0) gy__tile_remove(i);
        gy__c.tloaded = 0;
    }
    gy__c.in_frame = 0;
    gy__render();
    gy__c.frames++;
    if (gy__c.dirty_tiles) { gy__c.frames_drawn++; if (gy__c.hwnd) gy__present(); }
    if (gy__c.released[0]) gy__c.active = 0;
    if (gy__c.leave_pending && !(gy__c.down[0] | gy__c.down[1] | gy__c.down[2])) { gy_input_mouse(-32768, -32768); gy__c.leave_pending = 0; gy__c.passes = gy__maxi(gy__c.passes, 1); }
    gy__memset(gy__c.pressed, 0, sizeof gy__c.pressed);
    gy__memset(gy__c.released, 0, sizeof gy__c.released);
    gy__memset(gy__c.dbl, 0, sizeof gy__c.dbl);
    gy__c.nkev = 0; gy__c.nchars = 0; gy__c.wheel = 0;
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

/* every message GUICy cares about, for its own window and for an app's (gy_event); 1 = taken */
static int gy__handle(HWND hw, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res) {
    *res = 0;
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!gy__c.tracking) {
            TRACKMOUSEEVENT te;
            te.cbSize = sizeof te; te.dwFlags = TME_LEAVE; te.hwndTrack = hw; te.dwHoverTime = 0;
            gy__c.tracking = TrackMouseEvent(&te);
        }
        gy_input_mouse((short)LOWORD(lp), (short)HIWORD(lp)); gy__c.passes = 2; return 1;
    case WM_MOUSELEAVE:                        /* the mouse left the window; while a click is being handled it waits for the frame's end */
        gy__c.tracking = 0;
        if (gy__c.down[0] | gy__c.down[1] | gy__c.down[2] | gy__c.pressed[0] | gy__c.pressed[1] | gy__c.pressed[2]
            | gy__c.released[0] | gy__c.released[1] | gy__c.released[2]) gy__c.leave_pending = 1;
        else gy_input_mouse(-32768, -32768);
        gy__c.passes = 2; return 1;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK:
        SetCapture(hw);
        if (!gy__c.test_clock) gy__c.now = gy__os.ticks();
        gy_input_mouse((short)LOWORD(lp), (short)HIWORD(lp));   /* a click carries its own place: never one left over from before a leave */
        gy_input_button(msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK ? 0 : msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK ? 1 : 2, 1);
        gy__c.passes = 2; return 1;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        ReleaseCapture();
        gy_input_mouse((short)LOWORD(lp), (short)HIWORD(lp));
        gy_input_button(msg == WM_LBUTTONUP ? 0 : msg == WM_RBUTTONUP ? 1 : 2, 0); gy__c.passes = 2; return 1;
    case WM_MOUSEWHEEL: gy_input_wheel((float)(short)HIWORD(wp) / 120.0f); gy__c.passes = 2; return 1;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (msg == WM_SYSKEYDOWN && wp == VK_F4) return 0;   /* Alt+F4 still closes */
        gy_input_key((int)wp, 1); gy__c.passes = 2; return 1;
    case WM_KEYUP: case WM_SYSKEYUP: gy_input_key((int)wp, 0); gy__c.passes = gy__maxi(gy__c.passes, 1); return 1;
    case WM_SYSCHAR: return 1;                 /* no beep for Alt+key */
    case WM_CHAR: {
        WCHAR c = (WCHAR)wp;
        if (c >= 0xD800 && c < 0xDC00) { gy__c.surrogate = c; return 1; }
        if (c >= 0xDC00 && c < 0xE000 && gy__c.surrogate) { gy_input_char(0x10000u + (((uint32_t)gy__c.surrogate - 0xD800u) << 10) + (c - 0xDC00u)); gy__c.surrogate = 0; }
        else gy_input_char(c);
        gy__c.passes = 2; return 1;
    }
    case WM_SETFOCUS: case WM_KILLFOCUS: gy__memset(gy__c.keys_down, 0, sizeof gy__c.keys_down); gy__mods_update(); return 0;
    case WM_SIZE:                              /* noted; the frame is resized at the next gy_begin (not taken: the app may want it too) */
        if (wp != SIZE_MINIMIZED) { gy__c.resize_w = LOWORD(lp); gy__c.resize_h = HIWORD(lp); gy__c.passes = 2; }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        RECT r = { 0, 0, gy__c.min_w, gy__c.min_h };
        if (!gy__c.min_w) return 0;
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        mm->ptMinTrackSize.x = r.right - r.left; mm->ptMinTrackSize.y = r.bottom - r.top;
        return 1;
    }
    case WM_ERASEBKGND: *res = 1; return 1;    /* the frame covers everything */
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        if (gy__c.memdc) BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
                                ps.rcPaint.bottom - ps.rcPaint.top, gy__c.memdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(hw, &ps);
        return 1;
    }
    case WM_APP + 1: gy__c.passes = gy__maxi(gy__c.passes, 1); return 1;   /* gy_wake */
    }
    return 0;
}

static LRESULT CALLBACK gy__wndproc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT res;
    if (msg == WM_CLOSE) { gy__c.closed = 1; return 0; }
    if (gy__handle(hw, msg, wp, lp, &res)) return res;
    return DefWindowProcW(hw, msg, wp, lp);
}

/* windowed tests must never put a window on the user's screen: in a test build (GUICY_TESTS),
   gy_open and gy_attach refuse on the Default desktop (gytest --windowed runs on its own) */
static int gy__desktop_ok(void) {
#ifdef GUICY_TESTS
    WCHAR name[64];
    DWORD need = 0;
    name[0] = 0;
    GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, name, sizeof name, &need);
    if (!lstrcmpiW(name, L"Default")) { gy__set_error("GUICy test build: windows only on a test desktop", "run gytest --windowed"); return 0; }
#endif
    return 1;
}

int gy_attach(void *hwnd) {
    RECT r;
    if (!gy__init() || !hwnd || !gy__desktop_ok()) return 0;
    if (!GetClientRect((HWND)hwnd, &r)) { gy__set_error("gy_attach", "not a window"); return 0; }
    gy__c.memdc = CreateCompatibleDC(NULL);
    if (!gy__c.memdc || !gy__dib(gy__maxi(r.right, 1), gy__maxi(r.bottom, 1))) return 0;
    gy__c.hwnd = (HWND)hwnd; gy__c.attached = 1;
    gy__c.passes = 2;
    return 1;
}

int gy_event(void *hwnd, unsigned msg, uintptr_t wp, intptr_t lp) {
    LRESULT res;
    if (!gy__c.ready || (HWND)hwnd != gy__c.hwnd) return 0;
    return gy__handle((HWND)hwnd, msg, (WPARAM)wp, (LPARAM)lp, &res);
}

int gy_wants_frame(void) { return gy__c.passes > 0 || (gy__c.anim_on && (int)(gy__os.ticks() - gy__c.anim_at) >= 0); }

int gy_open(const gy_desc *d) {
    WNDCLASSEXW wc;
    WCHAR title[256];
    RECT r;
    if (!gy__init() || !gy__desktop_ok()) return 0;
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
    unsigned at = gy__c.now + (unsigned)(ms < 0 ? 0 : ms);
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
        if (gy__c.anim_on && (int)(gy__os.ticks() - gy__c.anim_at) >= 0) { gy__c.anim_on = 0; gy__c.passes = gy__maxi(gy__c.passes, 1); }
        if (gy__c.passes > 0) break;
        MsgWaitForMultipleObjectsEx(0, NULL, gy__c.anim_on ? (DWORD)gy__maxi(0, (int)(gy__c.anim_at - gy__os.ticks())) : INFINITE,
                                    QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    gy__c.passes--;                            /* an input event gets two passes: the second settles */
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
    for (i = 1; i < GY__IMAGES; i++) if (gy__c.img[i].px) HeapFree(GetProcessHeap(), 0, gy__c.img[i].px);
    if (gy__c.off_px) HeapFree(GetProcessHeap(), 0, gy__c.off_px);
    if (gy__c.dc) DeleteDC(gy__c.dc);
    if (gy__c.hwnd && !gy__c.attached) DestroyWindow(gy__c.hwnd);   /* an attached window is the app's */
    if (gy__c.dib) DeleteObject(gy__c.dib);
    if (gy__c.memdc) DeleteDC(gy__c.memdc);
    if (gy__c.perm.base) VirtualFree(gy__c.perm.base, 0, MEM_RELEASE);
    if (gy__c.frame.base) VirtualFree(gy__c.frame.base, 0, MEM_RELEASE);
    gy__memset(&gy__c, 0, sizeof gy__c);
}

#endif /* GUICY_IMPLEMENTED */
#endif /* GUICY_IMPLEMENTATION */

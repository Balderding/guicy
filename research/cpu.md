# GUICy research: CPU rendering and the GUI core

Scope: software (CPU) rendering, layout, input, text, widgets and API design for GUICy,
a plain-C immediate-mode GUI meant to replace Nuklear (GDI/GDI+ backends) in voxup,
pixelhunter and bbb_kit. GPU rendering is covered in a separate report.

Ground rules this report assumes: one static exe, only OS DLLs (user32, gdi32, dwrite,
usp10, kernel32...), clang with the MinGW target, x86-64 baseline (SSE2 always there),
ARM64 only noted.

Numbers marked "est." are back-of-envelope estimates, not measurements. Everything else
is from the cited sources.

---

## 0. Top 10 ideas, ranked by gain versus effort

| # | Idea | Gain | Effort |
|---|------|------|--------|
| 1 | **Only run a frame when something happened.** Block in `GetMessage`/`MsgWaitForMultipleObjects`, run 1-2 frames per input event, let widgets ask for a frame while they animate. | Idle CPU goes to ~0. This is the biggest win, and it is the Nuklear apps' main waste today. | Tiny (~30 lines) |
| 2 | **Hash the command stream per tile; rasterize and present only the tiles that changed.** | A typical frame (hover change, a caret blink, a progress bar) touches 1-5% of the window, so you get 20-100x less pixel work. | Small (~200 lines) |
| 3 | **A UI-specialised rasterizer instead of a general path renderer.** Solid spans for rect interiors, SDF coverage only in rounded corners and on AA edges, and blitted glyphs from an atlas. A general path fill only for icons. | Most UI pixels become memset-speed stores. | Medium |
| 4 | **Premultiplied BGRA8 everywhere, fixed 8-bit blend with the exact div-255 trick, SSE2 baseline plus AVX2 kernels picked once at startup by cpuid.** | 2-4x on blend kernels over scalar code. No JIT needed. | Small |
| 5 | **Present with a DIB section plus `BitBlt` of the dirty rects only** (not UpdateLayeredWindow, not GDI+). | Present cost scales with the change, not the window. | Tiny |
| 6 | **An implicit global context, IDs hashed from label plus an ID stack, and one-line widgets that return bool.** `if (gc_button("Save")) save();` | API simplicity: no ctx argument, no begin/end per widget. | Small |
| 7 | **Let the OS make the glyphs** (DirectWrite, or GDI/Uniscribe from plain C), cached in a CPU glyph atlas with 4 subpixel x-phases. Keep a small stb_truetype-style rasterizer only as a fallback. | Shaping, kerning, font fallback and ClearType come for free, with close to zero code for full Unicode. | Medium |
| 8 | **A two-pass layout (measure, then place) with one frame of lag for input rects** (Fleury), Clay-style grow/shrink/fit sizing, plus a trivial row-cursor layout for the 80% case. | Removes Nuklear's "declare the row count and height first" pain. Splitters and docking become natural. | Medium |
| 9 | **Bin draw commands into tiles and run the tiles on N worker threads** (persistent pool, no locks per command). | Near-linear scaling for full redraws (resize, scroll, first frame). | Small, once #2 exists |
| 10 | **No CRT, stb-style single header, a freestanding build** (`-nostdlib`, your own `memset`/`memcpy`, `-mno-stack-arg-probe`). | The exe overhead from the library is about 30-80 KB est. | Small |

Not recommended: a JIT pipeline compiler (Blend2D style), AVX-512 kernels, hand-written
assembly for blend loops, or a full sparse-strip path renderer. Each one is either too
much code for the gain or bandwidth-bound anyway (details in sections 1-2).

---

## 1. Software 2D rasterizers: how the fast ones work

### 1.1 The two families

**Scanline (sorted-edge) rasterizers.** Build an edge list, sort it by y, walk scanlines
keeping an active edge table, and emit spans with coverage. Anti-aliasing is done either
by supersampling (several sub-scanlines per pixel) or by computing analytic area per
cell. Examples: libart (Raph Levien's "sorted vector path", SVP), AGG, FreeType's
`ftgrays.c`, Skia's `SkScan` and its later "Analytic AA" (AAA), and Cairo's span
converters.

**Coverage accumulation (signed-area) rasterizers.** You do not sort anything. Each line
segment writes its signed area and cover deltas straight into a dense buffer. A prefix
sum along each row then gives the winding coverage, and `|acc|` clamped to 1 is the
alpha. Examples: font-rs, stb_truetype "v2", and Blend2D (banded and dense, with a
shadow bit buffer). Vello CPU's sparse strips are a sparse, tiled form of the same idea.

```
for each line (x0,y0)->(x1,y1) after flattening curves:
    for each pixel row the line crosses:
        for each pixel cell the line crosses in that row:
            area  = signed area of the part of the cell to the right of the line
            acc[y][x]   += area             // partial coverage of the touched cell
            acc[y][x+1] += dy_in_row - area // the rest carries to every pixel on the right
integrate:  for each row: s = 0; for x: s += acc[x]; alpha[x] = min(1, |s|)  (nonzero)
```

Why accumulation wins on modern CPUs (Levien, "Inside the fastest font renderer in the
world"):
- Buffer addressing is a single instruction with no branches. There is no sort and no
  span list.
- The drawing is fed straight from the parser, so no intermediate geometry is allocated.
- The integrate step is a prefix sum, which vectorises (font-rs ships an SSE prefix sum).
- The cost is O(w*h) clearing plus integration, which is fine up to roughly 60 px/em
  glyphs or band-sized buffers.

### 1.2 The individual libraries

- **font-rs** (Levien, 2016). A dense `Vec<f32>` accumulation buffer with one float per
  pixel, a pull-parser over glyph outlines (no point arrays), and quadratic Béziers
  flattened on the fly. Its README notes the same exact-area maths as AGG and FreeType,
  and that it is "very similar to libart" except that it draws immediately into a dense
  buffer instead of a sorted, sparse form.
- **stb_truetype** (Sean Barrett). Since v1.x it has a "v2" rasterizer that does
  signed-area accumulation per scanline using an active edge list. That keeps memory
  per scanline instead of per glyph. Sean's write-up "How the stb_truetype Anti-Aliased
  Software Rasterizer v2 Works" explains it. It is about 5k lines in total and includes
  the font parser. Its weaknesses: no hinting, a slow `stbtt_FindGlyphIndex` on big
  fonts if you do not cache it, and only the `kern` table, with no GPOS kerning in the
  old versions (newer versions read GPOS pair kerning).
- **Blend2D** (Petr Kobalíček). An analytic cell rasterizer (FreeType/AGG maths) with a
  **dense** cell buffer: one 32-bit integer per cell instead of two (cover plus area),
  so half the memory. It works in **bands** of 16 or 32 scanlines, so the buffers are a
  fixed size. A **shadow bit buffer** marks touched cells, so the compositor skips empty
  runs and clears as it goes. The compositor (coverage, fetch and composite fused, with
  data kept in registers) is **JIT-compiled** with AsmJit for SSE2 up to AVX-512, or
  ASIMD on ARM. There is no AGG-style scanline container: masks come straight from the
  cells. The author reports writing about 20 rasterizers before settling on this one.
  It is the fastest CPU vector renderer in most public benchmarks.
- **tiny-skia** (Rust port of a Skia subset, now under linebender). About 14 KLOC and
  roughly 200 KiB in a binary (versus 3-8 MiB for Skia). It copies **SkRasterPipeline**:
  a draw is a list of stage function pointers (load_dst, seed shader, blend, store...).
  A **lowp** (u16) pipeline is used when every stage has a lowp version, otherwise
  **highp** (f32). It is 20-100% slower than Skia on x86-64. The lesson for us: a u16
  fixed-point path covers everything a UI needs (srcover, solid, image, mask), and you
  never need the f32 path.
- **Skia raster backend.** SkScan (supersampled, later "analytic AA") plus
  SkRasterPipeline stages, built with clang vector extensions. Without clang it runs
  15-30x slower (per the tiny-skia README). Too big to copy, but the
  stage-pipeline idea is worth knowing.
- **AGG** (Maxim Shemanarev). Cells (cover, area) sorted into scanline containers, then
  span generators and pixel-format blenders built from C++ templates. High quality and
  very flexible, but it is template-heavy and its sorting step is the thing Blend2D
  removed.
- **libart** (Levien, around 1998). Sorted vector paths (SVP) for exact,
  overlap-resolved polygons. Historically important (Gnome canvas), slow by today's
  standards because of the sorting and sparse structures.
- **Pixman**. The compositing core of Cairo and the X server. Its rasterizer works on
  trapezoids, and it has a big table of **fast paths**
  (op × src format × mask format × dst format) with SSE2, SSSE3 and NEON versions, plus
  generic combiners as the fallback. Lesson: a small set of specialised fast paths
  covers almost every real call. GUICy needs about 6 of them.
- **Cairo**. On top of pixman, the image backend converts paths to spans (the newer
  "spans compositor", using a tor-style scan converter) instead of trapezoids. It is
  slow per call for tiny UI shapes because of the state and setup cost, which is a
  general warning against heavyweight per-draw setup.
- **refterm** (Casey Muratori, 2021). Not a vector rasterizer. It is the reference
  argument that a terminal or a text grid should be **cell tiles plus a glyph cache**:
  glyphs are made only on a cache miss (a two-level LRU hash keyed by a hash of the
  glyph's UTF-8 run), and each frame is just a copy per cell. The lesson for GUI text
  is the same: rasterize each glyph once, then every frame is blits.
- **Vello CPU / sparse strips** (Laurenz Stampfl's ETH master's thesis, Oct 2025). A
  path is flattened, cut into **4×4 tiles**, and only tiles that an edge touches get
  alpha values. Those tiles merge into **strips** (a run-length, sparse alpha form).
  The gaps between strips on a row are interior **sparse fills** with no alpha storage.
  Commands are then binned into **wide tiles** (256×4 px), each holding its own command
  list, and composited per wide tile. There are SIMD versions (SSE/AVX2/NEON/WASM via
  "fearless SIMD") and multithreading. The thesis benchmarks it against Blend2D, and it
  is close to Blend2D on many tests. Its key property is memory proportional to the
  edges, not the area, and an ordered command list per wide tile, which is exactly what
  our tile hashing (section 3) wants.

### 1.3 What GUICy should do

A GUI is not an SVG viewer. From the Nuklear command types the apps use: about 90% of
pixels are axis-aligned rects (filled or outlined), rounded rects, glyph blits and image
blits. Only icons, checkmarks, arrows, the colour-wheel hue ring and the occasional
diagonal line need real path coverage.

The plan:
1. **Primitive-specific kernels.** No general path for rects, rounded rects, circles,
   glyphs or images (section 2.5).
2. **One small coverage-accumulation path filler**, font-rs/stb v2 style, for
   everything else. It works in one tile- or band-sized scratch buffer
   (for example 64 rows × tile width × i32), with nonzero and even-odd fills, about
   300-400 lines. It also rasterizes the fallback font, so the code is shared.
3. **No JIT.** The pipeline set is tiny (below), so ahead-of-time per-ISA kernels give
   the same speed with no AsmJit. That keeps a static exe, and it does not need writable
   executable memory, which Windows ASR/CFG and this machine's GPO policy may dislike.

```c
/* The complete pipeline set: dst is always premultiplied BGRA8. */
enum gc_kernel {
    K_FILL,          /* solid colour, no coverage (rect interiors)   */
    K_FILL_COV,      /* solid colour * u8 coverage (AA edges, paths) */
    K_GLYPH,         /* colour * A8 glyph mask (grayscale text)      */
    K_GLYPH_LCD,     /* colour * 3x8 mask, per-channel (ClearType)   */
    K_IMAGE,         /* premultiplied image, srcover, 1:1             */
    K_IMAGE_SCALED,  /* nearest or bilinear sample, srcover          */
};
/* x {SSE2, AVX2} and an optional NEON set on ARM64 = 12-18 functions. */
```

---

## 2. SIMD for UI pixels

### 2.1 Pixel format

Use **premultiplied BGRA8**, top-down. It is exactly what a 32-bpp `BI_RGB` DIB section
wants (`biHeight = -h` for top-down), so presenting needs no conversion.

Premultiplied srcover is `d = s + d*(255 - sa)/255`. That is one multiply per channel,
it is associative (so layers and caches compose), and bilinear filtering is correct
without fringes. Convert images to premultiplied once, at load.

### 2.2 Exact 8-bit fixed-point blending

Divide by 255 without dividing (each of these is exact over the whole 0..255*255 range,
checked over that range in OpenSmalltalk VM issue #798):

```c
/* rounded (x / 255.0 + 0.5) for x in [0, 65025] */
static inline uint32_t div255r(uint32_t x) { x += 128; return (x + (x >> 8)) >> 8; }

/* SIMD, 8 lanes of u16, rounded: same trick */
static inline __m128i div255_epu16(__m128i x) {
    x = _mm_add_epi16(x, _mm_set1_epi16(128));
    return _mm_srli_epi16(_mm_add_epi16(x, _mm_srli_epi16(x, 8)), 8);
}
/* alternative, truncating: mulhi by 0x8081 then >> 7  ==  floor(x / 255) for 16-bit x */
```

Cheap approximations like `(a+1)*c >> 8` (the stereopsis "double blend trick") are off
by one in places. For a UI that blends many layers in the same spot (shadows, hover
overlays), use the exact rounded form. It costs the same in SIMD.

The **"two channels per multiply"** trick for scalar code and tails: mask `0x00FF00FF`
to get R and B into 16-bit slots, multiply both by alpha in one 32-bit multiply, then do
A and G after `>> 8` (stereopsis). The scalar tail loop runs at about half the speed of
SSE2 with this trick.

### 2.3 An AVX2 srcover kernel (8 pixels per iteration)

```c
__attribute__((target("avx2")))
static void blend_srcover_avx2(uint32_t *d, const uint32_t *s, int n) {
    const __m256i amask = _mm256_setr_epi8(   /* broadcast alpha into all 4 u16 lanes */
        6,7,6,7,6,7,6,7, 14,15,14,15,14,15,14,15,
        6,7,6,7,6,7,6,7, 14,15,14,15,14,15,14,15);
    const __m256i k255 = _mm256_set1_epi16(255), k128 = _mm256_set1_epi16(128);
    const __m256i z = _mm256_setzero_si256();
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256i sp = _mm256_loadu_si256((const __m256i *)(s + i));
        __m256i dp = _mm256_loadu_si256((const __m256i *)(d + i));
        __m256i slo = _mm256_unpacklo_epi8(sp, z), shi = _mm256_unpackhi_epi8(sp, z);
        __m256i dlo = _mm256_unpacklo_epi8(dp, z), dhi = _mm256_unpackhi_epi8(dp, z);
        __m256i ialo = _mm256_sub_epi16(k255, _mm256_shuffle_epi8(slo, amask));
        __m256i iahi = _mm256_sub_epi16(k255, _mm256_shuffle_epi8(shi, amask));
        __m256i tlo = _mm256_add_epi16(_mm256_mullo_epi16(dlo, ialo), k128);
        __m256i thi = _mm256_add_epi16(_mm256_mullo_epi16(dhi, iahi), k128);
        tlo = _mm256_srli_epi16(_mm256_add_epi16(tlo, _mm256_srli_epi16(tlo, 8)), 8);
        thi = _mm256_srli_epi16(_mm256_add_epi16(thi, _mm256_srli_epi16(thi, 8)), 8);
        __m256i r = _mm256_packus_epi16(_mm256_add_epi16(tlo, slo),
                                        _mm256_add_epi16(thi, shi));
        _mm256_storeu_si256((__m256i *)(d + i), r);
    }
    /* + an SSE2 4-pixel step and a scalar tail */
}
```

Note: unpack and pack work inside each 128-bit lane, so the pixel order is preserved.
That makes this lane-local code correct with no permutes.

The fast paths inside the kernels pay off more than wider SIMD does:
- Check each 8-pixel source alpha: `_mm256_movemask_epi8` on `sa == 255` stores the
  source as-is, and `sa == 0` skips. For text and icons most pixels are 0 or 255.
- `K_FILL` with an opaque colour is just `_mm256_storeu_si256` (or `rep stosd` for
  long spans: on CPUs with ERMSB/FSRM, `rep stosd` is as fast as SIMD stores for spans
  of more than ~512 bytes).
- Coverage masks: multiply the colour by coverage (`c*cov/255`), then use srcover. Do
  this for 8 or 16 pixels per iteration with `_mm256_cvtepu8_epi16` on 16 coverage
  bytes.

### 2.4 Runtime dispatch

```c
#include <cpuid.h>
static int has_avx2(void) {
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return 0;
    if (!(c & (1u << 27))) return 0;              /* OSXSAVE */
    unsigned lo, hi;                              /* XGETBV: OS saves YMM state? */
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    if ((lo & 6) != 6) return 0;                  /* XMM and YMM enabled */
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d)) return 0;
    return (b >> 5) & 1;                          /* AVX2 */
}
/* AVX-512: also check (lo & 0xE6) == 0xE6 and leaf 7 EBX bit 16 (F) and bit 30 (BW). */

typedef struct { void (*fill)(...); void (*fill_cov)(...); void (*glyph)(...); /*...*/ } gc_kern;
static gc_kern K;     /* filled once in gc_init(): K = has_avx2() ? kern_avx2 : kern_sse2; */
```

Compile each ISA's kernels with `__attribute__((target("avx2")))` in the same file. The
baseline build stays SSE2 (`-march=x86-64`), so the exe runs on any x86-64 machine.
Avoid `__builtin_cpu_supports` if you drop compiler-rt or libgcc: it needs
`__cpu_model`/`__cpu_indicator_init` from the runtime. The 15-line `cpuid` version above
has no dependencies.

**AVX-512.** Most Intel client CPUs since Alder Lake do not have it, and AMD Zen 4/5
does. UI blending is bound by memory and L2 bandwidth at 8 px per AVX2 iteration, so
512-bit buys an estimated 0-20% and doubles the kernel count. Skip it until you have a
profile that says otherwise.

**Assembly, honestly.** clang already generates near-optimal code for these loops
(unpack, mullo, shuffle, pack), and it inserts `vzeroupper` itself. Hand-written assembly
wins nowhere in this design that I can find support for. The only places assembly is
useful are tiny bits of glue: `xgetbv` (inline, above) and possibly a `___chkstk_ms`
stub in a CRT-free build (section 6). Blend2D's JIT advantage comes from fusing
arbitrary operator, style and format combinations at runtime, and GUICy only has 6
combinations.

**ARM64 (note only).** NEON is baseline on AArch64. `vmull_u8`/`vrshrn_n_u16` (rounding
narrowing shift) give a very cheap div255 (`vraddhn_u16` style tricks). There is no
dispatch needed for NEON itself. The kernels are a separate `#ifdef __aarch64__` file
of the same shape.

### 2.5 Rounded rects and anti-aliased circles

Use a signed-distance function, evaluated **only where coverage is fractional**:

```c
/* p relative to the rect centre, b = half size, r = corner radius */
float sd_rrect(float px, float py, float bx, float by, float r) {
    float qx = fabsf(px) - bx + r, qy = fabsf(py) - by + r;
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = (qx > qy ? qx : qy); in = in < 0 ? in : 0;
    return sqrtf(ox*ox + oy*oy) + in - r;
}
/* coverage = clamp(0.5 - d, 0, 1); a border of width w: cov(d) - cov(d + w) */
```

Split the rect into regions:
- The interior rows between the corners are plain `K_FILL` spans, plus one AA pixel
  column on each side if x is fractional.
- The 4 corner squares (r × r) run the SDF per pixel. With AVX2, do 8 pixels at a time
  with `_mm256_sqrt_ps` (or `rsqrt` plus one Newton step), then convert to u8 coverage
  and run `K_FILL_COV`.
- Rows above and below the corners are a span plus an SDF tail.

A circle is a rounded rect with `r = b`. A drop shadow is a rounded-rect SDF with a soft
falloff (`coverage = smoothstep(-blur, blur, -d)`), or better, the exact formula for a
Gaussian-blurred box, which is separable (Evan Wallace, "Fast Rounded Rectangle
Shadows"). Cache shadow nine-slices per (radius, blur), because shadows are large and
change rarely.

Cost: a 200×40 button with r=6 does SDF work on 4×36 = 144 pixels and stores on the
other 7,856. That is effectively memset speed.

**Lines.** Thin axis-aligned lines are rects. Diagonal lines (graphs, the tree view's
dotted lines are axis-aligned anyway) go through the path filler as a quad, or use a
distance-to-segment SDF in its bounding box for short lines.

### 2.6 Fast image scaling

The apps show sprites (pixelhunter, voxup), so scaling quality matters.

- **Nearest** for pixel art at integer zoom: precompute an x-offset table once per row
  width, and copy whole rows when the y index repeats. With integer zoom, use
  `_mm256_shuffle_epi8`/`permutevar8x32` to replicate pixels.
- **Bilinear** for smooth zoom: 16.16 fixed-point source coordinates, weights as u8,
  and `_mm_maddubs_epi16` / `_mm_madd_epi16` to combine 2 taps. Do it separably
  (horizontal into a 2-row ring buffer, then vertical) so each source row is filtered
  once.
- **Downscale** for thumbnails: box filter or mip pyramid. Bilinear below 0.5× aliases
  badly. Build the 2×2-average mip chain once at load (`_mm_avg_epu8` twice is close
  enough, or exact via u16), then bilinear from the nearest mip level.
- Cache the scaled result per (image id, generation, dst w, dst h). A scaled image is
  usually drawn many frames in a row, and with tile hashing (section 3) the scale cost
  is paid only when the image or zoom changes.

### 2.7 Throughput budget (est.)

At 1920×1080, the framebuffer is 8.3 MB. One core can store about 20-30 GB/s to L3 or
RAM (est.), so a full opaque clear is about 0.3-0.4 ms, and a full-window
read-modify-write blend is about 1-2 ms with AVX2 on one core. A whole-window redraw of a
typical tool UI (background, ~200 widgets, ~3,000 glyphs) should land at about
2-4 ms single-threaded and under 1 ms on 8 threads, est. With dirty tiles, a normal
interactive frame is tens of microseconds.

---

## 3. Doing as little work as possible

### 3.1 Only run a frame when needed

```c
for (;;) {
    if (!gc_wants_frame()) WaitMessage();          /* sleep: 0% CPU */
    while (PeekMessageW(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    gc_frame_begin(); app_ui(); gc_frame_end();    /* layout, cmds, tiles, present */
}
```

`gc_wants_frame()` is true when:
1. An input event arrived. Run **two** frames, because the layout's input rects lag one
   frame (section 4.3) and the second frame settles hover.
2. A widget called `gc_animate(until_ms)`: caret blink (a 530 ms timer, not a busy
   loop), animations, or a progress bar fed by a worker.
3. The app called `gc_wake()` from another thread. That posts `WM_NULL` or sets an
   event, so you use `MsgWaitForMultipleObjects` instead of `WaitMessage`.

Dear ImGui and Nuklear apps usually render continuously at vsync, which is a full core's
worth of waste when the window is idle. Pacing during animation: a `SetTimer`, or
`DwmFlush()` to sync to the compositor without a GPU.

### 3.2 Tile hashing ("immediate mode with a retained cache")

The UI is rebuilt every frame, but the **pixels** are retained. Each frame:

```c
typedef struct { int16_t x0, y0, x1, y1; } gc_rect;        /* clip-space bbox     */
typedef struct {
    uint8_t  kind;      /* RECT, RRECT, GLYPHRUN, IMAGE, PATH, CLIP...             */
    uint8_t  flags;
    uint16_t size;      /* bytes, so the stream is walkable and hashable           */
    gc_rect  bb;        /* after clipping                                          */
    /* payload follows: colour, radius, glyph ids + x positions, image id+gen ...   */
} gc_cmd;

#define TILE 64                                     /* 64x64 px = 16 KB of BGRA      */
typedef struct {
    uint64_t hash, prev_hash;
    uint32_t first, count;                          /* into a per-frame index array */
} gc_tile;
```

1. Widgets append `gc_cmd`s to a linear arena (a bump allocator, reset each frame).
2. **Binning.** For each command, loop over the tiles its bbox overlaps and append the
   command index. Mix the command's bytes into that tile's hash with a fast 64-bit hash
   (wyhash or a rapidhash-style mix; a hash of the payload, then
   `h = mix(h ^ cmd_hash)`). Hash once per command, combine per tile.
3. A tile whose `hash == prev_hash` is skipped: no rasterizing, no presenting.
4. Changed tiles are rasterized in clip order, from their own command lists, into the
   persistent backbuffer.
5. Changed tiles are merged into up to N rects (row runs of tiles), and only those are
   presented.

Rules that make this correct:
- Anything not in the payload must not affect pixels. Images go in by
  `(id, generation)`, never by pointer alone. Fonts by `(font id, size)`.
- Commands that span many tiles (a window background) are hashed once and combined
  cheaply per tile, so the cost is O(commands + tile overlaps).
- With a 64-bit hash, collision risk is negligible for UI (about 1 in 10^19 per tile).
- Invalidate everything on resize, DPI change or theme change.

This is the same shape as Vello CPU's wide tiles (256×4 with a command list per tile) and
refterm's cell grid, but with the hash check added so unchanged tiles cost nothing.
microui already hashes its command list per frame and only redraws when the whole hash
changes (the `mu_` demo renderer). Doing it **per tile** turns that whole-frame idea
into a partial one.

Tile size trade-off: 64×64 means 510 tiles at 1080p. It is cheap to bin and fits L1/L2
during rasterizing (16 KB). 32×32 gives tighter dirty regions but 4x the binning.
Start with 64×64.

### 3.3 Multithreading

- A persistent pool of `N = cores - 1` workers (`CreateThread`), each waiting on
  `WaitOnAddress` or a semaphore.
- Work item = one dirty tile. Use an atomic counter
  (`__atomic_fetch_add(&next, 1)`) over the dirty-tile list. Tiles do not overlap, so
  there are no locks and no false sharing (64×64×4 = 16 KB per tile, so rows are at
  least 256 B apart).
- The glyph atlas must be filled **before** rasterizing (on the main thread during
  command building), so workers only read it.
- Only go wide when more than ~16 dirty tiles exist. A one-tile update is faster on the
  main thread than waking the pool (a wake costs ~5-20 µs, est.).

### 3.4 Presenting on the CPU path

| Method | What it does | Verdict |
|---|---|---|
| `SetDIBitsToDevice` / `StretchDIBits` from plain memory | Copies through GDI each call, with format checks | OK, but it copies the whole rect you pass. Use per dirty rect. |
| **DIB section + memory DC + `BitBlt`** | `CreateDIBSection` gives a pointer we draw into. `BitBlt(wnd_dc, x, y, w, h, mem_dc, x, y, SRCCOPY)` per dirty rect | **Recommended.** No conversion (32-bpp BGRA), lowest overhead, and clipped to what changed. Do it in `WM_PAINT` (with `InvalidateRect` per dirty rect) or straight after the frame with `GetDC`. |
| `UpdateLayeredWindow` | Per-pixel alpha windows. Pushes the **whole** surface every call | Only for shaped or translucent popups (tooltips with shadows). Too slow for the main window. |
| DXGI flip-model swapchain, written by the CPU | Needs D3D11 (an OS DLL) and a map or upload per frame. Gives tear-free vsync, `Present1` with dirty rects and scroll rects, and a lower compositor latency | Optional later: it is what the GPU report owns. Keep GDI as the always-works path. |

Plus: handle `WM_ERASEBKGND` (return 1) to avoid flicker, set `CS_OWNDC` on the window
class, and declare per-monitor DPI awareness v2 in the manifest or with
`SetProcessDpiAwarenessContext`. That function lives in user32 on Windows 10 1703+, so
look it up with `GetProcAddress` to stay safe on older systems.

For live resize, render synchronously inside `WM_SIZE`/`WM_PAINT` during the modal
size loop (`WM_ENTERSIZEMOVE`), otherwise the window shows stretched garbage.

---

## 4. The immediate-mode GUI core

### 4.1 What the sources teach

- **Casey Muratori, "Immediate-Mode Graphical User Interfaces" (2005 video).** The
  original framing: the UI is a function of app state, called every frame. No widget
  objects, no callbacks, no syncing state. Interaction runs on **hot** (mouse over)
  and **active** (being pressed or dragged) IDs. A button is:
  `if (active == id) { if (mouse_up) { if (hot == id) clicked = 1; active = 0; } }
  else if (hot == id && mouse_down) active = id;  if (inside) hot = id;`
- **Ryan Fleury, "UI" series (10 parts, 2022-2023, Digital Grove).**
  - Part 2: build the widget tree every frame, but keep a **persistent cache keyed by
    hash** (with `last_frame_touched` for pruning). Layout is a separate pass with
    **one frame of lag for input rects but none for rendering**. "Semantic sizes":
    `Pixels`, `TextContent`, `PercentOfParent`, `ChildrenSum`, each with a
    `strictness` that controls how much a size may shrink when there is too little
    room.
  - Part 3: a widget-building language with push/pop style stacks (auto-pop
    "next" values).
  - Part 4 ("The widget is a lie"): one generic box with **feature flags**
    (`Clickable | DrawBackground | DrawText | DrawBorder | Clip | ScrollX | ...`).
    Buttons, checkboxes, list rows and so on are just different flag sets plus a few
    lines of code. This is the single best idea for keeping the widget count cheap.
  - Animation: per-box `hot_t`/`active_t` floats eased with the frame-rate
    independent exponential `t += (target - t) * (1 - pow(2, -rate*dt))`, which
    corrects itself when the target changes.
  - Parts 8-10: hotkeys and state mutation, keyboard and gamepad navigation, and a
    simple single-line text field.
- **microui (rxi).** About 1,100 lines of ANSI C. Its core design: a fixed-size
  **command list** (rect, text, icon, clip, jump), an **ID stack** hashing with FNV-1a,
  a fixed **pool** of retained containers and tree-node state (get-by-id, LRU by frame
  number), row layout with `mu_layout_row(ctx, n, widths, height)`, and no allocation
  at all. Its limits: the ctx argument everywhere, one font abstraction, no text
  shaping, and a simplistic layout. It shows what the minimum viable core is.
- **Dear ImGui (Omar Cornut).** The **ID stack** (`PushID`, `"label##id"`,
  `"###id"` to keep an ID across a label change), `ImGuiStorage` (a sorted id→int/ptr
  map per window for tree state), and window settings saved to an .ini file. **Tables**
  (columns with sizing policies, resizable, sortable, reorderable, clipping per column
  via draw-list channels), **docking** (a dock-node tree, windows docked by ID), and
  the **list clipper** (only submit visible rows of huge lists). Lesson: the clipper and
  the ID stack are essential, and the table and docking APIs are big but well tested.
  Worth copying the API shape, not the code (C++, about 60k+ lines).
- **Clay (Nic Barker).** A layout-only library in about 4-5k lines of C99, zero
  dependencies and no libc, a static arena (about 3.5 MB for 8,192 elements). Its
  output is a **sorted list of render commands** (rect, border, text, image, scissor,
  custom). The layout is flexbox-like: per axis it computes `FIT` sizes bottom-up, then
  distributes `GROW` space and shrinks overflowing children top-down. It wraps text
  between the X pass and the Y pass so heights know their final widths. It also has
  floating elements (tooltips, dropdowns) attached to a parent and z-sorted, scroll
  containers, and a pointer-over test by element ID. API:
  `CLAY({ .id = CLAY_ID("Side"), .layout = { .sizing = { CLAY_SIZING_FIXED(300),
  CLAY_SIZING_GROW(0) } }, .backgroundColor = c }) { ...children... }`. The `CLAY()`
  macro is a `for` loop that opens and closes the element. Clever, but it hides
  control flow (a `return` or `break` inside skips the close).
- **Nuklear's weaknesses** (what the apps suffer from today):
  1. `struct nk_context *ctx` is passed to all of the ~70 calls.
  2. Row layouts need the column count and row height **up front**
     (`nk_layout_row_dynamic(ctx, 30, 3)`), so content-sized rows and wrapping need
     manual measuring.
  3. Window-scoped state hashing by name and sequence. No general ID stack, so
     identically labelled widgets in loops collide and hiding a widget shifts the
     state of the ones after it.
  4. A huge style struct (`nk_style` with dozens of sub-structs), and tweaking one
     colour means knowing which of those to touch.
  5. Splitters, docking and tables do not exist, and the apps build them by hand.
  6. The command buffer is fixed-size and silently drops commands when full.
  7. The GDI/GDI+ backends redraw everything with GDI calls every frame. GDI+ is slow
     and anti-aliases inconsistently.
  8. About 30k lines in a single header, much of it optional and unused.

### 4.2 Recommended API style

Principles: every widget is one call that returns what changed; state lives in the app's
own variables; there is no ctx argument, no begin/end per widget, and no macros that hide
control flow.

```c
/* --- context: one implicit global, optionally thread-local --- */
gc_init(&(gc_desc){ .hwnd = h, .font = "Segoe UI", .size = 15 });   /* designated init */
gc_frame(hwnd);        /* begins a frame: input, size, DPI                        */
gc_end();              /* layout, tiles, present                                  */

/* --- widgets: label is also the ID ("Save##2" or gc_id_push(i) for duplicates) --- */
gc_text("Seeds: %d", n);                         /* printf-style, our own tiny fmt  */
if (gc_button("Draw 20")) draw();
gc_check("Hidden", &hidden);                     /* returns 1 when it changed       */
gc_slider("Size", &size, 8, 512);                /* float; gc_slider_i for int      */
gc_number("Seed", &seed);                        /* drag, wheel, or type            */
gc_combo("Model", &model, "a\0b\0c\0");          /* NUL-separated, no array         */
gc_edit("Path", buf, sizeof buf);                /* returns GC_EDIT_CHANGED|ENTER   */
gc_progress(done, total);
gc_image(img, 128, 128);                         /* gc_image handle from gc_image_make */
gc_tip("Right-click to redraw");                 /* tooltip for the previous widget */

/* --- containers: a scope function pair, explicit and greppable --- */
if (gc_tree("Settings")) { ...; gc_tree_end(); }
gc_scroll_begin("list", 0, 300); ...; gc_scroll_end();
gc_split("main", GC_H, &split_px);               /* draggable divider, two panes:   */
  ...left pane...; gc_next_pane(); ...right pane...; gc_split_end();

/* --- layout: rows by default, the cursor flows; sizes are optional --- */
gc_row(3, (float[]){ 80, -1, 60 });  /* px, -1 = grow, 0..1 = fraction; 0 args = auto */
gc_same_line();                      /* Dear ImGui-style escape hatch                 */
```

Why this shape:
- **The global context.** These apps have one UI thread. A `static gc_ctx G;` (or
  `_Thread_local` if a second window thread ever appears) removes about 70×N `ctx`
  tokens from the app code. `gc_ctx_set(other)` supports several windows.
- **Strings as IDs** give the zero-boilerplate path. Hash the string with the top of
  the ID stack (FNV-1a or wyhash, 32-bit is enough per window).
  `gc_id_push(i)`/`gc_id_pop()` covers loops. An `"##"` suffix hides the rest from
  display.
- **Return values instead of callbacks.** `gc_check`/`gc_slider` return 1 on
  change, so `if (gc_slider(...)) save_ini();` works.
- **Options in one optional struct** for rarely used settings:
  `gc_button_ex("X", &(gc_opt){ .w = 24, .icon = GC_ICON_CLOSE })`. C99 compound
  literals keep it to one line, and there are no flag soups.
- **The theme** is a small flat table of about 24 named colours plus metrics
  (`gc_theme.accent`, `.radius`, `.pad`), with the per-widget look derived from those.
  Not Nuklear's hundreds of fields. Push and pop single overrides:
  `gc_push_color(GC_COL_TEXT, c)`.

### 4.3 Core data structures

```c
typedef uint32_t gc_id;

typedef struct gc_box {           /* Fleury-style "one generic box"                 */
    gc_id     id;
    uint32_t  flags;              /* CLICK|DRAG|BG|BORDER|TEXT|CLIP|SCROLLX|SCROLLY  */
    uint32_t  last_frame;         /* prune if < frame - 1                           */
    struct gc_box *parent, *first, *last, *next;   /* rebuilt every frame           */
    struct gc_box *hash_next;     /* persistent cache chain                         */
    gc_size   size[2];            /* {kind: PX|TEXT|PCT|CHILDREN|GROW, value, strict} */
    float     fixed[2], rect[4];  /* solved layout (this frame) and last frame's    */
    float     hot_t, active_t;    /* animation                                      */
    float     scroll[2], view[2];
    const char *text; uint16_t text_len;
} gc_box;

typedef struct {
    gc_id hot, active, focus, last_id;
    gc_box *root, *cur;
    gc_box *table[4096];          /* open-addressed or chained, by id              */
    uint8_t *arena;               /* per-frame bump: strings, cmds, layout scratch  */
    gc_input in;                  /* mouse xy, buttons down/pressed/released, wheel,
                                     typed UTF-32 queue, key bits, mods             */
    gc_tile *tiles; int tw, th;
    uint32_t *fb; int fb_w, fb_h, stride;
} gc_ctx;
```

Two layout modes, both cheap:
1. **The cursor path** (microui/Nuklear style, the default). `gc_row` sets column
   widths, and each widget takes the next cell at once. There is no lag, and it covers
   forms (80% of these apps).
2. **The deferred box tree** (Fleury/Clay style). Used inside `gc_split`, toolbars,
   docking, or `gc_row()` with auto sizes. The hierarchy is built this frame. Hit tests
   use **last frame's** rects from the cache (by id), and the layout pass runs in
   `gc_end` before rasterizing:
   - pass 1 (post-order, per axis): `TEXT` and `PX` sizes, and `CHILDREN` sums
   - pass 2 (pre-order): `PCT` of the parent, distribute `GROW`, and shrink by
     `strictness` when the content overflows
   - pass 3: positions, with clip rects and scroll offsets
   That is O(n) per axis. Clay does the same and reports microsecond times for
   thousands of elements.

**Hit testing with the one-frame lag:** in `gc_button`, `inside =
pt_in_rect(cached->rect, mouse)` on the rects from the last layout. With
render-on-input, the second frame after an event makes the lag invisible.

**Overlapping windows, popups and menus** are layers: each window or popup gets its own
root and a z-order list. Hover goes to the topmost layer whose rect contains the mouse
(computed at frame start from last frame's rects). That is the standard Dear ImGui
approach.

### 4.4 Widgets on the generic box (the reuse table)

| Widget | Box flags plus extra code |
|---|---|
| button | CLICK, BG, BORDER, TEXT |
| checkbox / radio | CLICK, plus a drawn mark (path icon) |
| slider / number | DRAG, BG, plus value mapping, wheel, Ctrl+click to type (reuses edit) |
| combo | button plus a popup layer holding a list |
| menu bar / menu | row of buttons plus popup layers, keyboard navigation |
| tabs | row of toggle buttons, with the selected index in the app's variable |
| tree | CLICK row, open state in the cache (Dear ImGui `ImGuiStorage` style) |
| list / table | scroll container plus a **clipper** (only visible rows are submitted), header row of DRAG resizers; column widths in the cache |
| splitter | DRAG thin box between two GROW children; app owns the position |
| colour picker | SV square (2 gradients via `K_FILL_COV`), hue bar, number boxes |
| file picker | Use the OS dialog (`IFileOpenDialog` / `GetOpenFileNameW`), no code |
| date picker | 7×6 grid of buttons in a popup |
| docking | a tree of split nodes (H/V, ratio) with tab stacks at the leaves; drag a tab onto drop zones of 5 rects; save the tree as text |
| tooltip | popup layer at the mouse after a 500 ms hover (via `gc_animate`) |

---

## 5. Text

### 5.1 Where glyphs come from

The rule is OS DLLs only, so the cheapest full-Unicode path is to **let Windows rasterize
and shape**, and to own only the cache and the blitting:

1. **DirectWrite** (`dwrite.dll`, Windows 7+). `IDWriteFactory` →
   `IDWriteTextAnalyzer::GetGlyphs`/`GetGlyphPlacements` for shaping, or just
   `IDWriteFontFace::GetGlyphIndices` plus `GetDesignGlyphAdvances` for simple
   English UI, and `IDWriteGlyphRunAnalysis::CreateAlphaTexture` for
   `DWRITE_TEXTURE_ALIASED_1x1` or `DWRITE_TEXTURE_CLEARTYPE_3x1` coverage at any
   subpixel offset. Font fallback is `IDWriteFontFallback` (Windows 8.1+). It is a COM
   API, and calling it from C needs the `lpVtbl->` style. Check whether the mingw-w64
   `dwrite.h` exposes the C interfaces. If it does not, declare a minimal vtable struct
   ourselves (only the ~8 methods used).
2. **GDI and Uniscribe** (fully C-friendly): `GetGlyphOutlineW(..., GGO_GRAY8_BITMAP)`
   (65 gray levels, so convert `v*255/64`) or `ExtTextOutW` with
   `CLEARTYPE_QUALITY` into a DIB, plus `ScriptItemize`/`ScriptShape`/`ScriptPlace`
   (usp10.dll) for shaping, and `GetKerningPairsW` for simple kerning.
3. **The built-in fallback**: an stb_truetype-style rasterizer over an embedded font,
   for when no system font is wanted (for example a pixel or icon font). The icon set
   (checkmark, arrows, close, folder) is better as a tiny embedded path list rasterized
   by our own path filler, so it scales with DPI.

### 5.2 The glyph cache

```c
typedef struct {
    uint32_t key;           /* hash(font_id, size_px*64, glyph, subpx_phase)         */
    uint16_t x, y, w, h;    /* in the A8 atlas                                        */
    int16_t  bx, by;        /* bearing                                                */
    uint32_t last_used;     /* frame number, for LRU when the atlas fills             */
} gc_glyph;
/* CPU atlas: one A8 (or 3x8 for ClearType) 1024x1024 page, shelf packer;
   when full: evict LRU shelves, or add a page. 1 MB per A8 page. */
```

- **Subpixel positioning.** Quantize the pen x to 1/4 px and cache 4 phases per glyph.
  It removes the uneven spacing of whole-pixel snapping at a 4x cache cost, and the cache
  is tiny anyway. Keep y snapped to whole pixels (horizontal text).
- **Text drawing** is then a `GLYPHRUN` command (font, colour, origin, glyph ids, x
  positions). The tile kernel blits each glyph's A8 mask with `K_GLYPH`. For
  multithreading, all glyphs are inserted into the atlas while building the command,
  so workers only read.
- **Measure caching.** Labels are measured every frame. Cache
  `(hash(string), font) → width` for one frame back, or store the width in the box
  cache. Most measuring then becomes a hash lookup.
- **ASCII fast path.** A 96-entry direct table per (font, size) for glyph id and
  advance, skipping the hash, plus a pair-kern lookup for the common pairs.

### 5.3 Shaping: how much is enough

- These apps' UI text is English, but **file names and web titles** (pixelhunter) can
  be anything. Requirements: correct UTF-8 decoding (reject overlongs, map invalid
  bytes to U+FFFD), font fallback for CJK and emoji, and kerning.
- **No shaper** (codepoint → glyph, plus kerning) is correct for Latin, Greek, Cyrillic
  and CJK. Arabic, Indic and Thai, and also combining marks, need real shaping.
- **kb_text_shape** (Jimmy Lefevre, single header, about 22k+ lines, segmentation plus
  OpenType shaping, the author reports about 4.5x faster than HarfBuzz). A good choice
  if we rasterize fonts ourselves and want complex scripts. A HarfBuzz discussion
  (#5375) criticised slow face loading for big Arabic fonts and missing cluster info for
  cursor mapping in the version then current. Check whether that is fixed before
  relying on it for text editing.
- **Recommendation:** use DirectWrite or Uniscribe shaping (zero code, the OS owns
  correctness) when a string has any non-ASCII codepoint, and the ASCII fast path
  otherwise. Only add kb_text_shape if we ever drop the OS font path.

### 5.4 ClearType versus grayscale

- ClearType needs per-channel coverage (a 3×8 mask), per-channel blending (a separate
  kernel), knowledge of the panel's subpixel order (`SPI_GETFONTSMOOTHINGORIENTATION`),
  and it **breaks on any non-opaque background or transformed layer** because the
  colour fringes are baked in. It also triples atlas memory.
- Grayscale with gamma-correct blending (blend coverage in a contrast-boosted space,
  as DirectWrite and Skia do: apply `cov' = pow(cov, 1/1.45)`-style contrast for dark
  text on light, using a 256-entry LUT) looks close to ClearType at 125%+ DPI.
- **Recommendation:** grayscale first with a LUT-based gamma/contrast fix, and a
  ClearType kernel as an option for 100% DPI, off by default when the panel is
  rotated or the background is not opaque.

### 5.5 The text edit widget

- **stb_textedit** (Sean Barrett, about 1,400 lines, header-only, also used inside Dear
  ImGui as `imstb_textedit.h`). Gives cursor movement, word jumps, selection, undo and
  redo, and insert/delete over a user-supplied string API. It needs `STB_TEXTEDIT_*`
  callbacks for layout per row.
- **Fleury part 10**: a simple single-line field with cursor and mark as byte offsets,
  where every operation is `{delta, select}` → a pure function (state, op) → new state.
  Small and easy to test.
- **Recommendation:** our own one, about 400 lines. Cursor and mark as UTF-8 byte
  offsets, moves by grapheme (or codepoint for now), word moves by a char class, undo as
  a ring of snapshots (short strings), clipboard through `OpenClipboard`/`CF_UNICODETEXT`,
  and IME through `WM_IME_COMPOSITION` plus `ImmSetCompositionWindow` for the caret.
  Multi-line: the same core plus line starts from wrapping. Caret blink via
  `gc_animate`, so an idle text field costs one tiny tile per 530 ms.

---

## 6. Being tiny

### 6.1 The stb pattern

```c
/* guicy.h */
#ifndef GUICY_H
#define GUICY_H
/* public API only: types + prototypes, ~300 lines */
#endif
#ifdef GUICY_IMPLEMENTATION
/* everything else, all functions static except the API */
#endif
```

Options as `#define`s before the include (`GUICY_NO_THREADS`, `GUICY_NO_CLEARTYPE`,
`GUICY_FONT_STB`). Keep the backend (Win32 window, present, fonts via DirectWrite) in
the same header but under `#ifdef _WIN32`, so the core stays portable (Linux could get
an X11 or Wayland present later).

### 6.2 Size references

| Library | Size |
|---|---|
| microui | about 1,100 lines of C (core only, no renderer) |
| Clay | about 4-5k lines of C99, layout only (15 KB as WASM) |
| stb_truetype | about 5k lines |
| stb_textedit | about 1,400 lines |
| Nuklear | about 30k lines, a single header |
| Dear ImGui | about 60k+ lines of C++, core plus tables plus widgets |
| tiny-skia | about 14 KLOC (Rust), adds about 200 KiB |
| kb_text_shape | about 22-28k lines (mostly Unicode tables) |

**A GUICy target (est.):** core plus layout about 2.5k lines, widgets about 3k,
rasterizer plus kernels about 2k, Win32 platform plus DirectWrite about 1.5k, giving
**about 9k lines** and **about 60-120 KB** of code in the exe. That is less than
Nuklear and does more.

### 6.3 Dropping the CRT (clang, MinGW target)

- Link with `-nostdlib -lkernel32 -luser32 -lgdi32` and an entry
  `-Wl,-e,entry`. Build with `-ffreestanding -fno-builtin` only where needed (clang
  still emits `memset`/`memcpy` calls for struct copies and loops, so provide them,
  about 20 lines, using `rep stosb`/`rep movsb`, which are fast on ERMSB/FSRM CPUs).
- Large stack frames call `___chkstk_ms`. Avoid it with `-mno-stack-arg-probe` (and
  commit enough stack via `-Wl,--stack,...`), or provide a stub. Avoid big locals in
  any case.
- Floating point: MinGW needs `_fltused` in some cases (a one-line `int _fltused;`).
  Math: `sqrtf` is a builtin (`sqrtss`). Write your own `powf`, `expf` and similar for
  animation as small approximations, or use LUTs.
- No `printf`: a 150-line `gc_fmt` (`%d %u %x %s %f %.Nf %c`) is enough for labels.
  Chris Wellons ("Small, Freestanding Windows Executables") shows complete working
  setups with exes of a few KB.
- Memory: one `VirtualAlloc` at init, sliced into the per-frame arena, the box cache,
  atlas pages and the tile buffers. No `malloc` in the library at all. This is the same
  as Clay and microui.
- Since the apps already use the CRT, make "no CRT inside the library" the rule (no
  libc calls in guicy.h), not "no CRT in the app". The library then works in both kinds
  of build.

### 6.4 Keeping code small

- One generic box with flags instead of N widget structs (Fleury part 4).
- One command stream and one tile rasterizer. No separate code paths for "draw
  directly" versus "draw cached".
- Data tables instead of code: the theme, the icon paths and the kernel table.
- Use `float` everywhere in layout (no fixed-point mix, the Nuklear issue), and `int16`
  only in commands.

---

## 7. Papers and longer reading

- **L. Stampfl, "High-performance 2D graphics rendering on the CPU using sparse
  strips"**, MSc thesis, ETH Zurich, 2025. The best current survey of CPU 2D rendering
  plus the sparse strip method and Blend2D comparisons.
- **D. Ganacim, R. Lima, L. H. de Figueiredo, D. Nehab, "Massively-Parallel Vector
  Graphics"**, ACM TOG (SIGGRAPH Asia) 2014. Shortcut trees per cell, with a CPU
  reference. Useful for the theory of tile-based vector rendering.
- **D. Nehab, H. Hoppe, "Random-Access Rendering of General Vector Graphics"**,
  SIGGRAPH Asia 2008. Per-cell specialised shape lists. The ancestor of "bin commands
  into tiles".
- **G. Badros, A. Borning, P. Stuckey, "The Cassowary Linear Arithmetic Constraint
  Solving Algorithm"**, ACM TOCHI 2001. Constraint layout (Apple Auto Layout). Mostly
  a lesson in what **not** to put in an IMGUI: an incremental simplex solver is heavy.
  Flexbox-like two-pass sizing gets 95% of the benefit.
- **R. Levien, "Inside the fastest font renderer in the world"**, 2016. The
  accumulation-buffer rasterizer explained.
- **S. Barrett, "How the stb_truetype Anti-Aliased Software Rasterizer v2 Works"**.
- **P. Kobalíček, "Blend2D: High-performance 2D vector graphics"**, talk slides,
  Helsinki 2023. The JIT pipeline and the dense cell rasterizer.
- **C. Muratori, "Immediate-Mode Graphical User Interfaces"**, video, 2005.
- **R. Fleury, "UI" series parts 1-10**, Digital Grove, 2022-2023.
- **E. Wallace, "Fast Rounded Rectangle Shadows"**, 2016. A closed-form blurred
  rounded-rect shadow.
- **A. R. Smith, "Image Compositing Fundamentals"** (Microsoft Tech Memo 4, 1995) and
  **T. Porter, T. Duff, "Compositing Digital Images"** (SIGGRAPH 1984). Why
  premultiplied alpha.
- **J. Blinn, "Three wrongs make a right"** (IEEE CG&A, 1995) and "Dirty pixels"
  (1989). Integer blend arithmetic and gamma.
- The **Linebender blog** ("This Month in Linebender", 2025) for the Vello CPU SIMD and
  multithreading progress.

---

## 8. Sources

Rasterizers and rendering
- Levien, "Inside the fastest font renderer in the world": https://medium.com/@raphlinus/inside-the-fastest-font-renderer-in-the-world-75ae5270c445
- font-rs repo: https://github.com/raphlinus/font-rs (rasterizer: https://github.com/raphlinus/font-rs/blob/master/src/raster.rs)
- stb (stb_truetype, stb_textedit): https://github.com/nothings/stb
- Barrett, stb_truetype rasterizer v2: https://nothings.org/gamedev/rasterize/
- Blend2D about page: https://blend2d.com/about.html
- Blend2D Helsinki 2023 slides: https://blend2d.com/research/blend2d_presentation_helsinki_2023.pdf
- Blend2D author on the dense cell buffer (HN): https://news.ycombinator.com/item?id=19585574
- Blend2D vs AGG discussion (Haiku forum): https://discuss.haiku-os.org/t/replacing-agg-with-blend2d/7206
- tiny-skia: https://github.com/linebender/tiny-skia ; pipeline notes: https://docs.rs/tiny-skia/latest/src/tiny_skia/pipeline/mod.rs.html
- Skia docs: https://skia.org/docs/
- Anti-Grain Geometry: https://agg.sourceforge.net/antigrain.com/
- libart: https://levien.com/libart/
- Pixman: https://www.pixman.org/
- Cairo: https://www.cairographics.org/
- refterm: https://github.com/cmuratori/refterm ; glyph cache: https://github.com/cmuratori/refterm/blob/main/refterm_glyph_cache.h
- refterm hash write-up: https://m1el.github.io/refterm-hash/
- Stampfl thesis (sparse strips): https://ethz.ch/content/dam/ethz/special-interest/infk/inst-pls/plf-dam/documents/StudentProjects/MasterTheses/2025-Laurenz-Thesis.pdf
- Thesis HN thread: https://news.ycombinator.com/item?id=45881568 ; benchmark charts: https://laurenzv.github.io/vello_chart/
- Vello (sparse_strips/vello_cpu): https://github.com/linebender/vello
- Linebender monthly notes: https://linebender.org/blog/tmil-15/ , https://linebender.org/blog/tmil-18/ , https://linebender.org/blog/tmil-19/ , https://linebender.org/blog/tmil-22/
- VGCPU-Benchmark (CPU vector library comparisons): https://github.com/jpbruyere/VGCPU-Benchmark

Pixel arithmetic
- Exact div-255 forms, verified over the full range: https://github.com/OpenSmalltalk/opensmalltalk-vm/issues/798
- The "double blend trick": https://stereopsis.com/doubleblend.html
- "Alpha Blending with No Division Operations" (arXiv 2202.02864): https://arxiv.org/pdf/2202.02864v1.pdf
- Wallace, "Fast Rounded Rectangle Shadows": https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html

Windows presenting
- SetDIBitsToDevice: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setdibitstodevice
- CreateDIBSection: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection
- UpdateLayeredWindow: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow
- DXGI flip model and dirty rects: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model
- DirectWrite glyph run analysis: https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwriteglyphrunanalysis
- Uniscribe: https://learn.microsoft.com/en-us/windows/win32/intl/uniscribe

IMGUI core
- Muratori, IMGUI (2005): https://caseymuratori.com/blog_0001
- Fleury, UI Part 2: https://www.dgtlgrove.com/p/ui-part-2-build-it-every-frame-immediate ; series index: https://www.dgtlgrove.com/p/posts-table-of-contents
- microui: https://github.com/rxi/microui
- Dear ImGui: https://github.com/ocornut/imgui ; IMGUI paradigm wiki: https://github.com/ocornut/imgui/wiki/About-the-IMGUI-paradigm
- Clay: https://github.com/nicbarker/clay ; https://www.nicbarker.com/clay
- Nuklear: https://github.com/Immediate-Mode-UI/Nuklear

Text
- kb (kb_text_shape): https://github.com/JimmyLefevre/kb
- HarfBuzz discussion of kb_text_shape: https://github.com/harfbuzz/harfbuzz/discussions/5375

Small binaries
- Wellons, "Small, Freestanding Windows Executables": https://nullprogram.com/blog/2016/01/31/

Papers
- Ganacim et al. 2014: https://w3.impa.br/~diego/projects/GanEtAl14/
- Nehab and Hoppe 2008: https://hhoppe.com/proj/ravg/
- Cassowary: https://constraints.cs.washington.edu/solvers/cassowary-tochi.pdf
- Porter and Duff 1984: https://keithp.com/~keithp/porterduff/p253-porter.pdf

# GUICy stress test (2026-11-22, GUICy 0.4.0, CPU renderer only)

Machine: Intel i9-10980XE @ 3.0 GHz, Windows 11 Pro 26200, clang 23.1.4 -O2, MinGW target.
Harness: `bench/gystress.c` (built by `build.cmd` into `build\gystress.exe`). Raw output of the
second full run: `bench/stress-raw.txt`.

## Method

- Offscreen (`gy_offscreen`), no window, one thread, AVX2 kernels unless a row says otherwise.
- Times come from `QueryPerformanceCounter` (10 MHz). Each frame is split into two parts: the **UI pass**
  (`gy_begin` plus the widget calls) and **gy_end** (bookkeeping, the tile hash pass, tile clears,
  drawing). A row gives the median and p95 of N frames (after a few untimed warm-up frames), plus
  the median dirty-tile and instance counts from `gy_get_stats()`.
- "Full redraw" means `gy_tiles(0)`: every pixel is drawn every frame (the hash pass still runs).
- In a real window each input event runs **two** UI passes (`passes = 2`). The second pass is an
  unchanged frame (about 0.015 ms here), so the cost of one event is the row's number plus 0.015.
- The whole harness was run twice, plus single-scene re-runs. The two full runs agree within
  about 5 % (the medians quoted are from run 2, with run 1 in brackets where they differ). Other
  sessions were using the PC. Bash had a fork failure once (out of resources), so p95s are a bit noisy.
- A typing frame has the caret steady, because the test clock does not move.
- The windowed demo was run 3 times through `tools/baseline.py` (hidden desktop).

## Budgets (PLAN.md section 6)

| Budget | Measured | Result |
|---|---|---|
| Hover frame, CPU path, 1080p: 0.3 ms or less | typical screen **0.338 (0.355) ms**, 10 tiles; 1540x950: 0.277 ms; 1440p: 0.438 ms; 1,000 buttons: 0.306-0.334 ms | **MISS** at 1080p (by 13-18 %) |
| Caret / typing frame: 0.3 ms or less | 0.062 ms, 1 tile | pass |
| Full-window redraw, 1080p: 4 ms or less | typical screen **4.21 (4.30) ms**; 1,000 visible buttons **24.4 ms**; 1,000 rounded boxes alone 29.6 ms | **MISS** |
| UI pass, typical app screen: 0.2 ms or less | 0.009 ms (481 instances) | pass (20x under) |
| UI pass, big screens | 5,000 buttons 0.67 ms, 20,000 buttons 2.9 ms, 10,000 text lines 4.5 ms (mostly off screen) | MISS (no culling) |
| Startup to the first frame (library part): 30 ms or less | 6.95 (7.21) ms: init 0.22, gy_offscreen 0.98, first frame 5.76 | pass |
| Memory at 1440p: 16 MB or less for arena, atlases, tiles | 19.0 MB committed offscreen, but 14.1 MB of that is the offscreen frame itself. Without the frame: about 4.9 MB (tiles 1.1, atlas 1, glyph scratch 1, glyph table 0.16, frame arena 1) | pass for a window (its frame is a DIB, outside the arena); 18 MB if the frame counts |
| Idle CPU: under 20 ms per 60 s | demo: **0.0, 0.0, 46.9** ms | 2 pass; 1 miss, read as noise (3 scheduler ticks while other sessions were busy) |
| No worse than the Nuklear app | demo per mouse move **0.104, 0.078, 0.142 ms** (pixelhunter: 7.2) | pass |
| Frame on the GPU path | not built yet | n/a |

## 1. Typical app screen

This is the step-4 `v_scene` (split, sliders, numbers, combo, edit, chips, progress, buttons,
wrapped text, tree, a 30-row scroll list, 2 images, canvas line/dash/circle), plus 21 more widgets
in the right pane (6 buttons, 4 checks, 3 selects, texts, a slider, a progress bar). That makes
about 40 widgets and 481 instances.

| case | 1540x950 | 1920x1080 | 2560x1440 |
|---|---|---|---|
| first frame, cold (53 glyph misses), full | 5.65 ms (UI 2.10) | 6.25 ms (UI 2.07) | 7.70 ms (UI 2.07) |
| unchanged frame | 0.015 ms, 0 tiles | 0.015 ms | 0.015 ms |
| hover over one button (on/off every frame) | **0.277** ms, 8 tiles | **0.338** ms, 10 tiles | **0.438** ms, 13 tiles |
| full redraw, warm | 3.32 ms | **4.21 ms** | 5.44 ms |
| typing one char (char / backspace alternately) | 0.059 ms, 1 tile | 0.062 ms | 0.060 ms |
| dragging a slider, 3 px a frame | 0.166 ms, 7 tiles | 0.185 ms | 0.188 ms |

The UI pass is 0.009 ms in every steady case. All the cost is in `gy_end`.

**Why the full redraw misses:** only **0.47 Mpx** are drawn at 1080p (0.23 times the frame: 35
rounded or bordered boxes cover 0.44 Mpx, and glyphs, lines and images 0.04 Mpx). The clear costs
0.30 ms, so about 3.9 ms goes on 35 boxes. See slow spot 1.

**Why the hover grows with the window:** the button is wider, so it covers more tiles (8, 10, 13),
and every pixel of a button is a rounded, bordered box. The cost is about 8 ns per pixel of dirty
tile, against 2 ns for the average pixel of a full redraw.

## 2. Text heavy

### 2a. 1920x1080, one column, default font (about 30 lines visible), scrolled to the middle

| total lines | unchanged | wheel notch each frame | full redraw | UI pass | gy_end unchanged | instances |
|---|---|---|---|---|---|---|
| 1,000 | 0.51 ms | 1.09 ms (52 tiles) | 2.44 ms | 0.41 ms | 0.09 ms | 19,895 |
| 10,000 | 5.98 ms | 7.83 ms | 10.2 ms | 4.50 ms | 1.50 ms | 208,896 |
| 100,000 | **83.4 ms** | 106 ms | 116 ms | 60.2 ms | 23.2 ms | **2,188,897** |

**The cost grows linearly with the total rows: there is no culling.** Each line costs about
600 ns of UI pass (formatting plus about 22 glyph lookups and pushes), whether it is on screen or
not. Each instance then costs about 10.6 ns in `gy_end`, from three passes over the list, one per
layer, plus the hash. Only the 30 visible lines need drawing.

### 2b. About 2,000 visible lines: 3840x2160, 13 columns, 11 px font, 13 px rows, gap 1

| total lines | unchanged | wheel notch each frame | full redraw | instances |
|---|---|---|---|---|
| 2,000 (all visible) | 1.49 ms | **21.8 (23.7) ms**, 1,326 of 2,040 tiles | **16.0 (16.9) ms** | 40,895 |
| 10,000 | 6.20 ms | 18.4 ms | 24.0 ms | 208,896 |
| 100,000 | 83.7 ms | 119 ms | 128 ms | 2,188,897 |

**When most tiles are dirty, the dirty-tile path is slower than a full redraw:** 21.8 ms against
16.0 ms. The same scroll would be cheaper with tiles off. See slow spot 6.

## 3. Many buttons, gy_grid(100), 1920x1080

The buttons sit in a scroll region with 16 columns, so about 560 are on screen. The labels are
made before timing.

| buttons | unchanged (UI pass) | hover on/off | full redraw | instances |
|---|---|---|---|---|
| 1,000 | 0.143 ms (0.101) | 0.306 ms, 3 tiles | **21.1 ms** | 4,892 |
| 5,000 | 0.867 ms (**0.666**) | 1.21 ms | 21.2 ms | 28,892 |
| 20,000 | 3.76 ms (**2.91**) | 4.82 ms | 25.9 ms | 128,892 |
| 1,000, all on screen (gap 1) | 0.154 ms (0.106) | 0.334 ms | **24.4 ms** | 4,892 |

A button costs about 145 ns of UI pass and 6.6 ns per instance in `gy_end`, on screen or not.
A full redraw of about 560-1,000 visible buttons costs 21-24 ms, about 25-40 us per button, which
is the rounded-box cost.

## 4. Animated scene: a progress bar moves every frame (typical screen, 1080p)

| | median | p95 | dirty tiles |
|---|---|---|---|
| tiles on | **0.160 ms** | 0.264 ms | 3 |
| tiles off (full redraw) | 4.16 ms | 4.31 ms | 510 |

Dirty tiles make it 26 times cheaper.

## 5. Tiling, 1920x1080 (each tile: a text, a button, a check box, a slider)

| tiles | unchanged | focus move (Alt+Right/Left) | grow/shrink (Alt+Plus/Minus) | flip (Alt+F) |
|---|---|---|---|---|
| 4 | 0.006 ms | **2.18 ms, 135 tiles** | 2.14 ms, 135 tiles | 2.93 ms |
| 12 (5 stacked as tabs) | 0.010 ms | 0.92 ms, 34 tiles | 0.67 ms, 20 tiles | 0.63 ms |
| 32 | 0.032 ms | 0.031 ms, **0 tiles** | 0.14 ms, 1 tile | 0.13 ms |

- **A focus move redraws whole tiles.** The focused tile's outline is the border colour of the
  tile's panel box, which also holds the fill (`guicy.h:2293`). Changing the colour changes that
  box's hash in every 64-px tile it covers: 135 of 510 tiles, 2.2 ms, for 4 pixels of outline.
- **Bug: with 32 tiles the layout breaks.** 19 of the 25 leaves are under the 160x110 minimum, and
  the smallest leaves are -6 px wide and -5 px high. Focus never moves (node 0 stays 0), so the
  32-tile "pass" above is not real. The cause is `guicy.h:2123`:
  `if ((half < MIN) && L->ntabs < GY__TABS)`. When the focused leaf is too small *and* already
  holds 8 tabs, the code falls through and splits it anyway, again and again. Fix: when the leaf
  is full, add the tile as a tab to the largest leaf that has room, or to the leaf that can still
  be split. Never split below the minimum.

## 6. Resize: 30 sizes in a row, 800x500 up to 2560x1440, typical screen

| | median | p95 / max |
|---|---|---|
| gy_offscreen + frame | 4.94 ms | 7.74 ms |
| gy_offscreen alone | 1.07 ms | |
| frame alone (full redraw) | 3.88 ms | 5.62 ms max; 2.10 ns per pixel |

**Leak (offscreen only):** `gy_offscreen` pushes a new frame onto the permanent arena every call
(`guicy.h:775`) and never frees the old one. Process memory grew **204 MB** over the 30 sizes
(perm arena 213 MB committed). The window path (`gy__dib`) does not leak. Fix: reuse the buffer
when the new size fits, or keep the frame in its own `VirtualAlloc`.

## 7. Kernels at frame scale (1920x1080, full-redraw path; draw = gy_end minus an empty frame's)

| case | avx2 | sse2 | scalar | per item |
|---|---|---|---|---|
| full clear (gy_clear) | 0.304 ms | 0.307 | 0.344 | 0.15 ns/px (memory bound, 27 GB/s) |
| 1,000 plain rects 92x24 | 0.47 ms | 0.50 | 0.50 | 0.21 ns/px |
| **1,000 rounded boxes 92x24, radius 4, border 1** | **29.6 ms** | 31.2 | 31.9 | **29.6 us per box, 13.4 ns/px (63 times a plain rect)** |
| 10,000 glyphs (15 px) | 2.62 ms | **2.20** | 4.16 | 257 / 216 / 407 ns per glyph: **AVX2 is 19 % slower than SSE2** |
| 512x512 image to 1024x1024, bilinear | **20.1 ms** | 20.7 | 21.4 | 19.2 ns/px |
| same, nearest | 2.73 ms | 3.10 | 3.72 | 2.6 ns/px |
| **200 lines, 2 px wide, about 1,040 px long** | **296 ms** | 297 | 297 | **1.48 ms per line** |

## 8. First-frame costs (a cold process, 1540x950)

| step | ms |
|---|---|
| process start to main (loader and CRT; noisy) | 9.8 / 18.6 |
| context init (arenas, glyph table, DC, Segoe UI) | 0.216 |
| a second font (Consolas 15) | 0.56 in run 1, 0.016 in run 2 (the OS font cache) |
| gy_offscreen(1540, 950) | 0.98 |
| first frame (53 glyph misses): UI pass 2.22, gy_end 3.54 | 5.76 |
| second frame (unchanged) | 0.023 |
| **library startup to first frame** | **6.95** (budget 30: pass) |
| 1,024 new glyphs in one frame | 35.6, that is **34 us per miss** |
| the same 1,024 glyphs again (hits) | 0.037 |

## The slowest things, ranked, with suggested fixes

1. **The rounded and bordered box rasterizer: 13.4 ns/px, 29.6 us for one 92x24 button box
   (63 times a plain rect).** It causes both budget misses: the 1080p full redraw (4.2 ms for
   0.44 Mpx of boxes) and the hover (0.34 ms for 10 button tiles), and it makes 1,000 visible
   buttons cost 24 ms. Where: `gy__draw_box_cpu`, `guicy.h:2609-2637`. Every pixel of the top and
   bottom band rows, all `radius+1` of them, runs `gy__box_px` (`:2632`), across the whole width:
   two `gy__box_cov` calls with a sqrt, `gy__scale`, a per-channel add loop, then `gy__over`.
   The middle rows blend the precomputed row with `k_row` (0.32 ns/px), even where it is opaque.
   Fix:
   - (a) Work out pixels one by one only in the four corner squares (`band x band`), and cache
     their coverage per (radius, border, colours), since every button shares them.
   - (b) The straight parts of the band rows have one value per row: fill or blend them as a span.
   - (c) Use `k_fill` (stosd, about 0.02-0.05 ns/px) for the opaque inside of the middle rows,
     and blend only the edge pixels.

   Expected: a button box from about 30 us to about 1 us, the 1080p full redraw from 4.2 to under
   1 ms, and a hover from 0.34 to about 0.1 ms.
2. **No culling: off-screen widgets cost the same as visible ones.** 100,000 list lines take
   83 ms an unchanged frame (2.19M instances), and 20,000 buttons take 2.9 ms of UI pass. Where:
   `gy__text_v` (`:1331-1335`) formats and pushes every glyph (`gy__draw_text_n`, `:1047-1057`),
   and buttons and other widgets push the same way. Then `gy__render` walks the whole list 3 times,
   once per layer (`:2724, :2732, :2757`). Fix:
   - (a) When a widget's cell rect is outside the current clip, run its layout and input logic
     but skip formatting and pushing.
   - (b) Add a list clipper, `gy_scroll_rows(row_h, &first, &last)`, so apps skip the calls
     altogether. Then the cost is O(visible).
   - (c) Bucket the instances per layer once (or count them per layer and skip empty layers), so
     `gy__render` walks the list once, not three times.
3. **Lines scan their whole bounding box.** One 2-px diagonal 300x1000 px line costs 1.48 ms
   (305k pixels tested for about 2k covered). Where: `gy__draw_line_cpu`, `:2649-2660`, with
   `gy__seg_dist` per pixel. Fix: for each row, work out the x-span the capsule covers and test
   only those pixels (plus 1 px of anti-aliasing), so the cost goes with length times width.
   `gy_canvas_dash` makes many short lines, so it is cheaper, but the same applies.
4. **Image scaling: bilinear is 19.2 ns/px (20 ms for 1024x1024) and nearest is 2.6 ns/px.**
   Where: `gy__draw_image_cpu`, `:2662-2700`. It does one or two **64-bit divides per pixel**
   (`:2675`, `:2678`, `:2685`), and the lerp is a scalar per-channel loop. Fix: work out the
   source x positions and weights once per draw (they are the same for every row), step y with a
   16.16 DDA, and do the lerp with SSE2/AVX2 (`_mm_mullo_epi16` on 2 pixels by 2 weights).
   Expected: about 1-2 ns/px bilinear, under 0.5 ns/px nearest.
5. **A tile focus move redraws whole tiles: 135 of 510 tiles, 2.2 ms with 4 tiles.** Where:
   `:2293`, where one box carries both the panel fill and the focus colour as its border. Fix:
   draw the fill as its own box, and the outline either as four thin rects or as a border-only
   box that the hash pass marks only along its edges. A focus move then dirties only the
   perimeter tiles (about 0.3 ms with fix 1).

Also found:

6. **The dirty-tile path beats a full redraw only when few tiles are dirty.** With 1,326 of 2,040
   tiles dirty it took 21.8 ms, against 16.0 ms for a full redraw. Each dirty tile re-runs every
   overlapping instance (the setup for each box, plus the glyphs cut at tile edges). Fix: in
   `gy__render` (`:2741`), when the dirty count is over about half the tiles, take the full path.
7. **Glyph misses: 34 us each.** 53 misses make 2.1 of the first frame's 5.8 ms. Where:
   `gy__glyph_get`, `:992-995`. It calls `SelectObject` and `GetGlyphOutlineW` twice (a size query,
   then the data). Fix: one call into the 1 MB scratch (`cbBuffer = 1<<20`), select the font only
   when it changes, and optionally pre-bake ASCII 32-126 at `gy_font_add`.
8. **The AVX2 glyph kernel is 19 % slower than SSE2 on real 15-px text** (257 against 216 ns per
   glyph). Glyph rows are 6-12 px, so AVX2 does one 8-px block and then a scalar tail, and the
   function-pointer call is made per row (`:2645`). Fix: use SSE2 for runs under 16 px, or one
   call per glyph that loops over the rows inside the kernel.
9. **The gy_offscreen leak** (section 6, `:775`): 204 MB over 30 sizes. This hits tests and
   headless tools only.
10. **The 32-tile layout bug** (section 5, `:2123`): leaves are split below the minimum once a
    stack holds 8 tabs.

Not slow: an unchanged frame (0.015 ms), the UI pass of a normal screen (0.009 ms), the caret or
typing frame (0.06 ms), clears (0.15 ns/px), plain rects, the hash pass (about 3-4 ns per
instance), startup (7 ms), and idle.

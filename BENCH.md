# GUICy benchmarks

Every speed or size claim GUICy makes has a row here, measured on this PC.
Machine: Windows 11 Pro 26200, RTX 4090 desktop. Compiler: clang 23.1.4 (X:\tools\llvm), MinGW target.

## Step 0: the three Nuklear apps today (2026-11-22)

All three draw with Nuklear on the CPU through GDI (voxup, pixelhunter) or GDI+ (bbb_kit).
Measured from the outside by `tools/baseline.py`:
- Each exe is **copied** into `work\baseline\<app>\` and run there, so the apps' own folders are
  never touched.
- It runs on a **separate Windows desktop** (`CreateDesktop`, never switched to), so no window
  reaches the user's screen or takes focus.
- No input for 60 s gives the idle numbers. Then 600 `WM_MOUSEMOVE` messages are posted to the
  window at 60 a second, and their CPU cost (minus idle) gives "per mouse move".

| App | exe KB | start to window | idle CPU (% of one core) | idle CPU ms per 60 s | CPU ms per mouse move | client size | peak working set |
|---|---|---|---|---|---|---|---|
| bbb_kit (GDI+) | 23,608 | 497 ms | **31.5 %** | 18,891 | **26.1** | 2176x1183 | 36.5 MB |
| voxup (GDI) | 86,794 | 432 ms | 0.03 % | 16 | **14.4** | 2560x1369 | 119.2 MB |
| pixelhunter (GDI) | 674 | 107 ms | 1.3 % | 781 | **7.2** | 1540x950 | 122.1 MB |

What the numbers say:
- **Every mouse move costs a full redraw:** 7-26 ms of CPU. That is about 1-2 frames' worth of
  time at 60 Hz, spent even when the pointer crosses empty space. GUICy's budget for a hover is
  0.3 ms (section 6 of PLAN.md).
- **bbb_kit at idle burns about a third of a core.** Its PC-meters timer redraws the whole window
  twice a second (`SetTimer(hMain, 3, 500)`), and one full GDI+ redraw at 2176x1183 costs roughly
  150 ms. With dirty tiles, a meter tick redraws only the meter's few tiles.
- voxup idles well (it doesn't redraw without input) but has the most expensive moves (the
  largest window, plus its preview images).
- pixelhunter's 1.3 % idle includes a startup search: with the user's ini copied beside it, it
  searched and cached a few pictures during the run. Its true idle is likely lower. Its
  per-move number is not affected.
- The exe sizes are mostly the apps' own content (bbb_kit's models, voxup's built-in drawing
  engine), not their GUI.

**Nuklear's share of each exe:** Nuklear plus its backend, compiled alone with -O2 using each
app's own nk_conf (section sizes from llvm-size):

| App | text | data | bss | total |
|---|---|---|---|---|
| pixelhunter (nuklear.h + nuklear_gdi.h) | 214,249 | 21,958 | 18,540 | **254,747 B (249 KB)** |
| voxup (nuklear.h + nuklear_gdi.h) | 214,681 | 21,994 | 18,540 | **255,215 B (249 KB)** |
| bbb_kit (nk_ui.c: nuklear.h + nuklear_gdip.h) | 216,345 | 22,178 | 18,693 | **257,216 B (251 KB)** |

GUICy's budget: 120 KB or less for the CPU-only build, 140 KB with the GPU path.

**Not measured yet: frame ms inside the app.** That needs a timing hook in each app's code,
and the apps' folders are read-only for this session. The hook goes in on each app's port
branch (steps 6, 8, 10, coordinated through Master Control), before its Nuklear code changes.
Until then, "CPU ms per mouse move" stands in for the frame cost.

**Reference pictures** (each app's own hidden selftest, in `bench/baseline/`):
- `bbb_kit.png`, `bbb_kit.help.png`, `bbb_kit.small.png`: from `--resizetest`. Its sizes were
  start 1650x1240, larger 2154x1501, smallest 921x560.
- `voxup.png`: from `--selftest --novulkan`.
- `pixelhunter.png`: from `--selftest --no-sites`, so no network.

### The apps' own helpers, mapped to GUICy (for the ports)

| App | Helper | GUICy call |
|---|---|---|
| voxup | `nk_ext_row(h, n, w)` | `gy_row(n, widths)` with `gy_row_h` |
| voxup | `nk_ext_wrap_label` | `gy_text_wrap` (the row grows to fit) |
| voxup | `nk_ext_splitter` | `gy_split` / `gy_pane` (drag, double-click reset, tip) |
| voxup | `nk_ext_tooltip`, `nk_ext_tip_waiting`, `nk_ext_frame` | `gy_tip` (delay built in; wake timing via `gy_animate`) |
| pixelhunter | `label_fit(text, col)` | `gy_text_col` with fit-to-width (an ellipsis option on `gy_opt`) |
| pixelhunter | `splitter(id, &v, sign, def)` | `gy_split` |
| pixelhunter | `edit_field` | `gy_edit` |
| pixelhunter | `styled_button(label, kind, enabled)` | `gy_button_ex(&gy_opt{.accent})` inside `gy_disable_begin/end` |
| pixelhunter | `draw_asset_card`, `draw_asset_list_row` | `gy_grid` + `gy_image` + canvas calls (app code stays app code) |
| pixelhunter | `handle_shortcuts`, `draw_keys_overlay` | `gy_button_key`, `gy_keys_owned`, a layer for the overlay |
| bbb_kit | `nk_theme(t)`, `nkc(ARGB)` | `gy_theme_set(&t)` (three theme tables) |
| bbb_kit | `nk_section(label)` | `gy_section` |
| bbb_kit | `nk_chips(label, items, n, cols, sel, ...)` | `gy_chips(items, n, &sel)` in `gy_grid` |
| bbb_kit | `nk_act(kind, idx)` + `nkActed` redraw | not needed: a GUICy change redraws its own tiles |
| bbb_kit | `nk_dash_line`, `nk_preview` | `gy_canvas_dash`, canvas calls |
| bbb_kit | `nk_side(H)`, `nk_top(mx, mw)` | `gy_tile("Side")`, `gy_tile("Top")` or `gy_split` |
| bbb_kit | `nkui_init`, `nkui_font_of`, `nkui_image` | `gy_attach(hwnd)`, `gy_font(i)`, `gy_image_make` |
| bbb_kit | the card grid, drawn with its own GDI+ code (not Nuklear) | `gy_grid` + canvas, a separate decision at its port |

## Step 1: the skeleton (2026-11-22)

| Thing | Number |
|---|---|
| `guicy.o` (the library alone, -O2): text / data / bss | 1,798 / 544 / 664 = **3,006 B** |
| What `guicy.o` needs from outside | `CloseHandle`, `CreateFileA`, `VirtualAlloc`, `VirtualFree`, `WriteFile` (kernel32). **No C library.** |
| `gytest.exe` imports | kernel32 plus the Windows-shipped UCRT `api-ms-win-crt-*` (from the test host's stdio, not from GUICy) |
| `gytest.exe` / `probe_dwrite.exe` | 50,176 B / 43,008 B |
| `blank` golden (256x160) | equal byte for byte on repeated runs, and with `GUICY_USE_CRT` |
| DirectWrite from plain C | works through the MinGW `dwrite.h` C vtables, loaded at run time: 457 font families, Segoe UI found |

Found while proving: clang turns GUICy's own byte loop into a `memset` call, which would call
itself forever without the CRT. `__attribute__((no_builtin("memset")))` on the helpers stops it,
and `llvm-nm -u` now lists only the five kernel32 functions.

## Step 2: core, CPU renderer, first widgets (2026-11-22)

| Thing | Number |
|---|---|
| `guicy.o` (-O2): text / data / bss | 22,216 / 3,396 / 10,848 = **36,460 B** (Nuklear + GDI backend: about 255,000 B) |
| What `guicy.o` needs from outside | kernel32: `CloseHandle CreateFileA GetEnvironmentVariableA VirtualAlloc VirtualFree WriteFile`; gdi32: `CreateCompatibleDC CreateFontW DeleteDC DeleteObject GetGlyphOutlineW GetTextMetricsW SelectObject`. No C library. |
| Tests | 8 of 8 pass, each drawn with the **scalar, SSE2 and AVX2 kernels, all three byte-identical**, and equal to its golden hash. Also passes with `GUICY_USE_CRT`. |
| Scripted input | `click_check`: move, press, release over the check box turns it on; a select click moves the choice; a press on a disabled button does nothing |
| Draw list entry | 48 bytes (checked at compile time) |

Kernels (all exact-integer, rounded /255): solid fill, row blend (rounded-corner and border rows),
glyph coverage blend. Rounded corners and borders are worked out per pixel only in the top and
bottom bands; the rows between reuse one precomputed row.

Deviation from PLAN 3.8: there are no subpixel x phases yet. GDI's `GetGlyphOutline` can't offset
a glyph by a fraction of a pixel, so glyphs sit on whole pixels, the same as Nuklear's GDI backend.
The 4 phases come with DirectWrite (step 9).

## Step 3: sleep when idle, redraw only changed tiles (2026-11-22)

The demo (`examples/demo.c`: the step-2 widgets plus 20 lines of text, at pixelhunter's 1540x950)
was measured with the same tool and method as the Nuklear apps (hidden desktop, 60 s idle,
600 posted mouse moves):

| App | start to window | idle CPU ms per 60 s | CPU ms per mouse move | peak working set | exe |
|---|---|---|---|---|---|
| **GUICy demo** | **52 ms** | **15.6** (0.026 % of a core; one 15.6 ms scheduler tick) | **0.073** | 34.4 MB | 83 KB |
| pixelhunter (Nuklear GDI), same size | 107 ms | 781 | 7.2 | 122 MB | 674 KB |
| voxup (Nuklear GDI) | 432 ms | 16 | 14.4 | 119 MB | |
| bbb_kit (Nuklear GDI+) | 497 ms | 18,891 | 26.1 | 36.5 MB | |

So a mouse move costs about **100 times less** CPU than pixelhunter's at the same window size,
and about 200 and 360 times less than voxup's and bbb_kit's (their windows are bigger and fuller,
so those two ratios aren't like for like). Idle is under the 20 ms per 60 s budget.

`gytest tiles` (offscreen, 420x250, 28 tiles), dirty tiles per frame through a scripted sequence:
28 (first frame), **0** (nothing changed), **4** (hover over a button), 4, 4, 4, 5, 9, 8, 12.
Every frame equals a full redraw byte for byte.

How: `gy_frame` blocks in `MsgWaitForMultipleObjectsEx` with no timeout unless `gy_animate` asked
for one. An input event gets two UI passes. Each instance's 48 bytes plus its clip are hashed into
the 64x64 tiles it covers. Only tiles whose hash changed are cleared, drawn again (clipped to the
tile) and `BitBlt`ed to the window.

`guicy.o` is now 42,700 B in all (27,816 B of code).

Not proven yet: "a caret blink costs 2 frames a second". There is no caret until the edit box
(step 4). `gy_animate` is in, and its proof moves to step 4 with the caret.

## Step 4: the rest of v1, layers, tiling (2026-11-22)

| Thing | Number |
|---|---|
| Tests | **28 of 28 pass**, each drawn with the scalar, SSE2 and AVX2 kernels (byte-identical), each equal to its golden. Also pass with `GUICY_USE_CRT`. |
| `guicy.o` (-O2): text / data / bss | 78,520 / 7,623 / 73,284. In the exe that is about **86 KB** of code and data (bss takes no file space); the budget is 120 KB, and Nuklear plus its backend is about 255 KB. |
| What `guicy.o` needs | user32, gdi32 and kernel32 only. `memcpy` is the one other symbol: clang emits it for struct copies, every C runtime has it, and `GUICY_NO_CRT` defines it (that build has no undefined C symbols). |
| `guicy.h` | 2,974 lines |
| Demo window, hidden desktop, 3 runs | idle **15.6, 15.6, 62.5** ms CPU per 60 s (the 62.5 is a single run; the re-runs were one 15.6 ms tick); per mouse move **0.085, 0.28, 0.31 ms** (other sessions were busy on the PC). pixelhunter: 781 and 7.2. |
| Caret blink (`v1_caret`, test clock) | a window that only wakes when asked runs **4 frames in 2.12 s** (530 ms on, 530 ms off) |
| Tooltip (`v1_tip`) | nothing at 450 ms, shown at 510 ms |

What the scripted tests prove (all hidden, offscreen):
- `v1_slider`: a drag past the left end gives the minimum; one wheel notch over it gives 1/20 of the range.
- `v1_edit`: typing; Ctrl+Left; backspace; undo; Ctrl+A, cut, paste (GUICy's own clipboard when there is no window); two-byte characters typed and deleted whole; Enter reported; a click elsewhere takes the keys away.
- `v1_scroll`: the wheel outside the list does nothing, over it the list scrolls.
- `v1_split`: the divider drags, and a double-click puts it back to its default.
- `v1_keys`: `gy_button_key` fires on its key, the app gets its own D key, and while an edit box is focused neither happens.
- `v1_combo_pick`: a pick changes the value, a click outside closes the list without changing it.
- **`v1_popup_trap`** (Master Control's case from pixelhunter): open and close a combo (by Esc, by a click outside, by a pick), a tooltip, a context menu (by a click outside, and by an item), and the app's own popup (by Esc). Afterwards typing into an edit box arrives ("ok"), and the app's own key arrives.
- `tile_layout`: four tiles fill the window, the focused one outlined.
- `tile_keys`: Alt+Right moves the focus, Alt+Shift+Left swaps, Alt+F flips, Alt+Plus grows; the app's D key still arrives, and no tile key reaches the app.
- `tile_zoom`: Alt+Z fills the window with one tile.
- `tile_stack`: dragging Log's title onto the middle of Picker stacks them as tabs.
- `tile_restore`: the saved line loads in a new context and saves back the same. A tile the app no longer has is dropped.
- `tile_online`: a tile the app opens by itself joins the layout without taking the keys.
- `tile_clash`: with the app owning Alt+Left, that tile key is turned off, a gold banner says so in the window, the app gets its Alt+Left, and `focus_left=Ctrl+Alt+Left` rebinds it.
- `tile_small`: in a 330x240 window new tiles become tabs instead of splits that would be under the minimum tile size (160x110).

## Step 5: the kernel race, hand asm against C intrinsics (2026-11-22)

`bench/gybench.c` (build: `build.cmd`, run: `build\gybench.exe`). Each asm kernel is checked for the
same bytes as the C one on the same random input, at 6 sizes and 8 start offsets, and timed
(median of 31 runs of about 1 ms, one core, buffer reset subtracted). The rule: asm stays only if
bit-exact and at least 5 % faster at 1920 px.

CPU: Intel(R) Core(TM) i9-10980XE CPU @ 3.00GHz

| Kernel | asm version | pixels | C (intrinsics) ns/px | asm ns/px | asm vs C | bit-exact | verdict |
|---|---|---|---|---|---|---|---|
| fill, opaque | rep stosd | 16 | 0.397 | 0.477 | +20 % | yes |  |
| fill, opaque | rep stosd | 128 | 0.083 | 0.088 | +6 % | yes |  |
| fill, opaque | rep stosd | 256 | 0.062 | 0.060 | -3 % | yes |  |
| fill, opaque | rep stosd | 512 | 0.066 | 0.030 | -55 % | yes |  |
| fill, opaque | rep stosd | 1024 | 0.056 | 0.019 | -67 % | yes |  |
| fill, opaque | rep stosd | 1920 | 0.054 | 0.017 | -68 % | yes | **asm kept** |
| fill, opaque | AVX2 asm, 4 stores a loop | 16 | 0.392 | 0.418 | +7 % | yes |  |
| fill, opaque | AVX2 asm, 4 stores a loop | 128 | 0.083 | 0.076 | -9 % | yes |  |
| fill, opaque | AVX2 asm, 4 stores a loop | 256 | 0.062 | 0.063 | +1 % | yes |  |
| fill, opaque | AVX2 asm, 4 stores a loop | 512 | 0.066 | 0.066 | +0 % | yes |  |
| fill, opaque | AVX2 asm, 4 stores a loop | 1024 | 0.057 | 0.057 | -0 % | yes |  |
| fill, opaque | AVX2 asm, 4 stores a loop | 1920 | 0.055 | 0.054 | -0 % | yes | C kept, asm dropped |
| fill, blended | AVX2 asm | 16 | 0.562 | 0.595 | +6 % | yes |  |
| fill, blended | AVX2 asm | 128 | 0.263 | 0.276 | +5 % | yes |  |
| fill, blended | AVX2 asm | 256 | 0.236 | 0.246 | +4 % | yes |  |
| fill, blended | AVX2 asm | 512 | 0.226 | 0.241 | +7 % | yes |  |
| fill, blended | AVX2 asm | 1024 | 0.216 | 0.226 | +5 % | yes |  |
| fill, blended | AVX2 asm | 1920 | 0.215 | 0.223 | +4 % | yes | C kept, asm dropped |
| row blend | AVX2 asm | 16 | 0.430 | 0.539 | +25 % | yes |  |
| row blend | AVX2 asm | 128 | 0.338 | 0.390 | +15 % | yes |  |
| row blend | AVX2 asm | 256 | 0.323 | 0.374 | +16 % | yes |  |
| row blend | AVX2 asm | 512 | 0.319 | 0.375 | +17 % | yes |  |
| row blend | AVX2 asm | 1024 | 0.324 | 0.355 | +10 % | yes |  |
| row blend | AVX2 asm | 1920 | 0.317 | 0.350 | +11 % | yes | C kept, asm dropped |
| glyph blend | AVX2 asm | 16 | 0.661 | 0.751 | +14 % | yes |  |
| glyph blend | AVX2 asm | 128 | 0.607 | 0.640 | +6 % | yes |  |
| glyph blend | AVX2 asm | 256 | 0.620 | 0.637 | +3 % | yes |  |
| glyph blend | AVX2 asm | 512 | 0.614 | 0.634 | +3 % | yes |  |
| glyph blend | AVX2 asm | 1024 | 0.601 | 0.621 | +3 % | yes |  |
| glyph blend | AVX2 asm | 1920 | 0.599 | 0.613 | +2 % | yes | C kept, asm dropped |


**Result:**
- **Kept: `rep stosd` for opaque fills of 512 pixels or more**, 55-68 % faster than the AVX2 store
  loop on this CPU (fast strings). It's in `guicy.h` as `gy__asm_stosd`, used by the SSE2 and AVX2
  fill kernels when the run is long enough. `GUICY_NO_ASM` turns it off. It's used when a whole
  frame is cleared and for wide plain rectangles. It isn't used when a single 64-pixel tile is
  cleared, which is below the 512-pixel threshold.
- **Dropped:** the hand AVX2 fill loop (no faster), blended fill (4 % slower), row blend (11 % slower)
  and glyph blend (2 % slower). They stay in `bench/gybench.c` so the race can be run again on
  another CPU (an AMD Zen may differ), but they're not in the library.
- **Found by the race:** the first run had the glyph asm **70 % faster**. The cause was the C side:
  its 8-byte coverage load went through GUICy's own byte-by-byte copy helper. A plain unaligned
  load made the C glyph kernel **3.4 times faster** (2.04 to 0.60 ns per pixel), and then the asm
  no longer won. All 28 tests still pass with the same bytes on all three kernel sets.

`guicy.o` after step 5: 78,600 / 7,599 / 73,284 (text / data / bss).

## F1: fix what the stress tests found (PLAN-v2, 2026-11-23)

All pixel changes are proven byte-equal by the 33 gytest goldens on all three kernel sets, except
the tile focus outline (item 6), which was re-baselined once after a side-by-side look (at most
22 levels on anti-aliased corner pixels). gystress at 1920x1080, AVX2, median:

| Case | Before F1 | After F1 | F1 budget |
|---|---|---|---|
| hover over one button, 1080p | 0.354 ms | **0.046 ms** | 0.15 |
| full redraw of the typical screen, 1080p | 4.17 ms | **0.63 ms** | 2 |
| one rounded box with a border (92x24, r 4) | 30.0 us | **2.3 us** | - |
| 1,000 visible buttons, full redraw | 24.9 ms | **2.87 ms** | 4 |
| 100,000-line list, unchanged frame, culling only | 83 ms | **3.64 ms** | 20 |
| the same list with `gy_scroll_rows` | - | draws exactly what a 100-line list draws (test `f1_cull`) | 0.5 |
| 4-tile focus move | 135 tiles, 2.2 ms | **51 tiles, 0.18 ms** | 0.4 |
| a 1,040 px line, 2 px wide | 1.45 ms | **0.11 ms** | 0.02 (miss) |
| 512 -> 1024 image, bilinear | 19.2 ns/px | **6.4 ns/px** | 3 (miss) |
| 512 -> 1024 image, nearest | 2.6 ns/px | **0.8 ns/px** | 1 |
| startup to the first frame | 6.95 ms | **3.8 ms** | 30 |
| a glyph miss | 34 us | 34 us | 20 (miss) |

What changed:
1. **Boxes:** per-pixel work only in the corner columns, the straight span one fill, the inside of
   middle rows one fill, and a corner cache per (radius, border). Test `f1_corner_cache` checks it
   against working out every pixel on 400 random boxes in quarter-pixel radii and borders.
2. **Culling:** an instance or a line of text outside the clip isn't kept. The renderer sorts by
   layer once and walks the list once (it was 3 times). Over half the tiles dirty means one plain
   full redraw.
3. `gy_scroll_rows(n, &first, &last)`.
4. **Lines:** each row (or each column, for flat lines) only checks the span the line can reach.
5. **Images:** source columns and weights worked out once per draw; bilinear does two channels
   per multiply.
6. **Tile focus outline:** drawn on its own.
7. **Small:** SelectObject only on a font change; SSE2 for glyph rows under 16 px.
8. **Frozen layouts:** `tests/golden/layouts-2026-11-22.txt` holds 5 saved layout lines with their
   geometry, and test `old_layouts` checks every later step against it.
9. **check_px.py:** 50 unscaled pixel sizes in the widget code (report only; D1 brings it to 0).

Misses, said plainly:
- **The line** is 13 times faster but over budget: the exact per-pixel division is kept for the
  same bytes.
- **Bilinear** is 3 times faster but over budget: SIMD lerp is the next step there if an app
  needs it.
- **The glyph miss** is GDI's own time. A single 1 MB-buffer GetGlyphOutline call was tried: GDI
  was then 11 times slower and gave other pixels, so it was dropped.

Size: `guicy.o` is now 87,728 / 7,958 / 75,520 (text / data / bss), about **95.7 KB** in an exe.
That's +9.6 KB for F1 against the +4 KB estimate, leaving 24 KB of the 120 KB core.

Demo window, hidden desktop, 3 runs: idle 0, 15.6 and 31.2 ms per 60 s (0 to 2 scheduler ticks);
per mouse move 0.16, 0.33 and 0.23 ms (the PC was busy).

## H1: an app's own window, the test desktop, the test font (PLAN-v2, 2026-11-23)

| Thing | Number |
|---|---|
| New API | `gy_attach(hwnd)`, `gy_event(hwnd, msg, wp, lp)` (1 = taken), `gy_wants_frame()`; the app runs `gy_begin`..`gy_end` itself |
| Tests | **37/37** normal (`gytest`) and **2/2** windowed (`gytest --windowed`, relaunched on its own desktop) |
| `w_attach` | an app-made window driven through `gy_event`: the same instance list and the same pixels as off screen, through hovers and a click |
| `w_open` | `gy_open` + `gy_frame` driven by posted messages: 14 frames run, the same instance list and pixels as off screen |
| `h1_guard` | in a test build, `gy_open` and `gy_attach` refuse on the Default desktop, with a plain error |
| `h1_texts` | the text-record hook lists 26 texts of the v1 scene with their colour and the colour under them (Build on the button colour, Name on the bg) |
| `h1_test_font` | the synthetic font, through the OS table: at 16/32/48 px, widths 24/48/72, lines 16/32/48, glyph boxes exactly 2x |
| Examples | `examples/hello.c` and `examples/fifteen.c` build from a bare folder with clang **and MinGW gcc 16.2**, both at `-Werror` (`build.cmd examples`); hello.exe 157 KB with clang, 166 KB with gcc (all of GUICy + the static C runtime; the plan's optional `GUICY_SMALL` isn't built) |
| Size | `guicy.o` 89,712 + 8,082 = **97.8 kB** (+2.1 against the +2.5 budget) |

Found and fixed while making the windowed tests honest:
- **Clicks use their own position.** A click now takes its position from the button message itself.
- **A late mouse-leave waits for the frame.** A mouse-leave that arrives while a click is being
  handled now waits for the end of the frame. Before, a fast click and a quick leave could lose the
  click. The hidden desktop showed it: Windows queues the leave after the button messages.
- **No more made-up end pointers.** gcc found `s + 0x7FFFFFFF`, which is undefined behaviour in C.
  Text functions now take NULL for "to the end".
- **gcc gets its own no-builtin attribute** (`optimize("no-tree-loop-distribute-patterns")`).

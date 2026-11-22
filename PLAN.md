# GUICy: the plan

Draft, 2026-11-22. Sources: research/cpu.md and research/gpu.md. Binding rules: the "Always"
and "Desktop apps" sections of master-control-program/AGENT_RULES.md.

---

## 1. What GUICy is

GUICy is an immediate-mode GUI library in plain C. You call `gy_button("Save")` every frame and
it returns 1 on the frame it was clicked. There are no widget objects and no callbacks, and the
state lives in the app's own variables.

- **Who uses it first:** three Windows tools of the user's: bbb_kit (X:\bases), pixelhunter
  (X:\pixelhunter) and voxup (X:\voxup). Later, any C program that wants a window with
  controls in it.
- **What it replaces:** Nuklear with its GDI and GDI+ backends. Those apps use about 70 Nuklear
  calls plus their own helpers (nk_ext_row, nk_ext_wrap_label, nk_ext_splitter, nk_ext_tooltip,
  nk_chips, nk_section, nk_dash_line, nk_preview canvas drawing, the nk_theme colour table).
- **How it relates to Nuklear:** same idea (immediate mode, one header, C), different everything
  else. GUICy has its own API with the `gy_` prefix and no Nuklear names. There is no ctx
  argument, rows don't need their column count and height up front, IDs come from an ID stack,
  the theme is a small flat table, splitters and grids are built in, and the renderer redraws
  only what changed (on the GPU when there is one). Nuklear stays in each app until that app's
  GUICy port is signed off by the user.

## 2. Goals, non-goals, anti-goals

**Goals** (the user's words: "fastest ever, smallest ever, simplest usage ever, nothing outside
itself, easy to import into an EXE, CPU and/or GPU"):
- One header, `guicy.h`. Add it to an exe with one `#define GUICY_IMPLEMENTATION`. Nothing to
  install, no DLL of our own, only what Windows ships (user32, gdi32, kernel32, imm32,
  d3d11/dxgi and dwrite loaded at run time).
- 0% CPU and GPU when idle. Work done per frame scales with what changed, not with the window size.
- A GPU path (D3D11) when the machine has a real GPU, and our own CPU path otherwise. Both draw the
  same list, so the app never knows which one ran.
- Every widget the three apps need in v1, then every common widget type (section 4).
- The desktop-app rules built in, not left to each app: draggable dividers that report double-click
  for reset, grids that follow the width, rows that size to their panel, vertical-only scrolling by
  default, a minimum window size, a one-key shortcut on any button, and errors shown in the window.
- Every speed or size claim has a number from a repeatable bench beside it.

**Non-goals (for now):** arbitrary vector art (SVG, path editing), 3D-scaled or rotated text,
complex-script shaping beyond what the OS gives for free, D3D12, Vulkan, Metal, mobile, a
retained widget tree, a layout constraint solver, and a GUI builder.

**Anti-goals (things GUICy must never do):**
- A speed claim with no benchmark, or a "faster than X" without X measured on the same machine.
- Hand-written assembly that is kept without a bench showing it wins (section 3.10).
- Render at 60 Hz while nothing changes.
- Silently drop draw commands when a buffer fills (Nuklear does). It grows, or it reports.
- Show a message box, take focus, or open a console. Errors are text in the app's window
  (path plus reason) and a line in the log.
- Red text in any default theme.
- Depend on a runtime compiler, an installer, d3dcompiler_47.dll or vulkan-1.dll.
- Hide control flow in macros (no `for`-loop scope macros like Clay's).

## 3. Architecture

Each choice has a one-line reason. Numbers in brackets point at the research.

### 3.1 Shape
- **One stb-style header** `guicy.h`: the public API (about 300 lines) on top, everything else
  under `#ifdef GUICY_IMPLEMENTATION`. Reason: "easy to import to an EXE" means one file. [cpu 6.1]
- **No libc calls inside the library** (own memset/memcpy/fmt/tiny math). Reason: works in apps
  with or without the CRT, and keeps the size honest. The apps keep their CRT. [cpu 6.3]
- **One VirtualAlloc at init**, sliced into the frame arena, the box cache, atlas pages and tiles.
  The arena grows by doubling and never drops a command. Reason: no malloc, no silent loss. [cpu 6.3]
- **Options are `#define`s** before the include: `GUICY_NO_GPU`, `GUICY_NO_THREADS`,
  `GUICY_NO_DWRITE`. Reason: the smallest build drops what it doesn't use.

### 3.2 Context and IDs
- **One implicit global context** (`gy_ctx_set()` for a second window). Reason: removes the ctx
  token from every call; these apps have one UI thread. [cpu 4.2]
- **ID = hash(label, top of the ID stack)**, `"Save##2"` hides the suffix, `gy_id_push(i)` for
  loops. Reason: no state shifting when a widget hides, unlike Nuklear. [cpu 4.1]
- **Hot / active / focus IDs** (Muratori), with hit tests on last frame's rects. Reason: the
  classic IMGUI model; the one-frame lag is hidden by the second settle frame. [cpu 4.3]
- **One generic box with feature flags** (CLICK, DRAG, BG, BORDER, TEXT, CLIP, SCROLL). Widgets
  are flag sets plus a few lines. Reason: Fleury's "the widget is a lie" keeps the widget code
  small. [cpu 4.1, 4.4]

### 3.3 Layout
- **Row cursor by default:** `gy_row(n, widths)` with px, fraction or -1 = grow; no args = one
  auto row. Height defaults to the theme's row height, and wrapped text grows its row. Reason:
  covers the forms (80% of the apps) with no lag.
- **Deferred box tree** for splits, toolbars and auto-sized rows: measure bottom-up, grow and
  shrink top-down, then place (Clay/Fleury). Reason: content-sized rows and splitters without
  Nuklear's "declare first" pain. [cpu 4.3]
- **`gy_grid(cell_w)`** picks the column count from the panel width. Reason: the "grids follow the
  width" rule, which voxup and pixelhunter now compute by hand.
- **Popups, combos, tooltips and windows are layers** with their own root and z-order. Reason:
  hover goes to the top layer, as in Dear ImGui. [cpu 4.3]

### 3.4 The draw list (shared by both renderers)
- **One fixed 48-byte instance, `gy_inst`**, for every drawn thing: rect, rounded rect, border,
  circle, cheap shadow, glyph, icon, image. Fields: rect, atlas uv, two colours, radius, border
  width, softness, clip index, kind. Reason: the GPU draws it in one instanced call, the CPU walks
  the same array, and a fixed size makes hashing and binning trivial. [gpu 2.1]
- **Clip rects in a side table, referenced by index.** Reason: no scissor changes, so nested
  scroll groups don't split the GPU batch, and the CPU tile clip is one compare. [gpu 2.2]
- **Paths (checkmarks, arrows, dashed lines, polylines, the canvas API) are flattened to glyph-like
  coverage tiles in an icon atlas,** then drawn as instances. Reason: keeps one instance type; the
  small coverage-accumulation filler (about 350 lines) is shared with the fallback font. [cpu 1.3]
- **Premultiplied BGRA8, colours in sRGB space, no linear blending.** Reason: matches what GDI
  shows today, so CPU and GPU pictures and the old Nuklear pictures line up. [gpu 2.3, cpu 2.1]
- **All rect edges snapped to whole pixels on the CPU.** Reason: crisp 1 px borders on both
  renderers; AA only on curves. [gpu 2.2]

### 3.5 CPU renderer (always present)
- **Primitive kernels, not a general path renderer:** FILL, FILL_COV, GLYPH, IMAGE, IMAGE_SCALED
  (nearest and bilinear), plus SDF only in rounded corners and AA edges. Reason: most UI pixels
  become memset-speed stores. [cpu 1.3, 2.5]
- **SSE2 baseline plus AVX2 kernels picked once by cpuid/xgetbv.** No AVX-512, no JIT. Reason:
  2-4x over scalar on the blend loops, no writable code pages for ASR to dislike. [cpu 2.4]
- **Exact rounded div-255** everywhere. Reason: stacked hover overlays and shadows don't drift. [cpu 2.2]
- **Present with a DIB section plus `BitBlt` per dirty rect.** Reason: present cost follows the
  change, not the window. [cpu 3.4]

### 3.6 GPU renderer (when there is a real GPU)
- **D3D11, feature level 11.0, StructuredBuffer of `gy_inst`, `DrawInstanced(4, n)`**, one draw
  per frame (a few more around big user images). Reason: about 225 lines of C, on every GPU since
  about 2010. [gpu 2.1, 4.2]
- **One SDF shader pair, compiled offline with `fxc /Fh` and checked in as byte arrays** (about
  2 KB). fxc comes from the Windows SDK already on this PC; a build without it uses the checked-in
  headers. Reason: no d3dcompiler DLL, no 10-50 ms compile at start. [gpu 4.1]
- **Flip-discard swap chain, 2 buffers, frame-latency waitable at 1, vsync on.** Reason: one frame
  less lag and no DWM copy. [gpu 3.2, 3.3]
- **No `WS_EX_NOREDIRECTIONBITMAP`.** Reason: GDI must still be able to draw into the same window
  when we fall back. [gpu 5]
- **Image pages in a texture array; very large images get their own texture and draw.** Reason:
  pixelhunter and voxup previews must not force atlas churn. [gpu 2.5]
- OpenGL 3.3 is the later Linux backend only (section 5, step 13). Reason: on Windows it adds
  nothing over D3D11 plus our CPU path. [gpu 4.3]

### 3.7 GPU detection and fallback
1. `LoadLibrary("d3d11.dll")` and `GetProcAddress` (never linked). Reason: the exe starts even
   where D3D is broken.
2. Hardware device; if the adapter is "Microsoft Basic Render Driver" or flagged software, treat as
   no GPU. Under RDP (`SM_REMOTESESSION`) prefer the CPU path. Reason: GDI dirty rects beat a
   software D3D there. [gpu 5]
3. Otherwise the CPU renderer. WARP only when asked (`GUICY_RENDER=warp`) for testing the D3D
   path without a GPU.
4. On `DEVICE_REMOVED`/`RESET` mid-run: retry once, else switch to CPU in the same frame. The app is
   not involved, since the draw list is backend-neutral.
5. `GUICY_RENDER=cpu|gpu|warp` forces a path, so both are testable on this machine.
The path in use and why is one line in the log and readable with `gy_renderer_name()`.

### 3.8 Text
- **v1: GDI glyphs** (`GetGlyphOutlineW`, `GGO_GRAY8_BITMAP`) from the system fonts the apps use
  today. Reason: plain C, and the same rasterizer Nuklear's GDI backend uses, so old/new picture
  diffs stay small.
- **v1.1: DirectWrite** (`IDWriteGlyphRunAnalysis`) as the better-looking default once picture
  diffs are stable, plus font fallback for CJK and emoji in pixelhunter's file names. The MinGW
  `dwrite.h` question is answered in 7.
- **Glyph atlas:** 1024x1024 A8 pages, shelf packer, key = (font, size, glyph, subpixel x phase
  0..3), LRU. Rasterize only on a miss. ASCII fast table for id and advance. Reason: steady-state
  text costs zero rasterizing on both renderers (refterm). [gpu 2.4, cpu 5.2]
- **Grayscale AA with a contrast/gamma LUT** baked into the atlas; ClearType later as an option.
  Reason: ClearType breaks on non-opaque backgrounds and triples the atlas. [cpu 5.4]
- **Width cache** (string hash, font) to width, so measuring labels each frame is a lookup.
- **UTF-8 throughout,** invalid bytes become U+FFFD; non-ASCII runs go through the OS shaper
  (Uniscribe now, DirectWrite later).
- **Text edit: our own, about 400 lines:** UTF-8 byte cursor and mark, word moves, undo ring,
  clipboard, IME position, caret blink through the animation timer. [cpu 5.5]

### 3.9 Redraw only what changed, sleep otherwise
- **Event loop:** `MsgWaitForMultipleObjectsEx` with an infinite timeout when nothing animates.
  One input event = two UI passes (the second settles hover), then render. `gy_animate(ms)` asks
  for a later frame (caret blink = 2 frames a second, not 60). `gy_wake()` from worker threads
  (voxup's ComfyUI jobs, pixelhunter downloads). Reason: this alone takes idle CPU to about 0. [cpu 3.1]
- **Dirty tiles, 64x64:** each instance is hashed once and its hash mixed into every tile it
  covers. A tile whose hash matches last frame's is skipped. CPU: only changed tiles are
  rasterized and blitted. GPU v1: if no tile changed, skip upload, draw and Present entirely;
  `Present1` partial presents are a later step. Reason: a hover touches 1-5% of the window. [cpu 3.2, gpu 3.4]
- Images enter the hash by (id, generation), never by pointer. Resize, DPI and theme changes
  invalidate everything.
- Minimized or occluded windows present nothing.

### 3.10 Assembly, honestly
The CPU research found that clang's intrinsics output matches hand-written assembly on these
blend loops, so GUICy does not assume asm is faster. Assembly enters in three measurable ways:
1. **Glue that has to be asm anyway:** `xgetbv`, a `___chkstk_ms` stub for CRT-free builds,
   `rep stosd`/`rep movsb` fill and copy (fast on ERMSB/FSRM CPUs).
2. **The kernel race (`gybench`):** every hot kernel (FILL, FILL_COV, GLYPH, IMAGE, IMAGE_SCALED,
   the corner SDF, the tile hash) has its intrinsics version and may have a hand-asm version
   (clang's integrated assembler, `.S` files, no other tool). The bench checks both give the
   same bytes on fixed inputs, times them on several sizes (median of many runs, hidden, one core),
   and writes a table. An asm version is kept **only if it is bit-exact and at least 5% faster on
   the median on this PC**; otherwise it is deleted from the build (kept in git history). The
   table goes in BENCH.md with the CPU name.
3. **Runtime pick:** where both versions survive and the winner differs by CPU (for example AVX2
   versus SSE2 machines), the dispatch table picks per CPU at start, as it does for ISA levels.
If no asm wins, the plan says so in BENCH.md and GUICy ships pure C. That is still an honest
result.

### 3.11 Threading
- **The UI runs on one thread.** Widgets are called from the app's UI thread only. Reason: the
  apps work that way; no locks in the core.
- **Worker threads talk to the UI with `gy_wake()`** (thread-safe, posts a message).
- **CPU tile pool:** `cores - 1` persistent workers, an atomic counter over dirty tiles, used only
  when more than about 16 tiles are dirty (resize, scroll, first frame). The glyph atlas is filled
  before workers start, so they only read. Reason: near-linear scaling for full redraws, and none
  of the wake cost for a one-tile hover. Added in its own step, only if the full-redraw number
  misses its budget. [cpu 3.3]

### 3.12 Tiling (the user's ask, 2026-11-22)
An app's window works like a tiling window manager (i3/bspwm style) inside itself. Each part of a
tool comes online in its own tile, and the tiles always fill the window between them: no
overlaps and no floating windows to hunt for.
- **The app's side is one call per tile:** `if (gy_tile("Preview")) { ...widgets... }`. The app
  never computes a size. GUICy keeps the tiles in a binary split tree (each node is horizontal or
  vertical, with a ratio), and tabs where tiles are stacked.
- **New tiles:** opening one splits the focused tile; the longer side is split, and the other
  tiles shrink to make room. The app can open a tile itself when something comes online (a
  finished job, a download) with `gy_tile_open("Log")`. *Our call:* a tile the app opens never
  takes focus or the keyboard from the tile the user is in. `gy_tile_close`, `gy_tile_focus`.
- **Keys, one each, rebindable:** focus left/right/up/down, swap with a neighbour, grow and shrink,
  flip a split between side-by-side and stacked, fill the window and back (zoom), close, and cycle
  the tabs of a stack. **All tile keys use one modifier** (Alt by default), so they never clash with
  the apps' own keys (bbb_kit D/L/T/F/Enter/Esc; voxup F1-F12, Space, R, M, digits, Ctrl+Z,
  Ctrl+[ ]; pixelhunter's own). The app passes a table of the keys it owns
  (`gy_keys_owned(...)`), and at start GUICy refuses any tile binding that clashes, showing the
  clash as text in the window and logging it. The bindings are rebindable in the app's ini.
  *Proof:* a golden test where an app key still reaches the app while tiling is on.
- **Mouse:** drag a divider to resize, double-click it to reset, drag a tile's title bar onto
  another tile to swap the two or to stack them as tabs (drop zones are shown while dragging).
- **Remembered:** the tree is saved as one text line in the app's ini and restored at start. A tile
  that is no longer in the app is dropped from the tree on load.
- **The minimum size is respected:** a split that would make a tile smaller than its minimum is
  turned into tabs instead.
- It is built on the deferred box layout (3.3) and the splitter, so it adds about 600 lines.

### 3.13 Testing built in
- `gy_offscreen(w, h)` renders into memory with no window shown; `gy_shot(path)` writes a BMP of
  the current frame; `gy_script(path)` replays a text file of input (move, click, key, wait,
  shot). Reason: every proof below runs hidden, which the rules require, and each app gets this
  for free through its existing `--selftest`.
- `gy_stats()` returns frame ms, UI-pass ms, render ms, dirty tiles, instances, glyph misses and
  the renderer in use. A `gytest` exe holds the golden pictures and the bench.

## 4. The API

### 4.1 A complete small app (15 lines)

```c
#define GUICY_IMPLEMENTATION
#include "guicy.h"

int main(void) {
    int count = 3, pixel = 1; float size = 32; char name[64] = "base"; float split = 280;
    gy_open(&(gy_desc){ .title = "Demo", .w = 900, .h = 600, .min_w = 640, .min_h = 400 });
    while (gy_frame()) {                                   /* sleeps until something happens */
        gy_split("main", GY_H, &split, 280);               /* drag; double-click resets to 280 */
            gy_text("Bases: %d", count);
            if (gy_button_key("Build", 'B')) count++;      /* the B key does it too */
            gy_check("Pixel look", &pixel); gy_slider("Size", &size, 8, 512);
            gy_edit("Name", name, sizeof name); gy_tip("Saved with the model");
        gy_pane(); gy_canvas_rect(gy_avail(), 6, gy_theme()->card);
        gy_split_end();
    }
}
```

`gy_frame()` ends the previous frame (layout, render, present), sleeps until input, a timer or
`gy_wake`, and returns 0 when the window closes. Apps with their own window and message loop use
`gy_attach(hwnd)` plus `gy_event(msg, wp, lp)` and `gy_begin()`/`gy_end()` instead.

### 4.2 Widget set, in order

**v1: everything the three apps use today**

| Group | Calls (sketch) | Replaces |
|---|---|---|
| Text | `gy_text`, `gy_text_col`, `gy_text_wrap` (row grows to fit), `gy_section` | nk_label, nk_label_colored, nk_ext_wrap_label, nk_section |
| Buttons | `gy_button`, `gy_button_key`, `gy_button_ex(&gy_opt{.accent,.icon,.w})` | nk_button_label, the accent-button style swap |
| Choices | `gy_check`, `gy_select`, `gy_chips(items, n, &sel)`, `gy_combo(&i, "a\0b\0")` | nk_checkbox_label, nk_select_label, nk_chips, nk_combo* |
| Numbers | `gy_slider`, `gy_slider_i`, `gy_number`, `gy_number_i` (drag, wheel, type) | nk_slider_int, nk_property_int/float |
| Edit | `gy_edit` (returns CHANGED / ENTER), `gy_unfocus` | nk_edit_string_zero_terminated, nk_edit_unfocus |
| Status | `gy_progress`, `gy_tip` (500 ms delay built in), `gy_spacer` | nk_progress, nk_ext_tooltip, nk_spacing |
| Images | `gy_image_make/update/free`, `gy_image(img, w, h)` | nk_image, nk_draw_image |
| Layout | `gy_row`, `gy_row_h`, `gy_grid`, `gy_same_line`, `gy_place(rect)` | nk_layout_row_*, template, static, space, nk_ext_row |
| Containers | `gy_scroll_begin/end`, `gy_tree/gy_tree_end`, `gy_panel_begin/end`, `gy_split/gy_pane/gy_split_end`, `gy_tile/gy_tile_open/close/focus` (tiling, 3.12) | nk_group_*, nk_tree_*, nk_begin/end, nk_ext_splitter |
| State | `gy_disable_begin/end`, `gy_push_col/gy_pop_col`, `gy_font(i)`, `gy_theme_set(&t)` | nk_widget_disable_*, nk_style_push/pop*, nk_style_set_font, nk_theme |
| Canvas | `gy_canvas_rect/circle/line/dash/text/image`, `gy_last_rect()`, `gy_hovered()`, `gy_clicked(btn)` | nk_fill_*, nk_stroke_*, nk_draw_text, nk_widget_bounds, nk_input_is_mouse_* |
| Platform | `gy_open/gy_attach/gy_event/gy_frame`, `gy_wake`, `gy_animate`, `gy_error(text)` | nk_gdi_*, nk_input_* |

**v2: lists and menus** - `gy_list` with a clipper (only visible rows submitted), `gy_table` (resizable,
sortable columns, widths in the cache), `gy_menubar/gy_menu/gy_menu_item` with keyboard navigation,
`gy_tabs`, `gy_context_menu` (right-click), `gy_radio`, `gy_toggle`.

**v3: pickers** - `gy_color` (SV square, hue bar, number boxes), `gy_date` (7x6 grid popup),
`gy_file_open/save` (the OS dialog, only on a click by the user, never in tests), `gy_font_pick`.

**v4: windows** - movable and resizable inner windows, modal dialogs drawn inside the app (no OS
message boxes). Docking is covered by the v1 tiling (3.12).

**Later:** charts (line, bar), a zoomable canvas with SDF or Slug text, multi-line rich text,
drag and drop between widgets.

## 5. Steps

Each step ends with one local commit and "done: <commit> <what>" to Master Control. All runs are
hidden (offscreen, no focus), one at a time, and temporary files go in X:\guicy\work.
"Pictures" means BMPs from the app's own `--selftest` or `gytest`, compared with a pixel diff tool
that reports changed-pixel count and max channel difference.

| # | Deliverable | Proof |
|---|---|---|
| 0 | **Baseline of the three Nuklear apps.** A small timing hook (local branch, not committed to the apps' main) logging UI-pass and render ms per frame; the selftest pictures saved as references in work\baseline. | A table in BENCH.md per app: exe KB, startup to first frame ms, median and 95th-percentile frame ms (idle, hover, scroll, full resize), idle CPU % over 60 s (process CPU time delta), working set MB, and Nuklear's share of the exe KB. Reference pictures for each selftest scene. |
| 1 | **Skeleton:** guicy.h layout, build.cmd (clang, `-Wall -Wextra -Werror`), `gytest` host, offscreen target, `gy_shot`, the dwrite.h probe (section 7). | Builds clean; `gytest` imports only OS DLLs (read from the import table); an empty GUICy exe size in BENCH.md; a blank offscreen BMP matches its golden byte for byte. |
| 2 | **Core plus CPU renderer:** context, IDs, input, row layout, draw list, theme, FILL/FILL_COV/GLYPH kernels (SSE2 and AVX2), GDI glyph atlas; widgets text, button, check, select. | Golden pictures for each widget in each state (normal, hover, active, disabled) match byte for byte between runs; SSE2 and AVX2 pictures are identical; a scripted click test toggles the check and the log shows it. |
| 3 | **Idle loop and dirty tiles.** | Idle CPU 0.0% over 60 s (CPU time delta under 20 ms); a hover over one button re-rasterizes at most 4 tiles; a caret blink costs 2 frames a second; frame-hash numbers in BENCH.md. |
| 4 | **The rest of v1:** slider, number, combo, edit, scroll, tree, progress, tooltip, images (nearest and bilinear), disable, colour push/pop, fonts, theme table, split, grid, canvas calls, layers, tiling (3.12). | Golden pictures per widget; scripted tests for drag (slider, split), typing and undo (edit), wheel (scroll, number), double-click reset (split), the tooltip delay, `gy_button_key`; tiling: open, close, swap, flip, zoom, stack-as-tabs and the key moves, each with a golden picture, plus the saved tree restoring the same picture after a restart. |
| 5 | **`gybench` and the kernel race** (section 3.10), run as soon as the CPU kernels exist so every app port starts on the winners. | BENCH.md table: each kernel, intrinsics versus asm, ns per pixel at 3 sizes, bit-exact yes/no, kept or dropped. No asm stays without a winning row. |
| 6 | **bbb_kit port** behind a build switch (Nuklear build kept): nk_side.c and nk_top become GUICy, the three themes become GUICy theme tables. | Old/new pictures side by side for each theme, plus a diff; the same act() calls logged for the same scripted clicks; frame ms and idle CPU against step 0; exe KB change. **The user's sign-off**, then Nuklear is removed from bbb_kit only after asking. |
| 7 | **GPU renderer:** D3D11 loaded at run time, the shader bytecode headers, image textures, frame-hash skip, fallback chain, `GUICY_RENDER`. | GPU and CPU pictures of every golden scene differ by at most 1 level per channel on AA pixels and 0 on solid ones; forced cpu, warp and a simulated device-removed run all finish their scripts; GPU-path frame CPU ms and idle numbers in BENCH.md; KB added. |
| 8 | **pixelhunter port** (one file, about 1,900 lines), same switch approach. | Old/new pictures for its selftest scenes (search grid, preview, download banner, `--select-all`), the drag test, frame and idle numbers against step 0. **The user's sign-off** before Nuklear goes. |
| 9 | **DirectWrite text** and font fallback (CJK and emoji file names). | Pictures of a mixed-script name list; glyph misses 0 after the first frame; the GDI path still selectable. |
| 10 | **voxup port** (pick.c, about 3,100 lines, plus nk_ext.c). | Old/new pictures for its `--selftest` items (`--draw`, `--keep`, `--find`, `--redraw`, `--key`), frame and idle numbers against step 0, built with its own build_repo_new.cmd into local\demo if voxup is open. **The user's sign-off** before Nuklear goes. |
| 11 | **Tile thread pool,** only if a full redraw misses the budget in section 6. | Full-window redraw ms at 1080p and 1440p, 1 thread versus N, in BENCH.md; pictures identical to single-threaded. |
| 12 | **v2 to v4 widgets** (lists, tables, menus, tabs, pickers, windows, docking), one commit per group. | Golden pictures and scripted tests per widget; a 100,000-row list scrolls at the same frame ms as a 100-row one (the clipper works). |
| 13 | **Optional later:** `Present1` partial presents, ClearType, the exact Gaussian shadow, the Linux GL 3.3 backend. | Each one only with a number that improves (DWM power or ms) or a picture the user prefers. |

## 6. Budgets

Estimates from the research; step 0 and BENCH.md replace them with measurements. A step that misses
a budget either fixes it or reports why before moving on.

| Thing | Budget |
|---|---|
| guicy.h lines, v1 (core and layout 2.5k, widgets 3k, CPU raster and kernels 2k, Win32 and text 1.5k, D3D11 0.3k plus 70 HLSL) | about 9,000; hard cap 12,000 |
| KB added to an exe, CPU only | 120 KB or less |
| KB added with the GPU path | 20 KB more or less (shader bytes about 2 KB) |
| Each app's exe after its port | smaller than its step-0 Nuklear build |
| Idle CPU, window open, nothing animating | 0.0% (under 20 ms CPU time per 60 s) |
| Hover or caret frame, CPU path, 1080p | 0.3 ms or less, at most a few tiles |
| Full-window redraw, CPU path, 1080p, one thread | 4 ms or less |
| Frame on the GPU path, CPU side (UI pass, hash, upload) | 0.5 ms or less for 5,000 glyphs and 500 rects |
| UI pass for a typical app screen | 0.2 ms or less |
| Startup to the first frame (library part) | 30 ms or less, no shader compile |
| Memory | 16 MB or less for the arena, atlases and tiles at 1440p |
| Every app frame number | no worse than its step-0 Nuklear number, at every percentile |

## 7. Risks and open questions

1. **MinGW `dwrite.h` (from cpu.md).** Answered by reading the header: the sysroot's
   `X:\tools\gcc\mingw64\x86_64-w64-mingw32\include\dwrite.h` is widl-made and has the C
   vtables (`IDWriteFactoryVtbl`, `IDWriteGlyphRunAnalysisVtbl`, about 670 Vtbl lines). Still to
   confirm in step 1 with a 20-line probe that compiles in C and resolves `DWriteCreateFactory`
   through `LoadLibrary("dwrite.dll")`. Fallback: declare the 8 methods we use ourselves.
2. **Shader compiling without fxc.** fxc lives only in the Windows SDK on C:, and X:	ools\dxc only makes
   DXIL (D3D12), not the DXBC that D3D11 needs. But Windows itself ships
   `C:\Windows\System32\d3dcompiler_47.dll` (checked 2026-11-22). A ~40-line build tool `gyshader`
   loads it with LoadLibrary, calls D3DCompile and writes the bytecode headers. That uses an OS DLL
   and installs nothing. The headers are checked in, so normal builds never need it. Open: the
   user still confirms whether reading that OS DLL at build time is fine (Master Control asks).
3. **ASR:** X:\ is ASR-excluded since the move (2026-11-22), so `gytest` and `gybench` build and run in
   X:\guicyuild.
4. **Picture diffs versus Nuklear will not be zero.** Font rasterizing, AA and rounding differ. The
   sign-off is the user's eye on side-by-side pictures, with the diff as a guide, not a pass/fail.
5. **The one-frame input lag** of the deferred layout can show as a flicker on the frame after a
   click if the settle pass is skipped. Proof in step 3 is a scripted click whose second frame matches.
6. **Per-glyph instances on the CPU path** cost more binning than the research's glyph-run command.
   If step 2 numbers say so, add a glyph-run instance kind that the GPU expands.
7. **Live resize** inside the modal size loop must render from `WM_SIZE`/`WM_PAINT`; without
   `WS_EX_NOREDIRECTIONBITMAP` the GPU path may show stretched frames while dragging. Measure; accept
   or recreate the window on fallback.
8. **Hybrid laptops and RDP:** the default adapter is used, and RDP prefers the CPU path. Untested
   here; the user's machine is a desktop with one GPU.
9. **pick.c is 3,100 lines with app logic mixed into the UI.** The voxup port may need a separate
   untangling commit first; plan that after step 8.
10. **Open:** should the GPU path be on by default in the apps, or the CPU path, given that both
    will idle at 0? Decide with step 7 numbers.
11. **Open:** does the user want GUICy public on GitHub (and under which licence) once bbb_kit, a
    public repo, ships with it? No push without the user's yes in any case.

## 8. Creative and clever

1. **Immediate UI, retained pixels.** The UI code runs every frame, but each 64x64 tile carries
   the hash of what is drawn on it, and unchanged tiles are never touched. *Proof:* dirty-tile
   count and frame ms for a hover, a caret blink and a progress tick in BENCH.md, against the
   step-0 full redraw.
2. **One 48-byte instance for everything, on both renderers.** Rects, corners, borders, shadows,
   glyphs, icons and images are one struct, drawn by one GPU call or one CPU tile walk. *Proof:*
   draw calls per frame (1 on the GPU), and CPU versus GPU pictures within 1 level per channel.
3. **The frame that is never drawn.** If the frame hash equals the last one, the GPU path skips
   upload, draw and Present, and the CPU path blits nothing. *Proof:* Present count over a 60 s
   mouse wiggle over empty space is 0.
4. **Self-racing kernels.** Intrinsics and hand asm race in `gybench`; only bit-exact winners stay,
   and the dispatch picks per CPU. *Proof:* the BENCH.md race table.
5. **The renderer swaps under the app.** A lost GPU device switches to the CPU path in the same
   frame with no app code. *Proof:* a scripted run with a forced device-removed mid-script finishes
   and its final picture matches the CPU golden.
6. **Glyphs are made once, by the OS.** Hinted system glyphs cached with 4 subpixel phases; steady
   state rasterizes nothing. *Proof:* glyph misses per frame are 0 after the first frame of every
   golden scene.
7. **The widget is a lie.** Every widget is a flag set on one box, so a new widget costs tens of
   lines. *Proof:* lines per widget listed in BENCH.md; v1 widgets average under 60 lines.
8. **Tests are part of the library.** Offscreen frames, BMP shots and scripted input come with
   `guicy.h`, so every app using it gets hidden, focus-free tests with no extra code. *Proof:* the
   three ports drive their selftests through `gy_script` and `gy_shot`.

## 9. Master Control review (2026-11-22): GO, after the X: move

1. **Scope gate.** Do steps 0-5 first (baseline, skeleton, core and CPU renderer, idle and dirty
   tiles, the rest of v1, the asm race). Then STOP and show the user the gytest pictures and
   BENCH.md before any app port (step 6). The three apps are mid-work, so a port is a big change
   they need to hear about first.
2. **Ports are coordinated through Master Control.** Each one happens on a branch while that
   app's session holds its UI changes, never while that session is editing the same file.
3. **Keep a `GUICY_USE_CRT` switch** beside the no-libc build, so a bug can be bisected against
   the CRT versions.
4. **fxc on C:.** Master Control asks the user once. The checked-in shader headers keep builds
   independent of it either way.
5. **gytest and gybench follow the single-exe and resizable rules too.** Keep guicy\work small
   (400 GB cap on X:): prune the golden pictures to what is needed, and give the work folder's
   size in each done-report.
6. **Step 0's inventory maps the apps' own helpers** (voxup's nk_ext.c splitter, wrap label and
   tooltip; pixelhunter's label_fit) to gy_ calls, so nothing they rely on is missed.

Repo: private on GitHub (Balderding/guicy) at the user's word, 2026-11-22.

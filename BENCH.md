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

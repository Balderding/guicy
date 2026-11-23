# GUICy: the next plan (v2)

Draft 2, 2026-11-23 (draft 1 2026-11-22; changes per reviews/PLAN-v2-response-r1.md). Steps 0-5
of PLAN.md are done (GUICy 0.4.0, commit e58409b, 30 of 30 tests). This plan replaces PLAN.md
steps 6-13 and PLAN-tiling.md, and folds in bench/stress.md, bench/compare.md and
research/tiling-locks-gpu.md. AGENT_RULES.md ("Always", "Desktop apps") applies to every step.
Section 8 maps every ask to a step.

---

## 1. Where GUICy stands, honestly

Today, CPU renderer only:
- **Idle:** 0-15.6 ms CPU a minute (one tick). Only GUICy and Win32 controls do this. Libraries
  that draw every frame (ImGui, Nuklear D3D11, raygui) burn 1.3-6 s a minute; ImGui's
  wait-for-input loop gets that down to 0.23 s.
- **A mouse move:** about 0.1 ms (Nuklear GDI 4.2 ms, the three apps 7-26 ms).
- **Start:** about 65 ms to a window (ImGui 0.36 s, raygui 0.5 s).
- **Two budget misses** (stress.md): 1080p hover 0.34 ms (budget 0.3), 1080p full redraw 4.2 ms
  (budget 4), both from the rounded-box rasterizer. Also slow: no culling, lines, bilinear
  images, tile focus moves.
- **Win32 controls tie GUICy** on CPU per move (0.05 against 0.1 ms) and beat it on size (47 KB
  against the 142 KB demo exe). GUICy can't sell itself on "faster than native" for a form.

What GUICy offers beyond Win32 controls: (1) much less app code: one call per widget, state in
the app's variables, no handles, WM_COMMAND or dialog templates; (2) one look in every app from
one theme table, the same at any DPI; (3) a tiling window manager inside the app, with developer
locks; (4) hidden tests for free: offscreen frames, shots, scripted input; (5) the GPU where it
wins, with the CPU path always there; (6) canvas, images and custom widgets in tens of lines.

Claims stay measured: "fastest" and "smallest" are only said against libraries measured on this
PC. Until microui joins compare.md (M1), the size claim is only "smaller than Nuklear, ImGui and
raygui". compare.md is re-run at M1, M4 and M5.

## 2. The order, and why

```
F1 fixes -> H1 host + function table -> G1 GPU (M1) -> D1 DPI -> L1 locks (M2) -> G2 containers
-> T1 mouse -> X1 text + IME (M3) -> T2 T3 T4 T5 T6 -> P1 panels (M4) -> W1..W5 widgets (M5)
-> W6 second OS window.   Gated: C1 thread pool, A1-A3 app ports.
```

Master Control's order (G1, L1, G2 and T1, then T2-T6 and P1) is kept among those steps. Four
insertions need Master Control's OK (question 1), and **F1 starts only after that answer**:
- **F1 and H1 before G1:** G1 races the GPU against the CPU path in every case, so the CPU path
  must be fixed first, and G1's child window, D1's DPI message and X1's IME must work for apps
  that own their window (`gy_attach`), which H1 adds.
- **D1 before L1:** it changes tile minimums, title bars, dividers and the size-lock unit.
- **X1 between T1 and T2:** the widget steps then start with real text input.
- The G2 tab strip carries the » overflow menu early; W2 and P1 reuse it.

## 3. Rules for every step

- **One local commit per step**, then "done: <commit> <what>" to Master Control with the test
  count, the FAIL grep over the whole output, the exit code, the size table and the work folder
  size.
- **All runs hidden:** gytest and gystress offscreen, baseline.py on its own hidden desktop. No
  window on the user's screen, no focus, no `start`, one run at a time. GPU and WARP runs are
  short batches (see G1).
- **Never touch system settings.** OS calls GUICy reads (imm32, `SystemParametersInfo`, the DPI
  functions, d3d11/dxgi creation) go through one function table (H1), and tests swap in stubs.
- **Goldens:** every golden stays byte-equal unless the step says it changes pictures. A planned
  change shows old and new PNG side by side with the changed-pixel count and the largest channel
  difference, and the new hash is committed with a reason.
- **Old layouts keep loading:** at the start of F1 the 0333d67 build saves the layout line of
  every tile_* test, plus a 6-tile line with a stack and a zoom, into
  `tests/golden/layouts-2026-11-22.txt`, and beside each line a **geometry file**: every tile's
  name and rect, stack membership, active tab, focus and zoom. Test `old_layouts` loads each line
  at every step and the geometry must be equal; it is never re-baselined (tile rects are integer
  maths, kept through G2). A picture check sits beside it and follows the golden rule above.
- **Size table** in BENCH.md every step, -O2 and -Os:
  (i) `guicy.o` text+data, no gc, with the W groups switched off: the honest upper bound;
  (ii) `core.exe`, built with `GUICY_STATIC` (every GUICy function `static`, so the compiler drops
  what is unused with no linker flags), calling at least one function of every default feature
  (a tile, a lock, a dock, an edit box with IME, DPI);
  (iii) `hello.exe`, one button, with `GUICY_STATIC`, default and with `GUICY_SMALL`;
  (iv) each switch on and off: `GUICY_NO_TILING`, `GUICY_TILING_FULL`, `GUICY_NO_GPU`,
  `GUICY_NO_ANIM`, `GUICY_NO_IME`, `GUICY_NO_DWRITE`, and each W group.
  GUICy's share of an exe is read from the lld map file (a measuring build with
  `-ffunction-sections`, summing the `gy_`/`gy__` sections).
- **Budgets:** the **core** (default build: v1 widgets, containers, locks, DPI, IME, fallback
  glyphs, docks, CPU path) is **120 KB or less** in (i) and in (ii). The GPU path adds **20 KB or
  less**. `hello.exe` with `GUICY_SMALL` (= NO_TILING, NO_IME, NO_DWRITE, NO_GPU) is **40 KB or
  less** of GUICy. `GUICY_TILING_FULL` and each W group report their KB and don't count.
- **Idle stays at one tick:** under 20 ms CPU per 60 s, both renderers, at every step that touches
  the loop, a renderer or animation.
- **Animations are off by default** (`gy_anim(1)` turns them on), and stay off when Windows says
  client-area animation is off (read through the function table).
- **Imports**, read from the import table every step. Linked: kernel32, user32, gdi32. Loaded at
  run time: d3d11, dxgi, imm32, dwrite, ole32. Nothing else; no run-time shader compiler; no DLL
  of our own. Test and bench code (readback, timestamps, stubs) sits behind `GUICY_TESTS`.
- **Milestones M1-M5:** scan the repo for the AGENT_RULES list (the real name, email, the banned
  words, C:\Users, O:\ and X:\ paths, keys, tokens), report the counts, push to the private repo
  only after the user's yes through Master Control.
- **No app port** without the user's OK; then on a branch through Master Control.

---

## 4. The steps

### F1. Fix what the stress tests found

Deliverable:
1. **Rounded and bordered boxes:** per-pixel work only in the four corner squares, coverage cached
   per (radius, border, colours); straight band rows as spans; the opaque inside of middle rows
   by `k_fill` (stosd); only edge pixels blended. Fractional canvas radii are snapped to 1/4 px
   for the cache key, so one rounding step can differ from today there (named, tested).
2. **Culling:** a widget outside the current clip runs layout and input but formats and pushes
   nothing. Instances are counted per layer, so the renderer walks the list once, not three times.
3. **Visible-range helper:** `gy_scroll_rows(n, row_h, &first, &last)`, the base of W1's clipper.
4. **Line spans:** each row of a line tests only its x-span (plus 1 px of AA).
5. **Image scaling:** x positions and weights once per draw, 16.16 steps in y, SSE2/AVX2 lerp,
   no 64-bit divide per pixel.
6. **Focus outline on its own:** the tile body is one box, the outline four thin rects, so a focus
   move dirties only perimeter tiles.
7. Small: full-redraw path when over half the tiles are dirty; one `GetGlyphOutlineW` per miss,
   `SelectObject` only on a font change; SSE2 for glyph rows under 16 px.
8. Freeze the layout lines and geometry files of today (section 3) before any code changes.

Proof:
- All 30 goldens byte-equal on scalar, SSE2 and AVX2 for items 1-4, 6, 7, except the named
  quarter-pixel radius case. Items 5 and 6 may move pixels by 1 level: side by side, then
  re-baselined once.
- New goldens per fix (radius 0-12 with borders 0-2, lines at 8 angles, images up and down),
  compared with the old kernel kept in gybench: bit-equal or at most 1 level.
- **Counts first, then times:** culling passes when a 100,000-line list pushes the same instances
  and dirties the same tiles as a 100-line one (`gy_get_stats`). gystress times beside them.

Budgets (1080p, one thread): hover **0.15 ms**; typical full redraw **2 ms**; 1,000 visible
buttons **4 ms** (was 24); 100,000-line list unchanged frame **0.5 ms** with `gy_scroll_rows`,
**20 ms** with culling only (was 83); a 1,040 px line **0.02 ms** (was 1.48); bilinear
**3 ns/px** (was 19.2), nearest **1 ns/px**; 4-tile focus move **0.4 ms**; glyph miss **20 us**.
KB: +2 net (it replaces code). Idle unchanged.

### H1. The host: gy_attach, the function table, hello.c

Deliverable: `gy_attach(hwnd)`, `gy_event(msg, wp, lp)` and `gy_begin`/`gy_end` for apps with
their own window and loop (PLAN.md 4.1; bbb_kit needs it). `gy_ctx_set` for a second context
(used by W6). One table of the OS functions GUICy calls at run time, so tests can stub them.
`examples/hello.c` (one button) and the 15-line PLAN.md example build as tests from a folder with
only guicy.h and the .c, with one clang line and with MinGW gcc.

Proof: every test passes on an app-made hidden window driven through `gy_event`; the same
pictures as through `gy_open`; hello and the 15-line example build and run their script. From H1
on, G1, D1, L1 and X1 each add their tests on the `gy_event` path too.
Budgets: +2 KB; `hello.exe` with `GUICY_SMALL` 40 KB or less of GUICy.

### G1. The GPU renderer (D3D11), with the CPU path as fallback. Milestone M1

Deliverable:
- **Drawing:** D3D11 FL 11.0 loaded at run time; one `DrawInstanced(4, n)` over the same 48-byte
  instances from a StructuredBuffer; clip index per instance; R8 glyph atlas and image pages in
  texture arrays; big images in their own texture. Shaders made at build time by `tools/gyshader`
  through the OS's d3dcompiler_47.dll and checked in as byte arrays.
- **The shader copies the CPU's integer maths** where SM5 allows, so most pixels are exact:
  images by `Load()` of 4 texels with the CPU's 8-bit weights and rounding (no sampler filter);
  glyphs by point loads; coverage quantised as `floor(c * 255 + 0.5)` then scaled and rounded as
  `gy__scale`; rounded corners read from F1's corner-coverage cache uploaded as a small texture.
- **Full frame on every changed frame.** The GPU draws every pixel of the back buffer on each frame
  that changed (one draw, small GPU cost), and passes only this frame's dirty tiles to
  `Present1` as dirty rects (`FLIP_SEQUENTIAL`, 2 buffers). Nothing depends on a back buffer's
  age, so there is no N-2 logic to get wrong. A frame with no dirty tile skips upload, draw and
  Present.
- **Occlusion and idle policy** (this replaces PLAN.md 3.9's "occluded presents nothing" for the
  GPU path): GUICy draws and presents only on change, treats `DXGI_STATUS_OCCLUDED` as success,
  and never polls with `DXGI_PRESENT_TEST` or a timer. A minimized window (`IsIconic`) draws
  nothing and marks everything dirty on restore. The frame-latency waitable joins the wait only
  while a frame or an animation is pending, so the idle wait never spins.
- **The swap chain lives on a child window** over the client area, made `WS_DISABLED` so it never
  takes focus, returning `HTTRANSPARENT` from `WM_NCHITTEST`, and its window procedure hands every
  mouse, cursor, wheel and key message to the parent's handler (same coordinates, it sits at 0,0).
  GDI never presents to a flip-model window, so the CPU fallback BitBlts to the parent after the
  child is destroyed. The child is created only once the first GPU frame is ready, so it never
  shows a black frame.
- **Fallback chain:** hardware adapter; Basic Render Driver, software adapters and RDP go to the
  CPU path; `DEVICE_REMOVED`/`RESET` retries once, then switches to the CPU path in the same frame
  with no app code. `GUICY_RENDER=cpu|gpu|warp` and `gy_desc.render` force a path;
  `gy_renderer_name()` and a log line say which and why. The device is made on a worker thread
  while the first frame draws on the CPU path. `GUICY_NO_GPU` drops it all.
- `gy_stats` gains `presents` and `gpu_draws`, written to the log so baseline.py can read them.
  GPU timestamps (disjoint query, a ring read 2-3 frames late) and readback are `GUICY_TESTS` only,
  read only while frames run anyway.

**The default renderer** (the user's answer: "every case must win"): the GPU becomes the default
only if it is faster than the CPU path in **every** measured case: hover, caret blink, typing, a
slider drag, a progress tick, a wheel scroll, a resize sweep, a full redraw, a tile focus move, a
tile drag, the first frame, and start to window. Each case is timed as CPU ms plus GPU-timestamp
ms per frame on the same scene, at 1540x950, 1080p and 1440p, median of 3 runs. A difference
inside the run-to-run spread counts as **not faster**. If the GPU loses or ties any case, the CPU
path stays the default. Every case's numbers go in BENCH.md either way, with working set, idle,
the GPU load seen from other programs during the runs (nvidia-smi if present, else "unknown"),
and the note that this is one PC with presentation and composition excluded for both paths.
Hardware-adapter, not-remote and not-Basic-Render-Driver stay coded conditions; apps can still
choose with `gy_desc.render`. Expectation, said now: start to window and the hover are hard for
the GPU to beat (7 ms library start and about 0.1 ms after F1), so the CPU default is the likely
result.

Proof:
- **Pixel classes, defined mechanically:** in `GUICY_TESTS` builds the CPU renderer writes a 1-bit
  mask of pixels touched by coverage under 255, colour alpha under 255, a glyph or a filtered
  image. Masked pixels: **at most 1 level per channel**; every other pixel **equal**. A histogram
  of differences goes in BENCH.md. Run on every golden, hardware GPU against CPU, by offscreen
  render target and staging readback.
- **WARP on a small set:** one scene per kind, process affinity set to 2 cores; WARP against CPU
  and WARP against hardware, same rule.
- **Child window:** hidden tests `SendMessage` `WM_NCHITTEST` to the child (expect
  `HTTRANSPARENT`) and post `WM_LBUTTONDOWN/UP`, `WM_MOUSEMOVE`, `WM_MOUSEWHEEL` and `WM_SETCURSOR`
  to the child's HWND; the parent's scripted log shows each one, and a drag keeps capture.
- **Hidden-desktop runs** that finish their scripts and end on the CPU golden: forced cpu, forced
  warp (small window), and a hook that makes `Present1` return `DEVICE_REMOVED` halfway; the
  picture after the switch equals the CPU picture (by readback before, BitBlt DIB after).
- **Drawing really happens:** on the hidden desktop the GPU path's `gpu_draws` equals the CPU
  path's drawn-frame count for the same script; `presents` is 0 over 60 s idle and over a 60 s
  wiggle over empty space; idle under 20 ms CPU per 60 s.
- The every-case table, and compare.md re-run with the GPU path and microui (with a small GDI
  renderer) added.

Budgets: +20 KB or less (shader bytes about 2 KB, bench code not counted); GPU-path frame CPU side
**0.5 ms** for 5,000 glyphs and 500 rects; a tile drag **1 ms** CPU per frame.

### D1. Per-monitor DPI v2

Deliverable: `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` through the function table
(an app manifest or an attached app's own awareness wins; a DPI-unaware process is scaled by
Windows and GUICy sees 96). The `WM_DPICHANGED` handler takes the DPI only from `wParam` and the
rect only from `lParam`. One scale for every theme size, tile minimum, title, divider, scrollbar
and caret; fonts re-made, glyph atlas and width cache flushed, all dirty.
**Units:** every GUICy API is in 96-DPI units (DIPs): `gy_desc`, `gy_row`, `gy_avail`,
`gy_last_rect`, canvas coordinates, mouse positions, `gy_split`'s `*pos`, size locks, `gy_dock`.
`gy_image(w, h)` is DIPs and scales nearest by default (pixel art stays sharp), bilinear by option;
`gy_canvas_image(..., GY_IMG_DEVICE_PX)` draws 1:1 device pixels for previews. `gy_dpi_scale()`
tells an app the factor. Layout lines store ratios, so they load the same at any scale.

Proof: at 96 DPI every golden byte-equal and `old_layouts` equal. A 144 DPI (150 %) set of
goldens for every widget and tile test (CPU; GPU by the G1 rule). A script sends
`WM_DPICHANGED` 96 to 144 to 120 to the hidden window with `wParam`/`lParam` set and ends on the
same picture as a fresh start at 120; the client size in DIPs is kept. Canvas tests at 144 DPI
show app coordinates landing where they did at 96.
Budgets: +4 KB; the DPI-change frame 10 ms or less.

### L1. Developer locks. Milestone M2

Deliverable (research 2.9):
- **The code layout:** `gy_tile_rule(name, {.side, .w, .h, .tab_of, .lock})` (moved here from
  T5). The code layout is the tree built from the rules and the app's open order with no saved
  line; GUICy builds it once at start and keeps it. "Reset" returns to it.
- **Flags:** `GY_LOCK_MOVE`, `GY_LOCK_SIZE`, `GY_LOCK_CLOSE`, `GY_LOCK_FLOAT`, `GY_LOCK_TABS`,
  `GY_LOCK_SPLIT` (nothing splits it or lands in it), `GY_LOCK_ALL`; `gy_layout_lock(1)` fixes the
  whole layout, adding to the tiles' own locks and never clearing them when turned off.
- **What each flag holds** (the invariants the tests check):
  MOVE: the tile's path in the tree (parent container, index, stack) is unchanged; a sibling
  closing may shorten the path, which is allowed and logged. SIZE: its width (or height, on the
  locked axis) in DIPs is unchanged while the window is at or above its raised minimum. MOVE +
  SIZE: the rect is unchanged. CLOSE, FLOAT, TABS, SPLIT: the action never happens.
- **"Not if the dev locked the windows":** a code lock can't be undone by the user in any way: no
  key, click, ini line, setting or override, and no API lets a developer give that power away.
  Locks are never read from a saved line; after every load they are applied again (a MOVE tile
  goes back to its code-layout place, a SIZE tile gets its size back, one log line).
- **The window can't undo a lock either:** the window's minimum size is raised to fit the locked
  sizes plus the other tiles' minimums. On a screen smaller than that minimum, locked tiles still
  keep their size and place: unlocked tiles fold into tabs and shrink to their minimum, and what
  still doesn't fit is clipped at the right and bottom edge, with a vertical scroll bar and a gold
  banner "this layout needs W x H". Nothing a lock holds ever moves, so this needs no exception.
- **The same gate everywhere:** `gy__tile_may(node, action)`, written for containers from the
  start, used by the mouse, keys, the loader, reset, undo, the automatic layouts of T2, and also by
  `gy_split(..., GY_LOCK_SIZE)` dividers, P1's `gy_dock` strips and W5's inner windows. A refusal
  changes nothing and shows "Locked: <name>" for 2 s; a refused tile key is eaten, never passed on.
- **User locks** only on what the developer left unlocked, and only when the app calls
  `gy_allow_user_lock(1)` (a padlock in the title, Alt+L, and a whole-layout toggle). Saved in the
  line with a `u` marker.
- **Reset** (Alt+Shift+Backspace) and **undo** (Alt+Backspace, a ring of the last 16 lines) go
  through the gate. Padlocks on locked titles, no resize cursor on locked dividers, no drop zones
  over SPLIT tiles.

Proof:
- Per flag: scripted drags, swaps, resizes, closes, floats and key moves on a locked tile are
  refused and the saved line stays byte-equal; on an unlocked tile they work. The same for a
  locked `gy_split` divider.
- **A hostile line** (hand-edited to move, resize and "unlock" locked tiles) loads with every
  locked tile back on its invariant.
- **Lock fuzz:** 10,000 random mouse, key, load, reset, undo and window-resize events. After every
  event each locked tile meets its own flags' invariant, and over the run at least one unlocked
  tile moved and one resized (the "freely moved" half).
- A screen-too-small run: locked rects unchanged, the banner shown.
- Tile keys on a locked tile never reach the app; goldens of padlock, locked divider and "Locked"
  text at 96 and 144 DPI; `old_layouts` equal (old lines carry no locks).
Budgets: +5 KB; a refused drag 0.1 ms or less; idle unchanged.

### G2. Containers instead of the binary tree, and the tab overflow » menu

Deliverable: i3-style containers with any number of children and one arrangement each (side by
side, stacked, tabbed); the binary tree is the two-child case, with the same integer rect maths.
Pools grow to 64 tiles and 16 tabs a stack. New lines start `gy2:v<n>:` (n from
`gy_desc.layout_version`; a line from another version is converted or ignored, never
half-applied). **A line starting with `[`, `H` or `V` is today's step-4 format** and is converted.
The loader bounds-checks counts, ratios (0.05-0.95) and names. A one-tab stack shows a plain title.
**Tab overflow:** when a stack's names don't fit, the strip shows the tabs that fit around the
active one plus a » button listing every tab (click or first letter). W2's tabs and P1's toolbar
fold reuse it. Switches start here: `GUICY_NO_TILING`, default, `GUICY_TILING_FULL`.

Proof: every step-4 tiling test and every L1 test passes on the new tree; `old_layouts` geometry
equal; an old line saved again gives a `gy2:` line that loads to the same geometry; 10,000 fuzzed
lines never crash and either load or fall back to the code layout with the error shown. Goldens of
8 and 16 tabs at 3 widths with the » menu open and closed; a scripted pick activates that tab.
Budgets: +6 KB; layout of 32 tiles 0.05 ms or less.

### T1. Mouse: drop zones

Deliverable: dragging a title shows 5 zones per tile (4 sides split, the middle stacks) with a
tinted preview of the exact rect; a drag starts only after 4 px; Alt+drag anywhere moves a tile,
Alt+right-drag resizes from the nearest edge. In `GUICY_TILING_FULL`: outer-edge strips of the
window, preselection (Alt+Ctrl+arrow, Esc cancels), a right-click menu on dividers. All through
the gate.
Proof: a scripted drag into each zone (and each strip in the full build) with a golden; save and
load round trip; the lock fuzz again with drags; a click on a title moves nothing.
Budgets: +3 KB default, +2 KB full; a drag frame 1 ms CPU (GPU path), 2 ms (CPU path), 1540x950.

### X1. Text: fallback glyphs and IME. Milestone M3

Deliverable:
- **DirectWrite only for glyphs the GDI font lacks** (CJK, symbols, emoji in file names). GDI stays
  the look, so no golden changes; `gy_text_engine(GY_TEXT_DWRITE)` switches an app wholly.
  `GUICY_NO_DWRITE` drops it. **Out of scope:** colour emoji (the atlas is A8, so emoji are drawn
  in one colour), and shaping of Arabic, Hebrew, Indic and Thai scripts.
- **IME:** `WM_IME_STARTCOMPOSITION/COMPOSITION/ENDCOMPOSITION` (`GCS_COMPSTR`, `GCS_RESULTSTR`),
  composition and candidate windows at the caret, the composition string underlined in the edit
  box. imm32 is called through the function table. `GUICY_NO_IME` drops it.

Proof: goldens of a mixed-script list (Latin, Japanese, Chinese, Korean, a symbol, an emoji) at 96
and 144 DPI; old goldens byte-equal; glyph misses 0 after the first frame. IME: the test swaps
stub imm32 functions into the table and sends real `WM_IME_*` messages to the hidden window, so the
real handler runs: a composition shown, changed, committed and cancelled, then typing still
arrives; undo removes a commit in one step. A real IME is the user's check at a port.
Budgets: +3 KB IME, +3 KB fallback; the first fallback (dwrite and the font collection loading)
30 ms or less, once; a later fallback miss 100 us or less.

### T2. Layout types (`GUICY_TILING_FULL`)

Alt+Space cycles a container through side by side, stacked, tabbed, master + stack (Alt+Enter
makes master, Alt+, / Alt+. the master count), grid, spiral, centred master, monocle. Each is a pure
function `(n, area, params) -> rects`, so the hand-made tree under it survives; locked tiles are
placed first. The app picks the cycle (`gy_tile_layouts(mask)`) and count rules; a manual change
turns the rules off until reset. Too small: two pane, then tabs, never under the minimum.
Proof: a golden of every layout with 1-6 tiles; lock invariants hold in every layout; app keys
untouched. Budgets: +5 KB (full only).

### T3. Workspaces (`GUICY_TILING_FULL`)

Alt+1..9 switch; Alt+Shift+number sends the focused tile (refused for MOVE tiles); pin a tile to
every workspace; all in one saved line.
Proof: away and back gives identical pictures; round trip; idle one tick. Budgets: +3 KB (full).

### T4. Floating tiles, scratchpad, auto-hide (`GUICY_TILING_FULL`)

Alt+F2 floats a tile on an overlay layer and puts it back; a scratchpad tile drops over everything
with one key and hides with the same key; an auto-hide tile folds to an edge label and slides out
while focused. FLOAT tiles refuse.
Proof: the popup-trap test with floating and scratchpad tiles (typing still arrives); goldens of
each state; round trip. Budgets: +4 KB (full).

### T5. Slots, regions, attention (`GUICY_TILING_FULL`)

Slots (an empty named place in the code layout that a tile fills when it first opens); `.region`
on a rule (the tile stays in one region, which is also a lock); a gold dot on an unfocused tile
with news; a strip of closed tiles (one click brings one back; the last tile can't close unless
the app allows it); gaps, focus follows mouse, focus border, each with an on/off setting.
Proof: goldens of slots, dots and the strip; app keys reach the app. Budgets: +4 KB (full).

### T6. Keyboard modes, minimap, overview, presets, animation (`GUICY_TILING_FULL`)

Resize mode (Alt+R, arrows, Enter or Esc leaves); a minimap of tile names while Alt is held for
600 ms; an overview (Alt+O) of every tile on every workspace, each shown as the **last picture
drawn while it was visible** (cached, marked stale if older than its last change), click or
number to jump; named presets; slide animations, off by default, through a per-clip offset
table so a slide re-hashes only moved tiles' offsets; `GUICY_NO_ANIM` removes the code.
Proof: slide frame cost on both renderers; **0 dirty tiles once it ends**; idle one tick with
animations on and nothing moving; with the OS setting stubbed to "off" no slide runs.
Budgets: +5 KB (full); a slide frame 1 ms (GPU path), 4 ms (CPU path) at 1540x950.

### P1. Panel layouts inside a tile. Milestone M4

Default build: `gy_dock(TOP/BOTTOM/LEFT/RIGHT, size)` for toolbars, status bars and side strips
(call order decides the corners), lockable through the gate; row items sized `GY_PX`, `GY_PCT`,
`GY_STRETCH`, `GY_AUTO` with min/max, so left, centre and right alignment need no special case; a
strictness per item for what gives way on overflow (text never cut by default); N-way splits with
min/max and percentages; a toolbar that folds its lowest-priority buttons into the » menu.
`GUICY_TILING_FULL`: wrap rows and sticky section headers.
Proof: goldens at 3 widths and 2 DPIs; a 30-size resize sweep (640x400 to 2560x1440) never leaves
blank space or overlaps (rect checks, not only pictures); `gy_row` goldens byte-equal; a locked
dock refuses its drag. compare.md re-run.
Budgets: +3 KB default, +2 KB full; the typical screen's UI pass still 0.2 ms or less.

### W1. Lists and tables

`gy_list` on the F1 clipper: single and multi select (click, Shift, Ctrl), keys (arrows,
Home/End, PgUp/PgDn, first letter), drag to reorder. `gy_table`: resizable and sortable columns, a
header that stays put, a frozen first column, widths saved in a line, selection, tree rows. **No
sideways scrollbar:** columns shrink to their minimum with an ellipsis, then the rightmost
columns hide behind the » menu; horizontal scrolling only if the app opts in.
Proof: goldens per state; scripted selection, sort and column drag; **counts:** 100 and 100,000
rows format, push and dirty the same numbers (times reported beside them).
Budgets: +6 KB, a W group (`GUICY_NO_TABLES`).

### W2. Menus and tabs

`gy_menubar`, `gy_menu`, submenus, check and radio items, key hints, keyboard navigation (F10 and
arrows; Alt+letter only where it clashes with neither tile keys nor the app's keys, by the same
check as `gy_keys_owned`); `gy_tabs` (the G2 strip with »); submenus in the right-click menu.
Proof: goldens; keyboard-only scripts through every menu; the popup-trap test with menus; the
clash banner. Budgets: +4 KB, a W group.

### W3. Every button, slider and edit box

Buttons: plain, accent, with key, icon, icon + text, toggle, radio, repeat, split (button +
menu), link. Sliders: horizontal and vertical, int and float, range (two thumbs), log, ticks, a
knob; a spin box. **A multi-line edit box** (wrap, scroll, selection, undo, IME from X1).
Proof: goldens of every state (normal, hover, down, disabled, focused) at 96 and 144 DPI;
scripted drags, wheel, keys, and IME in the multi-line box. Budgets: +6 KB, a W group.

### W4. Pickers

`gy_color` (SV square, hue, alpha, hex and number boxes), `gy_date` (7x6 month), `gy_time`, a
searchable combo, `gy_key_pick` (press a key to bind; used to rebind tile keys in the app),
`gy_font_pick`, and `gy_file_open/save/folder` through `IFileOpenDialog`/`IFileSaveDialog`
(ole32 at run time; folders by `FOS_PICKFOLDERS`), opened only on the user's click, never in tests
(a stub in the function table returns a path).
Proof: goldens open and closed; scripted picks; the stub proves no test opens a real dialog.
Budgets: +6 KB, a W group.

### W5. Windows inside the app. Milestone M5

Movable, resizable inner windows (title, close, min size, z-order on a layer) for apps that don't
tile, lockable (MOVE, SIZE, CLOSE) through the gate; modal dialogs drawn in the app (never an OS
message box) with Enter and Esc. Docking stays the tiling of G2-T6.
Proof: goldens; scripted move, resize, close; keys never reach widgets behind a modal; locked
windows refuse; the popup-trap test with dialogs; compare.md re-run; README's widget table with a
golden per widget. Budgets: +4 KB, a W group.

### W6. A second OS window

A second `gy_open` (or `gy_attach`) with its own context through `gy_ctx_set`: its own DIB or
swap chain, DPI and input; a tile can be popped out into it and back (`GUICY_TILING_FULL`), and
locks hold across both.
Proof: two hidden windows run scripts in turn; each matches its golden; a pop-out and back gives
the same geometry; idle one tick with two windows. Budgets: +3 KB, a W group.

### Gated and later

- **C1. Tile thread pool:** only if a full redraw misses its budget after F1 (cores - 1 workers,
  only when 16 or more tiles are dirty). Proof: 1 against N threads, pictures byte-equal.
- **A1-A3. App ports** (bbb_kit, pixelhunter, voxup): only with the user's OK, on branches
  through Master Control; a timing hook first, old/new pictures side by side, same actions logged,
  numbers against step 0, the user's sign-off before Nuklear goes.
- **Later, each only with a number that improves or a picture the user prefers:** a command
  palette, toasts, ClearType, the exact Gaussian shadow, scroll rects in `Present1`, colour emoji,
  the strip layout, UI Automation, the Linux GL 3.3 backend.

---

## 5. Budgets in one place

| Thing | Budget | Today |
|---|---|---|
| Core KB, (i) and (ii) | 120 KB | about 86 KB (i) |
| Core estimate after P1: F1 +2, H1 +2, D1 +4, L1 +5, G2 +6, T1 +3, X1 +6, P1 +3 | about 117 KB | |
| GPU path | +20 KB | not built |
| `hello.exe`, `GUICY_SMALL` | 40 KB of GUICy | not measured |
| `GUICY_TILING_FULL`, each W group | reported, not in the core | |
| Idle, both renderers | under 20 ms CPU per 60 s; 0 presents | 0-15.6 ms |
| Hover, 1080p, CPU path | 0.15 ms (after F1) | 0.34 ms |
| Full redraw, 1080p, CPU path | 2 ms (after F1) | 4.2 ms |
| GPU-path frame, CPU side | 0.5 ms for 5,000 glyphs + 500 rects | |
| Tile drag | 1 ms GPU path, 2 ms CPU path, 1540x950 | 2.2 ms (focus move) |
| UI pass, typical screen | 0.2 ms | 0.009 ms |
| 100,000-row list, unchanged frame | 0.5 ms with the clipper | 83 ms |
| Start to window (baseline.py) | no worse than today's 65 ms | 53-71 ms |
| Library start to first frame (gystress) | 30 ms | 7 ms |
| Memory at 1440p (arena, atlases, tiles) | 16 MB | about 5 MB |

If the core estimate passes 115 KB at any step, these move behind a switch in this order before
more code lands: T1's Alt+drag and Alt+right-drag, P1's strictness, the DirectWrite fallback (off
by default, on by define). The done-report says which.

## 6. Open questions

None for the user: the GPU default is answered ("every case must win"), the screen-too-small case
is designed so locks never give way, and second OS windows are scheduled (W6).

For Master Control:
1. The order insertions in section 2: F1 and H1 before G1, D1 between G1 and L1, X1 between T1
   and T2. F1 starts after this answer.
2. Making the shaders at build time through the OS's `d3dcompiler_47.dll` (PLAN.md 7.2): still
   fine? Normal builds use the checked-in byte arrays.

## 7. Risks

1. **The hidden desktop hides presentation.** DXGI reports the window occluded there, so the
   compositor's cost and frame pacing on a visible screen are not measured, for either path. The
   every-case table says so; the user's eye at the first port is the check.
2. **Pixels:** if a class of GPU pixels can't be made exact in SM5, the fix goes in the shader, not
   in the tolerance; a rule change goes to Master Control.
3. **The child window** may still differ from the parent in ways the posted-message tests don't
   reach (drag-and-drop from Explorer, touch). Those are checked at the first port.
4. **Size:** about 117 KB of 120 KB is planned for the core; the switch order in section 5 is the
   answer if estimates are low.
5. **`hello.exe` at 40 KB** is a guess: the glyph atlas, kernels and window code are shared by any
   app. If it misses, the done-report says what it costs.
6. **F1 bit-exactness:** a fast kernel that can't be bit-equal changes pictures by up to 1 level
   once, with side-by-side proof.
7. **Alt is busy:** tile keys, menu letters and app keys share one clash check; a clash shows a gold
   banner, never a silent loss.
8. **Clipping on a too-small screen** breaks "main buttons always visible" there; it is the only
   way to keep developer locks absolute, and the banner says what size is needed.
9. **IME, fallback glyphs and the DPI change** are tested with stubbed OS calls and sent messages;
   a real IME and a real second monitor are the user's check at a port.
10. **Busy PC:** every number is a median of 3 runs with its spread; GPU and WARP runs are short,
    WARP on 2 cores, and other GPU load is logged.
11. **Work folder growth on X:** goldens stay hashes plus a pruned PNG set; size in each report.

## 8. Requirements trace

| Ask or note | Where | How it is proved |
|---|---|---|
| "fastest ever" | F1, G1, C1; section 1 | stress.md budgets; compare.md at M1, M4, M5; claims only against measured libraries |
| "smallest ever" | section 3, every step | (i)-(iv) size table, -O2 and -Os; microui added at M1 |
| "simplest usage ever" | H1, every step | one call per feature; hello.c and the 15-line example build as tests |
| "nothing outside itself" | section 3; G1; X1; W4 | import table every step; no run-time compiler; OS DLLs at run time only |
| "easy to import to an EXE" | H1; `GUICY_STATIC` | one define, one clang or gcc line, no linker flags |
| "CPU and/or GPU" | G1 | pixel-class rule; forced cpu/gpu/warp; `gy_desc.render`; `GUICY_NO_GPU` |
| "make sure the GPU is being used too or can be used" | G1 | `gy_renderer_name`, `gpu_draws`, readback tests, the every-case table |
| "yes to the GPU if it is faster" / "every case must win" | G1 | GPU default only if it wins all cases beyond the spread; all numbers reported |
| every type of window | W5, W6, T4, G2-T6 | goldens and scripts per window type |
| every type of button, slider | W3 (and v1) | goldens of every state at 2 DPIs |
| tables, lists | F1, W1 | equal counts for 100 and 100,000 rows |
| pickers | W4 | goldens; stub proves no real dialog |
| menus, tabs, inner windows, multi-line edit | W2, G2, W5, W3 | goldens; keyboard-only scripts |
| tiling-WM features | G2, T1-T6 | goldens per layout, zone, workspace, state |
| other panel alignments | P1 | goldens at 3 widths; resize-sweep rect checks |
| lock panels, else freely moved | L1 (tiles, splits, docks, windows) | per-flag refusals; fuzz shows unlocked tiles move |
| "not if the dev locked the windows" | L1 | no override API; hostile line; lock fuzz; too-small screen run |
| MC: order G1, L1, G2+T1, T2-T6+P1 | section 2 | kept; insertions in question 1 |
| MC: advanced tiling behind switches, KB with and without | G2 on, section 3 | size table (iv) every step |
| MC: animations off, idle one tick | section 3, T6 | baseline.py idle; 0 dirty tiles after a slide |
| MC: G1 proof (AA 1 level, solid 0; cpu/warp/removed; idle) | G1 | pixel-class mask; hidden runs; presents 0 |
| MC: locks hold against mouse, keys, saved layouts | L1, T1 | refusals; hostile line; fuzz |
| MC: old layout lines keep loading | section 3, G2 | `old_layouts` geometry from today, every step |
| MC: commit every step, push at milestones after a scan | section 3 | M1-M5 with scan counts |
| MC: no app port without OK, on branches | A1-A3 | gated |
| MC: tab-overflow » menu | G2 (W2, P1 reuse) | goldens of 8 and 16 tabs at 3 widths; scripted pick |
| stress fixes: boxes, culling, visible range, lines, bilinear, focus outline | F1 | budgets and counts in F1 |
| research G1: child window, FLIP_SEQUENTIAL + Present1, readback, WARP, timestamps | G1 | child message tests; readback; presents counter |
| research: per-monitor DPI v2 | D1 | 96 DPI byte-equal; 144 DPI set; sent `WM_DPICHANGED` |
| research: IME | X1 | real handler on stubbed imm32 |
| research: locks in code, SPLIT, whole-layout lock, reset, undo | L1 | as in L1 |
| compare bench lessons | section 1 | what GUICy offers beyond Win32 controls |

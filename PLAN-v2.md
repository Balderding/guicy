# GUICy: the next plan (v2)

Draft 1, 2026-11-22. Steps 0-5 of PLAN.md are done (GUICy 0.4.0, commit e58409b, 30 of 30 tests).
This plan replaces PLAN.md steps 6-13 and PLAN-tiling.md. It folds in bench/stress.md,
bench/compare.md and research/tiling-locks-gpu.md. The rules in AGENT_RULES.md ("Always" and
"Desktop apps") apply to every step. Section 8 maps every ask to a step.

---

## 1. Where GUICy stands, honestly

What the benches say today (CPU renderer only):
- **Idle:** 0-15.6 ms of CPU a minute (one scheduler tick). Only GUICy and plain Win32 controls
  do this. ImGui, Nuklear D3D11 and raygui burn 1.3-6 s a minute.
- **A mouse move:** about 0.1 ms, against 4.2 ms for Nuklear GDI and 7-26 ms in the three apps.
- **Start:** about 65 ms to a window (ImGui 0.36 s, raygui 0.5 s).
- **Two budget misses** (stress.md): a 1080p hover costs 0.34 ms (budget 0.3), a 1080p full redraw
  4.2 ms (budget 4). Both come from the rounded-box rasterizer (29.6 us per button box). Also slow:
  no culling (100,000 list lines cost 83 ms a frame), lines scan their whole bounding box
  (1.5 ms each), bilinear images (19 ns per pixel), a tile focus move redraws whole tiles (2.2 ms).
- **Win32 controls tie GUICy** on CPU per move (0.05 against 0.1 ms) and beat it on size (47 KB
  against the 142 KB demo exe). So GUICy can't sell itself on "faster than native" for a form.

What GUICy offers that Win32 controls don't, and what this plan builds on:
1. **Much less app code:** one call per widget, state in the app's own variables, no window
   handles, no WM_COMMAND, no dialog templates, layout that follows the window by itself.
2. **One look in every app** from one theme table (no red), the same at any DPI.
3. **A tiling window manager inside the app**, with developer locks. Win32 has nothing like it.
4. **Hidden tests for free:** offscreen frames, BMP shots and scripted input in the library.
   Win32 controls can't be drawn offscreen or scripted without UI Automation.
5. **The GPU when it pays,** with the CPU path always there.
6. **Canvas, images, custom widgets** in tens of lines, where Win32 needs owner-draw code.

Claims stay measured: "fastest" and "smallest" are only ever said against libraries measured on
this PC (compare.md), and compare.md is re-run at milestones M1, M4 and M5.

## 2. The order, and why

```
F1 fix the slow spots -> G1 GPU (M1) -> D1 DPI -> L1 locks (M2) -> G2 containers -> T1 mouse
-> X1 text + IME (M3) -> T2 T3 T4 T5 T6 -> P1 panels (M4) -> W1..W5 every widget type (M5)
gated: C1 thread pool (only if a budget misses), A1-A3 app ports (only with the user's OK)
```

- Master Control's order is kept: G1, then L1, then G2 and T1, then T2-T6 and P1.
- **F1 goes before G1** because G1 compares GPU pictures with CPU pictures and decides the
  default renderer on speed. Both need the fixed CPU path, or the comparison is unfair to it.
- **D1 (DPI) goes before L1** because it changes tile minimums, title bars, divider widths and
  the size locks, and adds a 150 % set of goldens that every later step must keep.
- **X1 (DirectWrite fallback + IME)** sits after T1 and before T2 so the widget steps start with
  real text input.
- **The widget types (W1-W5) come after the tiling work.** Their groundwork lands earlier where it
  is cheapest: the list clipper in F1, the tab strip with its » overflow menu in G2, the toolbar
  fold in P1.

## 3. Rules for every step

- **One local commit per step**, then "done: <commit> <what>" to Master Control with the test
  count, the FAIL grep result, the exit code, the KB table and the work folder size.
- **All runs hidden:** `gytest` offscreen, `gystress` offscreen, `tools/baseline.py` on its own
  hidden desktop. No window on the user's screen, no focus taken, no `start`. One run at a time.
- **Goldens:** every existing golden must stay byte-equal unless the step says it changes
  pictures. A planned picture change shows the old and new PNG side by side with the
  changed-pixel count and largest channel difference, and the new hash is committed with a reason.
- **The layout golden of today:** at the start of F1, the 0333d67 build saves the layout lines of
  every tile_* test (and a 6-tile line with a stack and a zoom) into
  `tests/golden/layouts-2026-11-22.txt`, with each line's picture hash. Test `old_layouts` loads
  each line and checks the picture **at every step from then on**, including after the G2 format
  change and at 100 % DPI after D1. It never gets re-baselined.
- **Size table** in BENCH.md at every step (llvm-size, -O2, the exe linked with
  `-ffunction-sections -Wl,--gc-sections` so an app pays only for what it calls):
  (a) `guicy.o` alone; (b) `hello.exe`, one button; (c) the demo with every v1 widget; and with
  the switches `GUICY_NO_TILING`, default, `GUICY_TILING_FULL`, `GUICY_NO_GPU`, `GUICY_NO_ANIM`.
- **The core budget is 120 KB** of code and data from GUICy in (c) with default tiling, locks,
  DPI and IME, CPU only. The GPU path adds at most 20 KB. `GUICY_TILING_FULL` and each W group
  report their KB separately and do not count against the core.
- **Idle stays at one tick:** under 20 ms of CPU per 60 s on the demo, both renderers, measured by
  baseline.py at every step that touches the loop, the renderer or animation.
- **Animations are off by default** (`gy_anim(1)` turns them on), and stay off when Windows has
  client-area animation turned off. An animation ends; then the loop blocks again.
- **Imports:** read from the import table at every step. Linked: kernel32, user32, gdi32 only.
  d3d11, dxgi, imm32, dwrite and comdlg32 are loaded at run time with LoadLibrary. Nothing else,
  no d3dcompiler at run time, no DLL of our own.
- **Milestones (M1-M5):** scan the repo (real name, email, the banned words, local paths, keys, tokens),
  report the counts, and push to the private repo only after the user's yes through Master
  Control.
- **No app port** (bbb_kit, pixelhunter, voxup) without the user's OK; then on a branch, through
  Master Control, never while that app's session edits the same file.

---

## 4. The steps

### F1. Fix what the stress tests found

Deliverable:
1. **Rounded and bordered boxes:** pixel-by-pixel work only in the four corner squares, with the
   corner coverage cached per (radius, border, colours); the straight band rows as spans; the
   opaque inside of middle rows as `k_fill` (stosd); only edge pixels blended.
2. **Culling:** a widget whose cell is outside the current clip runs its layout and input but
   formats and pushes nothing. The instances are counted per layer, so `gy__render` walks the
   list once, not three times.
3. **Visible-range helper:** `gy_scroll_rows(n, row_h, &first, &last)` inside a scroll region, so
   an app calls only the visible rows (the clipper W1 builds on).
4. **Line spans:** each row of a line covers one x-span (plus 1 px of AA); only that is tested.
5. **Image scaling:** source x positions and weights once per draw, a 16.16 step in y, the lerp
   in SSE2/AVX2; no 64-bit divide per pixel.
6. **Focus outline on its own:** the tile body is one box and the focus outline four thin rects,
   so a focus move dirties only the perimeter tiles.
7. Small ones: take the full-redraw path when more than half the tiles are dirty; one
   `GetGlyphOutlineW` call per glyph miss and `SelectObject` only when the font changes; SSE2 for
   glyph rows under 16 px (AVX2 was 19 % slower there).

Proof:
- All 30 goldens byte-equal on scalar, SSE2 and AVX2 (items 1-4, 6, 7 keep the same maths).
  Item 5 may move bilinear pixels by 1 level: shown side by side, at most 1 level, then
  re-baselined. Item 6 may change AA pixels at the outline corners: same treatment.
- A new golden per fix at 3 sizes (box radius 0-12 with borders 0-2, lines at 8 angles, images
  scaled up and down) compared against the old kernel kept in `gybench` (bit-equal or <= 1 level).
- `gystress` again, both full runs, the table in stress.md.

Budgets (1080p, one thread): hover **0.15 ms or less**; full redraw of the typical screen
**2 ms or less**; 1,000 visible buttons full redraw **4 ms or less** (was 24); a 100,000-line list
unchanged frame **0.5 ms or less** with `gy_scroll_rows` (was 83), and **20 ms or less** without
it (culling only); a 1,040 px 2-px line **0.02 ms or less** (was 1.48); bilinear **3 ns/px or
less** (was 19.2), nearest **1 ns/px or less**; focus move with 4 tiles **0.4 ms or less**; glyph
miss **20 us or less**. KB: +4 or less. Idle unchanged.

### G1. The GPU renderer (D3D11), with the CPU path as fallback. Milestone M1

Deliverable:
- D3D11 feature level 11.0, loaded at run time; one `DrawInstanced(4, n)` over the same 48-byte
  instances from a StructuredBuffer; clip index per instance (no scissor changes); R8 glyph atlas
  and image pages in texture arrays; big user images get their own texture.
- **Shaders compiled at build time** by `tools/gyshader` through the OS's d3dcompiler_47.dll and
  checked in as byte arrays. The run time never loads a compiler.
- **The swap chain lives on a child window** that covers the client area and passes all input to
  the parent (`HTTRANSPARENT`). GDI never presents to a flip-model window, so the CPU fallback
  can always BitBlt to the parent: on fallback the child is destroyed.
- **`FLIP_SEQUENTIAL`, 2 buffers, `Present1` with dirty rects** from the dirty tiles: redraw the
  tiles dirty this frame or last frame (the back buffer holds frame N-2), present only this
  frame's. A frame with no dirty tile skips upload, draw and Present. Frame-latency waitable at 1.
- **Fallback chain:** hardware adapter; Basic Render Driver, software adapters and RDP go to the
  CPU path; `DEVICE_REMOVED`/`RESET` retries once, then switches to the CPU path in the same frame
  with no app code. `GUICY_RENDER=cpu|gpu|warp` and `gy_desc.render` force a path;
  `gy_renderer_name()` and one log line say which one and why. `GUICY_NO_GPU` drops it all.
- **The device is made on a worker thread** while the first frame draws on the CPU path, so start
  doesn't pay for it; the swap happens on a later frame with no visible change.
- **Tests with no window:** an offscreen render-target texture, `CopyResource` to a staging
  texture, `Map`, compare with the CPU picture. The same under WARP.
- **GPU timestamps** (disjoint query, a ring read 2-3 frames late) only in bench mode, read only
  while frames run anyway. They never wake an idle window.
- **The default renderer decision** (section 6, question 1), written into BENCH.md with the
  numbers, and the default set in code from them.

Proof:
- Every golden scene, GPU (hardware) against CPU: **at most 1 level per channel on AA pixels, 0 on
  solid pixels**. The same for WARP, and WARP against hardware.
- A fuzz test of the N-2 buffer: 500 random frames of hovers, typing and scrolls, each frame's GPU
  readback equal (within the rule above) to a CPU full redraw.
- Hidden-desktop runs that finish their scripts and end on the CPU golden: forced cpu, forced
  warp, and a test hook that makes `Present1` return `DEVICE_REMOVED` halfway.
- baseline.py on the GPU path: idle **under 20 ms CPU per 60 s**, **0 Presents** in 60 s idle and
  in a 60 s mouse wiggle over empty space.
- The decision table: CPU ms per frame and GPU ms per frame for each scene of the session mix
  (section 6) at 1540x950, 1080p and 1440p, both paths, plus start to window and working set.
- compare.md re-run with the GPU path added.

Budgets: +20 KB or less (shader bytes about 2 KB); GPU-path frame, CPU side, **0.5 ms or less**
for 5,000 glyphs and 500 rects; start to window no worse than the CPU path by more than 10 ms;
a tile drag **1 ms or less** CPU per frame.

### D1. Per-monitor DPI v2

Deliverable: `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` looked up at run time (an app
manifest wins); `WM_DPICHANGED` uses the suggested rect; one scale for every theme size, tile
minimum (160x110), title height, divider width, scrollbar and caret; fonts re-made, glyph atlas
and width cache flushed, everything dirty; `GetSystemMetricsForDpi`, `AdjustWindowRectExForDpi`.
Layout lines store ratios, so they load the same at any scale. `gy_offscreen_dpi(w, h, dpi)` for
tests. App sizes in `gy_desc` and `gy_row` are in 96-DPI units, so apps change nothing.

Proof: at 96 DPI every golden byte-equal (and `old_layouts`). A new set of goldens at 144 DPI
(150 %) for every widget and tile test, CPU and GPU within the G1 rule. A scripted DPI change
mid-run (96 to 144 to 120) ends on the same picture as a fresh start at 120. Hidden-desktop run:
the window keeps its client size in 96-DPI units across a simulated `WM_DPICHANGED`.

Budgets: +4 KB or less; the DPI-change frame 10 ms or less; nothing else changes.

### L1. Developer locks. Milestone M2

Deliverable (research 2.9):
- `gy_tile_lock(name, flags)` and `.lock` in `gy_tile_rule`. Flags: `GY_LOCK_MOVE` (drag, swap,
  move into tabs, send to a workspace), `GY_LOCK_SIZE` (its dividers, grow/shrink, balance; the
  automatic layouts keep its size), `GY_LOCK_CLOSE`, `GY_LOCK_FLOAT`, `GY_LOCK_TABS` (tabs in or
  out of its stack), **`GY_LOCK_SPLIT`** (nothing may split it or land in it), `GY_LOCK_ALL`.
- `gy_layout_lock(1)`: the whole layout fixed (a kiosk screen). It adds to each tile's own locks
  and never clears them when turned off.
- **"Not if the dev locked the windows":** a lock set in code can't be undone by the user by any
  means: no key, no click, no ini line, no setting, no override switch. There is no API that lets
  a developer hand that power to the user either.
- **Locks live in code.** They are never read from a saved line. After every `gy_tiles_load`
  they are applied again: a MOVE-locked tile the line put elsewhere goes back to its code place,
  a SIZE-locked tile gets its code size back, and one log line says so.
- **User locks** only on tiles the developer left unlocked, and only when the app calls
  `gy_allow_user_lock(1)`: a padlock in the title bar and Alt+L. Those are saved in the line with
  a `u` marker. The user's own "lock the whole layout" toggle is allowed under the same call.
- **One gate**, `gy__tile_may(tile, action)`, used by the mouse code, the key code, the loader,
  reset, undo and (later) every automatic layout. A refused action changes nothing and shows
  "Locked: <tile>" in the status text for 2 s. A refused tile key is eaten, never passed to the app.
- **The window can't undo a size lock either:** the window's minimum size is raised to fit the
  locked sizes plus the other tiles' minimums. Only a screen smaller than that minimum makes
  locked sizes give way (in proportion, logged).
- **Reset and undo:** "reset to the app's layout" is one key (Alt+Shift+Backspace) and one menu
  item; layout undo is a ring of the last 16 lines (Alt+Backspace). Both go through the gate, so
  neither can move a locked tile.
- What the user sees: a small padlock on locked titles, no resize cursor on locked dividers, no
  drop zones over SPLIT-locked tiles.

Proof:
- For each flag: scripted drags, swaps, resizes, closes, floats and key moves on a locked tile are
  refused and the saved line stays byte-equal; the same actions work on an unlocked tile.
- **A hostile line:** a hand-edited line that moves, resizes and "unlocks" locked tiles loads with
  every locked tile back in its code place, size and flags.
- **A lock fuzz:** 10,000 random mouse, key, load, reset and undo events on a layout with locked
  tiles; after every event each locked tile's rect equals its code rect.
- Tile keys on a locked tile never reach the app (`gy_key_pressed` stays 0).
- Goldens of the padlock, the locked divider and the "Locked" text, at 96 and 144 DPI.
- `old_layouts` still passes (old lines carry no locks; the code's locks apply on top).

Budgets: +4 KB or less; a refused drag costs 0.1 ms or less; idle unchanged.

### G2. Containers instead of the binary tree, and the tab overflow » menu

Deliverable: i3-style containers: any number of children, one arrangement each (side by side,
stacked, tabbed). The binary tree is the two-child case. Saved lines start with
`gy2:v<n>:` (n = the app's own layout version, from `gy_desc.layout_version`); an older app
version's line is converted or ignored, never half-applied. The loader bounds-checks every count,
ratio (0.05-0.95) and name. A one-tab stack shows a plain title, not a tab bar.
**Tab overflow:** when the names of a stack don't fit (8 tabs show about 4 today), the strip shows
the tabs that fit around the active one and a » button that lists every tab in a popup (click or
first letter). The same strip code serves W2's tabs widget and P1's toolbar fold.
Compile switches start here: `GUICY_NO_TILING`, default (containers, tabs, locks),
`GUICY_TILING_FULL` (T2-T6 extras).

Proof: every step-4 tiling test and every L1 test passes on the new tree; `old_layouts` (format
`gy:` from today) loads to the same pictures; old line, then save, gives a `gy2:` line that loads
to the same picture; a fuzzed line (random bytes, 10,000 cases) never crashes and either loads or
falls back to the app's layout with the error shown. Goldens of 8 tabs at 3 widths with the »
menu open and closed; a scripted pick from the » menu activates that tab.

Budgets: +6 KB or less in the default build; layout of 32 tiles 0.05 ms or less.

### T1. Mouse: drop zones, preselection

Deliverable: dragging a title shows 5 zones per tile (left, right, top, bottom split that side;
the middle stacks it as a tab) plus strips on the window's outer edges (take a whole side), with
a tinted preview of the exact rect. A drag starts only after 4 px, so a click never moves a tile.
Alt+drag anywhere moves a tile, Alt+right-drag resizes from the nearest edge. Preselection:
Alt+Ctrl+arrow marks where the next tile goes (Esc cancels). Right-click on a divider: a small
menu (equalize, flip, swap). All through the L1 gate.

Proof: a scripted drag into each zone and each outer strip, each with a golden; save and load
round trip; locked tiles refuse (L1 fuzz again with drags added); a click on a title moves nothing.

Budgets: +5 KB or less; a drag frame 1 ms CPU (GPU path) / 2 ms (CPU path) at 1540x950.

### X1. Text: DirectWrite fallback glyphs and IME. Milestone M3

Deliverable:
- **DirectWrite only for the glyphs the GDI font lacks** (CJK, emoji, other scripts in file
  names). GDI stays the default look, so no golden changes. `gy_text_engine(GY_TEXT_DWRITE)`
  switches the whole look for an app that wants it.
- **IME:** `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION` (`GCS_COMPSTR`, `GCS_RESULTSTR`),
  `ImmSetCompositionWindow` and the candidate window at the caret; the composition string drawn
  underlined in the edit box; imm32 loaded at run time. Test hooks `gy_input_ime(comp, result)`.

Proof: goldens of a mixed-script list (Latin, Japanese, Chinese, Korean, an emoji) at 96 and
144 DPI; every old golden byte-equal; glyph misses 0 after the first frame. Scripted IME: a
composition shown, changed, committed and cancelled in an edit box, then typing still arrives;
undo removes the committed text in one step. A real IME can't be driven hidden, so the user
checks one by eye when an app ports (noted in the done-report).

Budgets: +6 KB or less; a fallback glyph miss 100 us or less.

### T2. Layout types (`GUICY_TILING_FULL`)

Deliverable: Alt+Space cycles a container through side by side, stacked, tabbed, master + stack
(Alt+Enter makes master, Alt+, / Alt+. change the master count), grid, spiral, centred master,
monocle. Each automatic layout is a pure function `(n, area, params) -> rects`, so the hand-made
tree under it survives. Locked tiles are placed first; the function gets the rest. An app picks
which layouts are in the cycle (`gy_tile_layouts(mask)`) and can set count rules (1 tile: tall,
4: grid); a manual change turns the rules off for that container until reset. Too small:
two pane, then tabs, never under the minimum.

Proof: a golden of every layout with 1-6 tiles; a locked tile doesn't move in any layout; a
small window falls back to tabs; tile keys never reach the app. Budgets: +5 KB (full build only).

### T3. Workspaces (`GUICY_TILING_FULL`)

Deliverable: Alt+1..9 switch; Alt+Shift+number sends the focused tile (refused for MOVE-locked
tiles); a tile can be pinned to every workspace. All workspaces in one saved line.
Proof: switching away and back gives identical pictures; save/load round trip; idle one tick.
Budgets: +3 KB (full build only).

### T4. Floating tiles, scratchpad, auto-hide (`GUICY_TILING_FULL`)

Deliverable: Alt+F2 floats a tile on an overlay layer and puts it back where it was; a scratchpad
tile (a Log) drops over everything with one key and hides with the same key; an auto-hide tile
folds to a label on an edge strip and slides out while focused. FLOAT-locked tiles refuse.
Proof: the step-4 popup-trap test again with floating and scratchpad tiles (typing still
arrives); goldens of each state; save/load round trip. Budgets: +4 KB (full build only).

### T5. Rules, slots and attention

Deliverable: `gy_tile_rule(name, {.side, .w, .h, .floating, .tab_of, .slot, .region, .lock})`
for where a tile goes the first time; slots (an empty named place in the developer's layout that
a tile fills when it first opens); `.region` (a tile allowed only in one region, which is also a
lock); a gold dot on the title or tab of an unfocused tile with news; a strip of closed tiles (one
click brings one back; the last tile can't close unless the app allows it); gaps, focus follows
mouse and the focus border, each with an on/off setting.
Proof: goldens of rules, slots, dots and the strip; app keys reach the app. Budgets: +4 KB
(rules, slots and dots in the default build; the rest in the full build).

### T6. Keyboard modes, overview, presets, animation

Deliverable: resize mode (Alt+R, arrows, Enter or Esc leaves); a minimap of tile names while Alt
is held for 600 ms; an overview (Alt+O, every tile on every workspace as a small picture, click
or number to jump, drawn by the GPU when there is one); named layout presets; slide animations
when tiles move, **off by default**, through a per-clip offset table so a slide re-hashes only
the moved tiles' offsets; `GUICY_NO_ANIM` removes the code.
Proof: frame cost during a slide on both renderers; **0 dirty tiles once it ends**; idle one tick
with animations on and nothing moving; animations stay off when Windows turns them off.
Budgets: +5 KB (full build); a slide frame 1 ms CPU (GPU path) / 4 ms (CPU path) at 1540x950.

### P1. Panel layouts inside a tile. Milestone M4

Deliverable (research 3.2): `gy_dock(TOP/BOTTOM/LEFT/RIGHT, size)` for toolbars, status bars and
side strips (the order of calls decides the corners, VS Code style); row items sized
`GY_PX`, `GY_PCT`, `GY_STRETCH`, `GY_AUTO`, each with min/max, so left, centre and right
alignment need no special case; a strictness value per item for what gives way on overflow (text
never cut by default); wrap rows; N-way splits with min/max and percentages; sticky section
headers in scroll regions; a toolbar that folds its lowest-priority buttons into the » menu.
Proof: goldens at 3 window widths and 2 DPIs each; a resize sweep (30 sizes, 640x400 to
2560x1440) never leaves blank space or overlaps (checked by rect tests, not only pictures); the
existing `gy_row` goldens byte-equal. compare.md re-run.
Budgets: +5 KB or less in the default build; the UI pass of the typical screen still 0.2 ms or less.

### W1. Lists and tables

Deliverable: `gy_list` with the F1 clipper, single and multi select (click, Shift, Ctrl), keys
(arrows, Home/End, PgUp/PgDn, first letter), drag to reorder; `gy_table` with resizable and
sortable columns, a header that stays put, a frozen first column, widths saved in a line, the
clipper, selection; a tree list (tree rows in a table).
Proof: goldens per state; scripted selection, sort and column drag; **a 100,000-row list and
table scroll at the same frame ms as a 100-row one** (within 10 %). Budgets: +6 KB, paid only by
apps that call them.

### W2. Menus, tabs, toolbars

Deliverable: `gy_menubar`, `gy_menu`, submenus, check and radio items, key hints, keyboard
navigation (F10 and arrows; Alt+letter only where it doesn't clash with tile keys or the app's
own keys, checked by the same clash test as `gy_keys_owned`); `gy_tabs` (the G2 strip with »);
the right-click menu gains submenus; a command palette (Ctrl+Shift+P, unless the app owns it)
listing every tile action, every `gy_button_key` and app commands from `gy_command(label, key)`.
Proof: goldens; scripted keyboard-only use of every menu; the popup-trap test with menus; the
clash banner when a menu letter meets an app key. Budgets: +6 KB, paid per use.

### W3. Every button and slider

Deliverable: buttons: plain, accent, with key, icon, icon + text, toggle, radio, repeat (fires
while held), split (button + menu), link; sliders: horizontal and vertical, int and float, a range
slider with two thumbs, log scale, ticks, a knob; a spin box (the number box with arrows).
Proof: goldens of every state (normal, hover, down, disabled, focused) at 96 and 144 DPI;
scripted drags, wheel and keys. Budgets: +5 KB, paid per use.

### W4. Pickers

Deliverable: `gy_color` (SV square, hue bar, alpha, hex and number boxes), `gy_date` (a 7x6
month popup), `gy_time`, a searchable combo (type to filter), `gy_key_pick` (press a key to bind,
used for rebinding tile keys in-app), `gy_font_pick`, and `gy_file_open/save/folder` through the
OS dialog (comdlg32 at run time), shown only on the user's click and never in tests (tests use a
stub that returns a path).
Proof: goldens of each picker open and closed; scripted picks; the stub proves no test opens a
real dialog. Budgets: +6 KB, paid per use.

### W5. Windows inside the app. Milestone M5

Deliverable: movable, resizable inner windows (title, close, min size, z-order on a layer) for
apps that don't tile; modal dialogs drawn in the app (never an OS message box), with Enter and
Esc; toasts (a short notice in a corner that fades without animation by default); an "errors"
window that lists `gy_error` texts with their paths. Docking stays the tiling of G2-T6.
Proof: goldens; scripted move, resize, close and modal focus (keys don't reach widgets behind a
modal); the popup-trap test with dialogs. compare.md re-run; README's "every widget" table filled
with a golden per widget. Budgets: +5 KB, paid per use.

### Gated steps (not scheduled)

- **C1. Tile thread pool:** only if a full redraw misses its budget after F1 (cores - 1 workers,
  an atomic counter over dirty tiles, only when 16 or more are dirty). Proof: 1 against N
  threads at 1080p and 1440p, pictures byte-equal.
- **A1-A3. App ports** (bbb_kit, then pixelhunter, then voxup): only with the user's OK, on
  branches through Master Control. Each: a timing hook first, old/new pictures side by side, the
  same app actions logged for the same script, frame and idle numbers against step 0, exe KB,
  and the user's sign-off before Nuklear goes.
- **Later, each only with a number that improves or a picture the user prefers:** ClearType text,
  the exact Gaussian shadow, scroll rects in `Present1`, pop-out tiles in a second OS window,
  the niri-style strip layout, UI Automation, the Linux GL 3.3 backend.

---

## 5. Budgets in one place

| Thing | Budget | Today |
|---|---|---|
| Core KB (v1 widgets, default tiling, locks, DPI, IME, CPU) | 120 KB | about 86 KB |
| GPU path | +20 KB | not built |
| `GUICY_TILING_FULL`, each W group | reported, not in the core | |
| Idle, both renderers | under 20 ms CPU per 60 s, 0 Presents | 0-15.6 ms |
| Hover, 1080p, CPU path | 0.15 ms (after F1) | 0.34 ms |
| Full redraw, 1080p, CPU path | 2 ms (after F1) | 4.2 ms |
| GPU-path frame, CPU side | 0.5 ms for 5,000 glyphs + 500 rects | |
| Tile drag / layout change | 1 ms GPU path, 2 ms CPU path, 1540x950 | 2.2 ms (focus move) |
| UI pass, typical screen | 0.2 ms | 0.009 ms |
| 100,000-row list, unchanged frame | 0.5 ms with the clipper | 83 ms |
| Start to first frame (library) | 30 ms; GPU no more than +10 ms | 7 ms |
| Memory at 1440p (arena, atlases, tiles) | 16 MB | about 5 MB |

## 6. Open questions

For the user (one):
1. **What "faster" means for the GPU default.** The two paths will likely each win somewhere:
   the CPU path on small changes (a hover is about 0.1 ms after F1), the GPU on big redraws
   (resize, scroll, tile drags). Proposed rule: replay one fixed "session mix" on both paths
   (hovers, typing, a slider drag, list scrolls, 3 resizes, tile moves, a first frame) at the
   user's three window sizes, and add up CPU plus GPU ms per frame. **The GPU is the default if
   its total is lower, it is never more than 0.1 ms slower on a single small change, and it
   starts no more than 10 ms later. Otherwise the CPU path.** RDP and software adapters are
   always CPU. If the user doesn't answer, G1 uses this rule and reports it.

For Master Control (not the user):
2. F1 before G1 and D1 between G1 and L1: a change to the approved order, for the reasons in
   section 2.
3. Making the shaders at build time through the OS's `d3dcompiler_47.dll` (PLAN.md 7.2): still
   fine? The checked-in byte arrays keep normal builds independent of it.

## 7. Risks

1. **The hidden desktop hides real presentation.** DXGI reports the window occluded there
   (compare.md notes 1-2), so Present never waits and the compositor's cost isn't seen. G1 times
   the GPU with offscreen textures and timestamps, and proves the swap chain, fallback and idle on
   the hidden desktop, but frame pacing on a visible screen stays unmeasured. The user's eye at
   the first app port is the check.
2. **GPU against CPU within 1 level** may fail on AA corners: the CPU path is exact integer
   maths, the shader is float. Fix in the shader (same coverage formula, same rounding), not by
   loosening the rule. A rule change goes to Master Control.
3. **The child window** must not break capture, cursors, IME or drag-and-drop. If
   `HTTRANSPARENT` doesn't pass everything, the child forwards its messages to the parent. The
   G1 scripted tests run on the windowed path to catch it.
4. **The N-2 back buffer with Present1:** a missed tile leaves an old picture on screen. The
   500-frame fuzz in G1 is the guard.
5. **Size:** about 86 KB of the 120 KB core is used; F1, D1, L1, G2, T1 and X1 add about 29 KB.
   That leaves little room. If the core passes 115 KB, the next feature goes behind a switch
   before more code lands, and the done-report says so.
6. **Bit-exactness of F1:** if a fast box or line can't be made bit-equal, its pictures change by
   up to 1 level and every affected golden is re-baselined with side-by-side proof, once.
7. **Alt is busy:** tile keys, menu letters (W2) and the app's keys all want it. The one clash
   check covers all three; a clash shows a gold banner, never a silent loss.
8. **A screen smaller than the locked layout** forces locked sizes to give way. This is the only
   case, it is logged, and the user can't cause it from inside the app.
9. **IME and fallback glyphs** can only be tested with synthetic messages here; a real IME is
   the user's check at a port.
10. **Busy PC:** other sessions skew timings. Every number is the median of 3 runs, with the
    spread, as in compare.md.
11. **Work folder growth on X:** goldens stay as hashes plus a pruned PNG set; its size is in
    each done-report.

## 8. Requirements trace

| Ask or note | Where | How it is proved |
|---|---|---|
| "fastest ever" | F1, G1, C1; section 1 | stress.md budgets; compare.md re-runs at M1, M4, M5; claims only against measured libraries |
| "smallest ever" | section 3 size table, every step | KB per step and per switch; core 120 KB; GPU +20 |
| "simplest usage ever" | every step; section 1 | one call per feature; `hello.c` and the 15-line example build as tests; app lines in compare.md |
| "nothing outside itself" | section 3 imports rule; G1 shaders; X1 | import table read every step; no run-time compiler; OS DLLs loaded at run time |
| "easy to import to an EXE" | section 3 size table | `hello.exe` built from guicy.h plus one .c with one clang line |
| "CPU and/or GPU" | G1 | GPU/CPU picture rule; forced cpu/gpu/warp; `gy_desc.render`; `GUICY_NO_GPU` |
| "make sure the GPU is being used too or can be used" | G1, T6 (overview) | `gy_renderer_name()`; the decision table; GPU readback tests |
| "yes to the GPU if it is faster" | G1, question 1 | the session-mix rule; default set from the G1 numbers |
| every type of window | W5, T4, G2-T6, later pop-out | goldens and scripted tests per window type |
| every type of button and slider | W3 (plus v1) | goldens of every state at 2 DPIs |
| tables and lists | F1 (clipper), W1 | 100,000 rows at 100-row frame ms |
| pickers | W4 | goldens; stub proves no real dialog in tests |
| menus, tabs, inner windows | W2, G2, W5 | goldens; keyboard-only scripts |
| tiling-WM features | G2, T1-T6 | goldens per layout, zone, workspace, state |
| other panel alignments | P1 | goldens at 3 widths; resize sweep rect checks |
| lock panels, else freely moved | L1 (and the gate in T1-T6) | per-flag refusals; unlocked tiles move freely |
| "not if the dev locked the windows" | L1 | no override API; hostile line; 10,000-event lock fuzz |
| MC: order G1, L1, G2+T1, T2-T6+P1 | section 2 | kept; F1 and D1 inserted (question 2) |
| MC: advanced tiling behind switches, KB with and without | G2 onward, section 3 | size table per switch every step |
| MC: animations off, idle one tick | section 3, T6 | baseline.py idle; 0 dirty tiles after a slide |
| MC: G1 proof (1 level AA, 0 solid; cpu/warp/removed; idle) | G1 | as listed in G1 |
| MC: locks hold against mouse, keys, saved layouts | L1, T1 | scripted refusals; hostile line; lock fuzz |
| MC: old layout lines keep loading | section 3, G2 | `old_layouts` golden from today, every step |
| MC: commit every step, push at milestones after a scan | section 3 | M1-M5, scan counts in the report |
| MC: no app port without OK, on branches | A1-A3 | gated |
| MC: tab-overflow » menu | G2 (W2, P1 reuse it) | goldens of 8 tabs at 3 widths, scripted pick |
| stress: rounded boxes, culling, visible range, lines, bilinear, focus outline | F1 | budgets in F1; goldens byte-equal |
| research: child-window swap chain, FLIP_SEQUENTIAL + Present1, readback tests, WARP, timestamps | G1 | fallback runs; N-2 fuzz; readback goldens; idle 0 Presents |
| research: per-monitor DPI v2 | D1 | 96 DPI byte-equal; 144 DPI goldens; DPI-change script |
| research: IME | X1 | scripted composition tests |
| research: locks in code, GY_LOCK_SPLIT, whole-layout lock, reset, undo | L1 | as listed in L1 |
| compare bench lessons | section 1 | what GUICy offers beyond Win32 controls, said plainly |

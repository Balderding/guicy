# GUICy: the next plan (v2)

Draft 6, 2026-11-23. Changes: reviews/PLAN-v2-response-r1.md to -r4.md, the lead's ruling
(reviews/PLAN-v2-ruling.md) and Master Control's evaluation
(reviews/EVAL-PLAN-v2-2026-11-23-0207.md, "GO WITH CHANGES", answered in
reviews/PLAN-v2-response-eval.md). Steps 0-5 of PLAN.md are done (GUICy 0.4.0). This plan
replaces PLAN.md steps 6-13 and PLAN-tiling.md. AGENT_RULES.md ("Always", "Desktop apps") applies
to every step. Section 9 maps every ask to a step.

---

## 1. Where GUICy stands, honestly

Today (CPU path only): idle 0-15.6 ms CPU a minute, which only GUICy and Win32 controls reach
(libraries that draw every frame burn 1.3-6 s; ImGui's wait loop 0.23 s); a mouse move about
0.1 ms (Nuklear GDI 4.2, the three apps 7-26); about 65 ms to a window. Two budget misses
(1080p hover 0.34 ms, full redraw 4.2 ms) come from the rounded-box rasterizer; culling, lines,
bilinear images and tile focus moves are also slow. **Win32 controls tie GUICy** on CPU per move
(0.05 against 0.1 ms) and beat it on size (47 against 142 KB), so GUICy can't sell "faster than
native" for a form. What it offers beyond them: much less app code (one call per widget, no
handles, WM_COMMAND or templates); one look in every app, scaled the same on any screen; a tiling
window manager inside the app with developer locks; hidden tests for free; canvas, images and
custom widgets in tens of lines. Claims are only made against libraries measured on this PC;
until microui joins compare.md, the size claim is only "smaller than Nuklear, ImGui and raygui".

**Where the value is:** the three apps' ports. A mouse move costs them 7-26 ms today and about
0.1 ms with GUICy. So this plan builds what the ports need first (the fixed CPU path, the host
API, scaling, text, locks), ports the apps, and builds the rest after, sized by what the ports
showed.

## 2. The order, and why

```
F1 fixes -> H1 host -> G0 GPU spike -> D1 scaling (M1) -> X1 text and input -> L1 locks (M2)
-> A1 bbb_kit pilot (M3) -> A2 pixelhunter -> A3 voxup (M4)
-> G1 GPU path, opt-in, only if G0 allows -> G2 containers, T1 mouse, P1 panels, default parts (M5)
-> on the user's ask: T2-T6, W1-W6, the deferred items (section 6)
```

This is Master Control's evaluated order. What each move unblocks:
- **F1 first:** the CPU path is the default renderer and the base of every port; its two budget
  misses must go before anything is measured against it.
- **H1 next:** every windowed test, every port (bbb_kit draws into its own window through
  `gy_attach`) and every OS stub used later.
- **G0 before D1:** one day of numbers settles whether the full GPU path is worth building now. If
  the CPU stays the default (the likely result), G1 drops to opt-in work after the ports.
- **D1, X1 and L1 before the ports:** the ports should land once, already scaled (both of the
  user's monitors get 125 %), with real text input and the developer locks.
- **The ports before G1, G2-P1 and the W steps:** they test the API on real code before more
  widgets are designed blind, and they show which W steps an app really needs.

## 3. Rules for every step

- **One local commit per step**, then "done: <commit> <what>" to Master Control with the test
  count, the FAIL grep over the whole output, the exit code, the size table and the work folder
  size.
- **All runs hidden.** Offscreen tests run in gytest and gystress. Every test that makes a real
  window runs in `gytest --windowed`, which relaunches itself on its own desktop (`CreateDesktopW`
  plus `STARTUPINFO.lpDesktop`, as baseline.py does) and writes its results to a file. In
  `GUICY_TESTS` builds `gy_open` and `gy_attach` refuse to run on the "Default" desktop, with an
  error naming the reason. No window on the user's screen, no focus, no `start`, one run at a
  time, no priority or affinity changes.
- **Never touch system settings.** OS calls GUICy reads (imm32, `SystemParametersInfo`, DPI and
  monitor functions, d3d11/dxgi) go through one function table (`gy__os`, H1); tests swap in stubs.
- **Pass and fail by counts:** functional tests pass or fail on counts (frames drawn, presents,
  dirty tiles, wake-ups, instances), never on milliseconds. Times are reported beside them and
  compared with the budgets, but they don't fail the suite. **Idle means 0 frames drawn and 0
  presents over 60 s**, run with baseline.py only at steps that touch the loop or a renderer.
- **Goldens:** structural checks are instance-list checks drawn with the synthetic test font
  (H1), so they survive font, theme and ClearType changes. Real-font PNG goldens are a small look
  set per step. Existing goldens stay byte-equal unless the step says it changes pictures; a
  planned change shows old and new side by side with the changed-pixel count and largest channel
  difference, and the new hash is committed with a reason.
- **Old layouts keep loading:** at the start of F1 the 0333d67 build saves the layout line of
  every tile_* test, plus a 6-tile line with a stack and a zoom, into
  `tests/golden/layouts-2026-11-22.txt`, each with a **geometry file** (every tile's name and rect,
  stacks, active tab, focus, zoom). Test `old_layouts` loads each line at every step at factor 1;
  the geometry must be equal, and it is never re-baselined.
- **Size table** in BENCH.md every step, -O2 and -Os:
  (i) `guicy.o` text+data, no gc, W groups off, `GUICY_NO_GPU`: the honest upper bound, and what a
  multi-file app pays without linker flags;
  (ii) `core.exe` calling one function of every default feature, GUICy as its own translation
  unit (`guicy_impl.c`), linked with `-ffunction-sections -Wl,--gc-sections`; GUICy's share is
  `guicy_impl.o`'s whole contribution in the lld map file, including `.rdata`, `.pdata` and
  `.xdata`;
  (iii) `hello.exe`, one button, the same way, default and with `GUICY_SMALL`;
  (iv) each switch on and off.
- **`GUICY_STATIC` is for single-file apps only:** `GUICY_API` on every prototype and definition
  is empty normally and `static __attribute__((unused))` under the switch, so unused code drops out
  with no linker flags and still builds under `-Wall -Wextra -Werror`.
- **Budgets:** the core (default build with `GUICY_NO_GPU`) is **120 KB or less** in (i) and (ii);
  the default build with the GPU path is **140 KB or less**; `hello.exe` with `GUICY_SMALL` (=
  NO_TILING, NO_IME, NO_DWRITE, NO_GPU) is **40 KB or less** of GUICy. `GUICY_TILING_FULL` and each
  W group report their KB and don't count.
- **Animations are off by default**, and stay off when Windows says client-area animation is off.
- **Imports**, read from the import table every step. Linked: kernel32, user32, gdi32. Loaded at
  run time: d3d11 and dxgi (G0, G1), imm32, dwrite. Nothing else; no run-time shader compiler; no
  DLL of our own. Test and bench code sits behind `GUICY_TESTS`.
- **Milestones M1-M5:** scan the repo for the AGENT_RULES list (the real name, email, the banned
  words, C:\Users, O:\ and X:\ paths, keys, tokens), report the counts, push to the private repo
  only after the user's yes through Master Control.
- **No app port** without the user's OK; then on a branch through Master Control.

---

## 4. The core steps

### F1. Fix what the stress tests found

Deliverable:
1. **Rounded and bordered boxes:** per-pixel work only in the four corner squares, coverage cached
   per (radius, border, colours); straight band rows as spans; the opaque inside of middle rows by
   `k_fill` (stosd); only edge pixels blended. Fractional canvas radii snap to 1/4 px for the cache
   key, so one rounding step can differ from today there (named, tested).
2. **Culling:** a widget outside the current clip runs layout and input but formats and pushes
   nothing. Instances are counted per layer, so the renderer walks the list once, not three times.
3. **Visible-range helper:** `gy_scroll_rows(n, row_h, &first, &last)`, the base of W1's clipper.
4. **Line spans:** each row of a line tests only its x-span (plus 1 px of AA).
5. **Image scaling:** x positions and weights once per draw, 16.16 steps in y, SSE2/AVX2 lerp, no
   64-bit divide per pixel.
6. **Focus outline on its own:** the tile body is one box, the outline four thin rects, so a focus
   move dirties only perimeter tiles.
7. Small: full-redraw path when over half the tiles are dirty; one `GetGlyphOutlineW` per miss,
   `SelectObject` only on a font change; SSE2 for glyph rows under 16 px.
8. Freeze the layout lines and geometry files of today (section 3) before any code changes.
9. **The raw-pixel check** (`tools/check_px.py`, used by D1). A "size" is, in guicy.h's widget,
   layout and tiling sections: an integer literal of 2 or more in arithmetic with a rect field or
   passed as a size to a draw or layout helper, and every `#define` ending in `_W`, `_H`, `_PAD`,
   `_GAP`, `_R` or `_SIZE`. Intended device-pixel constants (the 64-px dirty tile, 1024 atlas
   pages, the 16384 frame limit) carry a `/*px*/` mark. It reports both counts; F1 only reports,
   D1 brings the unscaled count to zero and lists the marked ones for review.

Proof: all 30 goldens byte-equal on scalar, SSE2 and AVX2 for items 1-4, 6 and 7, except the
named quarter-pixel radius case; items 5 and 6 may move pixels by 1 level (side by side, then
re-baselined once). New goldens per fix are compared with the old kernel kept in gybench:
bit-equal or at most 1 level. Culling passes on counts: a 100,000-line list pushes the same
instances and dirties the same tiles as a 100-line one.
Budgets (1080p, one thread, reported): hover 0.15 ms; typical full redraw 2 ms; 1,000 visible
buttons 4 ms (was 24); 100,000-line list unchanged 0.5 ms with `gy_scroll_rows`, 20 ms with
culling only (was 83); a 1,040 px line 0.02 ms (was 1.48); bilinear 3 ns/px (was 19.2), nearest
1 ns/px; 4-tile focus move 0.4 ms; glyph miss 20 us. KB: +4 until measured.

### H1. The host: gy_attach, the function table, the test desktop, the test font

Deliverable: `gy_attach(hwnd)`, `gy_event(msg, wp, lp)` and `gy_begin`/`gy_end` for apps with
their own window and loop. The OS function table `gy__os`. `gytest --windowed` and the
Default-desktop guard. **The synthetic test font:** stubs for `GetGlyphOutlineW` and
`GetTextMetricsW` whose advances, heights and bitmaps are exact multiples of the size, for every
structural check. **The text-record hook** (`GUICY_TESTS`): a list of every drawn text's rect,
foreground and background, so the apps' WCAG 4.5:1 contrast self-tests keep working after a port.
`examples/hello.c` and a corrected 15-line example against the real API build as tests from a
folder with only guicy.h and the .c, with clang and with MinGW gcc.
Proof: every test passes on an app-made window driven through `gy_event` in `gytest --windowed`,
with the same instance lists as through `gy_open`; the guard refuses a windowed test on the Default
desktop; the text-record hook lists every text of a test scene with the right colours.
Budgets: +2 KB; `hello.exe` with `GUICY_SMALL` 40 KB or less of GUICy.

### G0. A one-day GPU spike

Deliverable: `bench/gyspike.c`, not in guicy.h: the smallest D3D11 path that a real GUICy frame
would pay for. A device, a `FLIP_DISCARD` swap chain on a child window, and per frame GUICy's tile
hash pass, one `Map` of about 500 instances, one `DrawInstanced` with a trivial shader and
`Present(1, 0)`. Run windowed on the test desktop, timed with `QueryThreadCycleTime` (CPU side)
plus GPU timestamps, 9 runs, against the fixed CPU path's caret/typing frame (about 0.06 ms, 1 tile,
stress.md) and its hover and full-redraw frames. Other CPU and GPU load is logged.
Output: one table to Master Control. Rule: under the user's "every case after startup, ties lose",
if the GPU's fixed cost per frame already loses or ties any running case, the CPU stays the default,
the user is told plainly, and G1 becomes opt-in work after the ports (visible race A is never
needed). **The likely result is exactly that:** a D3D11 present alone usually costs tens to
hundreds of microseconds, against a 0.06 ms CPU caret frame. If G0 shows the GPU can win every
case, G1 comes right after A3 with the full race.
Budgets: one day; 0 KB in guicy.h.

### D1. Everything scales: DPI, resolution, zoom, font. Milestone M1

The user's asks: **"depending on the resolution everything should scale"** and **"You can do what
vscode does and scales with the fonts too?"**

Deliverable:
- **The factor:** pixel factor = monitor factor x zoom x font-size ratio.
  - **Monitor factor** (`GY_SCALE_DPI_AND_RESOLUTION`, the default): max(DPI / 96, min(w, h) /
    1080), rounded to the nearest 25 % step (an exact half rounds up), using the monitor's shorter
    side so a portrait screen isn't over-scaled. A 4K screen at 100 % gets 200 %.
    `GY_SCALE_DPI_ONLY` follows only Windows; `GY_SCALE_FIXED` with `gy_scale_set(f)` is the app's
    own number. **The user's monitors** (tools/monitors.py): DISPLAY2, 2560x1440 at 96 DPI, and
    DISPLAY3, 1440x2560 portrait at 96 DPI, both get **125 %**, where the apps draw at 100 % today.
  - **Zoom, VS Code style:** Ctrl+= and Ctrl+- (and the numpad keys) step it by 10 %, Ctrl+0
    resets, Ctrl+wheel steps it. Saved with the font size in one ini line
    (`gy_view_save/gy_view_load`). The keys go through `gy_keys_owned` like the tile keys;
    Ctrl+wheel has its own code, `GY_KEY_WHEEL`, so an app can keep its own wheel zoom; a clash turns
    that key off with the gold banner. The too-small-screen fit (L1) also lowers the zoom, in steps
    of 5 % of the normal factor. **The 0.75 floor limits the zoom only** (user zoom and fit zoom
    together, 0.75 x the normal factor, about 94 % on 125 % screens); the font ratio sits outside
    it. A zoom-in first grows the window up to the work area, then the zoom is capped with a gold
    banner.
  - **Font-size ratio:** `gy_font_size(px)` sets the user's font size (saved in the same line); its
    ratio to the theme's `font_px` is the third term.
- **Sizes from the font, resolved to whole pixels:** every theme size (`row_h`, `pad`, `gap`,
  `radius`, icon and check size, scroll bar, splitter, title bar, tile minimums) is stored as a
  float **in em** of the theme's `font_px` (one number per size, so a px and an em can't disagree;
  no app theme is ported yet). **Once per factor or font change** GUICy resolves them into a
  whole-pixel theme (`floor(em x font_px x factor + 0.5)`). The layout keeps its integer maths
  unchanged (for example guicy.h:2104 and :2108, `(int)((float)(r.w - 6) * t->ratio)`, with the 6
  taken from the resolved theme). Today's `row_h` 28 becomes 28 / 15 em, pad 8 / 15, gap 6 / 15,
  radius 4 / 15; at factor 1 the resolved theme equals today's integers exactly, so today's goldens
  stay byte-equal. Gaps are even: gap 6 DIP at 125 % is 7.5 px, resolved once to 8 px, never
  alternating 7 and 8.
- **Whole pixels:** each rect edge from DIPs is rounded on its own and widths come from the edges;
  a line or border width is `max(1, floor(w x f + 0.5))`, a crisp 1 px at 125 %, 2 px at 150 % and
  200 %. **Thin canvas lines are pixel-snapped:** an axis-aligned line's centre is snapped to a pixel
  centre (or pixel edge for even widths), so a 1-DIP canvas line at a .25 offset stays 1 px, not a
  blur over 2.
- **Monitors:** per-monitor DPI v2 through the function table (an app manifest or an attached app's
  own awareness wins; bbb_kit and voxup are already per-monitor v2). On `WM_DPICHANGED` the `lParam`
  rect is applied **at once** with `SetWindowPos`, the documented contract. Only the same-DPI
  resolution-term switch (a monitor with another short side, no `WM_DPICHANGED`) waits for
  `WM_EXITSIZEMOVE`, and keeps the window's centre so it can't bounce. On the user's own two
  monitors **neither path fires** (same DPI, same short side, both 125 %), so this code stays
  minimal and stub-tested only.
- **Units:** every API is in DIPs. `gy_rect` becomes float DIPs; GUICy returns each rect as its
  pixel edges divided by the factor, so a canvas call at those DIPs rounds back to the same edges.
  Hit tests run in device pixels. Instances stay whole device pixels, so floats never enter the
  tile hash. API note: don't cast `gy_rect` to `int` before a canvas call (it brings back 1-px
  seams). `gy_image(w, h)` scales nearest by default (pixel art stays sharp);
  `GY_IMG_DEVICE_PX` draws 1:1 for previews.
- **Opening size:** a window bigger than the work area opens at the work area's size; only a
  minimum that doesn't fit starts the fit zoom.
- **Zoom is crisp, with no preview:** every scale change is a fresh layout at the new factor
  (boxes and SDF corners at any scale, glyphs re-made once), drawn as the first frame after the
  zoom input. No stretched bitmap is ever shown. The GPU stretched preview is waiting on the user
  (section 6, O1).
- **High contrast:** with `SPI_GETHIGHCONTRAST` on (through the function table), the theme takes
  the system colours, and `WM_SETTINGCHANGE` switches it live.

Proof (structure by counts and instance lists, look by a small PNG set):
- `tools/check_px.py` reports zero unscaled sizes.
- **Exact scaling:** every scene with the synthetic font at factors 1, 2 and 3 and at three font
  sizes; each instance rect is the factor-1 rect scaled by the rounding rule. This also catches
  `+ 1` insets, which check_px.py skips.
- **The resolved theme** at factor 1 equals today's integers; every existing golden byte-equal and
  `old_layouts` equal.
- **Look set:** real-font goldens for one scene per widget group at 96, **120 (125 %)**, 144 and
  192 DPI.
- **Monitor stubs:** 3840x2160 at 96 picks 200 %; 2560x1440 at 96 picks 125 %; portrait 1440x2560
  at 96 picks 125 %; 1920x1080 at 144 picks 150 %; each mode the same way. **The user's case:** a
  stubbed move between the two 125 % monitors changes nothing (no re-layout, no resize).
- A windowed script sends `WM_DPICHANGED` 96 to 144 to 120 and the window takes each `lParam` rect
  at once; a stubbed 1080p-to-4K move at 96 DPI settles after one switch on `WM_EXITSIZEMOVE`; a
  1540-DIP window on the portrait stub opens inside the work area.
- **Zoom:** scripted Ctrl+=, Ctrl+-, Ctrl+0 and Ctrl+wheel give the expected factors; the floor
  holds; a zoom-in past the work area shows the banner; a reload restores zoom and font size; with
  the app owning Ctrl+=, that key is off, the banner shows, and the app gets its Ctrl+=. After a
  zoom: the next frame is crisp, then 0 frames and 0 presents.
- **No seams:** at every zoom step from 1.25 x 1.00 down to 1.25 x 0.75, pixel to DIP to pixel is
  the identity, a canvas rect at `gy_avail()` covers exactly the panel's pixels, a click on each 1-px
  border hits its own widget, and a 1-DIP canvas line is exactly 1 px wide.
- High contrast stubbed on: every theme colour comes from the system colours.
Budgets: +6.5 KB; a scale-change frame 10 ms or less (reported).

### X1. Text and input: fallback glyphs, IME, keyboard focus

Deliverable:
- **DirectWrite only for glyphs the GDI font lacks** (CJK, symbols, emoji in file names; pixelhunter
  pulls art from any country). GDI stays the look, so no golden changes. This needs a hand-written
  `IDWriteTextAnalysisSource` in C. `GUICY_NO_DWRITE` drops it. Out of scope: colour emoji (the
  atlas is A8) and shaping of Arabic, Hebrew, Indic and Thai.
- **IME:** `WM_IME_STARTCOMPOSITION/COMPOSITION/ENDCOMPOSITION`, composition and candidate windows
  at the caret, the composition string underlined; imm32 through the function table; `GUICY_NO_IME`.
- **Keyboard focus:** Tab and Shift+Tab move focus through every focusable widget in order, Enter or
  Space activates, and the focused widget shows the focus outline. Today focus comes only from a
  click. It fits "fewer clicks".
Proof: a mixed-script list (Latin, Japanese, Chinese, Korean, a symbol, an emoji) in the look set at
96 and 120 DPI; old goldens byte-equal; glyph misses 0 after the first frame. IME, windowed: stub
imm32 functions, real `WM_IME_*` messages, so the real handler runs: shown, changed, committed and
cancelled, then typing arrives; undo removes a commit in one step. Focus: a scripted Tab walk visits
every widget of a test scene once, in order, and Enter/Space activate each by counts.
Budgets: +3 KB IME, **+5-8 KB fallback**, +1 KB focus; the first fallback 30 ms or less, once.

### L1. Developer locks. Milestone M2

Deliverable (research 2.9):
- **The code layout:** `gy_tile_rule(name, {.side, .w, .h, .tab_of, .lock})`. The code layout is the
  tree built from the rules and the app's open order with no saved line; a tile the app opens later
  takes its rule place at first open and joins the code layout then. Reset returns to it.
- **Flags:** `GY_LOCK_MOVE`, `GY_LOCK_SIZE`, `GY_LOCK_CLOSE`, `GY_LOCK_FLOAT`, `GY_LOCK_TABS`,
  `GY_LOCK_SPLIT` (nothing splits it or lands in it), `GY_LOCK_ALL`; `gy_layout_lock(1)` fixes the
  whole layout, adding to the tiles' own locks and never clearing them when turned off.
- **What each flag holds** (the invariants the tests check): MOVE: the tile's path in the tree is
  unchanged (a sibling closing may shorten it, logged). SIZE: its size on the locked axis, in DIPs,
  is unchanged (except below the zoom floor). MOVE + SIZE: the rect in DIPs is unchanged. CLOSE,
  FLOAT, TABS, SPLIT: the action never happens.
- **"Not if the dev locked the windows":** a code lock can't be undone by the user: no key, click,
  ini line, setting or override, and no API gives that power away. Locks are never read from a saved
  line; after every load they are applied again (one log line). The one exception is the user's own:
- **Too-small screen (the user's answer):** the minimum window size is raised to fit the locked sizes
  plus the other tiles' minimums, capped at the work area. When that doesn't fit, **first GUICy
  shrinks its own UI scale (like a browser zoom) down to a 75 % floor, so the locks hold and
  everything stays visible** (0.75 x the normal factor, in 5 % steps, D1). **If the window is too
  small even at 75 %, "locked sizes give way":** locked tiles shrink in proportion so everything
  fits, their positions stay locked, and their sizes come back when the window grows again. This is
  the user's own exception, for this case only, logged and shown in a gold banner.
- **The same gate everywhere:** `gy__tile_may(node, action)`, written for containers from the start,
  used by the mouse, keys, the loader, reset, and `gy_split(..., GY_LOCK_SIZE)` dividers (and later
  `gy_dock` strips, automatic layouts and inner windows). A refusal changes nothing and shows
  "Locked: <name>" for 2 s; a refused tile key is eaten. Padlocks on locked titles, no resize cursor
  on locked dividers, no drop zones over SPLIT tiles. Reset is Alt+Shift+Backspace.
- `GUICY_TILING_FULL`: user locks on unlocked tiles (`gy_allow_user_lock(1)`, Alt+L) and layout undo
  (Alt+Backspace, 16 lines), both through the gate.
Proof: per flag, scripted drags, swaps, resizes, closes, floats and key moves on a locked tile are
refused and the saved line stays byte-equal, while the same work on an unlocked tile; the same for a
locked `gy_split`. A hostile hand-edited line loads with every locked tile back on its invariant.
**Lock fuzz:** 10,000 random mouse, key, load, reset and resize events; after each, every locked tile
meets its invariant, and over the run an unlocked tile moved and one resized. **Too-small screen,**
stubbed work area at three sizes: fits at 100 % (nothing changes); fits only by zooming (zoom lowered,
locked rects unchanged in DIPs, nothing clipped); below the floor (0.75 x the normal factor, locked
tiles shrunk in proportion, tree paths unchanged); grown back, zoom and locked sizes return exactly.
Tile keys on a locked tile never reach the app; `old_layouts` equal.
Budgets: +4 KB default, +1 KB full.

---

## 5. The app ports (A1-A3)

Each port happens only with the user's OK, on a branch through Master Control, never while that
app's session edits the same files. A1 is the pilot: A2 and A3 start only after the user has OK'd
A1's before/after sheet.

**For each app, before any code changes:**
- **(a) The API map:** every Nuklear call and every helper (nk_ext_row, nk_ext_wrap_label,
  nk_ext_splitter, nk_ext_tooltip, nk_chips, nk_section, nk_dash_line, label_fit, splitter,
  styled_button, the theme tables) mapped to `gy_` calls, with every **gap named**. A gap becomes a
  small core addition or an early W1/W2 piece, decided with Master Control; nothing else from the W
  steps is pulled in.
- **A timing hook** in the app's own loop, so frame numbers come from inside the app.

**During the port:**
- **(b) bbb_kit:** it draws in its own window through `gy_attach` (bbb_kit.c:1897-1924, already
  per-monitor v2 at :1923). Its card grid is its own GDI+ code: the grid draws into a DIB that
  becomes a GUICy image (`gy_image_update` on change), or moves to `gy_grid` plus canvas, decided at
  the port.
- **(c) Contrast self-tests:** voxup (pick.c:2189-2214) and bbb_kit (bbb_kit.c:533, :2045-2048) check
  WCAG 4.5:1 on drawn text. They read H1's text-record hook instead of Nuklear's state, and must pass
  with the same thresholds.
- **(d) Ini and layout:** old splitter positions in the ini load as 96-DPI px into `gy_split`'s DIPs;
  window sizes likewise; nothing the user set is lost.
- The apps keep their own file and folder dialogs (pixelhunter.c:243 and :271, voxup pick.c:3056 and
  :3067), so W4's dialogs aren't needed.

**Sign-off:**
- **(e) The before/after sheet:** today's Nuklear picture at 100 % (the original, beside), GUICy at
  100 % and GUICy at 125 %, for each selftest scene, plus a 1540-DIP window on the portrait monitor
  (capped to the work area). Frame, idle and size numbers against step 0. The same app actions logged
  for the same script.
- **The first-port checklist,** checked by the user: the before/after sheet, a real IME, and visible
  check B or C if they were declined earlier (section 7).
- **(f) Rollback:** Nuklear stays in each app behind a build switch until the user signs off **and**
  one release has shipped with GUICy. Each app stays one exe either way.

A1 is milestone M3; A2 and A3 together are M4.

---

## 6. After the ports

### G1. The GPU path, opt-in (default contender only if G0 allowed it)

Deliverable:
- **Drawing:** D3D11 FL 11.0 loaded at run time; one `DrawInstanced(4, n)` over the same 48-byte
  instances; clip index per instance; R8 glyph atlas and image pages in texture arrays. Shaders made
  by `tools/gyshader` through d3dcompiler_47.dll from System32, **at build time only** (never copied
  into the repo or onto C:, never loaded at run time), checked in as byte arrays so a build without
  it works.
- **The shader copies the CPU's integer maths:** images (and GPU image scaling) by `Load()` of 4
  texels with the CPU's 8-bit weights and rounding; glyphs by point loads; coverage quantised as
  `floor(c x 255 + 0.5)` and scaled as `gy__scale`; rounded corners from F1's coverage cache.
- **Swap chain:** `FLIP_DISCARD`, 2 buffers, `Present(1, 0)`, a full frame on every changed frame;
  the tile hash pass still decides whether a frame changed. `Present1` dirty rects are deferred.
- **Occlusion and idle:** draw and present only on change; `DXGI_STATUS_OCCLUDED` counts as success;
  never poll with `DXGI_PRESENT_TEST` or a timer; a minimized window draws nothing and marks all dirty
  on restore; the latency waitable joins the wait only while a frame is pending.
- **The child window:** the swap chain lives on an enabled child over the client area that answers
  `WM_NCHITTEST` with `HTTRANSPARENT` (Windows sends the mouse to the parent) and hands focus back on
  `WM_SETFOCUS`; its context sits in `GWLP_USERDATA`. **No black frame:** the child is made visible
  with a NULL background brush over the parent's last BitBlt, and the parent gets `WS_CLIPCHILDREN`
  only after the first present returns, so until then the CPU frame shows through.
- **Device loss from every call:** `DXGI_ERROR_DEVICE_REMOVED`/`RESET` from any call (`Map`,
  `ResizeBuffers`, `CreateTexture2D`, `Present`) starts recovery: `ClearState`, `Flush`, release,
  destroy the child, make a new child and device, retry once; else switch to the CPU path in the same
  frame (GDI BitBlts to the parent, which was never bound to a flip chain).
- **Paths:** RDP, software adapters and the Basic Render Driver use the CPU path.
  `GUICY_RENDER=cpu|gpu|warp` and `gy_desc.render` choose; `gy_renderer_name()` and a log line say
  which and why. `GUICY_NO_GPU` drops it all.
Proof:
- **Pixel classes:** a 1-bit mask from the CPU renderer (coverage under 255, alpha under 255, a
  glyph or a filtered image) marks pixels allowed 1 level per channel; all others equal; a histogram
  in BENCH.md. Every golden, hardware GPU against CPU, by offscreen readback; WARP on a small set.
- **No black frame:** a windowed readback (`PrintWindow` with `PW_RENDERFULLCONTENT`) between child
  creation and the first present shows the CPU frame.
- **Device loss:** a hook returns `DEVICE_REMOVED` from each of `Map`, `ResizeBuffers`,
  `CreateTexture2D` and `Present` in turn; each run finishes its script on a new child or on the CPU
  path, ending on the CPU golden.
- **Counts:** with `GUICY_RENDER=gpu` the GPU draws as many frames as the CPU path for the same
  script; idle is 0 frames and 0 presents over 60 s.
- **The race, only if G0 allowed a GPU default:** a real window per path on the test desktop at
  1540x950, 1080p and 1440p (exact client size by a hook, work area stubbed larger). The CPU side of
  each frame is timed with `QueryThreadCycleTime` from taking the input to the return of the present
  call, the GPU side by timestamps; **9 or more runs**, other CPU and GPU load logged. Per case and
  path it records the presents that returned OCCLUDED and the present call's own time. The user's
  rule: the GPU is the default only if it wins **every case once running** (hover, caret, typing,
  slider, progress, scroll, resize, full redraw, tile focus, tile drag). A tie (inside the spread)
  loses; **a case whose winner flips between runs counts as a CPU win**; a win smaller than the
  present-call time is not proven and loses until visible race A confirms it. First frame and start
  to window are reported, not decided on.
Budgets: +20 KB; GPU-path frame CPU side 0.5 ms for 5,000 glyphs and 500 rects (reported).

### G2, T1, P1: containers, drop zones and panel docks (default parts). Milestone M5

- **G2:** i3-style containers with any number of children; the default build keeps side-by-side,
  stacked and tabbed (other arrangements are in `GUICY_TILING_FULL`). Pools grow to 64 tiles and 16
  tabs. Lines start `gy2:v<n>:`; a line starting with `[`, `H` or `V` is today's format and is
  converted; the loader bounds-checks everything. **Tab overflow:** the strip shows the tabs that fit
  around the active one and a » button listing all. Proof: every tiling and L1 test passes on the new
  tree; `old_layouts` geometry equal; 10,000 fuzzed lines never crash; 8 and 16 tabs at 3 widths with
  a scripted » pick. +4 KB default, +2 KB full.
- **T1:** dragging a title shows 5 zones per tile with a tinted preview of the exact rect; a drag
  starts after 4 DIPs; all through the gate. Full build: Alt+drag, Alt+right-drag, outer-edge
  strips, preselection, a divider menu. Proof: a scripted drag into each zone; round trip; the lock
  fuzz with drags; a click on a title moves nothing. +2 KB default, +3 KB full.
- **P1:** `gy_dock(TOP/BOTTOM/LEFT/RIGHT, size)` (lockable), row items `GY_PX`, `GY_PCT`,
  `GY_STRETCH`, `GY_AUTO` with min/max, N-way splits, and on overflow stretch items shrink, then
  low-priority items fold into the » menu (text is never cut). Full build: strictness, wrap rows,
  sticky headers. Proof: a 30-size resize sweep never leaves blank space or overlaps (rect checks);
  `gy_row` goldens byte-equal; a locked dock refuses its drag; compare.md re-run. +2 KB default.

### On the user's ask (designs kept, not scheduled)

These serve the user's library-wide goals ("every type of window, button, slider, table, list and
picker", "the tiling-WM features"), not the three apps. Each is built only when the user asks,
after the ports, sized against what the ports showed. W1 and W2 pieces come earlier only if a
port's API map names them as a gap.
- **T2 layout types:** side by side, stacked, tabbed, master + stack, grid, spiral, centred master,
  monocle, as pure functions with locked tiles placed first; count rules; Alt+Space cycles.
- **T3 workspaces:** Alt+1..9, send a tile, pin to all, one saved line.
- **T4 floating, scratchpad, auto-hide tiles** on an overlay layer; FLOAT-locked tiles refuse.
- **T5 slots, regions, attention dots, a closed-tiles strip, gaps, focus follows mouse.**
- **T6 resize mode, the Alt-held minimap, an overview of last-drawn tile pictures, presets, slide
  animations** (off by default, 0 dirty tiles once a slide ends).
- **W1 lists and tables** on the F1 clipper: multi-select, keys, sortable resizable columns, a frozen
  column, no sideways scrollbar (columns shrink with an ellipsis, then hide behind »). Proof by
  counts: 100 and 100,000 rows format, push and dirty the same.
- **W2 menus and tabs:** menubar, submenus, check and radio items, F10 navigation, Alt letters through
  the clash check, `gy_tabs` on the G2 strip.
- **W3 every button, slider and edit box,** including a multi-line edit box with IME.
- **W4 pickers:** colour, date, time, searchable combo, key and font pickers. The file dialogs are
  deferred further: the apps have their own.
- **W5 inner windows and in-app modal dialogs,** lockable through the gate.
- **W6 a second OS window and `gy_ctx_set`:** a pointer context or a swap-on-change static struct,
  judged by code size, a disassembly check that hot loops reload no context pointer, and
  `QueryThreadCycleTime` 31-run medians above the spread; routing by `GWLP_USERDATA`; one `gy_frame`
  for all windows; `SW_SHOWNOACTIVATE`; pop-out of unlocked tiles. Real focus is visible check C.
- **Later, only with a number or a picture the user prefers:** `Present1` dirty rects and scroll
  rects, a command palette, toasts, ClearType, the exact Gaussian shadow, colour emoji, the strip
  layout, UI Automation, the Linux GL 3.3 backend, C1 (a tile thread pool, only if a full redraw
  misses its budget after F1).

---

## 7. Open items and questions

**Waiting on the user (Master Control is asking):**
- **O1. The zoom's GPU stretched preview** ("Or use the GPU to scale"). Honest numbers: with the
  two-pass rule the preview shows first and the crisp frame one refresh later, while D1's crisp
  frame takes 10 ms or less, under one 60 Hz refresh; the preview adds a blurred frame, a copy on
  every drawn frame and about 15 MB of VRAM at 1440p. Until the user answers, D1 ships the crisp
  zoom with no preview. If the user wants it, it is built as the ruling defined it, on the GPU path
  only: a kept copy of the last frame stretched as one image instance (no new shader), drawn on the
  first pass after a zoom event and replaced by the crisp frame on the settle pass, never the
  resting frame; covering `WM_DPICHANGED` during a drag; its KB and memory counted in the GPU
  budget. Its proof then lives in D1: by counts, the last frame drawn is never a preview, the crisp
  frame comes within 1 frame after the input stops, then 0 frames and 0 presents.
- **The A1 pilot:** the user's OK to port bbb_kit, then their OK on its before/after sheet.

**The visible checks,** the only things that need a window on the user's screen, each asked
separately through Master Control when its step is done: **A.** the visible race (only if G1 is a
default contender), **B.** the mouse over the GPU child with `GUICY_RENDER=gpu` (once G1 is built),
**C.** a restored second window doesn't take focus (once W6 is built). The protocol: the user names
the time; one window is shown with `SW_SHOWNOACTIVATE`; a run counts only if every present returned
`S_OK`, otherwise it stops and reports. If the user declines B or C, they go on the first-port
checklist of the next port.

**Answered by Master Control:** d3dcompiler_47 at build time only; the order in section 2, with its
reasons.

## 8. Budgets and risks

| Thing | Budget | Today |
|---|---|---|
| Core KB, (i) and (ii), `GUICY_NO_GPU`; default build with the GPU path | **125 kB** (user, 11-23); 145 kB | **95.7 KB** (i), after F1 |

**KB budget per step** (kB = 1,000 bytes, code + data of `guicy.o`, `GUICY_NO_GPU`; written 2026-11-23 after F1 came in at
+9.6 KB against its +4 estimate; the actual goes in each done-report beside its budget):

| Step | Budget | Running total | Measured |
|---|---|---|---|
| (F1 done) | +4 (estimate) | | **+9.6, total 95.7** |
| H1: gy_attach/gy_event, the OS table, the test desktop guard, the test font hook, the text-record hook (tests only) | +2.5 | 98.2 | **+2.1, total 97.8** |
| G0: the spike lives in `bench/gyspike.c`, outside guicy.h | 0 | 98.2 | |
| D1: one scale function, the em theme resolved per factor, monitors, zoom keys, high contrast | +6 | 104.2 | **+9.0, total 106.8: over, see below** |
| X1: DirectWrite fallback glyphs (5-8), IME (1.5), Tab/Shift+Tab focus (1) | +10 | 114.2 | |
| L1: locks, the gate function, placement rules, the undo ring | +4 | 118.2 | |
| A1-A3: the port hooks (timing hook, anything a port's API map finds missing) | +1.5 | **119.7** | |
| **After D1, measured:** 106.8 + X1 10 + L1 4 + ports 1.5 | | **122.3: past the 120 kB cap** | the stop rule applies before X1 |

**The user's decision (2026-11-23): the core cap is raised to 125 kB,** with everything on by default. The cap stays a hard stop: a step that would pass 125 kB stops before it is built and goes back to the user.
| G2, T1, P1 default parts | +8 | **127.7: over the cap** | |

**Stop rule (Master Control, 2026-11-23):** if a step's measured size plus the budgets still to come
passes 120 KB, the step stops before it is built and Master Control is told, so **the user decides
what gives**. Through the ports the budget holds with about 0.3 KB to spare. G2/T1/P1 as planned
would pass the cap by about 8 KB, so that decision comes to the user before G2, with the cut list
below as the options.
| `hello.exe`, `GUICY_SMALL` | 40 KB of GUICy | not measured |
| Idle, both renderers | 0 frames and 0 presents per 60 s | 0-15.6 ms CPU |
| Hover / full redraw, 1080p, CPU path (reported) | 0.15 ms / 2 ms (after F1) | 0.34 / 4.2 ms |
| Start to window (baseline.py, reported) | no worse than today's 65 ms | 53-71 ms |
| Memory at 1440p (arena, atlases, tiles) | 16 MB | about 5 MB |

**The size rule:** after each step, the measured (i) and (ii) plus the estimates of the steps still to
come must stay under 120 KB. Cuts, in this order, by measurement: (1) the tab strip's first-letter
jump and the "Locked" status text (-1 KB); (2) G2's arrangements beyond side-by-side and tabbed into
`GUICY_TILING_FULL` (already planned); (3) only then, and only with Master Control's OK, the
DirectWrite fallback and IME off by default, **last**, because CJK names showing boxes is a real loss
for pixelhunter. Even at the cap GUICy is about half of Nuklear's 249-251 KB.

Risks:
1. **Size:** the estimate reaches the cap at G2; the measured rule and the cut order above apply.
2. **Pixels:** if a class of GPU pixels can't be made exact in SM5, the fix goes in the shader, not
   the tolerance.
3. **Presentation and real input are unseen until the visible checks:** the GPU's present cost,
   mouse routing over the GPU child (a forwarder is the fix if it fails), second-window focus.
4. **The look of the ports:** at 125 % everything is 25 % bigger than today, and a 1540-DIP window
   is capped on the portrait monitor. It is the user's choice ("everything should scale"), shown on
   the before/after sheet; `GY_SCALE_DPI_ONLY` is the opt-out.
5. **Keys are busy:** Alt for tiles, Ctrl for zoom, Tab for focus, menu letters and the app's keys
   share one clash check; a clash shows a gold banner, never a silent loss.
6. **Known gaps, said plainly:** no UI Automation, so screen readers see one opaque window (fine for
   three personal tools); no colour emoji; no complex-script shaping.

## 9. Requirements trace

| Ask or note | Where | How it is proved |
|---|---|---|
| "fastest ever" | F1, G0, G1 | stress budgets; compare.md at M1, M4, M5; claims only against measured libraries |
| "smallest ever" | section 3, every step | (i)-(iv) size table, -O2 and -Os, `.pdata`/`.xdata` counted |
| "simplest usage ever" | H1, every step | one call per feature; hello.c and the 15-line example build as tests |
| "nothing outside itself" | section 3; G1; X1 | import table every step; no run-time compiler |
| "easy to import to an EXE" | H1; `GUICY_STATIC` | one define, one compile line; multi-file apps pay (i) or link with gc |
| "CPU and/or GPU"; "make sure the GPU is being used too or can be used" | G0, G1 | spike table; `gy_desc.render`; forced paths; pixel-class readback |
| "yes to the GPU if it is faster"; "every case after startup" | G0, G1 race | ≥9 runs, ties and flips lose, present-call rule, visible race A |
| "depending on the resolution everything should scale" | D1 | zero unscaled sizes; factors 1, 2, 3 with the synthetic font; look set at 96/120/144/192 DPI; monitor stubs |
| "do what vscode does and scales with the fonts too" | D1 | em theme resolved to whole pixels; three font sizes exact; zoom keys scripted, saved, clash-checked |
| "Or use the GPU to scale" | section 7, O1; G1 images | waiting on the user; crisp zoom meanwhile; GPU image scaling by the pixel rule |
| every type of window, button, slider, table, list, picker | section 6, on the user's ask | designs kept; W1/W2 pieces earlier if a port needs them |
| the tiling-WM features, other panel alignments | L1, G2, T1, P1; T2-T6 on ask | geometry, fuzz and rect checks |
| lock panels, else freely moved; "not if the dev locked the windows" | L1 | per-flag refusals; hostile line; lock fuzz shows unlocked tiles move |
| too-small screen: 75 % zoom floor, then "locked sizes give way" | L1, D1 | stubbed work area at three sizes; sizes return on growth |
| MC: the evaluated order, a reason per move | section 2 | as listed |
| MC: advanced tiling behind switches, KB with and without | section 3, G2 on | size table (iv) |
| MC: animations off, idle | section 3 | 0 frames and 0 presents per 60 s |
| MC: G1 proof (AA 1 level, solid 0; cpu/warp/removed; idle) | G1 | pixel-class mask; device loss from every call; counts |
| MC: locks hold against mouse, keys, saved layouts | L1, T1 | refusals; hostile line; fuzz |
| MC: old layout lines keep loading | section 3, G2 | `old_layouts` geometry, every step |
| MC: commit every step, push at milestones after a scan | section 3 | M1-M5 with scan counts |
| MC: no app port without OK, migration and rollback | section 5 | API map, before/after sheet, Nuklear switch until a release ships |
| MC: tab-overflow » menu | G2 | 8 and 16 tabs at 3 widths, scripted pick |
| MC: Tab focus, high contrast, UI Automation named | X1, D1, risk 6 | Tab walk by counts; high-contrast stub |
| stress fixes | F1 | budgets reported, counts pass/fail |
| compare bench lessons | section 1 | what GUICy offers beyond Win32 controls |

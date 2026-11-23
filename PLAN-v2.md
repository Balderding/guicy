# GUICy: the next plan (v2)

Draft 4, 2026-11-23 (changes in reviews/PLAN-v2-response-r1.md, -r2.md and -r3.md). Steps 0-5 of PLAN.md
are done (GUICy 0.4.0, commit e58409b, 30 of 30 tests). This plan replaces PLAN.md steps 6-13 and
PLAN-tiling.md, and folds in bench/stress.md, bench/compare.md and research/tiling-locks-gpu.md.
AGENT_RULES.md ("Always", "Desktop apps") applies to every step. Section 8 maps every ask to a step.

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
window manager inside the app with developer locks; hidden tests for free; the GPU where it wins;
canvas, images and custom widgets in tens of lines. Claims are only made against libraries
measured on this PC; until microui joins compare.md (M1) the size claim is only "smaller than
Nuklear, ImGui and raygui". compare.md is re-run at M1, M4 and M5.

## 2. The order, and why

```
F1 fixes -> H1 host + test desktop -> G1 GPU (M1) -> D1 scaling -> L1 locks (M2) -> G2 containers
-> T1 mouse -> X1 text + IME (M3) -> T2 T3 T4 T5 T6 -> P1 panels (M4) -> W1..W5 widgets (M5)
-> W6 second OS window.   Gated: C1 thread pool, A1-A3 app ports.
```

Master Control's order (G1, L1, G2 and T1, then T2-T6 and P1) is kept among those steps. Four
insertions need Master Control's OK (question 1), and **F1 starts only after that answer**:
- **F1 and H1 before G1:** G1 races the GPU against the CPU path, so the CPU path must be fixed
  first; G1's child window, D1's DPI messages and X1's IME must work for apps that own their
  window (`gy_attach`), and every windowed test needs H1's own-desktop runner.
- **D1 before L1:** it sets the one DIP-to-pixel scale that tile minimums, size locks and the
  too-small-screen rule use.
- **X1 between T1 and T2:** the widget steps then start with real text input.
- The G2 tab strip carries the » overflow menu early; W2 and P1 reuse it.

## 3. Rules for every step

- **One local commit per step**, then "done: <commit> <what>" to Master Control with the test
  count, the FAIL grep over the whole output, the exit code, the size table and the work folder
  size.
- **All runs hidden.** Offscreen tests run in gytest and gystress. **Every test that makes a real
  window runs in `gytest --windowed`**, which relaunches itself on its own desktop
  (`CreateDesktopW` plus `STARTUPINFO.lpDesktop`, as baseline.py does) and writes its results to a
  file the parent reads. In `GUICY_TESTS` builds `gy_open` and `gy_attach` refuse to run on the
  "Default" desktop (`GetThreadDesktop`), with an error naming the reason. This holds from H1 on.
  No window on the user's screen, no focus, no `start`, one run at a time, no priority or
  affinity changes. GPU and WARP runs are short batches.
- **Never touch system settings.** OS calls GUICy reads (imm32, `SystemParametersInfo`, DPI and
  monitor functions, d3d11/dxgi) go through one function table (H1); tests swap in stubs.
- **Goldens:** every golden stays byte-equal unless the step says it changes pictures. A planned
  change shows old and new PNG side by side with the changed-pixel count and the largest channel
  difference, and the new hash is committed with a reason.
- **Old layouts keep loading:** at the start of F1 the 0333d67 build saves the layout line of
  every tile_* test, plus a 6-tile line with a stack and a zoom, into
  `tests/golden/layouts-2026-11-22.txt`, and beside each line a **geometry file**: every tile's
  name and rect, stack membership, active tab, focus and zoom. Test `old_layouts` loads each line
  at every step (at scale 1.0) and the geometry must be equal; it is never re-baselined. A picture
  check sits beside it and follows the golden rule above.
- **Size table** in BENCH.md every step, -O2 and -Os:
  (i) `guicy.o` text+data, no gc, W groups off: the honest upper bound, and what an app that calls
  GUICy from several files pays without linker flags;
  (ii) `core.exe` calling one function of every default feature (a tile, a lock, a dock, an edit
  box with IME, scaling), GUICy as its own translation unit (`guicy_impl.c`), linked with
  `-ffunction-sections -Wl,--gc-sections`; GUICy's share is `guicy_impl.o`'s whole contribution in
  the lld map file (code, `.rdata`, tables), not a sum of names;
  (iii) `hello.exe`, one button, the same way, default and with `GUICY_SMALL`;
  (iv) each switch on and off: `GUICY_NO_TILING`, `GUICY_TILING_FULL`, `GUICY_NO_GPU`,
  `GUICY_NO_ANIM`, `GUICY_NO_IME`, `GUICY_NO_DWRITE`, and each W group.
- **`GUICY_STATIC` is for single-file apps only:** `GUICY_API` on every prototype and definition
  is empty normally and `static __attribute__((unused))` under the switch, so unused code drops out
  with no linker flags and still builds under `-Wall -Wextra -Werror`, while normal builds keep
  `-Wunused-function` working on GUICy's own dead code. hello and core are also built this way.
- **Budgets:** the **core** (default build with `GUICY_NO_GPU`: v1 widgets, containers, locks,
  scaling, IME, fallback glyphs, docks, CPU path) is **120 KB or less** in (i) and (ii). The GPU
  path adds **20 KB or less**, so the default build with the GPU path, which an app pays even when
  the CPU is the default renderer, is **140 KB or less** and is reported beside it. `hello.exe` with `GUICY_SMALL` (= NO_TILING, NO_IME, NO_DWRITE, NO_GPU) is **40 KB or
  less** of GUICy. `GUICY_TILING_FULL` and each W group report their KB and don't count.
- **Idle stays at one tick:** under 20 ms CPU per 60 s, both renderers, at every step that touches
  the loop, a renderer or animation.
- **Animations are off by default** (`gy_anim(1)` turns them on), and stay off when Windows says
  client-area animation is off (read through the function table).
- **Imports**, read from the import table every step. Linked: kernel32, user32, gdi32. Loaded at
  run time: d3d11, dxgi, imm32, dwrite, ole32. Nothing else; no run-time shader compiler; no DLL
  of our own. Test and bench code (readback, timestamps, stubs, the test desktop) sits behind
  `GUICY_TESTS`.
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
   by `k_fill` (stosd); only edge pixels blended. Fractional canvas radii snap to 1/4 px for the
   cache key, so one rounding step can differ from today there (named, tested).
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
9. **The raw-pixel check** (`tools/check_px.py`, used by D1). A "size" is, in guicy.h's widget,
   layout and tiling sections: an integer literal of 2 or more in arithmetic with a rect field
   (`.x .y .w .h`) or passed as a size to a draw or layout helper, and every `#define` whose name
   ends in `_W`, `_H`, `_PAD`, `_GAP`, `_R` or `_SIZE`. Intended device-pixel constants (the 64-px
   dirty tile, 1024 atlas pages, the 16384 frame limit) carry a `/*px*/` mark. It reports both
   counts, unscaled and marked; F1 only reports, D1 brings unscaled to zero and lists the marked
   ones in its done-report for review.

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
KB: +4 until measured. Idle unchanged.

### H1. The host: gy_attach, the function table, the test desktop, hello.c

Deliverable: `gy_attach(hwnd)`, `gy_event(msg, wp, lp)` and `gy_begin`/`gy_end` for apps with
their own window and loop (PLAN.md 4.1; bbb_kit needs it). One table of the OS functions GUICy
calls at run time (`gy__os`), so tests can stub them. `gytest --windowed` and the Default-desktop
guard (section 3). `examples/hello.c` (one button) and a corrected 15-line example written
against the real API (`gy_canvas_rect(r, fill, radius)`, `gy_theme_get()`; PLAN.md's sketch stays
as history) build as tests from a folder with only guicy.h and the .c, with one clang line and
with MinGW gcc.

Proof: every test passes on an app-made window driven through `gy_event` inside
`gytest --windowed`, with the same pictures as through `gy_open` there; the guard refuses a
windowed test started on the Default desktop; hello and the example build and run their script.
From H1 on, G1, D1, L1 and X1 add their windowed tests on the `gy_event` path too.
Budgets: +2 KB; `hello.exe` with `GUICY_SMALL` 40 KB or less of GUICy.

### G1. The GPU renderer (D3D11), with the CPU path as fallback. Milestone M1

Deliverable:
- **Drawing:** D3D11 FL 11.0 loaded at run time; one `DrawInstanced(4, n)` over the same 48-byte
  instances from a StructuredBuffer; clip index per instance; R8 glyph atlas and image pages in
  texture arrays; big images in their own texture. Shaders made at build time by `tools/gyshader`
  through the OS's d3dcompiler_47.dll and checked in as byte arrays.
- **The shader copies the CPU's integer maths** where SM5 allows: images by `Load()` of 4 texels
  with the CPU's 8-bit weights and rounding (no sampler filter); glyphs by point loads; coverage
  quantised as `floor(c * 255 + 0.5)` then scaled and rounded as `gy__scale`; rounded corners
  from F1's corner-coverage cache uploaded as a small texture.
- **Full frame on every changed frame:** one draw covers every pixel. The CPU tile hash pass still
  decides whether a frame changed and which rects go to `Present1` (`FLIP_SEQUENTIAL`, 2
  buffers); its cost counts in the GPU path's CPU ms. **A full present** (no dirty rects) follows
  any `DXGI_STATUS_OCCLUDED`, restore, `ResizeBuffers`, scale change or renderer swap, so nothing
  changed while hidden stays stale. A frame with no dirty tile skips upload, draw and Present.
- **Occlusion and idle** (replaces PLAN.md 3.9 for the GPU path): draw and present only on
  change; OCCLUDED counts as success; never poll with `DXGI_PRESENT_TEST` or a timer; a minimized
  window draws nothing and marks all dirty on restore; the latency waitable joins the wait only
  while a frame or an animation is pending.
- **The swap chain lives on a child window** over the client area. It is enabled and answers
  `WM_NCHITTEST` with `HTTRANSPARENT`, so Windows sends the mouse to the window beneath in the
  same thread, the parent: no forwarding code. Keys go to the parent, and the child hands focus
  back on `WM_SETFOCUS`. GDI never presents to a flip-model window, so after a fallback the CPU
  path BitBlts to the parent once the child is gone. The child is made only when the first GPU
  frame is ready (no black frame).
- **Fallback chain:** hardware adapter; Basic Render Driver, software adapters and RDP use the CPU
  path; `DEVICE_REMOVED`/`RESET` retries once, then switches to the CPU path in the same frame.
  `GUICY_RENDER=cpu|gpu|warp` and `gy_desc.render` force a path; `gy_renderer_name()` and a log
  line say which and why. The device is made on a worker thread while the first frames draw on
  the CPU (`GUICY_RENDER=gpu` waits for it instead). `GUICY_NO_GPU` drops it all.
- `gy_stats` gains `presents`, `gpu_draws`, `cpu_draws` (logged for baseline.py). Timestamps
  (disjoint query, read 2-3 frames late, only while frames run) and readback are `GUICY_TESTS` only.

**The race harness:** `gytest --windowed --race` on its own desktop runs a real window per path at
1540x950, 1080p and 1440p and replays each case's script through the window's own loop. A frame
is timed (QueryPerformanceCounter) from taking the input to the return of the present call, so
`BitBlt`, and the hash pass, upload and `Present1`, are inside each path's CPU ms; the GPU path
adds its timestamp ms. Median of 3 runs with the spread; offscreen numbers beside, for reference.
The hidden desktop is occluded, so a present call may return without doing its work: per case and
path the race also records how many presents returned `DXGI_STATUS_OCCLUDED` and the median time
of the present call alone.

**The default renderer** (the user's answer: "every case after startup"): the GPU is the default if
it is faster than the CPU path in **every case once running**: hover, caret, typing, slider,
progress, scroll, resize, full redraw, tile focus, tile drag. A tie (inside the run-to-run spread)
is a loss. First frame and start to window are reported but left out of the decision, because the
GPU can't take part there yet (the CPU draws the first frames while the device is made).
**A GPU win smaller than its own present-call time, timed while occluded, is not proven, and
counts as a loss.** So a GPU default needs **one visible run**: at G1 time, and only if the hidden
numbers allow a GPU win, Master Control asks the user for it (one window shown with
`SW_SHOWNOACTIVATE`, while the user is away, the race plus the checks listed for it). It is never
assumed. Until it has run and the GPU wins every case there, the CPU path stays the default, and
BENCH.md says "GPU wins not proven with presentation". Otherwise too the CPU path stays the
default. BENCH.md gets every case either way, with which path
drew each case, working set, idle, other GPU load during the runs (nvidia-smi if present), and the
note that it is one PC with composition excluded. Hardware, not remote and not Basic Render
Driver stay coded conditions; apps can still choose with `gy_desc.render`.

Proof:
- **Pixel classes, defined mechanically:** in `GUICY_TESTS` builds the CPU renderer writes a 1-bit
  mask of pixels touched by coverage under 255, colour alpha under 255, a glyph or a filtered
  image. Masked pixels: **at most 1 level per channel**; all others **equal**. A histogram of
  differences in BENCH.md. Every golden, hardware GPU against CPU, by offscreen render target and
  staging readback.
- **WARP on a small set only** (one scene per kind, small windows): WARP against CPU and against
  hardware, same rule.
- **Child window:** a windowed test sends `WM_NCHITTEST` to the child and gets `HTTRANSPARENT`;
  `SetFocus(child)` ends with the parent focused; the child stores its context in
  `GWLP_USERDATA` for that hand-back. Real mouse routing over the child (clicks, capture, cursor,
  wheel) can't be driven on a hidden desktop, so it is **listed for the visible run, with
  `GUICY_RENDER=gpu` forced** whatever the default is, and again at the first port. The M1 report
  says GPU-path input is unverified until then; if it fails, a forwarder is added.
- **Full-present rule:** a unit test with DXGI stubs in the function table: after occluded,
  restore, resize, scale change and swap, the next `Present1` has no dirty rects; after that, only
  the dirty tiles.
- **Windowed runs** that finish their scripts and end on the CPU golden: forced cpu, forced warp
  (small window), and a hook that makes `Present1` return `DEVICE_REMOVED` halfway; the picture
  after the switch equals the CPU picture (readback before, the DIB after).
- **The same frames are drawn:** with `GUICY_RENDER=gpu` (device made before the first frame),
  `gpu_draws` equals the CPU path's drawn-frame count for the same script; in the default
  (worker-thread) mode, `gpu_draws + cpu_draws` equals it. `presents` is 0 over 60 s idle and over
  a 60 s wiggle over empty space; idle under 20 ms CPU per 60 s.
- The every-case table; compare.md re-run with the GPU path and microui (small GDI renderer).

Budgets: +20 KB or less (shader bytes about 2 KB, test code not counted); GPU-path frame CPU side
**0.5 ms** for 5,000 glyphs and 500 rects; a tile drag **1 ms** CPU per frame.

### D1. Everything scales: DPI, resolution, one factor

The user's ask: **"depending on the resolution everything should scale".**

Deliverable:
- **One factor, one function.** GUICy works in 96-DPI units (DIPs) everywhere, and one function,
  `GY__PX(dip)`, turns DIPs into pixels with the current factor. Everything goes through it:
  fonts, row heights, padding and gaps, radii, border widths, icons, check marks, scroll bars,
  splitter and title-bar sizes, the tile minimum sizes, locked tile sizes, the tooltip and banner,
  and images shown "at their size".
- **Whole pixels, always:** each rect edge is rounded on its own (`floor(dip * f + 0.5)`) and
  widths come from the edges, so neighbours never gap or overlap; a line or border width is
  `max(1, floor(w * f + 0.5))`, so a 1-DIP border stays a crisp 1 px at 125 % (never a blurred
  1.25), and is 2 px at 150 % and 200 %.
- **The factor follows the monitor:** per-monitor DPI v2 (`SetProcessDpiAwarenessContext` through
  the function table; an app manifest or an attached app's own awareness wins; an unaware process
  is scaled by Windows and GUICy sees 96). `WM_DPICHANGED` takes the DPI only from `wParam` and
  the rect only from `lParam`. Because two monitors at the same DPI but different resolutions send
  no `WM_DPICHANGED`, GUICy also re-reads the monitor on `WM_DISPLAYCHANGE` and when the window
  moves to another monitor (`MonitorFromWindow` changes on `WM_WINDOWPOSCHANGED`).
- **Moving between monitors:** the monitor is the one under the window's **centre**. The switch is
  applied on `WM_EXITSIZEMOVE` (at once outside a move), and the resized window keeps its centre,
  so the centre stays on the new monitor and the factor can't bounce back.
- **Modes:** `gy_scale_mode(GY_SCALE_DPI_AND_RESOLUTION)` is the default: factor =
  max(DPI / 96, min(w, h) / 1080), rounded to the nearest 25 % step (an exact half rounds up, so
  1.125 gives 125 %), using the monitor's
  **shorter** side so a portrait screen isn't over-scaled. A 4K screen left at 100 % Windows
  scaling gets 200 %, not tiny text. `GY_SCALE_DPI_ONLY` follows only Windows; `GY_SCALE_FIXED`
  with `gy_scale_set(f)` is the app's own number. `gy_scale()` returns the factor in use.
- **On the user's own monitors** (read hidden with tools/monitors.py): DISPLAY2, the main one,
  2560x1440 at 96 DPI, and DISPLAY3, 1440x2560 portrait at 96 DPI, both get **125 %**, where the
  apps draw at 100 % today. That is the user's intent ("depending on the resolution everything
  should scale"); the first app port shows the user a before/after at 125 % before sign-off.
- **The too-small-screen zoom** (L1) is relative, like browser zoom: it multiplies the monitor's
  normal factor in steps of 5 % of it, down to 0.75 times it (about 94 % on the user's 125 %
  screens). Each step re-makes the fonts once; budget 10 ms a step.
- **Opening size:** a window whose DIP size times the factor is bigger than the work area opens at
  the work area's size (the UI scale is untouched); only a minimum that doesn't fit starts the zoom.
- **Units:** every API is in DIPs: `gy_desc`, `gy_row`, `gy_avail`, `gy_last_rect`, canvas
  coordinates, mouse positions, `gy_split`'s `*pos`, size locks, `gy_dock`. **`gy_rect` becomes
  float DIPs** (no app is ported yet), and GUICy returns each rect as its pixel edges divided by
  the factor, so a canvas call at those DIPs rounds back to exactly the same pixel edges. Canvas
  coordinates go through the same edge rounding; hit tests run in device pixels against the
  rounded rects, never in DIPs. `gy_image(w, h)` is DIPs and scales nearest by default (pixel art
  stays sharp), bilinear by option; `gy_canvas_image(..., GY_IMG_DEVICE_PX)` draws 1:1 device
  pixels for previews. Layout lines store ratios, so they load the same at any factor.
- On a change: fonts re-made, glyph atlas and width cache flushed, everything dirty.

Proof:
- **No pixel constant left unscaled, two ways:** `tools/check_px.py` (F1) reports zero unscaled
  sizes; and every golden scene is drawn at factors 1, 2 and 3 with a **synthetic test font**
  swapped in through H1's table (stubs for `GetGlyphOutlineW` and `GetTextMetricsW` whose
  advances, heights and bitmaps are exact multiples of the size), and each instance rect must be
  the factor-1 rect scaled by the rounding rule (an instance-list check). Real hinted fonts don't
  scale exactly, so the real-font goldens below are for the look.
- **No seams at 125 %:** a canvas rect drawn at `gy_avail()` covers exactly the panel's pixels, and
  a click on each 1-px border hits the widget the pixel belongs to.
- At 96 DPI and factor 1 every existing golden is byte-equal and `old_layouts` equal.
- New goldens at 96, 144 and 192 DPI for every widget and tile test (CPU; GPU by the G1 rule).
- **Monitor stubs:** 3840x2160 at 96 DPI picks 200 %; 2560x1440 at 96 picks 125 %; **portrait
  1440x2560 at 96 picks 125 %**; 1920x1080 at 144 picks 150 %; each mode checked the same way. A
  1-DIP border is exactly 1 px wide at 125 %.
- A windowed script sends `WM_DPICHANGED` 96 to 144 to 120 with `wParam`/`lParam` set and ends on
  the same picture as a fresh start at 120; a stubbed move from a 1080p to a 4K monitor at 96 DPI
  changes the factor with no `WM_DPICHANGED` and settles after one switch. The client size in DIPs
  is kept. A 1540-DIP window on the portrait 1440x2560 stub opens inside the work area at 125 %.
Budgets: +5 KB; a scale-change frame 10 ms or less.

### L1. Developer locks. Milestone M2

Deliverable (research 2.9):
- **The code layout:** `gy_tile_rule(name, {.side, .w, .h, .tab_of, .lock})`. The code layout is
  the tree built from the rules and the app's open order with no saved line. A tile the app opens
  later (a Log when a job ends) takes its rule place at first open and joins the code layout then.
  "Reset" returns to the code layout.
- **Flags:** `GY_LOCK_MOVE`, `GY_LOCK_SIZE`, `GY_LOCK_CLOSE`, `GY_LOCK_FLOAT` (also no pop-out,
  W6), `GY_LOCK_TABS`, `GY_LOCK_SPLIT` (nothing splits it or lands in it), `GY_LOCK_ALL`;
  `gy_layout_lock(1)` fixes the whole layout, adding to the tiles' own locks and never clearing
  them when turned off.
- **What each flag holds** (the invariants the tests check): MOVE: the tile's path in the tree
  (parent container, index, stack) is unchanged; a sibling closing may shorten it (allowed,
  logged). SIZE: its width or height on the locked axis, in DIPs, is unchanged (except below the
  zoom floor, next item). MOVE + SIZE: the rect in DIPs is unchanged. CLOSE, FLOAT, TABS, SPLIT:
  the action never happens.
- **"Not if the dev locked the windows":** a code lock can't be undone by the user: no key, click,
  ini line, setting or override, and no API lets a developer give that power away. Locks are never
  read from a saved line; after every load they are applied again (one log line). The one
  exception is the user's own, below.
- **Too-small screen (the user's answer):** the window's minimum size is raised to fit the locked
  sizes plus the other tiles' minimums, capped at the monitor's work area (`gy__os.work_area`, so
  tests can stub it). When that doesn't fit, **first GUICy shrinks its own UI scale (like a
  browser zoom) down to a 75 % floor, so the locks hold and everything stays visible** (the floor is
  relative: 0.75 times the monitor's normal factor, in 5 % steps, D1). **If the window is too small
  even at 75 %, "locked sizes give way":** locked tiles shrink in proportion
  so everything fits, their positions stay locked, and their sizes come back when the window grows
  again. This is the user's own exception to "a developer lock can never be undone by the user",
  for this case only. The zoom and the give-way are logged and shown in a gold banner.
- **The same gate everywhere:** `gy__tile_may(node, action)`, written for containers from the
  start, used by the mouse, keys, the loader, reset, the automatic layouts of T2, and by
  `gy_split(..., GY_LOCK_SIZE)` dividers, P1's `gy_dock` strips and W5's inner windows. A refusal
  changes nothing and shows "Locked: <name>" for 2 s; a refused tile key is eaten, never passed on.
- **Reset** (Alt+Shift+Backspace) goes through the gate. Padlocks on locked titles, no resize
  cursor on locked dividers, no drop zones over SPLIT tiles.
- **In `GUICY_TILING_FULL`:** user locks on what the developer left unlocked
  (`gy_allow_user_lock(1)`: a padlock in the title, Alt+L, a whole-layout toggle; saved with a `u`
  marker) and layout undo (Alt+Backspace, the last 16 lines), both through the gate.

Proof:
- Per flag: scripted drags, swaps, resizes, closes, floats and key moves on a locked tile are
  refused and the saved line stays byte-equal; on an unlocked tile they work. The same for a
  locked `gy_split` divider.
- **A hostile line** (hand-edited to move, resize and "unlock" locked tiles) loads with every
  locked tile back on its invariant.
- **Lock fuzz:** 10,000 random mouse, key, load, reset and window-resize events (plus user locks
  and undo in the full build). After every event each locked tile meets its flags' invariant, and
  over the run at least one unlocked tile moved and one resized.
- **Too-small screen, stubbed work area at three sizes:** fits at 100 % (factor unchanged, locked
  rects unchanged); fits only at 75-100 % (factor lowered, locked rects unchanged in DIPs, no
  clipped pixel); below the floor (factor 75 %, locked tiles shrunk in proportion, their tree paths
  unchanged, nothing clipped). Then the stub grows back: the factor and the locked sizes return
  exactly.
- Tile keys on a locked tile never reach the app; goldens of padlock, locked divider, "Locked" text
  and banner at 96 and 144 DPI; `old_layouts` equal (old lines carry no locks).
Budgets: +4 KB default, +1 KB full; a refused drag 0.1 ms or less; idle unchanged.

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
tinted preview of the exact rect; a drag starts only after 4 DIPs. In `GUICY_TILING_FULL`:
Alt+drag anywhere and Alt+right-drag to resize from the nearest edge, outer-edge strips of the
window, preselection (Alt+Ctrl+arrow, Esc cancels), a right-click menu on dividers. All through
the gate.
Proof: a scripted drag into each zone (and each full-build extra) with a golden; save and load
round trip; the lock fuzz again with drags; a click on a title moves nothing.
Budgets: +2 KB default, +3 KB full; a drag frame 1 ms CPU (GPU path), 2 ms (CPU path), 1540x950.

### X1. Text: fallback glyphs and IME. Milestone M3

Deliverable:
- **DirectWrite only for glyphs the GDI font lacks** (CJK, symbols, emoji in file names). GDI stays
  the look, so no golden changes; `gy_text_engine(GY_TEXT_DWRITE)` switches an app wholly;
  `GUICY_NO_DWRITE` drops it. **Out of scope:** colour emoji (the atlas is A8, so emoji are one
  colour) and shaping of Arabic, Hebrew, Indic and Thai.
- **IME:** `WM_IME_STARTCOMPOSITION/COMPOSITION/ENDCOMPOSITION` (`GCS_COMPSTR`, `GCS_RESULTSTR`),
  composition and candidate windows at the caret, the composition string underlined in the edit
  box; imm32 through the function table; `GUICY_NO_IME` drops it.
Proof: goldens of a mixed-script list (Latin, Japanese, Chinese, Korean, a symbol, an emoji) at 96
and 144 DPI; old goldens byte-equal; glyph misses 0 after the first frame. IME, windowed: stub
imm32 functions in the table, real `WM_IME_*` messages sent to the window, so the real handler
runs: a composition shown, changed, committed and cancelled, then typing still arrives; undo
removes a commit in one step. A real IME is the user's check at a port.
Budgets: +3 KB IME, +3 KB fallback; the first fallback (dwrite and the font collection) 30 ms or
less, once; a later fallback miss 100 us or less.

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
number to jump; named presets; slide animations, off by default, through a per-clip offset table
so a slide re-hashes only moved tiles' offsets; `GUICY_NO_ANIM` removes the code.
Proof: slide frame cost on both renderers; **0 dirty tiles once it ends**; idle one tick with
animations on and nothing moving; with the OS setting stubbed to "off" no slide runs.
Budgets: +5 KB (full); a slide frame 1 ms (GPU path), 4 ms (CPU path) at 1540x950.

### P1. Panel layouts inside a tile. Milestone M4

Default build: `gy_dock(TOP/BOTTOM/LEFT/RIGHT, size)` for toolbars, status bars and side strips
(call order decides the corners), lockable through the gate; row items sized `GY_PX`, `GY_PCT`,
`GY_STRETCH`, `GY_AUTO` with min/max, so left, centre and right alignment need no special case;
N-way splits with min/max and percentages; on overflow, stretch items shrink first, then the
lowest-priority items fold into the » menu (text is never cut). `GUICY_TILING_FULL`: a
strictness value per item (Fleury), wrap rows and sticky section headers.
Proof: goldens at 3 widths and 3 DPIs; a 30-size resize sweep (640x400 to 2560x1440) never leaves
blank space or overlaps (rect checks, not only pictures); `gy_row` goldens byte-equal; a locked
dock refuses its drag. compare.md re-run.
Budgets: +2 KB default, +3 KB full; the typical screen's UI pass still 0.2 ms or less.

### W1. Lists and tables

`gy_list` on the F1 clipper: single and multi select (click, Shift, Ctrl), keys (arrows,
Home/End, PgUp/PgDn, first letter), drag to reorder. `gy_table`: resizable and sortable columns, a
header that stays put, a frozen first column, widths saved in a line, selection, tree rows. **No
sideways scrollbar:** columns shrink to their minimum with an ellipsis, then the rightmost columns
hide behind the » menu; horizontal scrolling only if the app opts in.
Proof: goldens per state; scripted selection, sort and column drag; **counts:** 100 and 100,000
rows format, push and dirty the same numbers (times beside them).
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
Proof: goldens of every state (normal, hover, down, disabled, focused) at 96 and 192 DPI;
scripted drags, wheel, keys, and IME in the multi-line box. Budgets: +6 KB, a W group.

### W4. Pickers

`gy_color` (SV square, hue, alpha, hex and number boxes), `gy_date` (7x6 month), `gy_time`, a
searchable combo, `gy_key_pick` (press a key to bind; used to rebind tile keys in the app),
`gy_font_pick`, and `gy_file_open/save/folder` through `IFileOpenDialog`/`IFileSaveDialog` (ole32
at run time; folders by `FOS_PICKFOLDERS`). GUICy calls `CoInitializeEx` only around the dialog,
accepts `S_FALSE` and `RPC_E_CHANGED_MODE` (the app's own apartment stays), and uninitialises only
what it initialised. Opened only on the user's click, never in tests (a stub returns a path).
Proof: goldens open and closed; scripted picks; the stub proves no test opens a real dialog.
Budgets: +6 KB, a W group.

### W5. Windows inside the app. Milestone M5

Movable, resizable inner windows (title, close, min size, z-order on a layer) for apps that don't
tile, lockable (MOVE, SIZE, CLOSE) through the gate; modal dialogs drawn in the app (never an OS
message box) with Enter and Esc. Docking stays the tiling of G2-T6.
Proof: goldens; scripted move, resize, close; keys never reach widgets behind a modal; locked
windows refuse; the popup-trap test with dialogs; compare.md re-run; README's widget table with a
golden per widget. Budgets: +4 KB, a W group.

### W6. A second OS window, and gy_ctx_set

Design:
- **The context:** today all state is one static struct (`gy__c`, about 1,000 references). It
  becomes a pointer, `#define gy__c (*gy__cur)`, so no reference changes in the text; each window
  has its own context in its own `VirtualAlloc`. Measured beside it: keeping the static struct and
  swapping a context in only when a message's window differs from the current one (most messages
  go to one window, so swaps are rare). Each is judged on code size and on a deterministic proxy,
  the instruction count of the UI pass and render functions (llvm-objdump), not on timings, which
  vary more than the threshold. If the pointer adds more than 2 KB or 5 % instructions, the swap
  wins or `GUICY_MULTI_WINDOW` keeps it only for apps that ask.
- **Routing:** each GUICy window (and its GPU child) stores its context in `GWLP_USERDATA`; the
  window procedure sets the current context from the HWND for every message and puts it back after.
- **The loop:** one `gy_frame()` serves every window: one wait for all of them, then a frame for
  each window that needs one. Widgets go to a window by `if (gy_window(w)) { ... }`; `gy_frame()`
  returns 0 when the main window closes. `gy_ctx_set(ctx)` is the low-level form for `gy_attach`
  apps.
- **Focus:** a window the app opens itself (or one restored from the saved line) is shown with
  `SW_SHOWNOACTIVATE`, so it never takes focus.
- **Pop-out** (`GUICY_TILING_FULL`): a tile moves by name from one window's tree to the other's and
  back to its old place; FLOAT- or MOVE-locked tiles can't pop out; each window's locks hold in its
  own tree; the saved line records which window a tile is in.
Proof: two windows in `gytest --windowed` run scripts in turn, each matching its golden; input
posted to window B never changes window A's state; a pop-out and back gives the same geometry;
idle one tick with two windows. As a guard, not a proof: window B gets no `WM_ACTIVATE` or
`WM_NCACTIVATE` with a true flag, `GetActiveWindow()` stays on A, and B has no `WS_EX_TOPMOST`.
Real focus can't be judged on a hidden desktop, so "a restored window doesn't take focus" is listed
for the visible run. Budgets: +5 KB (W group), pop-out +3 KB (full).

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
| Core KB, (i) and (ii), `GUICY_NO_GPU`; default build with the GPU path | 120 KB; 140 KB | about 86 KB (i) |
| Core estimate after P1: F1 +4, H1 +2, D1 +5, L1 +4, G2 +6, T1 +2, X1 +6, P1 +2 | about 117 KB | |
| GPU path | +20 KB | not built |
| `hello.exe`, `GUICY_SMALL` | 40 KB of GUICy | not measured |
| `GUICY_TILING_FULL`, each W group | reported, not in the core | |
| Idle, both renderers | under 20 ms CPU per 60 s; 0 presents | 0-15.6 ms |
| Hover, 1080p, CPU path | 0.15 ms (after F1) | 0.34 ms |
| Full redraw, 1080p, CPU path | 2 ms (after F1) | 4.2 ms |
| Tile drag; 100,000-row list unchanged | 1 ms GPU / 2 ms CPU path; 0.5 ms | 2.2 ms (focus move); 83 ms |
| Start to window (baseline.py) | no worse than today's 65 ms | 53-71 ms |
| Library start to first frame (gystress) | 30 ms | 7 ms |
| Memory at 1440p (arena, atlases, tiles) | 16 MB | about 5 MB |

**The size moves are taken now**, not held as a fallback: T1's Alt+drag and Alt+right-drag, P1's
strictness value, and user locks with layout undo are in `GUICY_TILING_FULL` (about -4 KB). None
changes what the default build promises: text is still never cut (the » fold does it), and reset
stays in the core. **The trigger is measured, not estimated:** after each step, the measured (i)
and (ii) plus the estimates of the steps still to come must stay under 120 KB. If not, the next
candidates are, in order: the tab strip's first-letter jump and the "Locked" status text
(-1 KB, no behaviour change); then two that change the default and need Master Control's OK
first: the DirectWrite fallback off by default (CJK file names show boxes) and IME off by default.

## 6. Open questions

For the user: none now. **Later, at G1 time, only if the hidden race allows a GPU win:** one
visible run (section G1), asked through Master Control. It would carry the race with presentation,
real mouse routing over the GPU child (`GUICY_RENDER=gpu`), and W6's focus check once W6 exists.

For Master Control:
1. The order insertions in section 2: F1 and H1 before G1, D1 between G1 and L1, X1 between T1
   and T2. F1 starts after this answer.
2. Making the shaders at build time through the OS's `d3dcompiler_47.dll` (PLAN.md 7.2): still
   fine? Normal builds use the checked-in byte arrays.

## 7. Risks

1. **The hidden desktop hides presentation.** Presents there are occluded and may skip their work,
   and the compositor's cost and pacing aren't seen, for either path. The race counts occluded
   presents and times the present call alone; a GPU win within that time is not proven, and only a
   visible run approved by the user can prove it. Until then the CPU is the default.
2. **Pixels:** if a class of GPU pixels can't be made exact in SM5, the fix goes in the shader, not
   in the tolerance; a rule change goes to Master Control.
3. **Real mouse routing over the GPU child** (clicks, capture, cursor, wheel, drag-and-drop, touch)
   is unverified until the visible run or the first port with `GUICY_RENDER=gpu`; the fix if it
   fails is a forwarder in the child.
4. **Alt is busy:** tile keys, menu letters and app keys share one clash check; a clash shows a gold
   banner, never a silent loss.
5. **Busy PC:** every number is a median of 3 runs with its spread; GPU and WARP runs are short,
   WARP only on a small set, other GPU load logged. Work folder size is in each report.

## 8. Requirements trace

| Ask or note | Where | How it is proved |
|---|---|---|
| "fastest ever" | F1, G1, C1; section 1 | stress.md budgets; compare.md at M1, M4, M5; claims only against measured libraries |
| "smallest ever" | section 3, every step | (i)-(iv) size table, -O2 and -Os; microui added at M1 |
| "simplest usage ever" | H1, every step | one call per feature; hello.c and the 15-line example build as tests |
| "nothing outside itself" | section 3; G1; X1; W4 | import table every step; no run-time compiler; OS DLLs at run time only |
| "easy to import to an EXE" | H1; `GUICY_STATIC` | one define and one compile line; single-file apps need no linker flags, multi-file apps pay (i) or link with gc |
| "CPU and/or GPU" | G1 | pixel-class rule; forced cpu/gpu/warp; `gy_desc.render`; `GUICY_NO_GPU` |
| "make sure the GPU is being used too or can be used" | G1 | `gy_renderer_name`, `gpu_draws`, readback tests, the every-case table |
| "yes to the GPU if it is faster"; "every case after startup" | G1 | GPU default only if it wins all 10 running cases beyond the spread and beyond its present-call time, confirmed by a visible run the user approves; start cases reported, not decided on |
| "depending on the resolution everything should scale" | D1 | zero raw sizes; factor-2 instance check; 96/144/192 DPI goldens; 4K picks 200 %, 1440x2560 portrait picks 125 % |
| every type of window, button, slider, table, list, picker (plus menus, tabs, multi-line edit) | W1-W6, G2, T4 | goldens of every state at 2 DPIs; scripts; equal counts for 100 and 100,000 rows; dialog stub |
| tiling-WM features | G2, T1-T6 | goldens per layout, zone, workspace, state |
| other panel alignments | P1 | goldens at 3 widths; resize-sweep rect checks |
| lock panels, else freely moved | L1 (tiles, splits, docks, windows) | per-flag refusals; fuzz shows unlocked tiles move |
| "not if the dev locked the windows" | L1 | no override API; hostile line; lock fuzz |
| too-small screen: zoom to 75 % (relative, 5 % steps), then "locked sizes give way" | L1, D1 | stubbed work area at three sizes; sizes return on growth |
| MC: order G1, L1, G2+T1, T2-T6+P1 | section 2 | kept; insertions in question 1 |
| MC: advanced tiling behind switches, KB with and without | G2 on, section 3 | size table (iv) every step |
| MC: animations off, idle one tick | section 3, T6 | baseline.py idle; 0 dirty tiles after a slide |
| MC: G1 proof (AA 1 level, solid 0; cpu/warp/removed; idle) | G1 | pixel-class mask; windowed runs; presents 0 |
| MC: locks hold against mouse, keys, saved layouts | L1, T1 | refusals; hostile line; fuzz |
| MC: old layout lines keep loading | section 3, G2 | `old_layouts` geometry from today, every step |
| MC: commit every step, push at milestones after a scan | section 3 | M1-M5 with scan counts |
| MC: no app port without OK, on branches | A1-A3 | gated |
| MC: tab-overflow » menu | G2 (W2, P1 reuse) | goldens of 8 and 16 tabs at 3 widths; scripted pick |
| stress fixes (boxes, culling, visible range, lines, bilinear, focus outline) | F1 | budgets and counts in F1 |
| compare bench lessons | section 1 | what GUICy offers beyond Win32 controls |

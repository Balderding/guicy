# PLAN-v2: planner's response to critic round 2

Plan revised in place (draft 3, 2026-11-23). The user answered N1 and N3 and added "depending on
the resolution everything should scale"; their words are in L1, G1 and D1.

## P1

- **N1 Changed (the user's answer).** First GUICy shrinks its own UI scale down to a 75 % floor.
  Below the floor, "locked sizes give way": locked tiles shrink in proportion, their positions stay
  locked, and their sizes return on growth. The user calls this their own exception, for this case
  only. The minimum window size is capped at the work area, stubbed as `gy__os.work_area`. Tests
  cover three stubbed sizes plus growth.
- **N2 Changed (critic's fix).** `gytest --windowed` relaunches itself on its own desktop, and in
  `GUICY_TESTS` builds `gy_open`/`gy_attach` refuse to run on the Default desktop. Both are H1
  deliverables, and every windowed test from H1 on uses them.
- **N3 Changed (the user's answer).** The decision covers the 10 cases once running, and a tie is a
  loss. First frame and start to window are reported but not decided on. The table marks which
  path drew each case.

## P2

- **N4 Accepted.** A full `Present1` (no dirty rects) follows any occlusion, restore,
  `ResizeBuffers`, scale change or renderer swap, with a unit test through DXGI stubs.
- **N5 Changed (no forwarder).** The child is enabled and returns `HTTRANSPARENT`, so Windows
  routes the mouse to the parent, and `WM_SETFOCUS` hands focus back to the parent. The tests cover
  the hit-test answer and the focus return. Real routing can't be driven on a hidden desktop, so
  risk 3 is widened to "real mouse routing" and the user checks it at the first port. If that
  fails, a forwarder is added then.
- **N6 Accepted.** The hash pass is named as part of the GPU path's CPU ms, and the full present
  after a resize is in N4's rule.
- **N7 Accepted.** With `GUICY_RENDER=gpu` (synchronous device), `gpu_draws` equals the CPU path's
  frame count. In the default mode, `gpu_draws + cpu_draws` equals it.
- **N8 Accepted.** The race harness is defined: `gytest --windowed --race`, real windows on the
  hidden desktop, timed from frame start to the return of `BitBlt`/`Present1`, plus GPU timestamps,
  median of 3 runs with the spread. Offscreen numbers are for reference only.
- **N9 Changed (designed).**
  - The context becomes a pointer macro, so the text doesn't change. Its cost is measured first,
    with `GUICY_MULTI_WINDOW` as the way out.
  - Routing goes through `GWLP_USERDATA`, and one `gy_frame` serves every window with
    `gy_window(w)`.
  - Windows the app opens use `SW_SHOWNOACTIVATE`.
  - A pop-out moves a tile by name; FLOAT- or MOVE-locked tiles can't pop out.
  - The estimate is now +5 KB, plus 3 KB for pop-out.
  - `gy_ctx_set` moved from H1 to W6.
- **N10 Changed.**
  - The three moves are taken now: T1 Alt+drag, P1 strictness, and user locks with undo go to the
    full build.
  - F1 is back to +4 KB until measured.
  - The trigger is the measured (i) and (ii) plus the estimates of the remaining steps against
    120 KB.
  - A fourth candidate with no behaviour change is named. The two that change default behaviour
    (DirectWrite fallback off, IME off) are marked as needing Master Control's OK.
- **N11 Accepted.**
  - `GUICY_STATIC` is for single-file apps only, using `GUICY_API` and `__attribute__((unused))`,
    and it is built under build.cmd's `-Werror` flags.
  - GUICy's share is `guicy_impl.o`'s whole map-file contribution.
  - The trace says multi-file apps pay (i) or link with gc.

## P3

- **N12 Accepted.** A late-opened locked tile takes its rule place at first open and joins the code
  layout then.
- **N13 Accepted (rule set by Master Control).** D1 gives the rounding rule: every rect edge is
  rounded to a whole pixel on its own, and a line width is `max(1, floor(w * f + 0.5))`. That makes
  1 px at 125 % (crisp, never 1.25 blurred) and 2 px at 150 % and 200 %.
- **N14 Accepted.** H1 writes a corrected 15-line example against the real API (`gy_canvas_rect`
  argument order, `gy_theme_get`). PLAN.md's sketch stays as history.
- **N15 Changed.** The affinity setting is dropped. WARP runs only on the small set, and there are
  no priority or affinity changes (a section 3 rule).
- **N16 Accepted.** `CoInitializeEx` is called only around the dialog; `S_FALSE` and
  `RPC_E_CHANGED_MODE` are accepted, and GUICy uninitialises only what it initialised.
- **N17 Accepted.** The stub is named `gy__os.work_area`.

## The user's addition: "depending on the resolution everything should scale"

Added as D1, now titled "Everything scales". It is also in the trace table.
- One DIP-to-pixel function covers every size the coordinator listed.
- The factor follows the monitor's DPI. It is re-read on `WM_DPICHANGED`, on `WM_DISPLAYCHANGE` and
  when the window moves to another monitor, because a monitor change at the same DPI sends no
  DPI message.
- `GY_SCALE_DPI_AND_RESOLUTION` is the default: max(DPI/96, min(w, h)/1080), rounded to the nearest
  25 % step. Master Control's correction: it uses the **shorter** side, so the portrait monitor
  isn't over-scaled. `GY_SCALE_DPI_ONLY`, `GY_SCALE_FIXED` and `gy_scale_set` are available to apps.
- The plan states that both of the user's monitors (2560x1440 and the 1440x2560 portrait, both at
  96 DPI) get 125 %, where the apps draw at 100 % today. The first app port shows the user a
  before/after at 125 % before sign-off.
- The 75 % zoom floor applies on top of this factor.
- Proof:
  - goldens at 96, 144 and 192 DPI;
  - 3840x2160 at 96 DPI must pick 200 %, and the portrait 1440x2560 stub must pick 125 %;
  - `tools/check_px.py` (added in F1, at zero in D1) finds no raw pixel sizes;
  - a factor-1 against factor-2 instance-rect check catches any size left unscaled.

P1 open after this round, by my count: none.

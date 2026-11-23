# PLAN-v2: planner's response to critic round 3

Plan revised in place (draft 4, 2026-11-23). R3 was decided by the lead: the floor is relative,
like browser zoom, as the question was put to the user.

## P1

- **R1 Changed (critic's fix).** The race records, per case and path, how many presents returned
  OCCLUDED and the median time of the present call alone. A GPU win smaller than its present-call
  time is not proven, so it counts as a loss.
  - A GPU default needs one visible run (one window, `SW_SHOWNOACTIVATE`, while the user is away),
    asked of the user through Master Control at G1 time, only if the hidden numbers allow a win.
    It is never assumed.
  - Until then the CPU stays the default and BENCH.md says "GPU wins not proven with
    presentation". Risk 1 is rewritten to match.
- **R2 Changed (critic's fix).** The exact-scaling instance check runs at factors 1, 2 and 3 with a
  synthetic test font (stub `GetGlyphOutlineW`/`GetTextMetricsW` with exact multiples), swapped in
  through H1's OS table. The real-font goldens at 96, 144 and 192 DPI stay, for the look.

## P2

- **R3 Changed (lead's decision).** The zoom is relative: 0.75 times the monitor's normal factor
  (about 94 % on the user's 125 % screens), in steps of 5 % of the normal factor. Fonts are re-made
  once per step, budgeted at 10 ms a step. No new user question.
- **R4 Changed.** The real-mouse check over the GPU child forces `GUICY_RENDER=gpu`. It is listed
  for the visible run and again at the first port, and the M1 report says GPU-path input is
  unverified until then.
- **R5 Accepted.**
  - `gy_rect` becomes float DIPs. No app is ported yet, so nothing breaks.
  - Returned rects are the pixel edges divided by the factor, so a canvas call at those DIPs rounds
    back to the same edges. Canvas coordinates use the same edge rounding.
  - Hit tests run in device pixels against the rounded rects.
  - New test at 125 %: no seam at `gy_avail()`, and a click on a 1-px border hits the right widget.
- **R6 Changed (lead's variant of the critic's fix).** The monitor is the one under the window's
  centre. The switch applies on `WM_EXITSIZEMOVE` (at once outside a move), and the resize keeps the
  centre, so it can't bounce. A stub test checks that a 1080p-to-4K crossing settles after one
  switch.
- **R7 Changed.**
  - The hidden focus test is dropped as proof. The in-process activation checks stay as a guard,
    and real focus is listed for the visible run.
  - The context-cost threshold is now code size plus a deterministic instruction count from
    llvm-objdump (2 KB or 5 %), not timings.
- **R8 Accepted.** check_px.py's "size" is defined:
  - integer literals of 2 or more in arithmetic with rect fields or passed as sizes to draw or
    layout helpers;
  - `#define`s ending in `_W/_H/_PAD/_GAP/_R/_SIZE` in the widget, layout and tiling sections.

  Intended device-pixel constants carry a `/*px*/` mark. Both counts are reported, and the marked
  list is reviewed in D1's done-report.

## P3

- **R9 Accepted.** A factor exactly halfway between steps rounds up (1.125 gives 125 %).
- **R10 Accepted.** The opening size is capped at the work area; the zoom starts only when the
  minimum doesn't fit. Test: a 1540-DIP window on the portrait stub opens inside the work area.
- **R11 Accepted.** The GPU child also stores its context in `GWLP_USERDATA`.
- **R12 Accepted.** `__attribute__((unused))` comes only through `GUICY_API` under `GUICY_STATIC`,
  so normal builds still warn about GUICy's own dead code.
- **R13 Accepted.** (i) and (ii) are built with `GUICY_NO_GPU`. The default build with the GPU path
  is reported beside them, with a 140 KB budget.
- **R14 Accepted.** "Swap the static struct only when the window changes" is measured beside the
  pointer context.
- **R15 Accepted, but the file grew anyway.**
  - Cut as named: risks 4, 6 and 7, the "Answered" list in section 6, and trace row 613.
  - The R1-R10 fixes added more than that saved, so the plan is now 646 lines, past the ~560
    target. Every new line is proof or design asked for in this round.

P1 open after this round, by my count: none.

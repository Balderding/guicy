# PLAN-v2: critic review, round 3

Reviewed: X:\guicy\PLAN-v2.md (draft 3, 615 lines) and reviews/PLAN-v2-response-r2.md, against
the round-1 and round-2 evidence plus tools/monitors.py, which exists. Nothing was built or run.
Line numbers are draft-3 PLAN-v2.md lines unless a file is named.

**The user's decisions, as stated in the plan:**
- **(a) Too-small screen:** first zoom to a 75 % floor, then "locked sizes give way" in
  proportion, with positions locked and sizes returning on growth. Stated faithfully (l.302-309,
  329-333, 602). One wording gap is N2.
- **(b) The GPU default:** "every case after startup", and a tie loses. Stated faithfully
  (l.194-202, 595).
- **(c) Scaling:** max(DPI/96, min(w,h)/1080) in 25 % steps, per monitor, with edges rounded to
  whole pixels. Stated faithfully (l.241-259). The arithmetic is right: 1440/1080 = 1.33, whose
  nearest 25 % step is 125 % for both of the user's monitors; 2160/1080 = 200 %.

---

## Part 1: round-2 items

| # | Sev r2 | State | Reason |
|---|---|---|---|
| N1 | P1 | **closed** | The user's answer, stated faithfully. New nit N2 on the floor. |
| N2 | P1 | **closed** | `gytest --windowed` on its own desktop, plus the Default-desktop guard (l.48-52, 142-150). |
| N3 | P1 | **closed** | The user's answer: the start cases are reported, not decided on (l.197-198). |
| N4 | P2 | closed | Full-present rule with a stub test (l.167-169, 216-218). |
| N5 | P2 | **partly** | No forwarder; real routing is left to the user's check at "the first port", which may never use the GPU. See R4. |
| N6 | P2 | closed | The hash pass counts in the GPU path's CPU ms (l.165-167). |
| N7 | P2 | closed | (l.222-224) |
| N8 | P2 | **reopened as P1** | The harness is now defined, but its present timings are taken on an occluded window. See R1. |
| N9 | P2 | **partly** | Designed now. The cost threshold sits below the noise, and the focus test checks nothing on a hidden desktop. See R6 and R7. |
| N10 | P2 | closed | The moves are taken now, F1 is back at +4, the trigger is measured, and the two behaviour-changing candidates need Master Control's OK (l.542-549). |
| N11 | P2 | closed | Single-file only, `GUICY_API`, map-file share (l.67-79). Nit in R12. |
| N12-N17 | P3 | closed | |

---

## Part 2: new objections

### R1. P1. The race times present calls that the hidden desktop may skip

- **Section:** the race harness (l.188-192), the default renderer (l.194-202), risk 1 (l.564-566).
- **What is wrong:**
  - **The window is occluded.** The harness times each frame "to the return of the present call",
    on the hidden desktop. There, every DXGI present returns `DXGI_STATUS_OCCLUDED` and never
    waits (compare.md notes 1-2). An occluded flip-model `Present1` may return without queueing
    the flip at all. Likewise, GDI's `BitBlt` to a window on a desktop that is never shown may
    or may not do the copy.
  - **The two errors don't cancel.** Each path's present cost can be wrong by a different amount.
    Under "every case must win", small cases such as hover, caret and typing (about 0.1 ms on the
    CPU path after F1) are decided by margins of the same size as the `Present1` driver cost.
  - **So the decision rests on an unmeasured term.** The GPU can be declared the winner on numbers
    that leave out its own present cost. That breaks the user's rule ("faster in every case")
    without anyone knowing.
  - **Risk 1 assumes the call costs are known.** It says "the race counts the present calls, not
    what DWM does after them". When the call itself is short-circuited, that isn't true.
- **Fix:**
  - The race records, per case and path, how many present calls returned `OCCLUDED`, and the
    median time of the present call alone.
  - Any GPU win on a case whose margin is smaller than the GPU's present-call time, timed while
    occluded, counts as **not proven**, and therefore, by the user's rule, as a loss. The CPU stays
    the default.
  - A GPU default needs one visible race run, which needs the user's explicit yes through Master
    Control, because AGENT_RULES.md:6 forbids opening a window. For example: one window shown with
    `SW_SHOWNOACTIVATE`, while the user is away. Until then BENCH.md says "GPU wins not proven
    with presentation".
  - Rewrite risk 1 to match.

### R2. P1. The factor-1 against factor-2 instance check fails by design wherever text sets a size

- **Section:** D1 proof (l.269-271).
- **What is wrong:**
  - **Text doesn't scale exactly.** "Each instance rect at 2 must be the factor-1 rect scaled by
    the rounding rule". GDI glyphs are hinted (`GetGlyphOutlineW`, PLAN.md 3.8), so a font at
    twice the height doesn't have exactly twice the advances. A button sized to its label, a
    combo, a tab strip, the » fold point, wrapped text (`gy_text_wrap`) and tile-title ellipses
    then get rects that aren't exactly twice as big. Glyph instance rects aren't exact either.
  - **So the check fails on correct code.** Either every scene with text fails, or the check has
    to leave out so much that it proves little.
- **Fix:**
  - Run the check with a **synthetic test font** swapped in through H1's function table
    (`gy__os` stubs for `GetGlyphOutlineW` and `GetTextMetricsW`). Its advances, heights and
    bitmaps are exact multiples of the size, so text layout scales linearly and the check is
    exact.
  - Run it at factors 1, 2 and 3, since 3 also catches rounding written as `<< 1`.
  - Keep the real-font goldens at 96, 144 and 192 DPI for the look.

### R3. P2. The zoom floor and the zoom steps are ambiguous

- **Section:** D1 (l.260), L1 (l.304-306).
- "Multiplies this factor, down to its 75 % floor" can mean 75 % absolute, or 0.75 times the
  monitor factor. On the user's 125 % monitors that is 75 % against 94 %. The plan also doesn't
  say whether the zoom is continuous or moves in steps. A continuous zoom re-makes the fonts and
  flushes the atlas at every new size (l.266). Fractional sizes also break the "1 DIP is a whole
  number of pixels" property that D1's rounding is built around.
- **Fix:** Have Master Control confirm with the user which floor they meant, then state it. Make
  the zoom move in fixed steps (for example 5 %), and give the scale-change cost per step.

### R4. P2. Nobody may ever check the GPU child's mouse routing

(This is round-2 item N5, partly closed.)

- **Section:** G1 (l.174-179, 212-215), risk 3 (l.569-571).
- **What is wrong:** `HTTRANSPARENT` with no forwarder is a fair design: static controls send
  clicks to their dialog this way. But the only check is "the user's check at the first port".
  Under the "every case" rule the CPU is the likely default (l.194-202), so the first port runs on
  the CPU path and never creates the child. The GPU path would ship at M1 with real clicks,
  capture, cursor and wheel over it checked by no one, and "the GPU used or usable" depends on
  that.
- **Fix:** At the first port, the user's check runs once with `GUICY_RENDER=gpu`, whatever the
  default is. Or ask the user for one short, approved visible check at M1, alongside the R1 run.
  State in the M1 report that GPU-path input is unverified until then.

### R5. P2. DIPs held as whole numbers lose pixels at 125 %

- **Section:** D1 units (l.261-265), rounding (l.241-244).
- **What is wrong:** `gy_rect` holds whole numbers (guicy.h:43). Converting pixels to DIPs at
  125 % gives fractions. A panel 1,001 px wide is 800.8 DIPs, which `gy_avail` returns as 800 or
  801. When the app draws on the canvas at those DIPs, the round trip lands 1 px from GUICy's own
  edges, so the "never gap or overlap" promise breaks at the boundary between the app and GUICy.
  The same happens to mouse positions: hit tests done in whole DIPs pick the wrong widget along
  1-px borders.
- **Fix:**
  - Hit tests run in device pixels, against the rounded rects.
  - The canvas and query API is either in float DIPs (`gy_rectf`), or it gets the rects back
    exactly by storing the pixel rect and converting only for display.
  - Add a test at 125 %: a canvas rect drawn at `gy_avail()` exactly covers the panel's pixels,
    with no seam and no overlap.

### R6. P2. Moving between monitors at the same DPI has no anchor and can bounce

- **Section:** D1 (l.248-250, 279).
- **What is wrong:** Moving to a monitor with another factor and no `WM_DPICHANGED` (resolution
  only) makes GUICy resize the window itself, during the modal move loop, in order to keep its
  DIP size. If the new size moves most of the window back onto the first monitor, the factor
  flips back, and it can bounce. Windows avoids this with the suggested rect it sends in
  `WM_DPICHANGED`, which GUICy doesn't get here.
- **Fix:** Compute a suggested rect anchored at the cursor, as Windows does. Apply it on
  `WM_EXITSIZEMOVE`, or with hysteresis: switch only when more than 60 % of the window lies on the
  other monitor. Add a stub test that a 1080p-to-4K crossing settles after one switch.

### R7. P2. The W6 focus test is empty on a hidden desktop, and the cost threshold is below the noise

- **Section:** W6 (l.490-495, 509).
- **What is wrong:**
  - **The focus test.** `GetForegroundWindow` reports the foreground window of the desktop that
    has input, which is the user's, not the test desktop. So "unchanged" passes whatever GUICy
    does.
  - **The threshold.** "3 % on any stress case" is inside the run-to-run spread. The stress runs
    agree only "within about 5 %" (stress.md:17-19), and the move timings vary 3.6 times
    (BENCH.md:155).
- **Fix:**
  - Test activation inside the test process: window B never gets `WM_ACTIVATE` or `WM_NCACTIVATE`
    with a true flag, `GetActiveWindow()` on the GUI thread stays on window A, and B has no
    `WS_EX_TOPMOST`.
  - Judge the pointer's cost on code size and on a deterministic proxy (instructions retired, or
    a 31-run median of the gybench kernels as in BENCH.md step 5), with a threshold above the
    measured spread.

### R8. P2. `check_px.py` has no definition of a "size" and no way to allow intended pixels

- **Section:** F1 item 9 (l.119-121), D1 proof (l.269).
- **What is wrong:** A script can't tell a size literal from any other number. Some raw pixel
  numbers must **not** scale: the 64-px dirty tile (`GY__TILE`), the 1024 atlas pages, 16384 as
  the frame limit (guicy.h:775), and loop or bit constants. Others must, such as `r.w - 6`
  (guicy.h:2104) and `GY__TITLE_H`. "Zero" then means either false passes or false hits.
- **Fix:** Define the rule as a pattern list: integer literals in arithmetic on rect fields, plus
  `#define` sizes in the named files. Mark intended pixel constants with a `/*px*/` comment.
  Report both counts (unscaled and marked), so the marked list is reviewed rather than hidden.

### R9-R15. P3.

- **R9.** Say how a factor exactly halfway between two 25 % steps rounds (for example 1.125). A
  1215-px short side gives exactly that.
- **R10.** Cap the window's opening size at the work area. On the user's portrait monitor
  (1440 px wide), a 1540-DIP window at 125 % is 1925 px wide. It opens wider than the screen and
  goes straight into the zoom.
- **R11.** W6 routing: the GPU child window needs its context in `GWLP_USERDATA` too (for its
  `WM_SETFOCUS` hand-back).
- **R12.** Use `__attribute__((unused))` only under `GUICY_STATIC`, through the `GUICY_API` macro.
  On every definition in every build, it hides the library's own dead code from
  `-Wunused-function`.
- **R13.** Say that size (i) and (ii) are built with `GUICY_NO_GPU`, the CPU-path "core", and give
  the default build with the GPU path, up to 140 KB, beside them. A default app pays that even
  when the CPU is the default renderer.
- **R14.** The pointer context has a cheaper option that keeps the static struct: swap in the
  context only when a message's window differs from the current one. Most messages go to one
  window, so swaps are rare. Measure it beside the pointer.
- **R15. Length (615 lines against 555 in draft 2).** Almost all of the growth is D1 (about 50
  lines), which is new scope from the user, not padding. These repeat step text and can go
  (about 20 lines):
  - risks 4, 6 and 7 (sections 5 and D1 say the same);
  - section 6's "Answered" list (it repeats l.194, 302 and 233);
  - trace row 613 ("as listed in each step");
  - the second statement of the user's monitors in risk 6.

---

## Summary

- **Open P1s:** R1 and R2.
- **P2:** 6 (R3-R8). Round-2 items N5 and N9 are partly closed; their points are in R4 and R7.
- **P3:** 7 (R9-R15).

P1 open: 2

# PLAN-v2: planner's response to critic round 4

The plan is revised in place as draft 5 (2026-11-23).

This round folds in:
- the user's three additions: VS Code-style zoom, sizes from the font, and "Or use the GPU to scale";
- Master Control's two answers (shaders compiled at build time only, and the order changes, each with a reason).

The Vulkan request was cancelled by the user ("Nevermind"), so nothing about Vulkan is in the plan, and research/gpu.md 4.4 stands.

## P2

- **P2-1 Changed (critic's fix).** Section 6 now lists three visible checks, each asked separately through Master Control:
  - A: the visible race, only if the hidden race allows a GPU win;
  - B: the mouse over the GPU child, with `GUICY_RENDER=gpu`, whatever the race shows;
  - C: W6's focus check.

  The protocol for each:
  - the user names the time;
  - one window is shown with `SW_SHOWNOACTIVATE`;
  - the run counts only if every present returned `S_OK`, and otherwise it stops and reports.

  If the user declines B or C, they go on the first-port checklist in A1-A3.
- **P2-2 Accepted.** A test hook sets the race's client size exactly, and `gy__os.work_area` is stubbed larger, so the opening-size cap can't shrink the 1440p case. Each row records the real client size.
- **P2-3 Accepted.** W6 now judges each context option three ways:
  - code size;
  - a disassembly check that the inner loops of the kernels, the tile hash pass and the box and line raster loops load no context pointer;
  - `QueryThreadCycleTime` 31-run medians on the gystress scenes, against a threshold above the measured spread.

  The static instruction-count proxy is gone.

## P3

- **P3-4 Accepted.** The duplicate "Otherwise too" sentence is gone.
- **P3-5 Accepted.** Instances stay whole device pixels, so floats never enter the tile hash. The pixel-to-DIP-to-pixel round trip is tested at every zoom step from 1.25 x 1.00 down to 1.25 x 0.75.
- **P3-6 Accepted.** An API note says not to cast `gy_rect` to `int` before a canvas call, because that brings back the 1-px seams.
- **P3-7 Accepted.** The L1 proof now says "0.75 x the normal factor".
- **P3-8 Accepted.** The trace row now says "factors 1, 2 and 3 with the synthetic font".
- **P3-9 Accepted.** A swap copies only the small per-window part (a few KB). The arenas live in `VirtualAlloc` memory, so their pointers stay valid.
- **P3-10 Accepted.** The synthetic-font check is named as the backstop for the `+ 1` insets that check_px.py skips.
- **P3-11 Accepted.** The visible checks are listed in one place, section 6. G1, W6 and the risks point to it.
- **P3-12 Partly.**
  - Done:
    - old risk 1 cut, and old risks 2 and 3 merged into one;
    - risk 5 cut (section 3 says the same);
    - the duplicate sentence cut;
    - trace row 628 shortened to "G1, the default renderer".
  - Not met: the target length. The user's three additions brought the file to about 700 lines. Every new line is design or proof that was asked for.

## The user's additions and Master Control's answers

- **VS Code-style zoom (D1).**
  - Ctrl+= and Ctrl+- step the zoom by 10 % (0.75 to 3.0 times), Ctrl+0 resets it, and Ctrl+wheel steps it.
  - The zoom and font size are saved in one ini line (`gy_view_save/gy_view_load`).
  - All four go through `gy_keys_owned`. Ctrl+wheel gets its own code, `GY_KEY_WHEEL`, because an app may already use it to zoom a preview. A clash turns that key off and shows the gold banner.
- **Sizes from the font (D1).**
  - **Em floats, not em twins.** Each size has one number, so a theme can't hold a px value and an em value that disagree, and no app theme is ported yet to break.
  - Today's values are converted once (for example `row_h` = 24 / 15 em). The edge rounding gives back today's exact whole pixels at today's font size, so today's goldens stay byte-equal.
  - The one factor is monitor x zoom x font-size ratio.
  - The relative 0.75 floor covers the user zoom and the fit zoom together.
- **"Or use the GPU to scale" (D1, G1).**
  - The final frame is always a crisp re-layout at the new factor.
  - On the GPU path, during an active zoom or a move between monitors, the last frame is stretched as a preview. A preview frame sets dirty-all and the full-present flag.
  - Images are scaled on the GPU with the CPU's 8-bit weights, so they keep the CPU-match rule.
  - On the CPU path there is no preview: the zoom jumps straight to the crisp frame.
  - The proofs count frames instead of timing them:
    - the last frame is never a preview;
    - the crisp frame comes within 1 frame after the input stops;
    - idle is back to 0 afterwards;
    - GPU image scaling matches the CPU by the G1 pixel rule.

  The preview is explicitly never the resting frame.
- **Master Control's answers.**
  - Section 2 now gives a "what it unblocks" reason for each order change.
  - The shader step now says d3dcompiler_47 is used from System32 at build time only. It is never copied into the repo or onto C:, and never loaded at run time.

P1 open: none.

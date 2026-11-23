# PLAN-v2: the lead's ruling

2026-11-23. Lead: the GUICy session. Debate: the planner agent and the critic agent, 5 rounds.
Inputs:
- research/tiling-locks-gpu.md
- bench/stress.md
- bench/compare.md
- PLAN.md
- PLAN-tiling.md
- Master Control's notes

## The rounds

| Round | Draft | Critic: P1 / P2 / P3 | What closed it |
|---|---|---|---|
| r1 | 1 | 8 / 16 / 14 | draft 2: occlusion rule, full redraw + Present1 dirty rects, child-window design, pixel-class mask, size accounting, frozen geometry file, L1 lock invariants |
| r2 | 2 | 3 / 8 / 6 | the user's answers (too-small screen, GPU every case after startup); windowed tests on their own desktop |
| r3 | 3 | 2 / 6 / 7 | race proof needs present cost and a visible run; exact-scaling check uses a synthetic font; the lead set the 75 % floor as relative |
| r4 | 4 | **0** / 3 / 9 | ready |
| r5 | 5 (new user asks) | **0** / 3 / 7 | ready |

Files: reviews/PLAN-v2-critic-r1..r5.md and reviews/PLAN-v2-response-r1..r4.md.

## The user's decisions written into the plan (in their words)

1. "Not if the dev locked the windows": a developer lock can never be undone by the user.
2. Too-small screen: shrink GUICy's UI scale down to 0.75 times the normal factor. Below that,
   "locked sizes give way": they shrink in proportion, positions stay locked, and sizes come back
   when the window grows.
3. "Every case must win", then "every case after startup": the GPU is the default only if it
   beats the CPU in every running case. A tie loses. A win smaller than the present cost is not
   proven, so a GPU default also needs the visible run (check A).
4. "Depending on the resolution everything should scale": the monitor factor is
   max(DPI/96, min(w,h)/1080) in 25 % steps. Both of the user's monitors get 125 %.
5. "Do what vscode does and scales with the fonts too": Ctrl+= / - / 0 and Ctrl+wheel zoom, and
   every size is in font units.
6. "Or use the GPU to scale": a stretched preview only while zooming. The crisp frame always comes
   last.
7. Vulkan was asked about and then dropped ("Nevermind ... we use vulkan for uwr not the other
   apps"). The plan has no Vulkan.

## Master Control's answers written into the plan

- d3dcompiler_47 is used from System32 at build time only. The bytecode header is checked in, and
  the DLL is never loaded at run time.
- The step order is F1, H1, G1, D1, L1, G2, T1, X1, T2-T6, P1, W1-W6. Each move has a stated reason
  in section 2. This is provisionally OK, subject to Master Control's evaluator.

## Ruling

**PLAN-v2 draft 5 is accepted for Master Control's evaluation.** No P1 is open after r5.

The three r5 P2s are **binding** for D1, as the critic wrote them:
1. The em sizes become a whole-pixel theme once per factor or font change. The layout keeps its
   integer maths, and a test checks that theme against today's integers at factor 1.
2. The 0.75 floor limits zoom only; the font ratio sits outside it. A zoom-in first grows the
   window up to the work area, then the zoom is capped with a gold banner.
3. The GPU preview stretches a kept copy of the last frame as one image instance (no new shader).
   The first pass after a zoom event draws the preview, and the settle pass draws the crisp frame.
   The monitor-move case covers only `WM_DPICHANGED` during a drag. The preview's proof moves to
   D1, and its KB and memory are counted in the GPU path's budget.

The r5 P3s are taken during the build where they're cheap:
- numpad zoom keys
- a separate code for Ctrl+wheel, so an app can keep its own wheel zoom
- a one-page script for visible check B
- one trace row's wording

Nothing is built until Master Control's evaluator has read the plan. The first step after its OK
is F1, the stress-test slow spots, because the GPU has to be compared against the fixed CPU path.

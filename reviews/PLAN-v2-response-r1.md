# PLAN-v2: planner's response to critic round 1

Plan revised in place (draft 2, 2026-11-23). The user's answer on the GPU default ("every case
must win") overrides the draft-1 session-mix rule and the critic's fix for 5.

## P1

1. **Changed (critic's fix).** G1 states the occlusion policy: OCCLUDED counts as success, no
   PRESENT_TEST poll or timer, IsIconic skips drawing; proof: gpu_draws equals the CPU frame
   count, presents 0 when idle.
2. **Changed (critic's option a).** The GPU draws the whole frame on every changed frame and
   passes only this frame's dirty tiles to Present1, so the N-2 logic, its fuzz and risk 4 are gone.
3. **Changed (critic's fix).** The child is WS_DISABLED, returns HTTRANSPARENT and forwards every
   mouse, cursor, wheel and key message to the parent; hidden tests post them to the child's HWND.
   The "catches it" claim is gone.
4. **Changed (critic's fix).** A 1-bit mask from the CPU renderer defines the AA class; the shader
   copies the integer maths (Load-based images, point glyphs, quantised coverage, the corner cache
   as a texture); a difference histogram is reported.
5. **Changed (by the user's answer, not the critic's fix).** The GPU is the default only if it wins
   every measured case beyond the run-to-run spread; ties count as not faster; all numbers are
   reported either way. The critic's other points are kept: one PC, presentation excluded, other
   GPU load logged, coded adapter conditions written down.
6. **Changed (critic's fix, plus a switch order).** Switches decided (T1 strips/preselection, P1
   wrap/sticky, T5 to the full build; GUICY_NO_IME, GUICY_NO_DWRITE). The core is measured as (i)
   guicy.o with no gc and (ii) core.exe calling every default feature. GUICY_STATIC removes the
   need for linker flags. The new estimate is about 117 KB, with a named order of what moves behind
   a switch past 115 KB.
7. **Changed (critic's fix).** old_layouts checks a frozen geometry file (rects, stacks, active
   tab, focus, zoom) and is never re-baselined; the picture check beside it follows the golden rule.
8. **Changed (critic's fix).** gy_tile_rule placement moved from T5 into L1; the code layout is
   defined; each flag has its own invariant (MOVE = tree path, SIZE = DIPs on the locked axis,
   MOVE+SIZE = rect); the fuzz checks each invariant and that unlocked tiles did move and resize.

## P2

9. **Accepted.** X1 (and H1) are in Master Control question 1; F1 starts after the answer.
10. **Accepted.** New step H1: gy_attach, gy_event, gy_ctx_set, the OS function table; G1, D1, X1
    and L1 run tests through gy_event via H1's rule that every test also passes on an app window.
11. **Accepted.** GUICY_NO_IME, GUICY_NO_DWRITE, GUICY_SMALL; hello.exe at 40 KB of GUICy; -Os
    beside -O2; share read from the lld map file. Split and dock locks stay under GUICY_NO_TILING.
12. **Changed (design, no question).** Locks never give way: on a too-small screen unlocked tiles
    fold and shrink, then the layout clips with a vertical scroll and a banner. Risk 8 names the
    cost (main buttons can be clipped there).
13. **Accepted.** The same gate covers gy_split dividers, gy_dock strips and W5 windows, tested
    like the tile flags.
14. **Changed (scheduled).** New step W6, a second OS window with its own context, with pop-out
    tiles in the full build; no question needed.
15. **Accepted.** D1 has a units table (everything in DIPs, device-pixel option for previews,
    nearest scaling by default), reads only wParam/lParam, and says what happens with other
    awareness settings.
16. **Accepted.** imm32 goes through the function table with stubs, and real WM_IME messages run
    the real handler; emoji are one colour; Arabic, Hebrew, Indic and Thai shaping are out of scope.
17. **Accepted.** W1 and F1 pass or fail on counts from gy_get_stats; times are reported beside them.
18. **Accepted.** WARP on a small set at 2-core affinity; hardware runs in one short batch with
    other GPU load logged.
19. **Changed.** comdlg32 dropped; file, save and folder all go through IFileDialog on ole32,
    which is now in the run-time list.
20. **Changed (critic's first option).** The overview shows each tile's last picture drawn while
    it was visible, marked stale; the minimap of names comes first.
21. **Accepted.** The text now says that lines starting with [, H or V are today's format.
22. **Accepted.** The OS animation setting is read through the function table and stubbed; GUICy
    never sets system parameters (a section 3 rule).
23. **Accepted.** Tables don't scroll sideways: columns shrink with an ellipsis, then hide behind
    »; sideways scrolling is opt-in only.
24. **Accepted.** microui is added at M1; until then the claim is only "smaller than Nuklear,
    ImGui and raygui".

## P3

25. **Accepted.** The command palette and toasts moved to "later"; the errors window was dropped
    (the banner exists).
26. **Accepted.** Section 1 now says "draw every frame" and gives ImGui's wait-loop 0.23 s.
27. **Accepted.** hello.c and the 15-line example are H1 deliverables.
28. **Accepted.** `presents` and `gpu_draws` are added to gy_stats and the log.
29. **Accepted.** The waitable joins the wait only while a frame or animation is pending.
30. **Accepted.** Readback, timestamps and stubs are behind GUICY_TESTS and not in the 20 KB.
31. **Accepted.** The first DirectWrite fallback is budgeted at 30 ms, once.
32. **Partly.** Section 3 now names C:\Users, O:\ and X:\ paths. The banned word stays referenced
    through AGENT_RULES, not copied: copying it into the repo would make every scan hit the plan.
33. **Accepted.** G2 raises the pools to 64 tiles and 16 tabs; the » goldens use 8 and 16.
34. **Accepted.** The "no visible change" claim is now proved by readback before and after; the
    child is created only when the first GPU frame is ready.
35. **Accepted.** Fractional radii snap to 1/4 px for the cache key; named as the one allowed
    golden difference in F1.
36. **Accepted.** `gy__tile_may` is written for containers from L1.
37. **Accepted.** There are now two rows: start to window (baseline.py) and library start to first
    frame (gystress).
38. **Accepted.** A multi-line edit box is in W3, with IME tests.

P1 rejected: none.

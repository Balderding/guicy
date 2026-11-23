# PLAN-v2: planner's response to Master Control's evaluation (2026-11-23 02:07)

Draft 6 of PLAN-v2.md is 557 lines, down from 702. All 15 changes are taken. The steps the apps don't need are now short "on the user's ask" designs.

1. **Taken.** The binding r5 P2s are written into D1:
   - em values resolve into a whole-pixel theme once per factor or font change;
   - the layout keeps its integer maths (guicy.h:2104/2108);
   - the 0.75 floor limits the zoom only;
   - a zoom-in grows the window to the work area, then is capped with a banner;
   - the preview proof moved out of G1, into O1/D1;
   - the `row_h` example is fixed to 28 / 15 em.
2. **Taken.** The order is F1, H1, G0, D1, X1, L1, A1 (pilot, with the user's OK), A2, A3, then G1 opt-in, then the default parts of G2, T1 and P1, then the rest only on the user's ask. Section 2 says what each move unblocks.
3. **Taken.** G0 is defined as a one-day spike:
   - `bench/gyspike.c`, outside guicy.h;
   - FLIP_DISCARD, the hash pass plus Map/Draw/Present of about 500 instances;
   - 9 runs with QueryThreadCycleTime against the 0.06 ms caret frame.

   The plan states that the likely result is the CPU staying the default.
4. **Taken.** D1 ships the crisp zoom with no preview. The preview is item O1, waiting on the user, with the honest costs: one refresh later, a copy per frame, about 15 MB of VRAM. It keeps the ruling's design and the count-based proof in case the user wants it.
5. **Taken.** FLIP_DISCARD with `Present(1,0)`. Present1 dirty rects moved to "later", and the full-present rule and its stub test went with them. The occlusion rules are kept.
6. **Taken.**
   - Device loss is handled from Map, ResizeBuffers, CreateTexture2D and Present.
   - Recovery does ClearState and Flush, then makes a new child window and device.
   - "No black frame" works like this: the child has a NULL brush over the parent's last BitBlt, and the parent gets no WS_CLIPCHILDREN until the first present returns. A PrintWindow readback test checks it.
7. **Taken.** The race uses 9 or more runs with QueryThreadCycleTime and logs other load. A case whose winner flips between runs counts as a CPU win.
8. **Taken.** Section 3 now passes or fails tests by counts only; milliseconds are reported but don't fail anything. Idle means 0 frames and 0 presents per 60 s, run only at steps that touch the loop or a renderer.
9. **Taken.** The `WM_DPICHANGED` lParam rect is applied at once with SetWindowPos. Only the same-DPI resolution switch waits for `WM_EXITSIZEMOVE`. The user's two-125 % case, where neither path fires, has a stub test, and the code stays minimal.
10. **Taken.** Axis-aligned thin canvas lines snap to pixel centres. A test checks that a 1-DIP line is exactly 1 px at every zoom step.
11. **Taken.**
    - Structural checks are instance lists drawn with the synthetic test font, which moved into H1.
    - Each step keeps a small real-font look set.
    - 120 DPI (125 %) is added, so the set is 96/120/144/192.
12. **Taken.**
    - The DirectWrite fallback is budgeted at 5-8 KB.
    - `.pdata`/`.xdata` are counted in (ii).
    - The cut order is reversed, so the DWrite fallback and IME go last, and only with Master Control's OK.
    - The estimate is said plainly: the core reaches the cap at G2.
13. **Taken.** Deferred to the user's ask: T2-T6, W5, W6, P1's full-build extras and W4's file dialogs, since the apps have their own dialogs. W1 and W2 pieces come earlier only if a port's API map names them as a gap.
14. **Taken.** New section 5 for the app ports:
    - (a) the API map with gaps named, plus a timing hook in each app;
    - (b) bbb_kit's `gy_attach` and its GDI+ card grid;
    - (c) the WCAG contrast self-tests, reading H1's text-record hook;
    - (d) ini and layout migration;
    - (e) the before/after sheet: original at 100 % beside GUICy at 100 % and at 125 %, plus the portrait case;
    - (f) Nuklear rollback behind a build switch until the user signs off and one release has shipped.
15. **Taken.**
    - Tab/Shift+Tab focus, with Enter or Space to activate, is in X1 (+1 KB).
    - High-contrast mode is in D1.
    - UI Automation is named as a known gap in risk 6.

Not changed: no Vulkan (still 0 mentions). The user's binding decisions are unchanged: the lock rules, the 75 % floor then "locked sizes give way", "every case after startup, ties lose", the scaling formula and the 125 % monitors.

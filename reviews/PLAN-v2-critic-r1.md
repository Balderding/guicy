# PLAN-v2: critic review, round 1

Reviewed: X:\guicy\PLAN-v2.md (draft 1, 2026-11-22), against PLAN.md, PLAN-tiling.md, BENCH.md,
bench/stress.md, bench/compare.md, research/tiling-locks-gpu.md, research/gpu.md, guicy.h (0.4.0,
3,040 lines), tests/gytest.c, tests/v1_tests.inc, tools/baseline.py and AGENT_RULES.md.
Nothing was built or run. Line numbers are PLAN-v2.md lines unless a file is named.

Severity: **P1** = the plan is wrong or a proof can't be done as written; **P2** = should fix;
**P3** = nit.

---

## P1: must fix

### 1. P1. G1: the hidden-desktop GPU runs depend on an occlusion policy the plan never sets
- **Section:** G1 deliverable (l.140-142), G1 proof (l.161-164), risk 1 (l.448-452).
- **What is wrong:** On the hidden desktop every DXGI Present returns `DXGI_STATUS_OCCLUDED`
  (compare.md notes 1-2). PLAN.md 3.9 says "Minimized or occluded windows present nothing", and
  research/gpu.md:445-446 says to stop presenting when occluded and poll with
  `Present(0, DXGI_PRESENT_TEST)` on a slow timer. That poll is exactly what made ImGui draw
  nothing on the hidden desktop (compare.md note 1). If G1 follows the research, every
  hidden-desktop GPU proof (forced warp, device-removed halfway, 0 Presents, idle) tests a
  renderer that never draws, and the slow poll timer breaks "idle at one tick". If G1 ignores
  occlusion, the plan has to say so, because it overrides PLAN.md 3.9. PLAN-v2 says neither.
- **Fix:** State the policy in G1: GUICy renders and presents only on change, as now. It treats
  `DXGI_STATUS_OCCLUDED` as success and never polls with `DXGI_PRESENT_TEST` or a timer. A
  minimized window (`IsIconic`) skips drawing, and the next restore marks everything dirty. Then
  add a proof line: on the hidden desktop the GPU path's draw count equals the CPU path's frame
  count for the same script, and `presents` (see 28) is 0 over 60 s idle.

### 2. P1. G1: the N-2 back-buffer proof never tests a back-buffer ring
- **Section:** G1 deliverable (l.140-142), G1 proof (l.159-160), risk 4 (l.459-460).
- **What is wrong:** The "500 random frames" fuzz reads back "each frame's GPU readback". The
  only readback path in the plan is the offscreen render target (l.149-150), which is one
  texture. It has no second buffer, so the "redraw tiles dirty now or last frame" logic is never
  tested. On a real swap chain, the plan assumes buffers alternate strictly, so the buffer being
  drawn holds frame N-2. Nobody has checked whether occluded flip presents on the hidden desktop
  rotate the buffers at all, and a resize (`ResizeBuffers`) also resets the contents.
- **Fix (pick one, and say which):**
  - (a) **Recommended, and simpler:** on the GPU path, draw the whole frame on every frame that
    changes. That is one `DrawInstanced`, and the GPU side is tiny. Pass only this frame's dirty
    tiles to `Present1`. Every pixel of the back buffer is then current, nothing depends on its
    age, the N-2 logic and its fuzz go away, and risk 4 is gone.
  - (b) Keep the partial redraw, but track each buffer's age with
    `IDXGISwapChain3::GetCurrentBackBufferIndex`, not by parity, and treat "after a resize" and
    "unknown" as a full redraw. The fuzz then runs on a two-texture ring that copies this
    behaviour offscreen, plus a hidden-desktop run that reads the real back buffer
    (`GetBuffer(0)` to staging) before each `Present1`.

### 3. P1. G1: the child-window input pass-through has no test that can reach it
- **Section:** G1 deliverable (l.137-139), risk 3 (l.456-458).
- **What is wrong:** Risk 3 says "the G1 scripted tests run on the windowed path to catch it". But
  every gytest test feeds input through `gy_input_*` (tests/gytest.c:63-90,
  tests/v1_tests.inc:12-27), which bypasses Win32 messages. `tools/baseline.py:165` posts
  `WM_MOUSEMOVE` straight to the top-level HWND it finds with `EnumDesktopWindows`, which skips
  hit testing (`WM_NCHITTEST`) and the child altogether. `SendInput` can't be used either: it
  injects into the user's input desktop, which breaks "never take the user's focus". So nothing
  proves that `HTTRANSPARENT` passes on clicks, capture, the cursor or the wheel. guicy.h's
  window procedure (`guicy.h:2882-2938`) relies on `SetCapture`, `TrackMouseEvent` and
  `WM_SETCURSOR` going to the parent.
- **Fix:** Don't rely on `HTTRANSPARENT` alone. Have the child's window procedure hand every
  mouse, cursor and key message to the parent's handler. The child covers the client area at
  (0, 0), so the coordinates are the same. Create the child with `WS_DISABLED` so it never takes
  focus. Then add hidden tests that `SendMessage` `WM_NCHITTEST` to the child (expect
  `HTTRANSPARENT`) and post `WM_LBUTTONDOWN`, `WM_MOUSEMOVE`, `WM_MOUSEWHEEL` and `WM_SETCURSOR`
  to the child's HWND, then check that the parent saw them, through the scripted log. Drop the
  "catches it" claim from risk 3.

### 4. P1. G1: "0 on solid pixels, 1 level on AA pixels" can't be checked without a definition of each
- **Section:** G1 proof (l.157-158), risk 2 (l.453-455).
- **What is wrong:** The comparison has to sort every pixel into "solid" or "AA", and neither
  renderer records which is which. The CPU path also blends many non-AA pixels: hover tints,
  shadows, the select colour, disabled text, glyphs and every bilinear image pixel
  (`guicy.h:557-562` `gy__over`, `:2690-2732`). On the GPU, hardware bilinear filtering and
  sampler weights are not the CPU's 8-bit integer weights (`guicy.h:2707-2725`). Image pixels can
  differ by more than 1 level, and they are not "AA". The coverage maths is float on both sides
  (`gy__box_cov` uses `_mm_sqrt_ss`, `guicy.h:294, 2611-2621`), but the CPU quantises coverage to
  0..255 and scales it with `gy__div255`.
- **Fix:**
  - Define the classes mechanically. In test builds, the CPU renderer writes a 1-bit mask of
    "any contribution with coverage < 255, colour alpha < 255, a glyph or a filtered image". Masked
    pixels get the 1-level tolerance; every other pixel must be equal.
  - Write in G1 that the shader copies the CPU's integer maths where SM5 allows it, so most
    classes are exact by construction:
    - images: `Load()` the 4 texels and use the same 8-bit weights and rounding, with no sampler
      filtering;
    - glyphs: point loads;
    - coverage: quantise to 0..255 with `floor(c * 255 + 0.5)`, then `col * cov / 255` rounded,
      as `gy__scale` does;
    - corners: from F1's corner-coverage cache, uploaded as a small texture, so rounded corners
      match bit for bit.
  - Report the histogram of differences, not only pass/fail.

### 5. P1. "yes to the GPU if it is faster" gets vetoes the user never gave
- **Section:** G1 deliverable (l.153-154), question 1 (l.431-438), trace (l.488).
- **What is wrong:** The user decided "yes to the GPU if it is faster" (PLAN-tiling.md:58-61:
  GPU by default when it beats the CPU path on the same scenes). The proposed rule adds two
  vetoes: "never more than 0.1 ms slower on a single small change" and "starts no more than
  10 ms later". So the GPU can be faster on the whole session mix and still lose the default.
  That doesn't honour the decision as given, and "if the user doesn't answer, G1 uses this rule"
  sets it in place without the user. Three more problems:
  - The total is measured on one machine (the RTX 4090 and i9-10980XE in BENCH.md:4) and then set
    "in code", which makes it the default on every machine.
  - The timings run on a shared PC where the user plays games on that same GPU
    (AGENT_RULES.md:8).
  - The hidden desktop leaves out presentation and composition for both paths (risk 1).
- **Fix:**
  - The rule: GPU default if its session-mix total (CPU-side ms plus GPU-timestamp ms per frame,
    the sum as a stand-in for latency) is lower than the CPU path's. Per-case wins and losses,
    start time and working set (compare.md: GPU apps use 57-82 MB against 34) are **reported**,
    not used as vetoes.
  - A veto goes to the user as a question, and only with numbers behind it.
  - If the two totals are within the run-to-run spread (median of 3, l.472-473), report "tie"
    and ask the user, rather than picking.
  - Write down the coded rule (hardware adapter, not remote, not Basic Render Driver), and say
    that "faster" was measured only on this PC with presentation excluded. Note whether other
    GPU load was present during the runs.

### 6. P1. The 120 KB core budget doesn't hold by the plan's own numbers, and the way it is measured lets code slip out
- **Section:** section 3 size rules (l.70-76), budgets table (l.415), risk 5 (l.461-463).
- **What is wrong:**
  - **The arithmetic.** Today is about 86 KB (BENCH.md:152). The default-build adds are F1 +4,
    D1 +4, L1 +4, G2 +6, T1 +5 (T1 has no switch, so it is in the default build), X1 +6, P1 +5
    ("in the default build", l.346) and part of T5 ("rules, slots and dots in the default build",
    l.321-322). That makes about 120 KB before T5's part, and over 120 KB with it. Risk 5 counts
    only to X1 (+29, giving 115 KB), so the plan already knows it breaks its own cap, without
    saying what goes behind a switch.
  - **The measuring.** Size (c) is "the demo with every v1 widget", linked with
    `-ffunction-sections -Wl,--gc-sections`. If the demo calls no tile, lock, dock or IME
    function, the linker drops them, and the "core" number leaves out the very features the core
    budget says it includes. Today `examples/demo.c` is a widgets demo.
  - **The flags.** Those flags are the app's build flags. "Easy to import into an EXE" means one
    define and no special flags, and the apps' build scripts don't pass them.
- **Fix:**
  - Decide now which features are switchable and put that in the plan. For example, P1's wrap rows
    and sticky headers, and T1's preselection and outer strips, could go in `GUICY_TILING_FULL`;
    a `GUICY_NO_IME` and a `GUICY_NO_DWRITE` (dropped from PLAN.md 3.1) could exist. Re-add the
    numbers.
  - Measure the core three ways:
    - (i) `guicy.o` text+data with no gc, the honest upper bound;
    - (ii) a `core.exe` that calls at least one function of every default-build feature (a tile, a
      lock, a dock, an edit box with IME, DPI);
    - (iii) the same with gc.
  - The budget applies to (ii) with gc **and** to (i).
  - Offer `GUICY_STATIC` (stb's `STB_*_STATIC` pattern: every function `static`), so the
    compiler drops unused code with no linker flags. See also P2-11.

### 7. P1. `old_layouts` "never re-baselined" contradicts the planned picture changes
- **Section:** section 3 (l.65-69), F1 proof (l.115-117), trace (l.503).
- **What is wrong:** `old_layouts` stores each 0333d67 line **with its picture hash** and checks
  the picture "at every step from then on ... never re-baselined". F1 item 6 changes the AA pixels
  at the focus-outline corners (l.116-117), which is in every tile picture. Item 5 (bilinear)
  changes any tile picture with a smooth image. X1, D1 and T5 (gaps, focus border) can also touch
  tile pictures. So the test fails at F1 by design, or it gets re-baselined, which it promises
  never to be.
- **Fix:** Make `old_layouts` check **geometry, not pixels**: for each old line, the loaded tile
  list (name to rect, stack membership, active tab, focus, zoom) must equal a frozen geometry file
  made at 0333d67. That never changes, because the CPU maths of tile rects is integer and is kept
  across G2 (two-child containers, the same `(r.w - 6) * ratio` rounding, guicy.h:2104-2108). Keep
  a picture check beside it, re-baselined only with the side-by-side rule.

### 8. P1. L1: "code place" and "code rect" have no definition, and the lock fuzz assertion would freeze unlocked tiles
- **Section:** L1 deliverable (l.191-203, 210-212), L1 proof (l.224-225), T5 (l.315-316).
- **What is wrong:**
  - **No code place yet.** L1 says a MOVE-locked tile "goes back to its code place" and a
    SIZE-locked one "gets its code size back". But the developer has no way to state a place or
    size until T5's `gy_tile_rule` (l.315), and L1 already uses `.lock in gy_tile_rule` (l.192). In
    guicy.h today a tile's place is wherever the open order put it (`gy_tile_open`, guicy.h:2215;
    no rule API, guicy.h:184-193).
  - **The fuzz breaks "freely moved".** The fuzz requires "each locked tile's rect equals its code
    rect" after every event. For a tile with only `GY_LOCK_MOVE`, a neighbour's divider drag or a
    sibling closing changes its rect legitimately. Enforcing equal rects turns every lock into a
    full freeze, and stops the unlocked neighbours from resizing against it. That breaks the
    user's "if it isn't locked it is freely moved around".
  - **Unit not set.** The plan never says whether a size lock is in 96-DPI px or a ratio, or what
    happens to it when the window grows.
- **Fix:**
  - Move the placement part of T5 into L1: `gy_tile_rule(name, {.side, .w, .h, .lock})`, or "the
    code layout = the tree built from the app's open order with no saved line", built once and
    kept.
  - Define each flag's invariant. MOVE: the tile's path in the tree (parent container, index,
    stack) is unchanged, except when a sibling closes, which is allowed and logged. SIZE: w or h in
    96-DPI px along the locked axis is unchanged while the window is at or above its raised
    minimum. MOVE+SIZE: the rect is unchanged.
  - The fuzz checks each flag's own invariant, and also checks that at least one unlocked tile
    did move or resize during the run, which proves the "freely moved" half.

---

## P2: should fix

### 9. P2. X1's insertion changes the approved order but isn't in question 2
- **Section:** section 2 (l.40-54), question 2 (l.441-442).
- Master Control's order is G1, L1, G2/T1, T2-T6, P1. Placing X1 between T1 and T2 is a change
  too, and so is the earlier tab strip in G2. Question 2 lists only F1 and D1.
- **Fix:** List X1 in question 2. Say that F1 starts only after Master Control answers.

### 10. P2. No `gy_attach`, so G1, D1 and X1 are designed only for GUICy-owned windows
- **Section:** G1, D1, X1.
- PLAN.md 4.1 promises `gy_attach(hwnd)`, `gy_event` and `gy_begin`/`gy_end` for apps with their
  own window. bbb_kit uses `nkui_init` on its own HWND (BENCH.md:80). guicy.h has none of them
  (guicy.h:83-104). The child swap chain, the `WM_DPICHANGED` handling, the IME messages and the
  raised minimum window size from locks all live in GUICy's own window procedure
  (guicy.h:2882-2938). Built without `gy_attach` in mind, they will need a redesign at the first
  port.
- **Fix:** Add `gy_attach` and `gy_event` as an early small step (before G1, or in F1). G1, D1, X1
  and L1 each get one test through `gy_event` on an app-made hidden window.

### 11. P2. The switches don't let a basic app skip DPI, IME, DirectWrite or locks, and `hello.exe` has no budget
- **Section:** section 3 (l.70-76).
- Master Control: "compile switches, so basic apps don't pay". Only `GUICY_NO_TILING`, `NO_GPU`
  and `NO_ANIM` exist. Size (b) `hello.exe` is reported but has no cap. Because the window
  procedure and `gy_end` reference IME, DPI and the tile gate, gc-sections can't drop them from a
  one-button app.
- **Fix:** Add `GUICY_NO_IME` and `GUICY_NO_DWRITE` (and say whether locks can be dropped under
  `GUICY_NO_TILING`). Give `hello.exe` a budget, for example GUICy's share under 40 KB. Add `-Os`
  numbers beside `-O2`, since "smallest" is an ask. Say how "KB from GUICy" is attributed in an
  exe: an lld map file, or the exe minus the same exe with a stub library.

### 12. P2. The one exception to the absolute lock decision needs the user's OK
- **Section:** L1 (l.210-212), risk 8 (l.468-469).
- The user decided a developer lock can **never** be undone by the user. The plan lets locked
  sizes give way when the screen is smaller than the raised minimum. Moving the window to a
  smaller monitor, changing resolution or changing scaling (worse at 150 %, after D1) are user
  actions. It may be unavoidable, but it is an exception to an absolute decision.
- **Fix:** Put it to the user through Master Control as a stated exception, with the alternative:
  locked tiles keep their size and the rest of the window scrolls or clips, so nothing a lock
  holds ever moves.

### 13. P2. "Lock panels" covers more than tiles
- **Section:** L1, P1, W5.
- The user said "lock panels". Inside a tile, `gy_split` dividers are always draggable
  (guicy.h:171, :2309-2310), `gy_dock` strips (P1) have no lock, and W5's movable inner windows
  have none either.
- **Fix:** Use the same flags and the same `gy__tile_may` gate (or a twin of it) for
  `gy_split(..., GY_LOCK_SIZE)`, `gy_dock` and W5 windows (MOVE, SIZE, CLOSE). Test each the same
  way as the L1 per-flag refusals.

### 14. P2. "Every type of window": second OS windows are deferred with no OK
- **Section:** "Later" (l.405-407), trace (l.489).
- Pop-out tiles and second top-level windows are "later, only with a number or a picture". There
  is no `gy_ctx_set` in guicy.h (PLAN.md 3.2 promised it). The user asked for every type of window.
- **Fix:** Either schedule a minimal second-window step (a second `gy_open` context, one DIB or
  swap chain per window) after W5, or get the user's explicit "later" through Master Control and
  write it in the trace.

### 15. P2. D1: units and the DPI-change test
- **Section:** D1 (l.175-187).
- **Units.** "Apps change nothing" covers `gy_desc` and `gy_row` only. Today, canvas coordinates,
  `gy_avail`, `gy_last_rect`, `gy_image(w, h)`, mouse positions and the app-held `gy_split` `*pos`
  ("the first one's px", guicy.h:171) are all pixels. Mixed units at 144 DPI break apps that draw
  on the canvas (bbb_kit's preview, voxup). Pixel-art images also need a nearest-neighbour choice
  when scaled.
- **The test.** A message "simulated" by `SendMessage` gets 96 back from `GetDpiForWindow` on the
  hidden desktop, so the handler must take the DPI from `wParam` and the rect from `lParam`.
- **Fix:** Add a units table to D1 that gives each API's unit. Say that the handler reads only
  `wParam`/`lParam`, and test it that way. Say what happens when the process is already
  DPI-aware some other way, for example an app with its own manifest or `gy_attach`.

### 16. P2. X1: the IME test hook skips the code it should prove, and an R8 atlas can't draw colour emoji
- **Section:** X1 (l.273-283).
- **The hook.** `gy_input_ime(comp, result)` is a back door, so the `WM_IME_*` handler and its
  `ImmGetCompositionStringW` calls never run in a test.
- **Colour emoji.** The atlas is R8 (G1, l.134), so "an emoji" golden will be a monochrome
  glyph.
- **Shaping.** Glyph-by-glyph fallback doesn't shape Arabic or Indic scripts.
- **Fix:** Route the imm32 calls through GUICy's run-time function table, which it already needs
  for `LoadLibrary`. The test swaps in stubs, then sends real `WM_IME_STARTCOMPOSITION`,
  `WM_IME_COMPOSITION` and `WM_IME_ENDCOMPOSITION` messages to the hidden window, so the real
  handler runs. Say that emoji are monochrome (or add BGRA pages and their KB). Name the scripts
  that are out of scope.

### 17. P2. Some timing proofs are flaky on a shared PC
- **Section:** W1 proof (l.354-356), G1 decision (l.165-166), section 7 risk 10.
- "100,000 rows at the same frame ms as 100 rows, within 10 %" is at noise level. GUICy's own
  per-move numbers varied 0.085-0.31 ms between runs (BENCH.md:155).
- **Fix:** Make the pass/fail test deterministic: rows formatted, instances pushed and tiles
  dirtied must be equal for 100 and 100,000 rows, read from `gy_get_stats`. Report the times
  beside them. The same goes for F1's culling budget: count, then time.

### 18. P2. WARP runs and the GPU benches against "keep the PC light"
- **Section:** G1 proof, D1 proof (WARP at 144 DPI).
- WARP "uses all cores" (research/tiling-locks-gpu.md:532-533). WARP goldens of every scene, plus
  the 144 DPI set, plus the fuzz, can load every core while the user plays (AGENT_RULES.md:8).
- **Fix:** Run WARP on a small set (one scene per kind and the fuzz at a small size). Give the
  WARP device one thread if `D3D11_CREATE_DEVICE_*` allows, or set the affinity of the test
  process only. Run the hardware-GPU timings in one short batch and log other GPU load.

### 19. P2. The folder picker breaks the imports rule
- **Section:** section 3 imports (l.81-83), W4 (l.380-382).
- comdlg32 has no folder picker. `gy_file_folder` needs `IFileOpenDialog` (ole32 COM) or
  `SHBrowseForFolder` (shell32). The rule says "nothing else".
- **Fix:** Add ole32 (or shell32) to the run-time list, or drop the folder picker.

### 20. P2. The T6 overview of pictures doesn't fit immediate mode as written
- **Section:** T6 (l.326-328).
- A tile on another workspace isn't shown, so `gy_tile` returns 0 and its widgets never run.
  There is nothing to draw it from.
- **Fix:** Say that the overview shows the last drawn picture of each tile, cached when it was
  last visible and marked stale. Or cut it to the Alt-held minimap of names and defer the
  pictures.

### 21. P2. G2: today's layout lines have no `gy:` prefix
- **Section:** G2 proof (l.245-247).
- The plan says "format `gy:` from today". Today's lines are bare, `H0.500(V0.400([Picker],...))`
  (guicy.h:2389-2412, loader :2424-2468). The converter has to recognise the bare form, or old
  lines stop loading.
- **Fix:** Correct the text. The G2 loader reads `gy2:`, and anything starting `[`, `H` or `V` is
  the step-4 format.

### 22. P2. The T6 test that "animations stay off when Windows turns them off" can't flip the setting
- **Section:** section 3 (l.79-80), T6 proof (l.331-332).
- Testing this by calling `SystemParametersInfo(SPI_SETCLIENTAREAANIMATION)` would change the
  user's real system setting.
- **Fix:** Read the setting through the function table and use a test stub. Never set system
  parameters.

### 23. P2. W1 tables against the "no sideways scrollbars" rule
- **Section:** W1 (l.350-353).
- Resizable columns and a frozen first column suggest horizontal scrolling. AGENT_RULES.md:22 says
  no sideways scrollbars.
- **Fix:** Say what happens: columns shrink to their minimum with an ellipsis, then extra columns
  hide behind the » menu. Make horizontal scrolling an app opt-in only.

### 24. P2. "Smallest ever" is not measured against the smallest libraries
- **Section:** section 1 (l.34-35), trace (l.482).
- compare.md has no microui (about 1,100 lines). Win32 controls already beat GUICy on exe size
  (47 against 142 KB, compare.md:42 and :35).
- **Fix:** Add microui with a tiny GDI renderer to the compare.md re-run at M1. Until then, claim
  only "smaller than Nuklear, ImGui and raygui".

---

## P3: nits

25. **P3. Not asked, so defer:** the command palette (W2), toasts and the errors window (W5).
    They cost KB in W groups and weren't asked for. The errors banner already exists
    (guicy.h:161).
26. **P3. Section 1 (l.13-14):** "ImGui ... burn 1.3-6 s a minute" leaves out ImGui's
    wait-for-input loop at 0.23 s (compare.md:39). Say "when they draw every frame".
27. **P3.** `hello.c` and "the 15-line example builds as a test" (trace l.483-485) belong to no
    step's deliverable, and only `examples/demo.c` exists.
28. **P3.** The "0 Presents" proof needs a `presents` counter in `gy_stats` (guicy.h:224-229 has
    none), written to the log so `baseline.py` can read it.
29. **P3.** The frame-latency waitable goes in the wait only while a frame or an animation is
    pending (research/gpu.md:417-418). Say so in G1, or the idle wait spins.
30. **P3.** GPU timestamps and the readback helpers belong behind a `GUICY_BENCH` or
    `GUICY_TESTS` switch, so the +20 KB isn't spent on bench code.
31. **P3.** The first DirectWrite fallback loads dwrite and the system font collection, a
    one-time spike of several ms. Budget it in X1.
32. **P3.** Section 3's scan list should name "<scan word>" and X:\ and O:\ paths, as AGENT_RULES.md:12
    does.
33. **P3.** `GY__TABS` is 8 (guicy.h:391). Say whether containers lift the cap, since the » menu
    is tested with exactly 8.
34. **P3. G1 (l.147-148):** "no visible change" at the CPU-to-GPU swap can't be proven on the
    hidden desktop. Also say how the child avoids a black frame before its first Present
    (create it after the first GPU frame is ready).
35. **P3. F1 item 1:** with fractional canvas radii (`gy_canvas_rect` takes a float), cached
    corners may differ by one rounding step between box widths (`guicy.h:2615-2618`). Risk 6
    covers this. Name the case.
36. **P3. L1:** design `gy__tile_may` for containers from the start (research
    tiling-locks-gpu.md:582-585), so G2 doesn't rework it.
37. **P3.** The start budgets use two metrics: "start to window" in G1 (l.170-171) and "start to
    first frame (library)" in section 5 (l.425). Pick one per row.
38. **P3. Missing widget:** a multi-line edit box (common, and where IME matters most) is in no
    W step.

---

P1 open: 8

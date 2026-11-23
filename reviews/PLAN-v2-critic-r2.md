# PLAN-v2: critic review, round 2

Reviewed: X:\guicy\PLAN-v2.md (draft 2, 2026-11-23) and reviews/PLAN-v2-response-r1.md, against
the same evidence as round 1 plus build.cmd. Nothing was built or run. Line numbers are
PLAN-v2.md draft 2 lines unless a file is named.

The user's GPU decision ("every case must win": the GPU is the default only if it is faster in
every measured case, small changes included, otherwise the CPU) is taken as fixed. Below I check
only that the plan states it faithfully.

---

## Part 1: round-1 objections against draft 2

| # | Sev r1 | State | Reason |
|---|---|---|---|
| 1 | P1 | **closed** | The occlusion policy is stated (l.164-168). A proof detail it causes is new item N7. |
| 2 | P1 | **closed** | Full frame on every change, plus `Present1` dirty rects (l.159-163). The first present after a resize or after occlusion is new item N4, and the hash pass is N6. |
| 3 | P1 | **partly** (now P2) | The forwarding code is tested. But a `WS_DISABLED` child may never get real mouse messages, so the posted-message tests prove code that real input may never reach. See N5. |
| 4 | P1 | **closed** | Mask, integer shader maths and the histogram are in (l.155-158, 199-203). |
| 5 | P1 | **superseded** | By the user's "every case must win", stated faithfully at l.184-196. Its consequence is misreported, new N3, and the harness isn't defined, new N8. |
| 6 | P1 | **partly** (now P2) | The cap and the method are fixed. The 115 KB trigger already fires on the 117 KB estimate, and F1's +2 is unmeasured. See N10. |
| 7 | P1 | **closed** | Geometry file, never re-baselined (l.65-70). |
| 8 | P1 | **closed** | Code layout and per-flag invariants are defined; the fuzz checks both halves (l.244-254, 281-283). Nit: the code place of a locked tile the app opens late, N12. |
| 9 | P2 | closed | Question 1 lists all four insertions; F1 waits for the answer. |
| 10 | P2 | closed | H1 added. Its own problems are new items N2 and N9. |
| 11 | P2 | **partly** | The switches and the hello budget are in. The `GUICY_STATIC` claim overreaches, N11. |
| 12 | P2 | **reopened as P1** | "Changed (design, no question)" picks the side that breaks another of the user's rules without asking. See N1. |
| 13 | P2 | closed | The gate covers splits, docks and inner windows. |
| 14 | P2 | **partly** | W6 is scheduled but not designed, N9. |
| 15 | P2 | closed | Units table, `wParam`/`lParam` only. Nit on DIP rounding, N13. |
| 16-17 | P2 | closed | |
| 18 | P2 | closed | Nit: say why 2-core affinity isn't one of the "priority tricks" AGENT_RULES.md:7 bans, N15. |
| 19 | P2 | closed | Nit on COM init, N16. |
| 20-24 | P2 | closed | |
| 25-26 | P3 | closed | |
| 27 | P3 | **partly** | The 15-line example doesn't compile against today's API, N14. |
| 28-38 | P3 | closed | 32: keeping the banned word out of the repo is a fair reason. |

---

## Part 2: new objections

### N1. P1. A too-small screen clips content, choosing one user rule over another without asking

- **Section:** L1 (l.259-263); risk 8 (l.512-513); response 12.
- **What is wrong:**
  - **It breaks a rule.** When the locked layout doesn't fit, the plan clips at the right and
    bottom edges, with a vertical scroll bar only. AGENT_RULES.md:22 asks for "no sideways
    scrollbars" and "the main buttons always visible", so whatever is cut off at the right can
    never be reached on that screen. Risk 8 admits this breaks "main buttons always visible".
    AGENT_RULES.md:3 says that when two of the rules conflict, Master Control or the user is
    asked: "never pick the rule-breaking way on your own". Response 12 does exactly that ("design,
    no question").
  - **It can hide the locked tiles.** A locked tile at the right edge gets clipped too. "Nothing a
    lock holds ever moves", but the lock then makes its own tile unusable, which is not what a
    developer locks a tile for.
  - **The minimum size can exceed the screen.** Raising `WM_GETMINMAXINFO` above the monitor's
    work area (guicy.h:2918-2923) gives a window larger than the screen. Its edge then sits off
    the monitor rather than "clipped inside", and the plan doesn't say which happens.
- **Fix:**
  - First try a way that breaks neither rule. When the locked minimum doesn't fit the monitor,
    GUICy lowers its own UI scale, like a browser zoom: the DIP-to-pixel factor drops from D1's
    scale, down to a floor such as 75 %. Locks are held in DIPs, so nothing a lock holds moves
    or changes size, and everything stays visible. D1 already makes every size go through one
    scale.
  - Cap the raised minimum at the monitor work area (through the function table, so tests can
    stub the monitor size).
  - Only below the zoom floor is there a real conflict, and that one goes to the user through
    Master Control: clip, or let locked sizes give way.
  - Test it with stubbed work areas of 1366x768 and 1024x600: the locked rects are unchanged in
    DIPs, and no clipped pixels appear above the floor.

### N2. P1. Where do the windowed tests run?

- **Section:** H1 proof (l.143-145); G1 child-window tests (l.206-208); D1 (l.235-237); X1
  (l.330-333); W6 (l.444); section 3 "All runs hidden" (l.57-59).
- **What is wrong:**
  - From H1 on, every test must also pass "on an app-made hidden window", and the G1, D1, X1 and
    W6 proofs send messages to real HWNDs. But section 3 says gytest runs offscreen, and nothing
    says gytest moves to a hidden desktop. Only baseline.py does that (tools/baseline.py:89-90,
    124-127).
  - `gy_open` calls `ShowWindow(SW_SHOWDEFAULT)` (guicy.h:2959). The H1 proof's "the same
    pictures as through `gy_open`", run from gytest on the user's desktop, would show and
    activate a window on the user's screen, which breaks the first rule in AGENT_RULES.md:6.
  - A window that is never shown still lives on the user's desktop, so one bug shows it.
- **Fix:**
  - Say that every windowed test runs in a gytest process on its own desktop. For example,
    `gytest --windowed` relaunches itself with `CreateDesktopW` and `STARTUPINFO.lpDesktop`, as
    baseline.py does, and collects the result from a file.
  - Add a guard: in `GUICY_TESTS` builds, `gy_open` and `gy_attach` refuse to run (with an error
    that names the reason) when `GetThreadDesktop` is the "Default" desktop.
  - Put both in H1's deliverable and proof.

### N3. P1. The every-case race is decided before it starts, and the plan calls the result only "likely"

- **Section:** G1, the default renderer (l.184-196); fallback chain (l.178-179).
- **What is wrong:**
  - The case list includes "the first frame" and "start to window". G1 itself makes the device
    on a worker thread "while the first frame draws on the CPU path" (l.178-179). So on the GPU
    path the first frame is drawn by the CPU, and start to window is the CPU path's: both cases
    are ties by construction.
  - "A difference inside the run-to-run spread counts as not faster" turns those ties into GPU
    losses, so the CPU default is **certain**, not "likely" (l.194-196).
  - If the device were made synchronously, start to window would instead pay for device creation
    (10-50 ms, research/tiling-locks-gpu.md:550-551) and lose. Either way the outcome is fixed
    before G1 measures anything. The rule itself is stated faithfully. Saying "likely" under-reports
    what follows from it, and the user should hear that before G1 spends work on the decision
    table.
- **Fix:**
  - Say it plainly in G1 and section 6: with this case list, start to window and the first frame
    can't be won by the GPU, so the CPU stays the default. Send that to the user through Master
    Control now, as a fact, not a question about the rule. The user may confirm it or say which
    cases they meant. G1 still builds the GPU path and measures every case either way.
  - In the table, mark which cases the GPU path draws with the GPU and which it hands to the CPU.

### N4. P2. Occlusion and dirty rects: the first present after the window comes back

- **Section:** G1 (l.159-168).
- **What is wrong:** With occluded presents treated as success and only this frame's tiles sent as
  dirty rects, no one has checked whether DWM shows the whole of an occluded or minimized frame
  once the window is visible again. A tile that changed while the window was hidden may never
  appear in a later dirty rect, which leaves stale pixels on screen. This can't be tested on the
  hidden desktop.
- **Fix:** Send the first `Present1` after any `DXGI_STATUS_OCCLUDED`, a restore, a
  `ResizeBuffers` or a renderer swap with no dirty rects (a full present). That costs one present.
  Add a unit test of the flag logic through the function table's DXGI stubs.

### N5. P2. The child: a test that holds only by construction

(This is round-1 item 3, partly closed.)

- **Section:** G1 (l.169-174, 206-208); risk 3 (l.502-503).
- **What is wrong:** With the child `WS_DISABLED`, Windows itself decides where real clicks over a
  disabled child go. The forwarding procedure may never get them. `PostMessage` to the child's
  HWND skips both the hit test and the disabled check, so the test proves the forwarder, not the
  real path. Risk 3 says only drag-and-drop and touch are unreached, which understates it: the
  whole real mouse path is unproven on the hidden desktop.
- **Fix:** Pick one mechanism and say why. Either use `WS_DISABLED` and rely on the system sending
  input to the parent (then the forwarder isn't needed and isn't tested), or keep the child
  enabled with `HTTRANSPARENT` plus the forwarder, and `SetFocus` back to the parent on
  `WM_SETFOCUS`. Widen risk 3 to "real mouse routing over the child", checked by the user's eye at
  the first port.

### N6. P2. Full redraw: what still decides the dirty rects

- **Section:** G1 (l.159-163).
- **What is wrong:** The dirty rects come from the CPU tile hashes. So the GPU path still runs the
  whole hash pass (about 3-4 ns an instance, stress.md:238-240), and the first present after
  `ResizeBuffers` has no previous frame to compare against.
- **Fix:** Say that the hash pass stays on the GPU path and is part of its CPU ms in the race.
  Make "full present after resize" part of N4's rule.

### N7. P2. The "drawing really happens" proof fails by design

- **Section:** G1 proof (l.212-214).
- **What is wrong:** "`gpu_draws` equals the CPU path's drawn-frame count" can't hold. The GPU
  path's first frame or frames are drawn on the CPU while the worker thread makes the device
  (l.178-179).
- **Fix:** Either compare `gpu_draws + cpu_draws_on_gpu_path` with the CPU path's count, or force
  a synchronous device in that test (`GUICY_RENDER=gpu` waits for the device) and say so.

### N8. P2. The every-case harness isn't defined

- **Section:** G1 (l.187-188).
- **What is wrong:** "CPU ms plus GPU-timestamp ms per frame on the same scene" doesn't say where
  it runs. Offscreen, the CPU path leaves out its `BitBlt` and the GPU path leaves out `Present1`
  and its driver cost, often 0.05-0.2 ms of CPU, which is the size of a hover. On the hidden
  desktop both are included, though occluded. The verdict can flip on that choice, and "every
  case must win" makes small cases decisive.
- **Fix:** Run the race windowed on the hidden desktop. Count each path's present call (`BitBlt`,
  `Present1`) inside its CPU ms, and give the offscreen numbers beside it for reference.

### N9. P2. W6 and `gy_ctx_set` are scheduled but not designed

- **Section:** H1 (l.138-139); W6 (l.439-445).
- **What is wrong:**
  - **The context.** All state is one static struct, `static gy__ctx gy__c;` (guicy.h:517), with
    1,037 `gy__c.` references in guicy.h. `gy_ctx_set` means either a pointer context, which
    touches every reference, costs code size and slows every access, or swapping the struct in and
    out. H1 budgets +2 KB and says nothing about which.
  - **The window procedure.** It writes every message into whichever context is current
    (guicy.h:2882-2936). With two windows, the second window's input lands in the first
    window's context unless the procedure looks up the context from the HWND (`GWLP_USERDATA`).
  - **The loop.** `while (gy_frame())` sleeps "until something happens" for one window, and the
    plan doesn't say how an app writes the frame of two windows.
  - **Pop-out.** A pop-out moves a tile between two trees, across contexts, while "locks hold
    across both". That is the largest piece of W6, and it is budgeted at +3 KB.
  - **Focus.** A second window opened by the app (for example a pop-out restored from the saved
    line at start) would be activated by `ShowWindow(SW_SHOWDEFAULT)` and take focus, against
    PLAN.md 3.12's "a tile the app opens never takes focus".
- **Fix:**
  - In H1, choose the context mechanism, and measure it: the size change, and F1's stress numbers
    before and after.
  - In W6, give:
    - the routing (HWND to context, set on every message);
    - the loop (one `gy_frame` that serves every window and returns per window, or
      `gy_frame_all`);
    - `SW_SHOWNOACTIVATE` for windows the app opens itself;
    - that a FLOAT-locked tile can't pop out (research table 2.9 lists pop-out under FLOAT).
  - Re-estimate the KB.

### N10. P2. The 117 KB estimate already trips its own 115 KB trigger, and F1's +2 has no number behind it

- **Section:** section 5 (l.464-465, 480-482); F1 budget (l.133); risk 4 (l.504-505).
- **What is wrong:**
  - **The trigger.** "If the core estimate passes 115 KB at any step, these move behind a switch".
    The estimate is 117 KB, so by the plan's own rule the three fallbacks have to be switched
    before F1, yet section 5 still counts them in the core.
  - **F1's number.** F1's budget went from +4 (draft 1) to "+2 net (it replaces code)". F1 adds
    the corner cache, line spans, SSE2 and AVX2 image kernels, culling, layer counts and
    `gy_scroll_rows`, so the cut fits the total, not a measurement.
  - **The order runs out.** The fallback order frees perhaps 4-6 KB, and the plan says nothing
    about an overrun larger than that.
  - **Two fallbacks change behaviour.** Switching off P1's strictness changes the default build's
    behaviour ("text never cut"). Making the DirectWrite fallback off by default brings back the
    boxes in CJK file names that pixelhunter needs it for (PLAN.md 3.8).
- **Fix:**
  - Trigger on the **measured** (i) and (ii), not on estimates. Keep F1 at +4 until F1 measures it.
  - Name a fourth and fifth candidate (for example user locks and the undo ring under a switch,
    or the strip code's » popup).
  - Mark which fallbacks change default behaviour, and say they need Master Control's OK before
    they are applied.

### N11. P2. `GUICY_STATIC` doesn't do what the trace says

- **Section:** section 3 (ii) and (iii) (l.73-76); trace "easy to import to an EXE ... no linker
  flags" (l.528).
- **What is wrong:**
  - **One file only.** With every function `static`, the API can only be called from the one .c
    file that holds the implementation. voxup (pick.c plus nk_ext.c) and bbb_kit (nk_side.c,
    nk_top, nk_ui.c; BENCH.md:45, 79-80) call the UI from several files, so they can't use it.
    For them, "no linker flags" means paying all of (i).
  - **The prototypes.** The public prototypes are non-static (guicy.h:88-230), and a later
    `static` definition is an error. A `GUICY_API` macro has to go on every prototype.
  - **The build fails.** build.cmd:14 builds with `-Wall -Wextra -Werror`. Unused static
    functions then raise `-Wunused-function`, so `hello.exe` and `core.exe` fail to build.
  - **The size count is low.** Summing the `gy_`/`gy__` sections leaves out string literals,
    tables and `.rdata` in other sections, and code inlined into app functions.
- **Fix:**
  - Say that `GUICY_STATIC` is for single-file apps, and give (i) as the number for apps with
    several files.
  - Add `GUICY_API` on the prototypes and `__attribute__((unused))` (or a pragma) on the static
    definitions, and build hello and core under build.cmd's flags.
  - Measure GUICy's share as the `guicy.o` contribution in the map file, with GUICy built as its
    own translation unit, rather than by summing names.

### N12-N17. P3.

- **N12.** L1: say what the code place of a locked tile is when the app opens it late (for example
  a Log tile that opens when a job finishes). It should be its rule place at first open, and the
  code layout gets it added then.
- **N13.** D1: give the DIP-to-pixel rounding rule at 125 % and 150 %. Rect edges snap to whole
  pixels (PLAN.md 3.4), so a 1-DIP border is 1 px or 2 px by a fixed rule. Otherwise `gy_rect`
  ints in DIPs lose precision and borders vary from widget to widget.
- **N14.** The 15-line PLAN.md example (PLAN.md:259) calls `gy_canvas_rect(gy_avail(), 6,
  gy_theme()->card)`. That has the wrong argument order against guicy.h:201, and `gy_theme()`
  doesn't exist (it is `gy_theme_get`). H1 has to fix the example or add the API, and say which.
- **N15.** G1: 2-core affinity for WARP is not a priority change, but AGENT_RULES.md:7 bans
  "priority tricks". Ask Master Control once, or limit WARP by running only the small set.
- **N16.** W4: `IFileDialog` needs `CoInitializeEx` on the UI thread, which can clash with an app's
  own COM apartment. Say that GUICy calls it only around the dialog and accepts
  `RPC_E_CHANGED_MODE`.
- **N17.** The too-small-screen test (L1 proof, l.284) needs the monitor work area stubbed through
  the function table. Name the stub.

---

## Summary

- **Open P1:** N1, N2, N3.
- **P2:** N4-N11, eight of them. Round-1 items 3, 6, 11 and 14 are partly closed and folded into
  N5, N10, N11 and N9.
- **P3:** N12-N17, six of them. Round-1 item 27 is partly closed and folded into N14.

P1 open: 3

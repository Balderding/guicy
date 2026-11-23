# PLAN-v2: critic review, round 4

Reviewed: X:\guicy\PLAN-v2.md (draft 4, 646 lines) and reviews/PLAN-v2-response-r3.md. Nothing
was built or run. Line numbers are draft-4 PLAN-v2.md lines.

**The decisions, as stated in the plan:**
- **The lead's R3 decision** (the floor is relative, like browser zoom: 0.75 times the monitor's
  normal factor, in steps of 5 % of it): stated faithfully in D1 (l.280-282) and L1 (l.338-339).
  One proof line still says "factor 75 %" (P3-7).
- **The user's decisions (a), (b) and (c):** still stated faithfully.

This round follows the rule that a P1 means the plan is wrong or can't be proven as written.

---

## Part 1: round-3 items

| # | Sev r3 | State | Reason |
|---|---|---|---|
| R1 | P1 | **closed** | The race records presents that returned occluded and times the present call alone. A win smaller than that call is a loss. A GPU default needs a visible run that the user approves. Risk 1 is rewritten (l.198-212, 603-606). The protocol for that run is P2-1. |
| R2 | P1 | **closed** | Synthetic test font at factors 1, 2 and 3; the real-font goldens are for the look (l.296-301). Trace row 629 still says "factor-2" (P3-8). |
| R3 | P2 | closed | The lead's decision is stated, with a 10 ms budget per step. |
| R4 | P2 | **partly** | `GUICY_RENDER=gpu` is forced for the check. But the visible run happens only if the GPU might win, so these checks can be left with no run at all. See P2-1. |
| R5 | P2 | closed | Float DIPs; rects come back as pixel edges divided by the factor; hit tests in pixels; a seam test and a border-click test. Notes in P3-10. |
| R6 | P2 | closed | The monitor is the one under the window's centre, the switch happens on `WM_EXITSIZEMOVE`, and the resize keeps the centre, so the centre stays on the new monitor and can't bounce. A stub test is in. |
| R7 | P2 | **partly** | The focus check is now a guard, with real focus moved to the visible run. But the new cost metric doesn't measure speed. See P2-3. |
| R8 | P2 | closed | Defined, with `/*px*/` marks and both counts reported. Note in P3-11. |
| R9-R14 | P3 | closed | |
| R15 | P3 | **partly** | Nothing large is padding; about 15 lines of repeats remain (P3-12). |

---

## Part 2: new objections

### P2-1. P2. The visible run needs a protocol, and some checks rely on a run that may never happen

- **Section:** G1 (l.207-212, 228-231), section 6 (l.591-593), W6 (l.545-548), risk 3 (l.609-611).
- **What is wrong:**
  - **"While the user is away".** No session can tell when the user is away. The user has to
    choose the moment.
  - **It can still be occluded.** A window shown with `SW_SHOWNOACTIVATE` but covered by the user's
    other windows, or by a full-screen game, is occluded too. Such a "visible" run would repeat
    the hidden-desktop problem without anyone noticing.
  - **It may never run.** The visible run is held only "if the hidden race allows a GPU win". With
    a likely CPU default it never happens. Real mouse routing over the GPU child then waits for
    "the first port with `GUICY_RENDER=gpu`", and W6's real-focus check has no other time at all.
- **Fix:**
  - The user names the time, through Master Control.
  - The run counts only if every present returned `S_OK`, with no `OCCLUDED`. Otherwise it
    stops and reports.
  - Split the visible checks off from the race: the GPU-child mouse check, and at W6 the focus
    check. Ask for them as one short approved visible check whatever the race shows, or put them
    explicitly on the first-port checklist (W6's included).

### P2-2. P2. The race's 1440p case collides with the opening-size cap

- **Section:** the race harness (l.193-194), D1 opening size (l.283-284).
- **What is wrong:** The race runs a real window at 2560x1440, but R10's new rule opens any window
  larger than the work area at the work area's size. This PC's main monitor is 2560x1440, so its
  work area is smaller once the taskbar and the title bar are counted. The 1440p case would
  quietly run at a smaller size than BENCH.md says.
- **Fix:** In the race, stub `gy__os.work_area` larger, or use a test hook that sets the client
  size exactly. Record the real client size in each row.

### P2-3. P2. The context-cost metric counts code, not run time

- **Section:** W6 (l.528-531).
- **What is wrong:** "The instruction count of the UI pass and render functions (llvm-objdump)" is
  a static count, so it is really code size measured a second time. The run-time cost of the
  pointer comes from reloading the pointer inside hot loops: a few bytes of code, executed millions
  of times. A static count barely moves for that, so the "5 % instructions" threshold can pass a
  slower default.
- **Fix:** Keep the static size check, and add two run-time checks that don't depend on wall-clock
  noise:
  - a disassembly check that the inner loops of the kernels, the tile hash pass and the box/line
    raster loops load no context pointer;
  - `QueryThreadCycleTime`, which counts the thread's own cycles and is far less noisy than wall
    time, over the gybench-style 31-run median on the gystress scenes, with the threshold set
    above the measured spread.

### P3-4 to P3-12. Nits

- **P3-4. The default renderer paragraph (l.211-213):** "Otherwise too the CPU path stays the
  default" repeats the sentence before it.
- **P3-5. Float `gy_rect`:** say that instances stay whole device pixels, so floats never enter
  the tile hash. Line endpoints are already hashed as float bits (guicy.h:337), and that stays.
  Also test the round trip (pixel to DIP to pixel equals the pixel) at every zoom step from 1.25
  times 1.00 down to 1.25 times 0.75, not only at 125 %.
- **P3-6. Float `gy_rect` for apps:** one line in the API notes. Apps that keep rects in `int`
  should use `gy_rect`'s floats as given. A cast before the canvas call brings back the 1-px seams
  at 125 %.
- **P3-7. L1 proof (l.365):** "factor 75 %" should read "0.75 times the normal factor". The stub
  monitor is at 100 %, so the numbers happen to match there.
- **P3-8. Trace row 629:** "factor-2 instance check" should read "factors 1, 2 and 3 with the
  synthetic font".
- **P3-9. The context options:** if the swap option wins, say how much of the struct is copied per
  swap, and that arena pointers stay valid (they point into `VirtualAlloc` memory, not the struct).
- **P3-10. check_px.py:** it skips literals under 2, so `+ 1` insets aren't checked. The synthetic
  font check at factors 1, 2 and 3 catches them. Say that it is the backstop.
- **P3-11. Visible-run list:** name the checks it carries in one place, section 6, and refer to it
  from G1, W6 and risk 3.
- **P3-12. Length (646 lines).** The growth since draft 3 is proof and design the reviews asked
  for, so it isn't padding. These repeat and can go (about 15 lines):
  - risk 1 (the same as G1 l.198-212);
  - the duplicate sentence in P3-4;
  - trace row 628's long text, which could just say "G1, the default renderer";
  - risk 5, which repeats section 3's "one run at a time, short batches".

---

## Summary

- **P1:** none.
- **P2:** 3 (P2-1 to P2-3).
- **P3:** 9 (P3-4 to P3-12).

P1 open: 0

Ready for Master Control's evaluator: yes. These P2s should still be done during the build:
- **P2-1, at G1 time:** the visible-run protocol (the user names the time; a run counts only with
  0 occluded presents), and the GPU-child mouse check and W6 focus check asked for apart from the
  race or put on the first-port checklist.
- **P2-2, in G1's race harness:** a work-area stub or an exact-size hook, so the 1440p case really
  runs at 2560x1440.
- **P2-3, at the start of W6:** the context cost judged by loads in the hot loops and by
  `QueryThreadCycleTime` medians, not by a static instruction count alone.

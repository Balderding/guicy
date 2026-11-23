# PLAN-v2: critic review, round 5 (only the new parts of draft 5)

Reviewed: X:\guicy\PLAN-v2.md (draft 5, 702 lines) and reviews/PLAN-v2-response-r4.md. As asked,
this round covers only what changed since draft 4. Nothing was built or run. Line numbers are
draft-5 PLAN-v2.md lines.

**Vulkan:** none. A case-insensitive search of PLAN-v2.md for "vulkan" finds nothing. The only
other backend named is the "later" Linux GL 3.3 one (l.612), which was already there.

**The new asks, as stated in the plan:**
- **(a) VS Code-style zoom and sizes from the font:** in D1 (l.293-312, 341-346).
- **(b) "Or use the GPU to scale":** in G1 (l.193-198, 241-244) and D1 (l.313-317). The final
  frame is always the crisp re-layout, and the preview is never the resting frame.
- **(c) Master Control's answers:** a reason per order change in section 2 (l.33-43), and the
  shader compiler at build time only (l.168, 657-659).

All three are stated faithfully.

---

## Part 1: round-4 items

| # | Sev r4 | State | Reason |
|---|---|---|---|
| P2-1 | P2 | **closed** | Visible checks A, B and C, each asked separately. The user names the time, and a run counts only if every present returned `S_OK`. B and C fall back to the first-port checklist (l.644-652, 607-609). Nit P3-7. |
| P2-2 | P2 | **closed** | An exact client-size hook and a larger stubbed work area; each row records the real size (l.204-205). |
| P2-3 | P2 | **closed** | Code size, a check of the hot loops for context-pointer loads, and `QueryThreadCycleTime` medians (response P2-3). |
| P3-4 to P3-11 | P3 | closed | |
| P3-12 | P3 | **partly** | Length, see P3-10. |

---

## Part 2: new parts

### P2-1. P2. "Today's goldens stay byte-equal" with em floats holds only if the em values become pixels once, before any layout

- **Section:** D1 (l.304-312, 341-343).
- **What is right:** The constants themselves round back exactly. For example 24/15 as a float,
  times 15, is within one float step of 24, and `floor(x + 0.5)` gives 24.
- **What isn't covered:** Today's layout code does integer arithmetic on those values: centring as
  `(w - tw) / 2`, equal columns as `w / n`, tile splits as `(int)((r.w - 6) * ratio)`
  (guicy.h:2104-2108), all truncating. If the values stay floats into the layout code, those sites
  start rounding instead of truncating. An odd centring gap then moves text by 1 px, and the
  "byte-equal" claim fails at D1.
- **Fix:** Say in D1 where the em values become pixels. On every factor or font change, GUICy
  resolves the theme once into a whole-pixel "resolved theme"
  (`row_h_px = floor(row_h_em * font_px * f + 0.5)`, and so on). All layout code keeps its
  current integer arithmetic on those values, so at factor 1 and today's font size the integers
  equal today's exactly. Add one test that the resolved theme at factor 1 equals today's
  `gy_theme` integers, field by field.

### P2-2. P2. The factor chain needs rules for the font term and for zooming past what locks allow

- **Section:** D1 (l.293-303); L1 too-small screen.
- **What is wrong:**
  - **Which factor is "normal"?** The 0.75 floor is "0.75 x the normal factor", and the product
    has three terms: monitor x zoom x font ratio. The plan doesn't say whether the normal factor
    includes the user's font ratio. Suppose a user picks a smaller font (ratio 0.8) and zoom 0.75.
    If the font term counts, the product is 0.6 x the monitor factor, already below the floor, and
    "locked sizes give way" with no screen problem at all. If it doesn't, the floor isn't on the
    whole factor.
  - **Zooming in past what locks allow.** Locks hold in DIPs, so zooming to 3.0 can make the locked
    layout bigger than the window. The plan doesn't say whether the window grows (up to the work
    area), whether the fit zoom then quietly cancels the user's Ctrl+=, or what the user sees.
    Pressing a key that visibly does nothing goes against "fewer clicks".
- **Fix:**
  - State that the floor bounds zoom only: user zoom x fit zoom >= 0.75 of the normal factor,
    where the normal factor = monitor factor x font ratio. The font size is the user's own choice,
    is never limited by the floor, and never triggers give-way by itself.
  - State the zoom-in order: the window first grows to its raised minimum, up to the work area.
    Then the fit zoom caps the effective zoom and the gold banner says "zoom limited by the
    layout: N %". Add both cases to the L1 too-small test.

### P2-3. P2. The zoom preview: what it stretches, when "the input stops", and the move case that never happens

- **Section:** G1 (l.193-198, 241-244), D1 (l.280-282, 313-317), section 2 order.
- **What is wrong:**
  - **What it stretches.** With `FLIP_SEQUENTIAL` and 2 buffers, the back buffer you can draw into
    doesn't hold the last frame. After `ResizeBuffers` (a monitor move, a resize) no swap-chain
    buffer survives at all. So "the last frame's texture" has to be a kept copy, made with
    `CopyResource` after each drawn frame or by drawing into an offscreen texture. That costs a
    full-frame copy each frame and about 14.7 MB of GPU memory at 1440p. The plan doesn't
    mention it.
  - **When "the input stops".** "Within 1 frame after the zoom input stops" depends on "an active
    zoom" (Ctrl+wheel moving, keys repeating), which isn't defined. Any definition based on time
    would need a timer, and the idle rule forbids one.
  - **The monitor move.** D1's own rule (R6) applies the factor at `WM_EXITSIZEMOVE`. During a
    same-DPI monitor move the size never changes, so there is nothing to preview. Only a real
    `WM_DPICHANGED` mid-drag (Windows resizes the window) or Ctrl+wheel can use the preview.
  - **The order.** G1's proof list holds a proof of the preview, but it needs D1's zoom keys. D1
    comes after G1 (l.28).
- **Fix:**
  - The preview stretches the kept last-frame texture, drawn as one image instance (the Load-based
    image path, so no new shader). Its memory and per-frame copy cost go in G1's budget.
  - Define "active" with the existing two-pass rule: a zoom event's first pass draws the preview,
    and the settle pass draws the crisp frame unless another zoom event is already queued
    (`PeekMessage`). That gives "crisp within 1 frame" with no timer, and it can be scripted.
  - Limit the move case to `WM_DPICHANGED` during a drag.
  - Move the preview's proof from G1 to D1, where the zoom exists.

### P3-4 to P3-10. Nits

- **P3-4. Zoom keys:** VS Code also takes Ctrl+numpad + and -. Say whether they are included,
  that Ctrl+= is `VK_OEM_PLUS` on any keyboard layout, and whether the zoom keys can be rebound in
  the ini, as the tile keys can.
- **P3-5. `GY_KEY_WHEEL`:** it is all or nothing. An app such as voxup, which zooms its preview
  with Ctrl+wheel, loses Ctrl+wheel UI zoom everywhere. Allow ownership over a region (for example
  the last widget or a canvas rect). Also show the clash banner only for an ini rebinding that
  clashes, not for a key the app knowingly declares in `gy_keys_owned`, or it shows on every
  start.
- **P3-6. Size:** about 118 of 120 KB leaves 2 KB. D1's +6 now holds DPI, monitors, units, zoom
  keys, `gy_view_save/load` and the em theme. Say that D1's done-report gives its measured KB
  against the +6, because the trigger (l.636-640) will likely fire there first.
- **P3-7. Visible check B** needs the user's hands, not just a time: clicks, a drag with capture,
  a wheel and the cursor shape over the child. Give a 30-second script of what to do and what the
  log shows. The "every present `S_OK`" rule fits A, not B or C.
- **P3-8. The preview's cost** belongs in the GPU path's +20 KB, and its memory beside the 16 MB
  budget (which counts the arena, atlases and tiles, not GPU memory). Say so.
- **P3-9. Trace row (l.685):** "crisp within 1 frame after input stops" should name the two-pass
  definition, once P2-3 is fixed.
- **P3-10. Length (702 lines).** The 56 new lines are the user's three asks plus visible checks
  A, B and C: design and proof, not padding. Cuts, if wanted (about 8 lines):
  - D1's monitor list at l.289-292 against the trace row and the first-port checklist;
  - G1 l.193-198 once the preview moves to D1 (P2-3).

---

## Summary

- **P1:** none. No new part is wrong or unprovable as written: the byte-equal claim, the floor
  and the preview each need one stated rule, which the P2 fixes give.
- **P2:** 3 (P2-1 to P2-3).
- **P3:** 7 (P3-4 to P3-10).

P1 open: 0

Ready for Master Control's evaluator: yes. These P2s should still be done during the build, in D1:
- **P2-1:** resolve em values to a whole-pixel theme once per factor or font change, and keep the
  layout's integer arithmetic.
- **P2-2:** the floor bounds zoom only (the font ratio is outside it), plus the zoom-in rule
  (grow the window, then cap the zoom with a banner).
- **P2-3:** the preview stretches a kept last-frame copy; "active" uses the two-pass rule, with no
  timer; the move case is only `WM_DPICHANGED` mid-drag; the preview's proof moves to D1.

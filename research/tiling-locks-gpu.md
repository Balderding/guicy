# GUICy research: tiling, locks, panel layouts and the GPU

Written 2026-11-22 for the next phase (PLAN-tiling.md, steps G1-P1). It reads that plan, PLAN.md,
BENCH.md, research/gpu.md and the step-4 tiling code in guicy.h (`gy__tile_insert`,
`gy__tile_remove`, `gy_tiles_load`: a binary tree of up to 64 nodes, 32 tiles, 8 tabs a stack,
minimum tile 160x110, a broken line loads as empty).

Every claim has a source at the end. Anything marked **(memory)** is from knowledge, not from a
page read today. Line counts are estimates for GUICy's style (plain C, one header), not measured.

---

## 0. Top 20, ranked by value for the user's asks against cost

"Lines" = new lines in guicy.h, estimated. "Step" = the PLAN-tiling.md step it belongs to.

| # | Feature or technique | Why it matters | Lines | Step |
|---|---|---|---|---|
| 1 | **A child HWND for the GPU swap chain** (GDI keeps the parent window) | Microsoft: after the first `Present1` on a flip-model swap chain, *GDI no longer works with that HWND, even after the swap chain is destroyed*. The CPU fallback (BitBlt to the window) would break on a device-lost mid-run. A GPU child window that is destroyed on fallback keeps PLAN 3.7 step 4 working with no window re-create. | 60 | G1 |
| 2 | **`FLIP_SEQUENTIAL` + `Present1` dirty rects from the dirty tiles** | `FLIP_DISCARD` "cannot be used with ... partial presentation". GUICy already knows which 64x64 tiles changed; with 2 buffers the back buffer holds frame N-2, so redraw (scissor) the union of this frame's and last frame's dirty tiles and pass them to `Present1`. Cost then follows the change on the GPU too, like the CPU path. Moves PLAN step 13's "Present1 later" into G1. | 80 | G1 |
| 3 | **Lock flags with one rule: code wins over the saved line** | Locks must hold against mouse, keys and a loaded layout. Qt's `restoreState` and ImGui's .ini can both silently undo a developer's arrangement. GUICy: developer locks live in code (`gy_tile_lock`, rules), are re-applied after every `gy_tiles_load`, and a locked tile that the line puts elsewhere goes back to its rule place. Only user-made locks are saved in the line. | 150 | L1 |
| 4 | **Two more lock kinds from real apps: `GY_LOCK_SPLIT` (nothing may split or land in this tile) and `GY_LOCK_TABS` per stack** | ImGui had to add `NoDockingSplit` and later `NoUndocking` (issue 6780); VS Code's "Lock Editor Group" means "new editors will not open here"; Hyprland has `lockactivegroup` and a global `lockgroups`. The planned flag set has no "nothing new lands here", which is the one users ask for most after "don't move". | 30 | L1 |
| 5 | **One global "lock layout" toggle, saved, with a padlock and a key** | The commonest complaint across Spyder, FreeCAD (Aug 2026), Premiere, Photoshop and pgAdmin: one stray drag wrecks a layout and getting it back is hard. The plan has `gy_layout_lock(on)`; add that the user may flip it when `gy_allow_user_lock` is set, and that "reset to the app's default" (JetBrains Shift+F12, VS Code "Reset View Locations") is always one key. | 40 | L1, T6 |
| 6 | **Compile switches: `GUICY_NO_TILING`, `GUICY_TILING_FULL`, `GUICY_NO_ANIM`** | Master Control's rule. Basic tiling (step 4) stays the default; containers' extra layouts, workspaces, scratchpad, overview and animation sit under `GUICY_TILING_FULL`. A dialog-sized app drops all of it. | 20 | G2 |
| 7 | **i3-style container tree, with the binary tree as its subset** | i3/sway: every container has a layout (splith, splitv, tabbed, stacking) and any number of children; `layout toggle` cycles. Old `gy:` lines are exactly binary containers, so they convert 1:1. Komorebi, Amethyst and Hyprland all ship the same layout list on top of this. | 500 | G2 |
| 8 | **Automatic layouts as a function `(n, area, params) -> rects`, not as tree surgery** | dwm/xmonad/Amethyst/river all do it this way: a layout is a pure function of the count. Master + stack, grid, spiral (dwindle), centred master, columns, rows, monocle are 20-60 lines each. Locked tiles are taken out first and the function lays out the rest. | 300 | T2 |
| 9 | **Komorebi's layout rules by tile count** (`{1: tall, 4: grid}`) and a **tabs fallback** when tiles drop below their minimum | Cheap automatic answer to "very many tiles": the layout changes itself as tiles come online. Komorebi notes the catch: with rules set the user can't change layout by hand. GUICy: a manual change turns the rules off for that container until "reset". | 60 | T2 |
| 10 | **Drop zones: the 5-way diamond (Visual Studio) plus edge strips of the whole window** | VS: dragging a title shows a diamond with Top/Bottom/Left/Right/Center; away from the guides it floats. ImGui does the same, with Shift to stop docking (`ConfigDockingWithShift`). Outer-edge zones let a tile take a full side of the window, which the 5-way per-tile diamond can't. | 200 | T1 |
| 11 | **bspwm preselection** (Alt+Ctrl+arrow marks where the *next* tile goes, shown as a tinted area; Esc cancels) | Lets a user place tiles that come online later (a job finishing, a download) without dragging. One field on a leaf plus a draw. | 60 | T1 |
| 12 | **Slots: bspwm receptacles / i3 placeholders** | An empty, named leaf in the developer's layout that a tile fills when it first opens (`gy_tile_rule(.slot = "right-bottom")`). Exactly "each part comes online in its own tile" with a designed layout. i3 shows a placeholder with a watch symbol until the window arrives. | 80 | T5 |
| 13 | **Per-monitor DPI v2** | GUICy has no DPI handling today (no `GetDpiForWindow`, no `WM_DPICHANGED`). On a 150% screen everything is 2/3 size. Needs a scale on the theme metrics, a glyph-atlas flush, and the suggested rect from `WM_DPICHANGED`. Without it no other "fits any role" feature matters. | 200 | new, before P1 |
| 14 | **Panel alignment: docks + rows with stretch units + min/max + wrap (Morphorm/Clay/Fleury), not flexbox or CSS grid** | Clay is 4.8k lines; Morphorm gets "similar layouts to flexbox, with fewer concepts" from pixels / percent / stretch / auto plus min/max in one pass. Fleury's strictness value is one float that solves overflow. That covers toolbars, status bars, centred rows, wrap rows and N-way splits for about 450 lines. | 450 | P1 |
| 15 | **Overflow » menu for toolbars and tab strips** | When a row is narrower than its fixed items, the last items fold into a popup (priority order from the app). GUICy already has popups and measured widths, so this is mostly bookkeeping. Also fixes 8+ tabs in a stack. | 120 | P1 |
| 16 | **GPU timestamps only in bench mode, read 2-3 frames later** | `D3D11_QUERY_TIMESTAMP` pairs inside a `TIMESTAMP_DISJOINT` query, once per frame at most. Reading them must never wake an idle window, so `gy_stats` reads them only while frames are already running (the bench). | 60 | G1 |
| 17 | **Hidden GPU tests with no window: an offscreen render target + a staging copy** | Draw into a `D3D11_USAGE_DEFAULT` texture, `CopyResource` to a `STAGING` one, `Map` it, compare with the CPU golden. Runs under `GUICY_RENDER=warp` too (WARP passes WHQL, results "within the tolerances" of hardware). No window, no focus. | 90 | G1 |
| 18 | **Command palette** (Ctrl+Shift+P style) listing every tile action, every `gy_button_key` and app commands | Fits "every action one key away" and makes the Alt tile keys discoverable. Built on existing popup + edit + list. Fuzzy match is ~40 lines. | 250 | T6 or new |
| 19 | **Scratchpad and float as states of a tile, not new windows** | i3: `move scratchpad` / `scratchpad show` toggles a centred floating window with one key; bspwm/Hyprland: tiled / pseudo-tiled / floating / fullscreen with `node -t ~` going back. GUICy's float is an overlay layer, so no OS window. | 200 | T4 |
| 20 | **Niri / PaperWM scrollable strip as one more container layout** | Columns on a strip wider than the window, preset widths (1/3, 1/2, 2/3), "center on overflow". The clean answer to very many tiles at full size, where tabs hide too much. Only under `GUICY_TILING_FULL`. | 200 | T2 (late) |

Below the line (useful, but not now): Blender corner split/join (the drop zones cover it, and
Blender itself moved to "docking" in 5.x), multi-window pop-out (see 5.3), UI Automation (5.4),
DirectComposition animation (4.5), RTL text (5.7).

---

## 1. Tiling window managers: what to take for an in-app tiler

### 1.1 i3 and sway: the container tree

What it is:
- A tree of outputs, workspaces and containers. Any container holds windows or more containers,
  nested to any depth. Each container has one layout: `splith`, `splitv`, `tabbed`, `stacking`.
  `layout toggle split` / `layout toggle all` cycle them. `default_orientation auto` makes wide
  screens horizontal.
- **Marks:** `mark foo` on a container; `[con_mark="foo"] focus`, `move container to mark foo`,
  `swap container with mark foo`. A mark is a name the user gives a place.
- **Criteria rules:** `for_window [class=... title=...] <command>` runs when a matching window
  appears; `assign` sends it to a workspace (first match wins); `no_focus` stops it taking focus.
- **Scratchpad:** `move scratchpad` hides a window; `scratchpad show` brings it back as a
  centred floating window, and the same command hides it again, so one key toggles.
- **Layout saving:** `i3-save-tree` dumps JSON with `layout`, `percent` and `nodes`;
  `append_layout` loads it and creates **placeholders** showing their swallow criteria until a
  matching window opens. The JSON needs hand editing and isn't strict JSON.
- `focus_wrapping yes|no|force|workspace` sets what happens at the edge when moving focus.
- `floating_minimum_size` / `floating_maximum_size` bound floating windows.

Take for GUICy:
1. **The container model** (rank 7). One container struct: `layout`, `children[]`, `sizes[]`
   (fractions), `active` (for tabbed/stacked). The current binary node is a container with two
   children; tabs are a `tabbed` container. Converting an old `gy:` line is a recursive copy.
2. **`no_focus` as the default for app-opened tiles** (GUICy already does this: "a tile the app
   opens never takes focus").
3. **Placeholders = slots** (rank 12). In i3 they are the weakest part (hand-edited JSON). In
   GUICy the developer writes them in code, so they are cheap and exact.
4. **Focus wrapping**: decide it once: `yes` (wrap only when nothing lies that way).

Edge cases:
- Closing the last window leaves an empty workspace, which i3 removes when you leave it
  **(memory)**. GUICy has one window and no workspace to leave; see 1.10.
- A container with one child keeps its own layout, so a tabbed container with one tab still shows
  a tab bar **(memory)**. ImGui's `AutoHideTabBar` is the fix; GUICy should hide a one-tab bar
  and show the plain title.
- Very many windows: nesting is unbounded, and deep nests are hard to read. GUICy's fixed
  pools (64 nodes, 32 tiles) are the right cap; the error goes in the window, as now.

### 1.2 bspwm: binary space partition done well

What it is:
- Every node is a leaf or a split in two. New windows split the focused leaf by
  `automatic_scheme` = `longest_side` (GUICy's current rule), `alternate` or `spiral`.
  `initial_polarity` says which side the new one goes.
- **Preselection** (`node -p DIR`, `node -o RATIO`): mark the focused leaf so the *next* window
  goes on that side with that ratio, with a coloured feedback area. `~DIR` cancels.
- **Receptacles:** empty leaves; a new window inserted on one replaces it. They form a template
  that windows are sent into later.
- **Node flags:** `locked` (ignores close), `sticky` (stays on the focused desktop), `private`
  (tries to keep its position and size when others come and go), `marked`.
- **States:** `tiled`, `pseudo_tiled` (shrinks but never grows past its floating size),
  `floating`, `fullscreen`; `node -t ~` returns to the previous state.
- **Tree operations:** rotate 90/180/270, flip, equalize (reset ratios), balance (equal areas),
  circulate.
- `removal_adjustment`: when a node goes, its sibling is re-split by the automatic scheme.

Take for GUICy:
1. **Preselection** (rank 11): one `presel_dir` and `presel_ratio` on a leaf, a tinted rectangle
   drawn over it, used by the next `gy_tile_open` / `gy__tile_insert`. Alt+Ctrl+arrow sets it.
2. **Balance and equalize** as one key each (20 lines): the cheapest answer to "my layout got
   lopsided".
3. **bspwm's flags map straight onto the planned locks:** `locked` = `GY_LOCK_CLOSE`,
   `private` = `GY_LOCK_SIZE` + `GY_LOCK_MOVE`, `sticky` = pinned to every workspace (T3).
4. **`pseudo_tiled`** is a good "fixed-size tool tile": centred in its slot at its own size, never
   stretched. It suits a colour picker or a small preview. 30 lines in the layout pass.

Edge cases: bspwm honours ICCCM size hints only when asked (`honor_size_hints`). A tiler that
honours minimums must decide what gives way. GUICy's existing rule (split → tabs below the
minimum) is the right one; keep it in every layout of T2.

### 1.3 Hyprland: dwindle, master, groups, pins

What it is:
- **Dwindle** is bspwm-like, but splits are not permanent by default: each split's direction
  comes from the parent's width/height ratio, so resizing the window can flip it.
  `preserve_split` makes it permanent (needed for `togglesplit`). `force_split` 0/1/2 (follow
  the mouse / always left-top / always right-bottom). `split_width_multiplier` biases wide
  screens. `smart_split` picks the side from where the cursor is.
- **Master:** one or more masters take `mfact` (default 0.55) of the screen, the rest stack.
  Since 0.52 `mfact` takes a relative delta (+0.05) as well as an absolute value.
- **Groups:** a tabbed group of windows (`togglegroup`, `moveintogroup DIR`,
  `moveoutofgroup`). **`lockactivegroup`** stops a group accepting new windows or being moved
  into another group; **`lockgroups`** does it for all groups, regardless of each group's own lock.
- **`pin`**: a floating window shown on every workspace.

Take for GUICy:
1. **Two levels of lock** (rank 4, 5): a per-tile/per-stack lock and one global lock that
   overrides them all. Hyprland shows the global lock must *add to* the local locks, never clear
   them when turned off.
2. **"Move into neighbour's tabs" as a key** (Alt+Shift+Ctrl+dir) so stacking doesn't need the
   mouse.
3. **Keep GUICy's splits permanent** (`preserve_split` on). Splits that flip with the window size
   make layouts jump during a live resize, which is worse inside an app than on a desktop.
4. **Relative `mfact` steps** for Alt+, and Alt+. in T2's master layout.

### 1.4 dwm and xmonad: layouts as functions

What it is:
- dwm: master area plus stack; Alt+i / Alt+d change the number of masters, Alt+Return swaps the
  focused window with the master; layouts tiled `[]=`, monocle `[M]`, floating `><>`, picked
  directly by key; 9 **tags** (a window can carry several tags, so it can appear in several views).
- xmonad: a layout is a value; `Tall ||| Mirror Tall ||| Full` combines them and `mod-space`
  cycles; `JumpToLayout` picks one by name (xmonad-contrib `LayoutCombinators`).

Take for GUICy:
1. **Each automatic layout is a pure function** `gy__lay_master(n, area, p, out_rects)`
   (rank 8). No tree changes, so cycling and going back costs nothing, and the hand-made tree under
   it survives. Locked tiles are placed first; the function gets what remains.
2. **A user-chosen cycle list** (Amethyst lets layouts be enabled/disabled for the cycle). Saves
   Alt+Space from stepping through 8 layouts the app doesn't want: `gy_tile_layouts(mask)`.
3. **Tags instead of workspaces** are more flexible but harder to explain. Keep workspaces for T3,
   with "pin to every workspace" covering the common tag use.

### 1.5 river

River (2026) is now a compositor only. Window management is a separate program speaking the
`river-window-management-v1` protocol; the older layout-generator design (rivertile) lives on as
river-classic. Lesson for GUICy: the split "the engine owns the tree, a layout is a replaceable
function" is the same as 1.4. A **layout callback** (`gy_tile_layout_fn`) lets an app add its own
layout with no GUICy change, about 30 lines. Low priority.

### 1.6 Amethyst (macOS)

Layouts: Tall, Tall-Right, Wide, Two Pane, 3Column-Left/Middle/Right (centred master),
Widescreen-Tall (main pane split into columns), Fullscreen, Column, Row, Floating, BSP, and
custom JavaScript layouts. `mod1+space` cycles, layouts can be left out of the cycle,
`mod1+,`/`mod1+.` change the main pane count, `mod1+h/l` resize it.
Take: the list itself is the T2 list; **3Column-Middle** (centred master) is the best layout for
very wide windows (the user's voxup window is 2560 wide); **Two Pane** (main plus one visible
secondary, the rest behind it) is a good "small window" layout before falling back to tabs.

### 1.7 komorebi and GlazeWM (Windows)

- **komorebi** layouts: BSP, Columns, Rows, VerticalStack, HorizontalStack,
  UltrawideVerticalStack (three columns), Grid, RightMainVerticalStack, Scrolling, plus custom
  layouts from JSON. **`layout_rules`** switch the layout by container count, e.g.
  `{"1": "BSP", "4": "UltrawideVerticalStack", "7": "Grid"}`. Caveat in its docs: while rules are
  set, the user can't change that workspace's layout by hand until the rules are cleared.
- **GlazeWM**: `alt+v` toggles where the next window goes (right or below), the same switch
  decides where a moved window lands; window rules by process/class/title run on open
  (`ignore`, `set-floating`); `keep_alive` workspaces; **binding modes** (a resize mode with
  arrows, Esc or Enter leaves); `alt+shift+p` pauses all other bindings. It has no lock feature
  and no per-app key exclusion ("This isn't currently supported").

Take for GUICy:
1. **Count rules** (rank 9), with GUICy's fix for komorebi's caveat: a manual Alt+Space turns the
   rules off for that container, and "reset to default" turns them on again.
2. **A resize mode** is already in T6 (Alt+R). GlazeWM shows Enter *and* Esc should both leave it.
3. **"Pause tile keys"** is GlazeWM's crude version of `gy_keys_owned`. GUICy's per-key clash
   check is better; keep it.

### 1.8 PaperWM and niri: scrollable tiling

What it is:
- Windows sit on one horizontal strip per workspace, wider than the screen. New windows open to
  the right of the active one. Focusing scrolls the strip. Windows can be absorbed into or
  expelled from vertical columns (niri `Mod+[`/`Mod+]`).
- niri: `preset-column-widths` (default 1/3, 1/2, 2/3) cycled by `Mod+R`;
  `center-focused-column` = `never` / `always` / `on-overflow` (centre only when the column
  won't fit beside the previous one); `always-center-single-column`; tabbed columns since 25.02.
- PaperWM: a **minimap** while the modifier is held, and a **scratch layer** of floating windows
  toggled by one key.

Take for GUICy:
1. **The strip as a container layout** (rank 20). It is the only layout where adding the 20th
   tile never shrinks the other 19. Columns of preset widths, a horizontal offset per container,
   Alt+Left/Right scrolls. It needs clipping per column (already there) and a scrollbar-like hint.
2. **The minimap while Alt is held** is a cheap version of T6's overview: small rectangles with
   names, no pictures. 60 lines, and it shows where tiles went after a drag.
3. **Preset widths cycling** (1/3, 1/2, 2/3) for grow/shrink on a key is friendlier than fixed
   steps: fewer key presses to a useful size.

### 1.9 Summary of tiler edge cases, and GUICy's answer

| Case | What the WMs do | GUICy rule (proposed) |
|---|---|---|
| A new tile won't fit at its minimum | i3 squeezes; bspwm ignores hints unless asked; Hyprland squeezes | Becomes a tab in the focused stack (step 4 already). In automatic layouts: Two Pane, then monocle/tabs. Never below the minimum. |
| Very many tiles | Nesting (i3), strip (niri), monocle (dwm), count rules (komorebi) | Count rules + the tabs fallback; the strip layout under `GUICY_TILING_FULL`; 32 tiles max with the error shown. |
| Closing the last tile | Empty workspace (i3, dwm) | Never an empty window: the last tile can't close unless the app allows it; if it does, the window shows the closed-tiles strip (T5) with one click per tile to bring it back. |
| Closing a tile next to locked ones | Sibling takes the space (bspwm, dwindle) | The unlocked sibling grows; a `GY_LOCK_SIZE` neighbour never does. If only locked neighbours are left, the space goes to the nearest unlocked tile further up the tree, else the locked tile is allowed to grow (the window must stay filled). |
| Window shrinks below the sum of minimums | Overlap or clipping | Unlocked tiles fold into tabs first, then locked sizes give way proportionally (Fleury strictness, 3.4); the window's own `min_w/min_h` should be set from the tiles' minimums. |
| A saved line names a tile the app no longer has | i3 placeholder waits | Dropped (step 4 already). A tile the line *lacks* opens by its rule. |
| A saved line is broken | i3: error | Start from the app's default and say so in the window (step 4 already). |

---

## 2. In-app docking and panels

### 2.1 Dear ImGui docking branch

- `DockSpace()` / `DockSpaceOverViewport()`; a hidden dockspace needs `KeepAliveOnly` or its
  windows undock. The DockBuilder API is internal and its author calls it weak; the docking code
  is heading for a third rewrite.
- Node flags (docking branch `imgui.h`, read today): `KeepAliveOnly`,
  `NoDockingOverCentralNode`, `PassthruCentralNode`, `NoDockingSplit` ("Disable other
  windows/nodes from splitting this node"), `NoResize` ("Useful with programmatically setup
  dockspaces"), `AutoHideTabBar`, `NoUndocking`. More (`NoTabBar`, `NoWindowMenuButton`,
  `NoCloseButton`) are internal **(memory for the internal list; `NoTabBar` is confirmed in
  issue 6780)**.
- Flags can be applied per window through `ImGuiWindowClass` + `SetNextWindowClass()`.
- `io.ConfigDockingWithShift`: docking only while Shift is held.
- **Locking (issue 6780, 2023):** a user wanted one checkbox to lock sizes *and* undocking. There
  was no single flag; the suggestions were `NoResize`, `NoTabBar` (breaks with several windows),
  the window's `NoMove` flag, or Shift-to-dock. `NoUndocking` was added two weeks later.
- Layouts persist in `imgui.ini` **(memory: the wiki's section on it is a TODO)**; the usual
  complaint is that a stale .ini overrides a layout the code changed, and the fix people use is
  a version key or deleting the file **(memory)**.

Lessons: (a) users want one switch, not a combination of five flags; (b) the lock has to cover
undock, split-into, resize and tab moves together; (c) the saved file must not beat the code.
All three are in the top 20 (3, 4, 5).

### 2.2 VS Code

- Primary and secondary side bars, a panel that can go top/bottom/left/right with **four
  alignments**: Center (spans the editor), Justify (full width), Left, Right. A maximise chevron.
- Views drag between side bars and the panel, or onto another view to form a group;
  "View: Move View" does it by keyboard.
- **"Reset View Locations"** for all, and "Reset Location" for one view.
- Editor groups in a grid with presets (Two Columns, Three Columns, Grid 2x2).
- **Lock Editor Group**: "New editors will not open in a locked group unless explicitly moved
  there"; it persists; terminals auto-lock by default (`workbench.editor.autoLockGroups`).
- There is no lock for the whole workbench layout.

Take: the **panel alignment** idea is the cheapest "other type of panel alignment": a docked
panel either spans the full window edge or only the centre region between the side bars. In GUICy
terms it is the order in which `gy_dock()` edges are cut (top-first versus sides-first), so it
costs about 10 lines (P1). And the group lock defines `GY_LOCK_SPLIT` (rank 4).

### 2.3 JetBrains tool windows

Five view modes per tool window: **Dock Pinned** (always visible, the default), **Dock Unpinned**
(visible only while active, hides on focus loss), **Undock** (covers part of the editor while
active), **Float** (over the main window), **Window** (a separate OS window). Layouts are saved by
name under Window | Layouts; Shift+F12 restores the default. Old releases had "Store Current Layout
as Default"; a floating window that won't dock back is fixed by Restore Default Layout.

Take: **"Dock Unpinned" = an auto-hide tile**: it lives in an edge strip as a tab label and slides
over the layout while focused (T4/T6, 120 lines, shares code with the scratchpad). And
**named layouts plus Shift+F12-style reset** belongs in T6 as planned.

### 2.4 Blender areas

Older manuals: drag inward from an area corner to split, outward to join (the closing area gets a
dark overlay; Esc or right-click cancels; in 2.93 joined areas had to match in size); Ctrl-drag
from a corner swaps contents; right-click on a border gives Area Options (split, join, swap). The
5.3 manual calls it "Docking": drag a corner into another area to join, drag left/right or
up/down to split, drag into the middle to replace; Ctrl+Alt+Space is Focus Mode.

Take: Blender's corner gestures are powerful but invisible: users ask how to undo a join. GUICy's
title-bar drag with visible drop zones teaches itself. Take only **right-click on a divider →
small menu (split here, join, swap, equalize)**, about 60 lines on top of the existing popup.

### 2.5 Visual Studio

Dragging a tool window's title or a document tab shows the **docking guide diamond** (Top, Bottom,
Left, Right, Center) plus edge guides of the whole frame; away from any guide it floats. Document
windows get a different diamond (they only dock inside the editor frame). Up to 10 saved layouts:
Window > Save Window Layout, Apply with Ctrl+Alt+number, Manage to rename or delete.

Take: (a) the diamond plus **outer-edge guides** (rank 10); (b) **two kinds of tile**: "document"
tiles that only move within a centre region and "tool" tiles that may go anywhere. In GUICy this
is `.region` in `gy_tile_rule`, which is also a lock (a tile allowed in one region only); (c)
numbered layouts on Ctrl+Alt+number are an existing convention; GUICy's tile keys use Alt only, so
Alt+Shift+F1..F9 or the command palette would be the GUICy way.

### 2.6 Qt QDockWidget and QMainWindow

- Features: `DockWidgetClosable`, `DockWidgetMovable`, `DockWidgetFloatable`,
  `DockWidgetVerticalTitleBar`, `NoDockWidgetFeatures` (all off). `allowedAreas` limits where a
  dock may go. A title bar can't be removed, but an empty `QWidget` as the title bar hides it.
- `QMainWindow` dock options: `AnimatedDocks`, `AllowNestedDocks`, `AllowTabbedDocks`,
  `ForceTabbedDocks`, `VerticalTabs`, `GroupedDragging`.
- `saveState(version)` / `restoreState(state, version)`: a version mismatch returns false and
  leaves the layout alone; every dock needs a unique `objectName`; Qt warns not to restore data
  from untrusted sources because fields aren't fully bounds-checked.
- `setCorner()` decides which side owns each corner (the VS Code alignment question again).

Take: (a) **a version number in the layout line** (`gy2:v3:...`): the app bumps it when its tiles
change meaning, and an older line is converted or ignored, never half-applied; (b) GUICy's loader
must bounds-check everything (it parses text from an ini the user can edit): node counts, ratios
in 0.05..0.95, names that exist; (c) **`allowedAreas` = the `.region` rule** above; (d) Qt's
`NoDockWidgetFeatures` is the "kiosk" lock; the planned `GY_LOCK_ALL` matches it.

### 2.7 AvalonDock (WPF)

v5 exposes `CanClose`, `CanFloat`, `CanPin` per dockable, auto-hide through a manager, and XML
or (new in v5) JSON layout serializers. Older versions had `CanHide`, `CanAutoHide`, `CanMove`,
`CanDockAsTabbedDocument`, and a serializer callback keyed by `ContentId` that lets the app
refuse or replace each saved item on load **(memory for the pre-v5 names)**.

Take: the **load callback** is how a docking library makes "the code wins over the file"
possible: for each tile in the loaded line, GUICy asks the lock table before placing it. That is
rank 3, done internally so the app writes nothing.

### 2.8 What users complain about (all docking systems)

1. **Accidental drags wreck the layout** (Spyder issue 2096, FreeCAD discussion 31969, Adobe
   Premiere and Photoshop threads, pgAdmin RM 3155, ImGui 6780). Fixes they ask for: one lock
   toggle, a padlock icon, a key, and the lock kept between sessions. → rank 5.
2. **No way back**: a floating window that won't re-dock, a join that can't be undone. → "reset to
   default" always one key; **an undo ring for layout changes** (keep the last 16 layout lines,
   Alt+Z is taken by zoom, so Alt+Backspace) costs about 40 lines because the layout already
   serialises to one line.
3. **The saved file beats the new code** after an update. → layout line version (2.6) and code
   locks re-applied after load (rank 3).
4. **Drop targets are tiny or ambiguous.** → big zones with a preview of the result (the tinted
   rectangle shows the exact rect the tile will get), and a drag threshold of a few pixels so a
   click on a title never starts a drag.

### 2.9 Lock design for GUICy (L1), consolidated

Flags (per tile, and per stack where it makes sense):

| Flag | Blocks | Source of the idea |
|---|---|---|
| `GY_LOCK_MOVE` | drag, swap keys, move into tabs, sending to a workspace | Qt Movable, ImGui NoMove |
| `GY_LOCK_SIZE` | its dividers by mouse and keys, grow/shrink, balance/equalize; automatic layouts keep its size | ImGui NoResize, bspwm private |
| `GY_LOCK_CLOSE` | close by key, title button, middle-click on a tab | bspwm locked, Qt Closable |
| `GY_LOCK_FLOAT` | float, scratchpad, pop-out | Qt Floatable, AvalonDock CanFloat |
| `GY_LOCK_TABS` | tabs dragged in or out of this stack | ImGui NoUndocking |
| `GY_LOCK_SPLIT` (new) | other tiles splitting this one or landing in it (drop zones hidden over it) | ImGui NoDockingSplit, VS Code group lock, Hyprland lockactivegroup |
| `GY_LOCK_ALL` | all of the above | Qt NoDockWidgetFeatures |
| `gy_layout_lock(1)` | every tile, regardless of its own flags; turning it off restores each tile's own flags | Hyprland lockgroups |

Rules that make them hold against all three inputs:
1. **One gate function**, `gy__tile_may(tile, action)`, called by the mouse code, the key code,
   the automatic layouts *and* the loader. Every refusal is a no-op that changes nothing (the
   layout line stays byte-equal, which is the L1 proof).
2. **Keys on a locked tile are eaten, not passed on.** The tile key was meant for GUICy; letting
   Alt+Left fall through to the app when the tile is locked would make app behaviour depend on
   lock state. (The plan already says "never reach the app".)
3. **Loaded lines:** developer locks are never read from the line. After `gy_tiles_load`, each
   `MOVE`-locked tile is checked against its rule place; if it is elsewhere, it is moved back and
   the loader logs one line. `SIZE`-locked tiles get their code size back. Only locks the *user*
   set (when allowed) are saved and loaded, with a `u` marker per tile.
4. **Old lines** (`gy:` from step 4) have no locks at all, so rule 3 applies the code's locks on
   top of them. Nothing in an old line can unlock anything.
5. **What the user sees:** a padlock on locked titles; no resize cursor on locked dividers; drop
   zones not drawn over `SPLIT`-locked tiles; a refused drag shows the tile snapping back (one
   frame, no animation unless animations are on) and a short status text "Locked: Preview".

Open question 1 in PLAN-tiling.md (can the user unlock what the developer locked?): every system
read today separates the two. ImGui and Qt give developer flags the user can't touch; Premiere,
FreeCAD and Spyder users ask for a user lock. The plan's answer (only with `gy_allow_user_lock`)
matches both. Suggest one refinement: the user's global "lock layout" toggle is always allowed
(it only adds locks), while removing a developer lock needs `gy_allow_user_lock`.

---

## 3. Panel alignment and layout inside a container (P1)

### 3.1 Options compared

| System | Model | Size | Fits GUICy? |
|---|---|---|---|
| CSS flexbox | main/cross axis, grow/shrink/basis, wrap, justify, align; the "resolve flexible lengths" step loops, freezing items that hit min/max | browsers; Yoga (C++ with a C API) implements "a subset of CSS, mostly focused on Flexbox" | The full algorithm with intrinsic sizes is far more than an IMGUI needs; the freeze loop is the useful part **(memory for the algorithm details; the spec's section 9 is the source)** |
| CSS grid | tracks with px / % / `fr` / `auto` / `minmax()`, areas by name | browsers | Named areas are nice but tiles already do 2D placement; `gy_grid` already covers equal cells |
| Clay | per-axis FIT(min,max) / GROW(min,max) / FIXED / PERCENT, direction, childAlignment, childGap, padding, floating elements with z-index, clip/scroll, cached text measure | single header ~4.8k lines, no stdlib, static arena (~3.5 MB for 8192 elements), "microsecond layout" claimed without numbers | The sizing set is the right one; the whole library is too big to copy and uses scope macros (a PLAN.md anti-goal) |
| Morphorm | row / column / grid / overlay; sizes in pixels, percent, **stretch** (share of the remaining space), auto; min/max in px, % or auto; gap can be stretch (spreads items) or negative (overlap); absolute children placed by left/right/top/bottom | small Rust crate, **one pass** | The best concept-to-code ratio: stretch units replace grow/shrink/justify in one idea |
| Fleury (RAD Debugger) | per-axis semantic size: Pixels, TextContent, PercentOfParent, ChildrenSum, each with a **strictness** 0..1; passes: standalone, upward, downward, solve violations (shrink by overflow x (1 - strictness)), positions; input uses last frame's rects | a few hundred lines in his series | GUICy's deferred box tree already follows it (PLAN 3.3); strictness is the missing piece for "what gives way" |
| Constraint solvers (Cassowary, Auto Layout) | linear equalities with priorities | thousands of lines, slow to debug | PLAN.md lists a constraint solver as a non-goal; keep it so |

### 3.2 What to build for P1 (about 450 lines)

1. **`gy_dock(side, size)`** cuts a strip off the current rect (toolbars, status bars, side
   strips). The order of calls decides the corners, which is the VS Code panel alignment and
   Qt's `setCorner` for free. ~60 lines.
2. **Sizes = Morphorm's four units on every row item**: `GY_PX(n)`, `GY_PCT(p)`,
   `GY_STRETCH(f)`, `GY_AUTO`, each with optional min/max. Left/centre/right alignment then needs
   no special case: `[STRETCH(1), items..., STRETCH(1)]` centres; `[items..., STRETCH(1),
   items...]` is left + right. ~150 lines.
3. **Strictness for overflow** (Fleury): when fixed items overflow, each shrinks by
   overflow x (1 - strictness); text items default to strictness 1 (never cut), spacers 0. One
   float per item, ~40 lines. This also answers "window smaller than the minimums" in 1.9.
4. **Wrap rows** (flexbox `flex-wrap`): break when the next item's min width doesn't fit, then
   apply stretch per line. ~60 lines.
5. **Overflow » menu** (rank 15): items carry a priority; when strictness can't save them, the
   lowest-priority ones go into a popup opened by a » button at the end. ~120 lines.
6. **Sticky section headers**: inside `gy_scroll_begin`, a `gy_section` that scrolls above the
   top is drawn pinned at the top until the next header pushes it out. Needs the header list from
   last frame's rects; ~40 lines.

Not worth it now: CSS grid named areas, intrinsic min-content sizing passes, baseline alignment.

---

## 4. The GPU for 2D UI, beyond research/gpu.md

gpu.md already covers the instanced SDF quad, one draw a frame, offline shaders, flip model,
the frame-hash skip and WARP. This section adds what tile moves, animation and testing need,
plus two corrections to the plan.

### 4.1 Correction 1: flip model and GDI on the same window

Microsoft (DXGI_SWAP_EFFECT): "After the first successful call to Present1 on a flip-model swap
chain, GDI no longer works with the HWND that is associated with that swap chain, even after the
destruction of the swap chain. This restriction even extends to methods like ScrollWindowEx."

PLAN 3.7 step 4 switches to the CPU path in the same frame when the device is lost, and the CPU
path presents with `BitBlt` to the window. On a flip-model window that BitBlt shows nothing.
PLAN 3.6's "No `WS_EX_NOREDIRECTIONBITMAP`" does not fix this.

Fix (rank 1): create the swap chain on a **child window** that covers the client area
(`WS_CHILD | WS_VISIBLE`, its window procedure returns `HTTRANSPARENT` from `WM_NCHITTEST` so
all input reaches the parent **(memory: HTTRANSPARENT passes hit testing to the window
underneath in the same thread)**). On fallback, destroy the child: the parent was never presented
to with flip model, so GDI still works there. Resize the child in `WM_SIZE`. About 60 lines, and
it also removes PLAN risk 7's "recreate the window on fallback".

### 4.2 Correction 2: FLIP_DISCARD can't do partial presents

The same page: `FLIP_DISCARD` "cannot be used with multisampling and partial presentation".
`FLIP_SEQUENTIAL` keeps each back buffer's contents across presents (supported for D3D11 since
Windows 8) and allows dirty rects and a scroll rect in `Present1`.

With 2 buffers, the buffer being drawn holds frame N-2. Microsoft's dirty-rect page says the app
must make every pixel inside the dirty rects current, and copy or redraw the overlap with the
previous frame's dirty rects. GUICy can do this with no copy:

- Keep last frame's dirty-tile bitmask (GUICy already compares tile hashes frame to frame).
- This frame, redraw (with a scissor per run of tiles, or a clip index per tile) the tiles dirty
  **now or last frame**, and pass only the tiles dirty **now** to `Present1` as dirty rects.
- A frame with no dirty tiles is still skipped entirely (the frame-hash skip in gpu.md).

DirectFlip, the reason to prefer `FLIP_DISCARD`, matters for a full-screen game, not a windowed
tool that DWM composes anyway. Cost: about 80 lines in G1, so PLAN step 13's "Present1 later"
moves into G1. The proof: Present1 dirty-rect area per frame in BENCH.md for the hover, caret and
progress cases, beside the CPU path's dirty-tile counts.

The scroll rect (`pScrollRect`, `pScrollOffset`) lets the OS move the old pixels when a list
scrolls, so only the new line is drawn. GUICy's dirty tiles currently treat a scroll as "every
tile changed". A later step could pass the scroll rect on the GPU path and do a `ScrollDC`-style
shift on the CPU path **(memory: ScrollDC / BitBlt of the DIB onto itself)**. ~100 lines; later.

### 4.3 Fast tile moves and animations

- **The UI pass, not the GPU, is the cost.** A 1540x950 frame of 5,000 glyphs and 500 rects is a
  single instanced draw; on any discrete GPU this is far under a millisecond **(memory; measure in
  G1 with 4.4)**. The CPU side (UI pass, hash, `Map(WRITE_DISCARD)` of ~250 KB) is what the
  PLAN-tiling budget of 1 ms limits.
- **A per-tile offset table** next to the clip table: each instance already carries a clip index;
  give each clip entry a translation too. During a slide animation, the instances of a tile don't
  change, only its offset. Combined with the frame hash per tile, an animation frame re-hashes
  nothing but the moved tiles' offsets, and on the GPU it uploads one small constant buffer instead
  of all instances. ~50 lines; only built with animations (`GUICY_NO_ANIM` off).
- **Animations off by default, and finite.** A slide of 120 ms at 60 Hz is 8 frames; then
  `gy_animate` stops asking and the loop blocks again. The T6 proof (0 dirty tiles after the end)
  is the right one. Respect the OS setting: `SystemParametersInfo(SPI_GETCLIENTAREAANIMATION)`
  returns FALSE when the user turned animations off in Windows **(memory)**; when FALSE, GUICy
  doesn't animate even if the app turned animations on.
- **Live resize**: on the GPU path, `ResizeBuffers` in `WM_SIZE` and draw in the same message;
  PLAN risk 7 still applies. The child-window fix (4.1) doesn't change this.

### 4.4 Measuring GPU frame time

- Per frame: `Begin(disjoint)`, `End(timestamp_start)`, the draw, `End(timestamp_end)`,
  `End(disjoint)`. Ticks to seconds with `D3D11_QUERY_DATA_TIMESTAMP_DISJOINT.Frequency`; throw the
  sample away when `Disjoint` is TRUE (the clock changed, e.g. power state). The disjoint query
  "should only be invoked once per frame or less".
- Read with `GetData(..., D3D11_ASYNC_GETDATA_DONOTFLUSH)` 2-3 frames later from a small ring of
  query sets, never by spinning **(memory for the flag and the ring pattern)**.
- **Idle rule:** GUICy only renders on change, so "2-3 frames later" may never come. Never wake
  the loop to collect a timestamp. Read them only when a frame runs anyway, and in `gybench` run
  a fixed number of forced frames and then read with a blocking loop (bench only).
- Also worth logging: CPU time from `gy_frame` entry to `Present` return, and the Present count,
  which proves idle on the GPU path.

### 4.5 DirectComposition: not now

DirectComposition (Windows 8+, `dcomp.dll`) composes visuals with transforms, clips, opacity and
animations "independently of the UI thread", so a slide could run with zero app frames.
Microsoft now recommends `Windows.UI.Composition` for Windows 10 apps. It needs a composition
swap chain (`CreateSwapChainForComposition`) and normally `WS_EX_NOREDIRECTIONBITMAP`
**(memory)**, and it is COM-heavy. It conflicts with the GDI fallback and saves only the 8
frames of a slide. Leave it out; the offset table (4.3) gets most of the benefit in 50 lines.

### 4.6 Testing GPU output with no window

1. **Offscreen device test** (rank 17): create the device with no swap chain; render into a
   `D3D11_USAGE_DEFAULT` BGRA texture bound as a render target; `CopyResource` into a
   `D3D11_USAGE_STAGING` texture with CPU read access; `Map(READ)`; compare with the CPU golden
   (1 level on AA pixels, 0 on solid ones, as G1 says). This tests shaders and the instance upload
   with no window and nothing on screen.
2. **WARP** (`D3D_DRIVER_TYPE_WARP`): supports feature levels up to 11_1 on Windows 8+, uses all
   cores with SSE/AVX JIT, and passes the WHQL conformance tests. Microsoft says most images
   match hardware almost exactly, with small differences "within the tolerances defined by the
   Direct3D 10 specification". So WARP goldens should use the same 1-level tolerance, and the
   WARP and hardware pictures should be compared with each other too. WARP is the CI path on a
   machine with no GPU.
3. **Swap-chain test on the hidden desktop** (as BENCH.md already does with `CreateDesktop`):
   proves the child window, `Present1` and the fallback switch. A forced device-removed run:
   `ID3D11Device5`'s `RemoveDevice` is not in D3D11 **(memory)**; simplest is a test hook that
   makes GUICy act as if `Present1` returned `DXGI_ERROR_DEVICE_REMOVED`.
4. **RDP / Basic Render Driver**: PLAN 3.7 already treats these as "no GPU". WARP shows up in
   adapter lists as "Microsoft Basic Render Driver" on Windows 8+, which is the name to check.

### 4.7 Should the GPU be the default? (PLAN-tiling open question 2)

Arguments from today's reading: both paths idle at zero; with 4.2 the GPU path's per-frame
present cost also follows the change; DWM composes either way. The GPU path wins on full redraws
(resize, scroll, tile drags, animation) where the CPU path is 4 ms versus well under 1 ms. The CPU
path wins on startup (no device creation; D3D11 device creation is typically 10-50 ms on first use
**(memory)**) and under RDP. Suggest: GPU by default when a hardware adapter is found and the
session isn't remote, decided with G1's numbers as planned, and the device created on a worker
thread while the first frame draws on the CPU, so startup doesn't pay for it (~40 lines; the
renderer swap code from the fallback does the switch).

---

## 5. New features that fit "fits into any role, every type of window"

Ranked by value for the three apps and general use, against cost.

| # | Feature | Value | Cost (lines) | Notes |
|---|---|---|---|---|
| 1 | **Per-monitor DPI v2** | Any screen over 100% looks wrong today | ~200 | Opt in with `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` (or the app's manifest). On `WM_DPICHANGED`, use the suggested rect (Microsoft: avoids a DPI-change loop and keeps the cursor in place), scale the theme's metrics, recreate the GDI fonts, flush the glyph atlas and the width cache, mark everything dirty. Use `GetSystemMetricsForDpi`, `AdjustWindowRectExForDpi`. The non-client area scales by itself in PMv2. Tile minimums (160x110) and title height (24) must scale too. |
| 2 | **IME** | voxup/pixelhunter file names; any CJK user | ~150 | PLAN.md lists imm32 but guicy.h has no IME code yet. `WM_IME_STARTCOMPOSITION` / `WM_IME_COMPOSITION` (`GCS_COMPSTR`, `GCS_RESULTSTR`), `ImmSetCompositionWindow` at the caret. Draw the composition string underlined in the edit box. **(memory for the message names)** |
| 3 | **Command palette** | Every action findable, one key | ~250 | Ctrl+Shift+P by default (an app key check through `gy_keys_owned`). Lists tile actions with their keys, every `gy_button_key` seen in the last frame (GUICy knows them), and app commands from `gy_command("Build", 'B')`. Fuzzy subsequence match, most recent first. |
| 4 | **Layout undo** | The #1 docking complaint | ~40 | A ring of the last 16 layout lines; Alt+Backspace steps back. Free, since the layout is already one line. |
| 5 | **Auto-hide tiles** (JetBrains Dock Unpinned) | Small windows, laptop screens | ~120 | A tile folded to a label on an edge strip; slides over the layout while focused. Shares the scratchpad's overlay code. |
| 6 | **Themes beyond the colour table**: light/dark following Windows, high contrast | "fits any role"; accessibility | ~80 | Read `AppsUseLightTheme` from the registry and listen for `WM_SETTINGCHANGE`; `SystemParametersInfo(SPI_GETHIGHCONTRAST)` switches to the system colours **(memory)**. Theme changes already invalidate everything. Keep "no red" in every built-in theme. |
| 7 | **Multi-window pop-out** (JetBrains "Window" mode, ImGui multi-viewports) | Second monitor | ~800 | Needs a context per OS window or one context with several roots, input routing, a swap chain or DIB per window, DPI per window. GUICy has `gy_ctx_set()`, so a tile could be rendered into a second `gy_open` window; still the biggest item here. After the ports. |
| 8 | **UI Automation** (screen readers, test tools) | Accessibility; scripted tests from outside | 1,500-2,500 | GUICy draws its own controls, so UIA sees one opaque HWND. A server-side provider answers `WM_GETOBJECT` with `UiaReturnRawElementProvider`, and implements `IRawElementProviderSimple` + `IRawElementProviderFragment` (+ `FragmentRoot` on the window) with patterns per widget (Invoke, Toggle, Value, RangeValue, Selection, ExpandCollapse, Scroll), plus events on focus and value changes **(memory for the interface list; the provider overview is the source for the model)**. In C that means hand-written COM vtables. An IMGUI has no stable objects, so providers must be keyed by GUICy's widget IDs and survive between frames. AccessKit solves this but needs Rust to build, which breaks "nothing outside itself". |
| 9 | **RTL text and bidi** | Arabic/Hebrew users | 1,000+ | DirectWrite (step 9) gives shaping and bidi runs; mirroring layouts (docks, rows, tab order) is a design pass of its own. Leave until someone asks. |

---

## 6. What this changes in PLAN-tiling.md

1. **G1 changes in design:** the swap chain on a child window (4.1); `FLIP_SEQUENTIAL` with
   `Present1` dirty rects from the dirty tiles (4.2) instead of `FLIP_DISCARD` with presents later;
   offscreen texture + staging readback as the golden test, WARP with the same tolerance (4.6);
   timestamps only when frames run anyway (4.4). Budget: about +230 lines over the plan.
2. **L1 before G2 is fine, but its gate function must be designed for containers** (one
   `gy__tile_may` used by mouse, keys, layouts and the loader). Add `GY_LOCK_SPLIT`; state that
   developer locks are re-applied after a load and never read from the line; add a layout version
   number to `gy2:` lines.
3. **A new small step before P1: per-monitor DPI** (5, item 1). Tile minimums, title bars and
   divider widths all depend on it, and every golden would change after it, so earlier is cheaper.
4. **T2's layouts as pure functions** with locked tiles placed first, plus count rules and a
   user-chosen cycle list. The strip (niri) is optional and last.
5. **T1 gains preselection and outer-edge drop zones**; **T5 gains slots** (receptacles); T6
   gains layout undo and the Alt-held minimap as a cheap overview before the picture overview.
6. **P1 uses stretch units + strictness** rather than a flexbox port.
7. **Compile switches:** `GUICY_NO_TILING` (no tiles at all), default = step-4 tiling plus locks,
   `GUICY_TILING_FULL` = containers' extra layouts, workspaces, scratchpad, overview, strip;
   `GUICY_NO_ANIM` removes the animation code. Locks are in the default build, since the user
   asked for them for every app.

---

## Sources

Tiling window managers
- i3 user's guide (containers, layouts, marks, criteria, scratchpad, focus_wrapping): https://i3wm.org/docs/userguide.html
- i3 layout saving (append_layout, placeholders, swallows): https://i3wm.org/docs/layout-saving.html
- bspwm manual (schemes, presel, receptacles, flags, states, tree ops): https://github.com/baskerville/bspwm/blob/master/doc/bspwm.1.asciidoc
- Hyprland dwindle layout: https://wiki.hypr.land/Configuring/Layouts/Dwindle-Layout/ and https://wiki.hypr.land/0.50.0/configuring/dwindle-layout/
- Hyprland master layout: https://wiki.hypr.land/0.52.0/Configuring/Master-Layout/
- Hyprland dispatchers (pin, togglegroup, lockgroups, lockactivegroup, moveintogroup): https://wiki.hypr.land/0.46.0/configuring/dispatchers/ and https://wiki.hypr.land/configuring/core/dispatchers/
- dwm tutorial: https://dwm.suckless.org/tutorial/
- xmonad-contrib LayoutCombinators: https://hackage-content.haskell.org/package/xmonad-contrib-0.18.2/docs/XMonad-Layout-LayoutCombinators.html (link found, page not read; xmonad details are memory)
- river: https://codeberg.org/river/river
- Amethyst: https://github.com/ianyh/Amethyst
- komorebi layouts and layout rules: https://komorebi.lgug2z.com/usage/layouts/ and https://lgug2z.github.io/komorebi/common-workflows/dynamic-layout-switching.html and https://komorebi.lgug2z.com/reference/komorebic-windows/
- GlazeWM: https://github.com/glzr-io/glazewm
- niri getting started and layout config: https://niri-wm.github.io/niri/Getting-Started.html and https://github.com/YaLTeR/niri/wiki/Configuration:-Layout
- PaperWM: https://github.com/paperwm/PaperWM

Docking and panels
- Dear ImGui docking wiki: https://github.com/ocornut/imgui/wiki/Docking
- Dear ImGui docking branch header (ImGuiDockNodeFlags_): https://raw.githubusercontent.com/ocornut/imgui/docking/imgui.h
- Dear ImGui issue 6780, locking docked windows: https://github.com/ocornut/imgui/issues/6780
- VS Code custom layout: https://code.visualstudio.com/docs/configure/custom-layout
- JetBrains tool window view modes: https://www.jetbrains.com/help/idea/viewing-modes.html and arrange tool windows: https://www.jetbrains.com/help/idea/manipulating-the-tool-windows.html
- Blender areas: https://docs.blender.org/manual/en/latest/interface/window_system/areas.html and https://docs.blender.org/manual/en/3.5/interface/window_system/areas.html
- Visual Studio window layouts and docking guides: https://learn.microsoft.com/hu-hu/visualstudio/ide/customizing-window-layouts-in-visual-studio?view=vs-2022 and https://visualstudiomagazine.com/blogs/tool-tracker/2018/08/controlling-default-window-layout.aspx
- Qt QDockWidget: https://doc.qt.io/qt-6/qdockwidget.html
- Qt QMainWindow (dock options, saveState/restoreState): https://doc.qt.io/qt-6/qmainwindow.html
- AvalonDock: https://github.com/Dirkster99/AvalonDock
- Lock-layout requests: https://github.com/spyder-ide/spyder/issues/2096 , https://github.com/FreeCAD/FreeCAD/discussions/31969 , https://community.adobe.com/feature-requests-730/lock-workspace-1651626 , https://community.adobe.com/questions-712/how-to-stop-undocking-windows-1138872 , https://postgresql.org/message-id/CAKKotZRdN%2BHdr-uP%2BdECHk6d7VYC-O5DxNM8tf-8WTTHH42GVw%40mail.gmail.com

Layout
- Clay: https://github.com/nicbarker/clay
- Ryan Fleury, UI Part 2 (semantic sizes, strictness, passes): http://www.dgtlgrove.com/p/ui-part-2-build-it-every-frame-immediate
- Morphorm: https://github.com/vizia/morphorm
- Yoga: https://www.yogalayout.dev/docs/about-yoga
- CSS Flexbox Level 1 (section 9, layout algorithm): https://www.w3.org/TR/css-flexbox-1/

GPU
- DXGI_SWAP_EFFECT (FLIP_DISCARD and partial presentation; GDI after flip-model Present1): https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect
- Flip model, dirty rectangles, scrolled areas: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements
- D3D11_QUERY (timestamps, disjoint): https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query
- D3D11_USAGE (staging readback): https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_usage
- WARP guide: https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp
- DirectComposition: https://learn.microsoft.com/en-us/windows/win32/directcomp/directcomposition-portal

New features
- High DPI desktop apps (PMv2, WM_DPICHANGED, pitfalls): https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
- UI Automation providers overview: https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-providersoverview
- AccessKit C bindings: https://github.com/AccessKit/accesskit-c

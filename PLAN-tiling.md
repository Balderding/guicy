# GUICy: more tiling, locks, the GPU, panel layouts

Draft, 2026-11-22. It follows on from PLAN.md, where steps 0-5 are done. These rules from PLAN.md
and AGENT_RULES.md still apply:
- tile keys only on Alt, and never one the app owns (`gy_keys_owned`)
- 0 CPU while idle
- every action one key or one click away
- hidden runs only
- no red

## What the user asked for

- More tiling window manager features in the GUI tool, and other kinds of panel alignment.
- **"Give the developer the ability to lock panels. Or if it isn't locked it is freely moved
  around."**
- **"Make sure the GPU is being used too or can be used."**

## The design change: containers instead of a binary tree

Today every split divides its space in two. From G2 on, the layout is a tree of **containers**.
Each container holds any number of children and has one arrangement: side by side, stacked,
tabbed, master + stack, grid, spiral, centred master or monocle. This is the i3/sway model. Any
container can change arrangement with one key, and the automatic layouts sit beside the
hand-made ones without throwing them away. Layout lines saved by the step-4 format still load
and are converted once (the new lines start with `gy2:`).

## Steps

Each step is one commit with tests and BENCH numbers, and gets a done-report to Master Control.
No app port happens without the user's OK.

| # | Deliverable | Proof |
|---|---|---|
| **G1** | **The GPU renderer** (PLAN.md step 7, moved to the front). D3D11 is loaded at run time and draws the whole frame as one call over the same 48-byte instances. The shaders are compiled at build time by `gyshader` through the OS's `d3dcompiler_47.dll` and checked in as byte arrays; nothing loads that DLL at run time. It falls back to the CPU renderer when there is no real GPU, under RDP, or when the device is lost mid-run. `GUICY_RENDER=cpu/gpu/warp` forces a path, and `gy_renderer_name()` says which one is in use. Frames that didn't change skip the upload, the draw and the Present. | Every golden scene drawn by the GPU is within 1 level per channel of the CPU picture (0 on solid pixels). Forced cpu, warp and a simulated device-removed run all finish their scripts. Idle stays at one tick. A hidden-desktop timing of the demo is taken on the GPU path. The KB added is recorded. |
| **G2** | **Containers** replace the binary tree, and old layout lines load. | Every step-4 tiling test still passes on the new tree. The old line → new line → load round trip gives the same picture. |
| **T1** | **Mouse:** 5 drop zones (left/right/top/bottom split that side, the middle stacks as a tab), Alt+drag anywhere to move a tile, Alt+right-drag to resize from the nearest edge. | A scripted drag into each zone, each with a golden. Save → load round trip. |
| **L1** | **Locks for the developer.** `gy_tile_lock(name, flags)` and a `.lock` field in tile rules. `GY_LOCK_MOVE` stops dragging and swapping; `GY_LOCK_SIZE` fixes its size (the dividers next to it don't move); `GY_LOCK_CLOSE` stops closing; `GY_LOCK_FLOAT` stops floating; `GY_LOCK_TABS` stops tabs being dragged in or out; `GY_LOCK_ALL`. `gy_layout_lock(on)` locks the whole layout (a kiosk-style fixed screen). An unlocked tile is free: drag it anywhere, split it on any side, stack it, float it. The developer can let the user toggle locks (`gy_allow_user_lock`, a padlock in the title bar plus Alt+L); otherwise the lock is the developer's alone. Locked tiles show a small padlock; locked dividers show no resize cursor. | Scripted moves, swaps, resizes, closes and floats on each lock flag: each is refused and the layout line is unchanged. The same actions work on an unlocked tile. The tile keys on a locked tile do nothing and never reach the app. Locks survive save and load. Goldens of the padlock. |
| **T2** | **Layout types**, Alt+Space cycles: side by side, stacked, tabbed, master + stack (Alt+Enter = make master, Alt+, / Alt+. = master count), grid, spiral, centred master, monocle (Alt+Tab steps). Locked tiles keep their place and size inside every layout. | A golden of every layout with 1-6 tiles. A small window falls back to tabs. A locked tile doesn't move. The tile keys never reach the app. |
| **T3** | **Workspaces:** Alt+1..9 switch; Alt+Shift+number sends the focused tile; a tile can be pinned to every workspace. | Switching back and forth gives identical pictures. All workspaces go in one ini line. Idle stays at one tick. |
| **T4** | **Floating tiles and the scratchpad:** Alt+F2 floats a tile and puts it back where it was; a scratchpad tile (Log, for example) drops over everything with one key and hides again with the same key. | The step-4 popup-trap test again with floating and scratchpad tiles: typing still arrives. |
| **T5** | **Rules and attention:** `gy_tile_rule(name, {.side, .w, .h, .floating, .tab_of, .lock})` for where a tile goes the first time; a gold dot on the title or tab of an unfocused tile that has news; a strip of closed tiles (one click brings one back); gaps, focus following the mouse and a focus border, each with an on/off setting. | Goldens of the rules, the dots and the strip. The app keys reach the app. |
| **T6** | **Keyboard and overview:** resize mode (Alt+R, arrows, Esc); overview (Alt+O, every tile on every workspace as a small picture, click or number to jump; drawn by the GPU when there is one); named layout presets and "reset to the app's default"; short slide animations when tiles move, with an off switch. They run only while something moves. | Frame cost during an animation against the budget, on both renderers. 0 dirty tiles once it ends. |
| **P1** | **Panel layouts inside a tile:** `gy_dock(TOP/BOTTOM/LEFT/RIGHT, size)` for toolbars, status bars and side strips; left/centre/right alignment in a row; flow rows that wrap like flexbox; N-way splits with min/max and percentages; section headers that stay put while their scroll area scrolls; a toolbar that folds extra buttons into a » menu when narrow. | Goldens at three window widths each. Resizing never leaves blank space or overlaps. |

## Budgets

- Each step adds at most about 8 KB of code. The GPU path adds 20 KB or less, plus about 2 KB of
  shader bytes.
- Idle CPU stays at one 15.6 ms tick per 60 s on both renderers.
- A tile drag or layout change costs 1 ms or less of CPU per frame on the GPU path, and 4 ms or less
  on the CPU path, at 1540x950.

## Open questions

1. Should the user be able to unlock what the developer locked? The plan says only when the
   developer allows it (`gy_allow_user_lock`).
2. On the GPU path, should the default in the apps be the GPU? The plan says yes when a real GPU
   is found, because both paths idle at zero. Decide with the G1 numbers.

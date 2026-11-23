# GUICy against other GUI libraries (2026-11-22)

## Method

- One small app per library, all drawing the same scene as `examples/demo.c` at a 1540x950
  client area: a line of text, 3 buttons (Build, Stop, Open folder), 2 checkboxes, 3 selectables
  (Small, Medium, Large) and 20 lines of text. App sources and the build script are in
  `X:\gui-compare\apps\` (`build_all.sh`, `run_all.sh`). The exes are in `X:\gui-compare\build\`.
- Every exe was built with clang 23.1.4, MinGW target (`--target=x86_64-w64-windows-gnu
  --sysroot=X:/tools/gcc/mingw64 -fuse-ld=lld -O2 -static -mwindows`). No library build scripts
  were run.
- Each app was measured by `tools/baseline.py`, the same tool used for the Nuklear apps and the
  GUICy demo. It runs the app on a separate hidden desktop, waits 60 s with no input (idle), then
  posts 600 `WM_MOUSEMOVE` messages at 60 a second. "Per move" is that CPU minus the idle rate.
- There were 3 rounds. In each round every app ran once, one at a time, all in the same session
  (20:55-21:31).
- Each figure is the **median [min-max]** of the 3 runs.
- The GPU apps and raygui write a frame counter to `X:\gui-compare\build\logs\` every 5 s, so the
  real frame rate on the hidden desktop is known. That is one small file write per 5 s.

## Versions

| Library | Version | Commit |
|---|---|---|
| Dear ImGui (+ imgui_impl_win32, imgui_impl_dx11) | v1.91.5 | f401021d5a5d |
| Nuklear (+ demo/d3d11, demo/gdi) | master | bab18497df98 |
| raylib | 5.5 | c1ab645ca298 |
| raygui | 4.0 | 25c8c65a6e5f |
| GUICy demo | `X:\guicy\build\demo.exe` (repo at 0333d67) | see note 5 |

## Results

| App | loop | start ms | idle CPU ms / 60 s | CPU ms per move | peak working set MB | exe KB |
|---|---|---|---|---|---|---|
| **GUICy demo** | sleeps, redraws changed tiles | **63** [53-71] | **0** [0-15.6] | 0.104 [0.021-0.104] | 34.1 [17.2-34.1] | 142 |
| ImGui default loop (as built) | every frame, vsync, skip when occluded | 364 [357-368] | 156 [94-234] | 0.082 [0.049-0.189] | 57.5 [46.1-57.9] | 1,099 |
| ImGui wait-for-input (as built) | input or 250 ms, 2 frames a wake | 363 [345-382] | 78 [63-219] | 0.158 [0.089-0.241] | 57.6 [46.2-57.8] | 1,099 |
| ImGui default loop, drawing (note 1) | every frame, about 64 fps | 365 [342-371] | 1,313 [891-1,547] | 1.161 [0.223-1.298] | 61.1 [60.8-62.1] | 1,099 |
| ImGui wait-for-input, drawing (note 1) | input or 250 ms, 2 frames a wake | 365 [360-370] | 234 [125-266] | 1.056 [0.501-1.117] | 60.3 [60.0-60.5] | 1,099 |
| Nuklear D3D11 (demo loop) | every frame, about 64 fps | 77 [75-83] | 2,188 [1,922-3,016] | 0.272 [0.068-0.477] (note 2) | 56.7 [56.7-56.8] | 503 |
| Nuklear GDI (demo loop) | waits, full redraw per input | 74 [66-74] | 47 [0-63] | **4.245** [3.163-4.382] | 27.2 [27.2-27.3] | 313 |
| Win32 controls (GetMessage) | waits, per-control paint | 72 [68-73] | 0 [0-0] | 0.052 [0.000-0.130] | 27.3 [27.3-27.4] | 47 |
| raylib 5.5 + raygui 4.0 | every frame, SetTargetFPS(60) | 495 [487-895] | **6,000** [5,594-8,547] | 1.448 [1.207-2.987] (note 2) | 81.9 [81.9-82.3] | 1,171 |

Idle CPU in % of one core = ms per 60 s / 600: GUICy 0 %, ImGui drawing 2.2 %, Nuklear D3D11 3.6 %, raygui 10 %.

### Imports and library code compiled

| App | DLL imports (besides KERNEL32 and the Windows UCRT `api-ms-win-crt-*`) | library lines compiled |
|---|---|---|
| GUICy demo | GDI32, USER32 | 3,011 (`guicy.h`) |
| ImGui (all variants) | d3d11, D3DCOMPILER_47, dwmapi, GDI32, SHELL32, USER32 | 52,507 (imgui.cpp, imgui_draw, imgui_widgets, imgui_tables, their headers and stb headers, plus the two backends) |
| Nuklear D3D11 | d3d11, USER32 | 32,750 (nuklear.h + nuklear_d3d11.h + 2 shader headers) |
| Nuklear GDI | GDI32, MSIMG32, USER32 | 32,561 (nuklear.h + nuklear_gdi.h) |
| Win32 controls | GDI32, USER32 | 0 (the controls are in Windows) |
| raylib + raygui | GDI32, SHELL32, USER32, WINMM. opengl32 is loaded at run time by GLFW | 24,938 in raylib's 8 .c units + 5,521 raygui.h. About 295,000 counting every header they pull in (GLFW, miniaudio, stb, and others), including code that `#ifdef`s leave out |

App code (ours, scene and loop) is 52-130 lines per app.

## Notes

1. **On the hidden desktop, ImGui's default loop draws nothing.** DXGI reports the window as
   occluded (`Present` returns `DXGI_STATUS_OCCLUDED`, 0x087A0001). The example then goes into its
   "minimised" path: `Present(0, DXGI_PRESENT_TEST)` and `Sleep(10)`, with no ImGui frame at all.
   No frame log was written in 60+ s. So the two "as built" rows measure a sleeping message loop,
   not ImGui. One retry with the WARP device (`BENCH_WARP=1`, 6 s idle smoke run) did the same:
   no frames, 312 ms idle / 60 s. The "drawing" rows remove only that occlusion skip
   (`-DNO_OCC_SKIP`) and sleep 10 ms after an occluded `Present`, the same as Nuklear's D3D11 demo.
   They drew 5,108 frames in 80 s (63.8 fps), and the wait variant 18.7 fps over a whole run with
   moves. No window was shown to get past this.
2. **Every D3D11 Present was occluded** (Nuklear and ImGui-drawing: 100 % of frames). Present
   never waits for vsync on the hidden desktop, so these loops are paced by `Sleep(10)` at
   about 64 fps rather than 60 Hz. On a visible desktop the frame rate would be the monitor's.
   The GPU work and DWM's composition cost are not in these CPU figures.
3. **Per-move numbers for apps that draw every frame are mostly their idle loop.** Nuklear D3D11
   and raygui draw about 60 frames a second whether or not the mouse moves, so a move only adds
   input handling to a frame that was drawn anyway. Their real cost per frame comes from idle:
   Nuklear D3D11 about 0.57 ms CPU per frame, ImGui (drawing) about 0.34 ms, raygui about
   1.7 ms (2,188 / 3,840 frames, 1,313 / 3,840, 6,000 / 3,600). `baseline.py` already subtracts
   idle, so the per-move column shows only the extra cost.
4. **raylib built at the first try** (the 8 units with `-DPLATFORM_DESKTOP
   -DGRAPHICS_API_OPENGL_33`, warnings off) and made a real OpenGL 3.3 context on the hidden
   desktop: "NVIDIA GeForce RTX 4090/PCIe/SSE2, 3.3.0 NVIDIA 617.42" (from its trace log).
   It drew at 59.9 fps through the whole run.
5. **The GUICy demo was rebuilt by another session during the run** (`build\demo.exe` 21:11).
   Rounds 1 and 2 used the 142 KB build and round 3 a 143 KB build, so its 3 runs are not of one
   exe. Its numbers agree with BENCH.md steps 3-4. Its exe is now 142 KB (83 KB at step 3),
   because the demo links the whole v1 library.
6. The Win32 app uses the classic (v5) controls, because it has no manifest. Nuklear GDI keeps
   the demo's Arial 14 font. ImGui and Nuklear D3D11 use their built-in fonts. Fonts and looks
   differ, but the layout is the same.
7. The ImGui exes carry the D3DCOMPILER_47 import: the DX11 backend compiles its shaders at
   start-up. That is part of its 0.36 s start, against about 0.07 s for the others.

## Conclusions

On this scene and machine, GUICy and plain Win32 controls are the only two that use essentially
no CPU at idle: 0-15.6 ms per minute. GUICy's cost per mouse move (about 0.1 ms) is in the same
range as native controls (about 0.05 ms). It is about 40 times less than Nuklear's GDI backend
(4.2 ms), which redraws the whole window on every input. The apps that render every frame
(Nuklear D3D11, ImGui on D3D11 once it is made to draw, raygui) cost 1.3-6 s of CPU a minute at
idle, 2-10 % of a core, plus GPU work these numbers don't count. ImGui's wait-for-input pattern
cuts its idle CPU about 5 times (234 ms against 1,313 ms a minute), but that is still well above
GUICy's. GUICy starts in about 65 ms, with ImGui at 0.36 s and raygui at 0.5 s. Its working set
of 34 MB is between the native and Nuklear GDI apps (27 MB) and the GPU apps (57-82 MB). Its
142 KB exe is larger than the native one (47 KB) and smaller than every other library's
(313-1,171 KB). The hidden desktop hides real presentation, so frame pacing and GPU and
compositor cost on a visible screen were not measured here.

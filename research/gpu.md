# GUICy research: GPU rendering

Topic: how GUICy should draw on the GPU, so it can be the fastest and smallest immediate-mode
GUI library around. The CPU/software path is covered in a separate document. This one only
covers how the GPU path falls back to it.

Constraints this document assumes: plain C, built with clang (MinGW target, lld), one static
exe, and nothing used beyond what the OS ships (d3d11.dll, dxgi.dll, opengl32.dll, gdi32.dll,
user32.dll, dwmapi.dll). There is no d3dcompiler_47.dll at runtime and no vulkan-1.dll
dependency.

---

## 0. Top 10 ideas, ranked by speed gain versus effort

| # | Idea | Gain | Effort | Why |
|---|------|------|--------|-----|
| 1 | **Redraw only on input or change, and block in `GetMessage` otherwise** | Huge: idle CPU and GPU fall to about 0% | Trivial | An idle UI that still renders at 60 Hz burns the most power and time of anything in the app. Nuklear apps that loop forever lose here, not in the drawing. |
| 2 | **One instanced SDF quad for everything** (rects, rounded rects, borders, circles, shadows, glyphs, images) | Large: 1 draw call per frame, about 4x fewer vertex bytes than ImGui-style triangle lists | Low (about 60 lines of HLSL) | No CPU tessellation of rounded corners, AA for free, and the vertex shader builds the quad from `SV_VertexID`. |
| 3 | **Flip model swap chain (`FLIP_DISCARD`, 2 buffers) plus a frame-latency waitable object** | Medium for latency: about 1 frame less lag. Saves DWM a copy. | Low | It is the modern DXGI path. Blt-model swap chains add a copy and a frame of latency. |
| 4 | **Glyph atlas rasterized by the OS (GDI or DirectWrite) into an R8 texture, cached by (font, size, glyph)** | Large compared with per-frame GDI text | Low to medium | The approach of refterm and Zed. Text is 70-90% of a GUI's pixels and quads. |
| 5 | **Single persistent instance buffer, `Map(WRITE_DISCARD)` once per frame** | Medium: one Map per frame, no per-draw uploads | Trivial | It renames the buffer in the driver for you, so CPU and GPU never stall. |
| 6 | **Bytecode shaders compiled at build time with `fxc /Fh`, embedded as `static const BYTE[]`** | Smaller, starts faster, no DLL | Trivial | Removes d3dcompiler_47.dll (about 4 MB) and its runtime compile, which takes about 10-50 ms. |
| 7 | **Scissor-free clipping: a per-instance clip rect tested in the pixel shader** | Medium: keeps 1 draw call with nested scroll groups | Low | Every `RSSetScissorRects` change splits a batch. A clip rect carried in the instance (or as a clip index) never splits one. |
| 8 | **Skip the GPU frame when the command list hashes the same as last frame** | Large when nothing visible changed (hover over dead space, timers) | Low | Immediate mode re-runs the UI code, but the GPU need not redraw. Hash the instance buffer with FNV/xxhash and skip Present. |
| 9 | **Dirty rects through `Present1` (`FLIP_SEQUENTIAL`), or simply scissor to the damaged region** | Medium for power and bandwidth on big windows | Medium | Small wins on a 4090-class GPU. Pays on laptops and iGPUs. Optional, phase 2. |
| 10 | **WARP fallback via `D3D_DRIVER_TYPE_WARP`, with our own software renderer as the last resort** | Robustness, not speed | Trivial | One extra `D3D11CreateDevice` call covers RDP, VMs and broken drivers. |

Ideas I do **not** recommend for GUICy v1: compute-shader rasterizers (Vello, Pathfinder,
piet-gpu), Slug or curve fonts, MSDF fonts, bindless designs, D3D12 and Vulkan. They solve
problems that arbitrary vector art and 3D-scaled text have and a desktop widget UI does not,
and each one costs 2-20x the code. Section 2 explains why.

---

## 1. How the fastest GPU UI renderers work

### 1.1 Dear ImGui backends (`imgui_impl_dx11.cpp`, `imgui_impl_opengl3.cpp`)
- The CPU builds an `ImDrawList` per window: indexed triangles (`ImDrawVert` = pos float2, uv float2,
  col u32, so **20 bytes per vertex**) plus `ImDrawCmd`s, each holding a clip rect, a texture id and an
  index count.
- AA lines and rounded corners are tessellated **on the CPU**, adding a 1 px "fringe" of extra
  triangles that fade alpha to 0. One rounded rect with AA can take 50-100 vertices.
- Text comes from the font atlas: one textured quad per glyph (4 vertices, 6 indices). The
  white-pixel UV trick lets solid shapes and text share one texture and one shader, so
  most windows become **1 draw call**. A scissor change starts a new `ImDrawCmd`.
- The backend is about 600 lines of C++. Each frame it maps a dynamic VB and IB with `WRITE_DISCARD`,
  sets the state once, and loops over the commands with `RSSetScissorRects` + `DrawIndexed`.
- Its shaders are compiled at **runtime** with `D3DCompile` (it links d3dcompiler). GUICy must not do that.
- Lesson: a "triangle soup + white pixel + scissor list" design is simple and fast enough, but it moves
  all the AA and corner work onto the CPU, and its vertex bandwidth is 4-10x what an SDF instance needs.
- Source: github.com/ocornut/imgui/tree/master/backends

### 1.2 microui (rxi)
- About 1100 lines of C. It only emits **commands**: `RECT`, `TEXT`, `ICON`, `CLIP`, `JUMP`. The renderer
  decides how to draw them. The demo renderer batches quads into a fixed 16384-quad buffer and flushes on a
  texture or clip change.
- There are no rounded corners and no AA. It is the smallest real API around.
- rxi's companion article "Cached Software Rendering" hashes the command list per screen cell and redraws
  only the cells that changed. That idea carries straight over to the GPU (idea 8 and the damage rects).
- Lesson for GUICy: **a tiny command vocabulary** (rect, text, image, clip) is all a widget set
  needs. Rounded corners and borders can be parameters on `RECT` instead of new commands.
- Sources: github.com/rxi/microui, rxi.github.io/cached_software_rendering.html

### 1.3 nanovg (Mikko Mononen)
- A canvas-style vector API on GL. Filled convex paths become triangle fans. Concave fills use the
  **stencil-then-cover** method (Kokojima et al. 2006): fill into the stencil buffer, then draw a covering quad.
- AA comes from CPU-generated fringe strips, as in ImGui. Text uses fontstash with stb_truetype into an atlas.
- It is general, but slower than SDF quads for UI because of per-path tessellation and stencil
  state changes. It is a good read on "paint" uniforms (gradients and image patterns in one uniform block).
- Source: github.com/memononen/nanovg

### 1.4 Zed GPUI
- Every primitive kind (quads, shadows, underlines, monochrome sprites as glyphs, polychrome
  sprites as images and emoji, paths) is a typed array. **One instanced draw per primitive kind per layer.**
- The quad shader evaluates a rounded-rect SDF with **per-corner radii and per-side border widths** in
  the fragment shader. Shadows use Evan Wallace's closed-form **Gaussian-blurred rounded rect**
  (an erf approximation integrated along one axis, with a few samples along the other).
- Text: the OS rasterizes glyphs (CoreText or DirectWrite) into an atlas with **subpixel variants**
  (glyph x-offset quantized to 4 positions), then instanced sprite quads draw them.
- It ships a D3D11 backend on Windows (it went from Blade to wgpu, then added a native DX11 one).
- Lesson: this is essentially the design GUICy should copy, minus the multi-layer z-sorting.
- Sources: zed.dev/blog/videogame, github.com/zed-industries/zed (crates/gpui)

### 1.5 RAD Debugger and Ryan Fleury's UI series
- "UI, Part 6: Rendering": **one instanced rect primitive**. Each instance carries dst rect, src rect
  (atlas UV), 4 corner colors (a gradient for free), corner radius, border thickness and edge
  softness. The pixel shader evaluates `RoundedRectSDF` and multiplies by `1 - smoothstep(0, 2*softness, d)`.
  It uses **premultiplied alpha** (`ONE, INV_SRC_ALPHA`).
- Text and solid rects share one shader: a solid rect samples a white texel, or a flag skips sampling.
- RAD Debugger (Epic, open source, C) has a D3D11 renderer in `src/render/d3d11/`. Rects batch per
  (texture, clip, transform) "batch group", and blur is a separate pass. Fonts are rasterized with
  DirectWrite into a dynamic atlas.
- The whole debugger UI, with thousands of rows of text, renders in well under 1 ms of GPU time.
- Sources: dgtlgrove.com/p/ui-part-6-rendering (formerly rfleury.com / ryanfleury.substack.com),
  github.com/EpicGamesExt/raddebugger

### 1.6 refterm (Casey Muratori, with Martins Mozeiko's D3D11 sketch)
- A terminal renderer: the CPU writes one **cell array** (glyph index + fg + bg per cell). A single
  **compute shader**, or equally a full-screen quad, fetches each pixel's glyph from a coverage atlas.
- The glyph cache is a hash table keyed by the UTF-8 run, with LRU recycling. DirectWrite
  rasterizes a glyph **only when it is first seen**.
- About 1200 lines in total. It is orders of magnitude faster than Windows Terminal was at the time.
- Lesson: the GPU work for text is nothing. **The cost is cache misses and CPU-side shaping.** Keep a
  hash-keyed atlas, and never rasterize in the steady state.
- Sources: github.com/cmuratori/refterm (refterm_example_d3d11.c, refterm_glyph_cache.h, refterm.hlsl)

### 1.7 4coder (Allen Webster) and Handmade Hero
- 4coder: GL, a CPU-built list of textured and colored quads, glyph atlases from FreeType or the OS, and one
  draw per texture. Handmade Hero went from a software renderer to OpenGL with the same "render group"
  command list feeding both backends. That model fits GUICy's GPU-plus-CPU fallback.
- Source: handmadehero.org (episodes on the render group and the OpenGL backend), 4coder.net

### 1.8 Slug (Eric Lengyel)
- It renders glyphs **directly from their quadratic Bezier outlines** in the pixel shader. Per pixel, it
  casts horizontal and vertical rays and counts the signed winding of the curves in its band, using
  band-split curve lists. Coverage is exact at every scale, rotation and perspective.
- Papers: JCGT 6(2), 2017, "GPU-Centered Font Rendering Directly from Glyph Outlines". **The patent was
  dedicated to the public domain on 2026-03-17**, and MIT reference shaders came out the same day
  (github.com/EricLengyel/Slug).
- Cost: a heavy pixel shader (tens of curve evaluations per pixel), plus a CPU preprocessing step
  for fonts, plus a font parser (TrueType glyf/CFF) that GUICy would have to write itself.
- Verdict: superb for zoomable or 3D text. For fixed-size desktop UI text, an atlas of OS-hinted glyphs
  is faster, **looks better at 9-13 px** (hinting, ClearType-like gamma) and needs about 50 lines of glue. Not for v1.
  It could be optional later for a zoomable canvas (voxup, pixelhunter previews).

### 1.9 Vello and piet-gpu (Raph Levien, Linebender)
- A fully **compute-shader** 2D renderer: path flattening, binning, coarse tile rasterization and fine
  rasterization all run as compute stages with prefix sums (from "A sort-middle architecture for
  2D graphics" and later posts). It renders a whole SVG scene with exact AA in one pipeline.
- Since 2025 there are three variants. Vello (GPU compute), **Vello CPU** (SIMD, multithreaded, "sparse
  strips"), and **Vello Hybrid**: geometry and strips on the CPU, strip painting and compositing in an ordinary
  fragment shader, so it runs even without compute. The sparse strips design comes from Raph's "Potato" doc.
  Laurenz Stampfl's ETH thesis (2025) is the best current write-up of 2D rendering background.
- Verdict: the right design for arbitrary vector graphics. GUICy draws axis-aligned rounded rects and
  text, which an SDF quad does in 1 instruction-light pass. Vello is tens of thousands of lines of Rust
  and WGSL. Read the thesis for its background chapter, not for GUICy v1.

### 1.10 Pathfinder (Patrick Walton, Servo)
- Tiles paths on the CPU (or in compute), sorts tiles into solid and alpha (mask) tiles, rasterizes
  the alpha tiles' coverage to a mask texture, then composites. Solid interior tiles cost almost nothing.
- Lesson: **classify tiles as solid or edge** and spend pixel-shader work only on edges. GUICy's SDF quad
  already does this per pixel cheaply, so it gains nothing here.
- Source: github.com/servo/pathfinder

### 1.11 Skia Graphite (Chrome, 2025)
- The successor to Ganesh. It records draws into a `Recording` and **sorts by pipeline and depth** (depth testing
  with a painter's-order z, so opaque draws can be reordered to cut overdraw). Pipelines are precompiled.
  It has analytic AA coverage for rrects, and compute-based path atlases.
- Lesson: **write opaque pixels front to back with depth test, translucent ones back to front**, which only
  matters if overdraw is high. A GUI with panels over a background has about 2-3x overdraw. Not worth it in v1.
- Source: blog.chromium.org/2025/07/introducing-skia-graphite-chromes.html

### 1.12 Rive Renderer
- A vector renderer for animated art. It uses pixel-local storage, rasterizer-ordered views or
  atomics to resolve path coverage and blending **in one pass, without MSAA or stencil**. That is
  impressive, but tied to D3D11.3 ROV, Vulkan or Metal features. Not needed for a UI.
- Source: rive.app/renderer, github.com/rive-app/rive-runtime

### 1.13 Blend2D (Petr Kobalicek)
- A CPU renderer (out of scope), but its notes on **analytic coverage accumulation cells** and JIT pipelines
  explain why a well-written SIMD CPU path stays within 2-5x of a GPU for UI-sized work. That
  explains why the software fallback can be good, not just a crutch.
- Source: blend2d.com, blend2d.com/research

### 1.14 Others worth naming
- **Direct2D + DirectWrite**: ships with the OS and is GPU-accelerated over D3D11. Simple to use, but
  `ID2D1RenderTarget` has a high per-call overhead (hundreds of ns per `FillRoundedRectangle`, and it batches
  internally) and a large COM surface. It would be a fine "medium" backend, but our own SDF quad is faster
  and smaller to call.
- **sokol_gfx + sokol-shdc** (Andre Weissflog): the best example of a tiny C multi-backend layer with
  **offline-compiled shaders** emitted as C arrays for D3D11 (DXBC), GL (GLSL) and Metal. Copy its pattern.
- **Clay** (Nic Barker): a C layout library that outputs rect, text, image and scissor commands. It is
  renderer-agnostic like microui, and has good layout ideas for the other agent.
- **Evan Wallace, "Fast Rounded Rectangle Shadows"**: the shadow formula GPUI and others use.
- **Inigo Quilez, "2D distance functions"**: the canonical SDF list.

---

## 2. Techniques, and when each wins

### 2.1 Instanced quads (the core)
One instance = one rectangle-shaped primitive. The vertex shader expands it to 4 corners from
`SV_VertexID` (0..3, triangle strip) with no vertex buffer for corners and no index buffer.

Suggested instance layout (**48 bytes**, 16-byte aligned):

```c
typedef struct gc_inst {
    float    x0, y0, x1, y1;   // dst rect in pixels (16)
    uint16_t u0, v0, u1, v1;   // atlas texels, 0 = solid (8)
    uint32_t color;            // RGBA8 premultiplied, top colour (4)
    uint32_t color2;           // RGBA8 bottom colour (vertical gradient) or border colour (4)
    uint16_t radius;           // corner radius * 16, fixed 12.4 (2)
    uint16_t border;           // border width * 16 (2)
    uint16_t softness;         // edge blur * 16 (shadow) (2)
    uint16_t clip;             // index into clip-rect table (2)
    uint32_t flags;            // kind: rect|glyph|image|circle|shadow, tex slice (4)
    uint32_t pad;              // (4)
} gc_inst;                     // 48 bytes
```

Compared with ImGui, a rounded AA rect costs **48 bytes** instead of about 60 verts x 20 B + 100 indices x 2 B,
which is about **1.4 KB**. A glyph is 48 B instead of 4 x 20 + 6 x 2 = 92 B.
A busy screen (5000 glyphs + 500 rects) is about 260 KB per frame. A PCIe 4 link moves that in microseconds.

D3D11 layout: feed it as a `StructuredBuffer<gc_inst>` read by `SV_InstanceID` (no input layout at all, which is
the smallest code) or as a per-instance vertex buffer with `D3D11_INPUT_PER_INSTANCE_DATA` (slightly faster on
old Intel iGPUs). The StructuredBuffer path is fewer lines: it needs no `ID3D11InputLayout`.

Draw: `ctx->DrawInstanced(4, n, 0, 0)` with `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP`. That is **one call**.

### 2.2 One SDF shader for rect, rounded rect, border, circle and shadow
Inigo Quilez's rounded box (per-corner radii possible):

```hlsl
float sd_rrect(float2 p, float2 h, float r) {      // p from centre, h half size
    float2 q = abs(p) - h + r;
    return length(max(q, 0)) + min(max(q.x, q.y), 0) - r;
}
```

The pixel shader core (about 40 lines in total):

```hlsl
struct Inst { float4 rect; uint2 uv; uint col, col2; uint rb, sc; uint flags, pad; };
StructuredBuffer<Inst> I : register(t0);
StructuredBuffer<float4> Clips : register(t1);
Texture2DArray<float> Atlas : register(t2);   // R8 coverage, slice per page
Texture2D<float4> Img : register(t3);
SamplerState S : register(s0);
cbuffer C : register(b0) { float2 inv_half_vp; };

struct V { float4 pos:SV_Position; float2 p:P; nointerpolation uint id:I; float2 uv:T; };

V vs(uint vid:SV_VertexID, uint iid:SV_InstanceID) {
    Inst k = I[iid]; float2 t = float2(vid & 1, vid >> 1);
    float pad = (k.sc & 0xffff) / 16.0 * 3;               // grow quad for shadow blur
    float2 a = k.rect.xy - pad, b = k.rect.zw + pad;
    float2 px = lerp(a, b, t);
    V o; o.pos = float4(px * inv_half_vp - 1, 0, 1); o.pos.y = -o.pos.y;
    o.p = px; o.id = iid;
    o.uv = lerp(float2(k.uv.x & 0xffff, k.uv.x >> 16), float2(k.uv.y & 0xffff, k.uv.y >> 16), t);
    return o;
}

float4 unpack(uint c) { return float4(c & 255, (c >> 8) & 255, (c >> 16) & 255, c >> 24) / 255; }

float4 ps(V i) : SV_Target {
    Inst k = I[i.id];
    float4 cl = Clips[k.sc >> 16];
    if (any(i.p < cl.xy) || any(i.p >= cl.zw)) discard;   // or: return 0 (no discard = keeps early-Z)
    float2 c = (k.rect.xy + k.rect.zw) * 0.5, h = (k.rect.zw - k.rect.xy) * 0.5;
    float r = (k.rb & 0xffff) / 16.0, bw = (k.rb >> 16) / 16.0, soft = (k.sc & 0xffff) / 16.0;
    float d = sd_rrect(i.p - c, h, r);
    float aa = max(soft, 0.5);                            // 0.5 px = analytic 1-px AA
    float cov = saturate(0.5 - d / (2 * aa));
    float4 col = unpack(k.col);
    if (bw > 0) {                                          // border: ring between d and d+bw
        float inner = saturate(0.5 - (d + bw) / (2 * aa));
        col = lerp(unpack(k.col2), col, inner);
    }
    uint kind = k.flags & 7;
    if (kind == 1) cov *= Atlas.Sample(S, float3(i.uv / 1024.0, k.flags >> 8)); // glyph
    if (kind == 2) col *= Img.Sample(S, i.uv / 1024.0);                         // image
    return col * cov;                                      // premultiplied
}
```

Notes:
- A **circle** is a rounded rect with `r = min(h)`. A **line** or separator is a 1 px rect. A **checkmark or arrow**
  is a glyph from an icon font or an icon atlas, so no triangle primitive is needed for the 70 Nuklear calls
  (checkbox, combo arrow, tree twisty, slider knob = circle, progress = rect).
- **Shadows**: in the cheap version, soft = blur radius and `cov = 1 - smoothstep(-soft, soft, d)`. Visually that is
  fine for UI. The exact version is Evan Wallace's Gaussian rounded rect: `erf` along x in closed form, with 4 samples along y.
  It costs about 20 ALU ops more and is only needed for big, soft drop shadows.
- **Gradient**: lerp `col` to `col2` by `t.y` (pass it as an interpolant). Themes get it for free.
- Branching on `kind` is coherent per quad, so it costs almost nothing on modern GPUs. Alternatively, use 3 shader
  permutations and split into ordered batches. Don't. One shader is simpler and the branch is free.
- **Pixel-snap** rect edges to whole pixels on the CPU (round x0,y0,x1,y1). Then the SDF gives crisp 1 px
  borders, and AA only shows on curves. That is the single biggest "looks native" trick.

### 2.3 Analytic anti-aliasing
- With a true distance in pixels, `cov = saturate(0.5 - d)` is exact box-filter AA for a straight edge.
  For curved edges it is within a few % of true. Use `fwidth(d)` instead of the constant 1 when the UI might
  be scaled by a transform (DPI zoom inside a canvas): `cov = saturate(0.5 - d / fwidth(d))`.
- It wins over MSAA (no 4x RT memory, no resolve) and over CPU fringe tessellation (no vertices).
- It loses only for **overlapping shapes of the same colour**, where coverage-as-alpha gives conflation
  seams. Those do not occur in widget UIs.
- Do blending in **linear space?** For UI, sRGB-space blending with premultiplied alpha matches what
  GDI/Nuklear users expect. Keep the back buffer `R8G8B8A8_UNORM` (not `_SRGB`) so colours match the GDI
  fallback pixel for pixel. Text gamma is handled in the atlas (2.4).

### 2.4 Fonts: glyph atlas versus SDF/MSDF versus GPU curves

| Method | Quality at 9-14 px | Scaling | Code | Runtime cost | Verdict |
|---|---|---|---|---|---|
| **OS-rasterized coverage atlas** (GDI `GetGlyphOutline GGO_GRAY8_BITMAP`, or DirectWrite `IDWriteGlyphRunAnalysis`) | **Best** (hinted, matches the system) | One entry per size | ~150 lines | 1 texture fetch | **Use** |
| stb_truetype or own TTF rasterizer atlas | Good, unhinted | Per size | ~300-2000 lines | 1 fetch | Only for Linux |
| Single-channel SDF (Valve 2007) | Soft, rounded corners | Any size from one 32-64 px bake | ~200 lines | 1 fetch + smoothstep | For zoom only |
| MSDF (Chlumsky msdfgen) | Sharp corners, still unhinted | Any size | msdfgen is about 5k lines of C++ | 1 fetch + median3 | No |
| Slug or Loop-Blinn curves | Exact, unhinted | Any size and transform | 1-3k lines + font parser | Heavy pixel shader | No (v1) |

Details for the recommended atlas:
- Key = hash(font id, px size, glyph index, subpixel x phase 0..3). Use open addressing with 4096 slots and LRU
  recycling like refterm. Pack it with a skyline or shelf packer into a **1024x1024 R8 page**. When a page fills,
  add a slice to a `Texture2DArray` (allocate 4 slices up front: 4 MB) or flush and rebuild.
- Upload new glyphs with `UpdateSubresource` on the atlas region at frame start, batched. In the steady state there
  are 0 uploads.
- **ClearType/LCD subpixel text**: needs per-channel coverage (RGB atlas, 3x the memory) and **dual-source
  blending** (`SV_Target1` as the per-channel alpha, `D3D11_BLEND_SRC1_COLOR`), plus a known opaque background or
  correct gamma handling. Windows 11 itself moves toward grayscale AA in modern UI. **Grayscale in v1**, with
  ClearType as an option later. Apply a contrast and gamma boost (DirectWrite's `enhancedContrast`, or a
  `pow(cov, 1/1.4)` lookup baked into the atlas) so dark-on-light text is not thin.
- The text shaping for these apps is Latin, so a per-codepoint cmap lookup + kerning is enough. Leave complex
  shaping to DirectWrite's `IDWriteTextAnalyzer` only if it is ever needed.
- DPI: rasterize at physical pixel size (`GetDpiForWindow`). On a DPI change, clear the atlas.

### 2.5 Getting everything into one draw call
What normally breaks a batch, and how GUICy avoids it:
1. **Texture change** (font atlas vs. images): put glyphs in a `Texture2DArray` and user images in a second
   array or one big **image atlas**. Both are bound once. `flags` selects the slice. Image sizes vary in
   pixelhunter (thumbnails), so give images their own atlas pages, with large images in their own `Texture2D`
   bound as a small table (`Texture2D imgs[8]` with a dynamic index needs SM5.1 / D3D12. In D3D11 use an
   array of same-size slices or a few fixed slots with a `switch`).
   Simplest rule: **one batch per distinct large image**, while everything else stays in batch 0. A frame with
   one big preview = 3 draws (UI below, image, UI above). That is still trivially cheap.
2. **Scissor change** (scroll groups, combos, tooltips): replace it with the per-instance clip index (2.2).
   The cost is one compare per pixel. `discard` disables early-Z, which we don't use anyway.
3. **Blend state change**: everything is premultiplied `ONE, INV_SRC_ALPHA`. Opaque rects are just alpha 255.
4. **Z order**: immediate mode emits in painter's order already. Popups and tooltips are emitted into a
   later "layer" list, and lists are concatenated before upload (Nuklear does the same with its window order).

Result: **1 draw call** for almost every frame. Draw-call count stops mattering below about 1000 on D3D11, so
the real reason for this is code size and simplicity.

### 2.6 Persistent mapped buffers
- **D3D11**: no persistent mapping. `D3D11_USAGE_DYNAMIC` + `Map(D3D11_MAP_WRITE_DISCARD)` once per frame is
  the idiom. The driver renames the memory, so there is no stall. For more than one upload per frame, use
  `WRITE_NO_OVERWRITE` with a ring offset (needs D3D11.1 for constant/SRV buffers, which is available on Win10+).
  Size the buffer at 64k instances (3 MB) and grow by doubling.
- Write the instances **directly into the mapped pointer** as the UI code runs? No. A mapped D3D11 buffer must be
  unmapped before draw, and an exception mid-frame would leave it mapped. Build into a CPU array (which the CPU
  fallback also consumes) and `memcpy` once. 260 KB memcpy = about 20 us.
- **OpenGL 4.4+**: `glBufferStorage(GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)` with 3 ring regions and
  `glFenceSync` per region. The fallback for GL 3.3 is `glBufferData(NULL)` orphaning + `glBufferSubData`.
  Orphaning is simpler and enough for UI.
- **D3D12/Vulkan**: persistent map of an UPLOAD/HOST_VISIBLE heap, ring by frame index and fence. Out of scope.

### 2.7 Texture arrays
- `Texture2DArray` R8 for glyphs: all pages bound as one SRV, so atlas growth never breaks a batch and never
  forces a reupload. On D3D11 feature level 10.0+ arrays are always there (up to 512 slices at FL10, 2048 at FL11).
- GL 3.0+ has `GL_TEXTURE_2D_ARRAY`.

### 2.8 Bindless
- Bindless (D3D12 descriptor heaps with `ResourceDescriptorHeap[]` in SM6.6, Vulkan descriptor indexing) lets each
  instance index any texture. It is a real win for apps with thousands of textures. GUICy has 1 atlas + a few images.
  It would force D3D12 (about 3x the backend code: command allocators, fences, barriers, descriptor heaps,
  root signature) and DXIL bytecode. **Not worth it.** Texture arrays give 95% of the benefit on D3D11.

### 2.9 Minimal state changes per frame (the whole D3D11 frame)
```
Map/memcpy/Unmap instance SB            (1)
UpdateSubresource glyph atlas, if dirty (0..n)
OMSetRenderTargets(backbuffer RTV)      (could be set once if FLIP_DISCARD rebinding is handled)
RSSetViewports                          (only on resize)
ClearRenderTargetView                   (or skip: the background is a full-window rect)
[ VS/PS/SRV/sampler/blend/IA topology set once at init and on device-loss only*  ]
DrawInstanced(4, n, 0, 0)               (1)
Present(1, 0)
```
*D3D11 keeps state between frames. Nothing else touches the context, so set the state once. After
`Present` with flip model, the RTV for the back buffer must be rebound each frame (buffer index 0 always
refers to the current back buffer in D3D11, so the same RTV object works. `FLIP_SEQUENTIAL` unbinds it, so call
`OMSetRenderTargets` every frame. It is one call).

---

## 3. Frame pacing, latency and idle

### 3.1 Redraw only on change (the biggest win)
- The loop: `GetMessage` blocks (0% CPU) until input, a timer, `WM_PAINT` or a user wake-up arrives. After
  each batch of messages, run the UI code once. If the app's state or the input "hot" item changed, run it a
  **second time** (immediate-mode UIs need one extra pass to settle the layout after a click, as Nuklear,
  ImGui and RAD do), then render.
- Animations (progress bars, caret blink, smooth scroll) ask for a next frame: `gc_request_frame(ms)`
  sets a `SetTimer` or a `MsgWaitForMultipleObjects` timeout. The caret blink at 530 ms means 2 frames per second
  in a text box, not 60.
- Background work (voxup's ComfyUI jobs, pixelhunter downloads) posts `PostMessage(hwnd, WM_APP, ...)` or sets an
  event, and `MsgWaitForMultipleObjectsEx` wakes on it.
- **Skip identical frames**: after building the instance list, hash it (xxhash64 at about 10 GB/s: 260 KB = 25 us).
  If it is the same as the last presented frame and the window was not resized or exposed, skip upload, draw and Present.
  Mouse moves over dead space then cost only the UI pass.
- Measured elsewhere: ImGui apps idle at a constant 60 fps use about 1-5% of a CPU core and keep the GPU out of
  its deepest idle state. Event-driven loops fall to about 0% and let the GPU clock down.

### 3.2 DXGI flip model
- `DXGI_SWAP_EFFECT_FLIP_DISCARD` (Win10+) with `BufferCount = 2`, `DXGI_FORMAT_R8G8B8A8_UNORM` or
  `B8G8R8A8_UNORM`, `SampleDesc.Count = 1`, `Scaling = DXGI_SCALING_NONE` (no stretched content during a live resize).
- Flip model hands the back buffer to DWM **without a copy** (DWM composes from it directly). Windowed apps can then
  get **independent flip** or **multiplane overlay** promotion, which gives fullscreen-equal latency.
  The old `DXGI_SWAP_EFFECT_DISCARD`/`SEQUENTIAL` (blt model) adds a copy and usually a frame of latency.
- Create the swap chain with `IDXGIFactory2::CreateSwapChainForHwnd`. Call
  `IDXGIFactory::MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)` so DXGI does not hijack Alt+Enter in a tool app.
- Resize: on `WM_SIZE`, release the RTV, call `ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, flags)` and recreate the RTV.
  To get smooth live resize, render **inside** `WM_SIZE` / `WM_PAINT` during the modal sizing loop (the message
  loop is not running then). For flicker-free sizing, also return 1 from `WM_ERASEBKGND` and use
  `WS_EX_NOREDIRECTIONBITMAP` (DirectComposition style). Do not do that if the GDI fallback must draw into the same
  window, since it needs a redirection bitmap. So choose the backend **before** creating the window, or recreate it.

### 3.3 Waitable swap chain (latency)
- Create with `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`, then
  `IDXGISwapChain2::SetMaximumFrameLatency(1)` and `GetFrameLatencyWaitableObject()`.
- In the loop: `WaitForSingleObjectEx(waitable, 1000, TRUE)` **before** reading input and building the UI. Then the
  frame is built from the freshest input, just before the GPU can take it. That removes up to 2 queued frames
  (about 16-33 ms at 60 Hz).
- With event-driven redraw, the waitable is signalled almost always (nothing queued), so the wait returns at once.
  Include the handle in `MsgWaitForMultipleObjects` only while animating.

### 3.4 Dirty rects and Present1
- `IDXGISwapChain1::Present1(sync, 0, &(DXGI_PRESENT_PARAMETERS){ n, rects, scroll_rect, scroll_off })`.
  DWM then recomposes only those rects, which saves memory bandwidth and power (Microsoft's stated reason).
- Rules: it needs **`FLIP_SEQUENTIAL`** (FLIP_DISCARD and MSAA ignore partial presents). The app must keep the rest of the
  back buffer valid, so with 2 buffers it must **copy the previous frame's dirty regions forward** (or redraw the union
  of the last 2 frames' damage). The scroll rect and offset can describe a scrolled list as a move plus a new strip.
- How GUICy finds damage: from rxi's cell-hash idea, split the window into 64x64 tiles. Per tile, hash the instances
  that touch it (an instance's bbox, inflated by shadow pad, decides its tiles). Changed tiles = damage. Merge them into
  at most 8 rects. Render with a scissor per rect, or one scissor of the union bounding box (simplest).
- When it pays: big windows (4K, 1440p) with a small change (hover, caret). It cuts GPU fill by 90%+,
  but GUICy's fill is already about 0.1-0.3 ms on a midrange GPU. The real gain is **DWM composition power on laptops**.
  Rank it as phase 2. The redraw-on-change loop (3.1) gives most of the benefit with none of the complexity.

### 3.5 Vsync versus latency
- `Present(1, 0)` = vsync, with no tearing (windowed flip model never tears unless you opt in).
- `Present(0, DXGI_PRESENT_ALLOW_TEARING)` (needs `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` and a check of
  `IDXGIFactory5::CheckFeatureSupport`) gives the lowest latency on VRR monitors, but a UI gains nothing from >refresh-rate frames.
- Recommendation: **vsync on (sync interval 1) + waitable latency 1 + event-driven loop**. Input-to-photon is then
  about 1 refresh interval + scanout, which is about the best a windowed app gets. Sync interval 0 only for a benchmark mode.
- With idle detection, vsync never spins: the frame is only presented when something changed.

### 3.6 Zero use when idle (checklist)
1. `GetMessage` / `MsgWaitForMultipleObjectsEx` with an infinite timeout when nothing animates.
2. No `PeekMessage` busy loops.
3. Hash-skip identical frames (3.1).
4. Minimized (`IsIconic`) or occluded (`Present` returning `DXGI_STATUS_OCCLUDED`): stop presenting, and test
   with `Present(0, DXGI_PRESENT_TEST)` on a slow timer.
5. Release staging buffers. Keep the atlas.
Result: 0.0% CPU in Task Manager and the GPU in its idle P-state.

---

## 4. Smallest code

### 4.1 Offline shader bytecode (no d3dcompiler at runtime)
- Build step (dev machine, Windows SDK `fxc.exe`, which compiles SM5.0 to DXBC):
  ```
  fxc /nologo /T vs_5_0 /E vs /O3 /Qstrip_reflect /Qstrip_debug /Fh gc_vs.h /Vn gc_vs_bytes gc.hlsl
  fxc /nologo /T ps_5_0 /E ps /O3 /Qstrip_reflect /Qstrip_debug /Fh gc_ps.h /Vn gc_ps_bytes gc.hlsl
  ```
  `/Fh` writes `const BYTE gc_vs_bytes[] = {...};`. Check the generated headers into the repo, so a build
  without the SDK still works. A VS plus PS of this size come to about **1.5-3 KB of DXBC** in total.
- Alternatives: `dxc -T vs_6_0` gives DXIL (D3D12 only). `D3DCompile` at runtime needs d3dcompiler_47.dll, which is on
  Win10/11 in System32 but is technically not a documented OS component for apps. Avoid it.
- Feature level: request `D3D_FEATURE_LEVEL_11_0, 10_1, 10_0`. To allow FL 10.x, compile `vs_4_0`/`ps_4_0`
  (StructuredBuffer reads in VS need cs/vs 4.0 + `ComputeShader_Plus_RawAndStructuredBuffers_Via_Shader_4_x`, which is
  optional). The safest small path: **vs_4_0/ps_4_0 with a per-instance vertex buffer** works on every D3D10+ GPU and WARP.
  Using a StructuredBuffer needs FL 11.0 (all GPUs since about 2010, and WARP). Pick FL 11.0 and the StructuredBuffer path,
  with the CPU fallback for anything older.

### 4.2 How small a D3D11 backend can be
A reference: Martins Mozeiko's public gist "win32_d3d11.c" does device + flip swap chain + shaders + texture +
draw in **about 400 lines including comments**. The GUICy backend needs less (no depth, no 3D):

| Part | Lines of C (approx.) |
|---|---|
| `D3D11CreateDevice` (hardware, then WARP) + `CreateSwapChainForHwnd` + waitable | 45 |
| Create VS/PS from embedded bytecode, blend, sampler, raster states | 30 |
| Instance StructuredBuffer + SRV, grow on demand | 25 |
| Clip-rect buffer + SRV | 15 |
| Glyph atlas `Texture2DArray` + SRV + region upload | 25 |
| Image textures (create, update, free) | 30 |
| Frame: map, copy, bind RTV, viewport, clear, draw, present | 25 |
| Resize + device-lost (`DXGI_ERROR_DEVICE_REMOVED` / `RESET`: recreate everything) | 30 |
| **Total** | **about 225 lines of C + about 70 lines of HLSL** |

In C, COM calls go through `obj->lpVtbl->Method(obj, ...)`. Define `COBJMACROS` and use
`ID3D11DeviceContext_DrawInstanced(ctx, 4, n, 0, 0)` for readability. The MinGW-w64 headers ship
d3d11.h and dxgi1_3.h with C interfaces. Link `-ld3d11 -ldxgi -ldxguid`, or load `d3d11.dll` with `LoadLibrary`/`GetProcAddress`
(`D3D11CreateDevice`, `CreateDXGIFactory2`) so the exe even **starts** on a box where d3d11 is broken, and the
software path takes over. Recommend the dynamic load: it is 10 lines.

### 4.3 OpenGL as the second backend, kept tiny
- On Windows, opengl32.dll only exports GL 1.1. The rest comes from `wglGetProcAddress` after creating a context.
  Modern context: create a dummy window + pixel format + legacy context, get `wglCreateContextAttribsARB`
  and `wglChoosePixelFormatARB`, then make a 3.3 core context. That is about 80 lines (the dummy-window dance is the ugly part).
- You only need about 25 GL functions: `glGenBuffers glBindBuffer glBufferData glBufferSubData glCreateShader glShaderSource
  glCompileShader glCreateProgram glAttachShader glLinkProgram glUseProgram glGetUniformLocation glUniform2f
  glGenVertexArrays glBindVertexArray glVertexAttribPointer glVertexAttribIPointer glVertexAttribDivisor
  glEnableVertexAttribArray glDrawArraysInstanced glTexImage3D glTexSubImage3D glActiveTexture glBlendFunc glScissor`.
  Load them with one X-macro list: `#define GL_FUNCS X(void,glGenBuffers,(GLsizei,GLuint*)) ...`. That is 30 lines.
- GLSL is compiled by the driver at runtime, so ship it as a C string (about 70 lines, mostly a copy of the HLSL with
  macros: `#define float2 vec2`, etc. A shared shader source between HLSL and GLSL through a header of macros is
  doable for this simple shader and keeps one source of truth).
- GL 3.3 has no StructuredBuffer in VS (SSBOs need 4.3), so use per-instance attributes with `glVertexAttribDivisor(i, 1)`.
  That is the same instance struct, so D3D11 could also use the vertex-buffer path, keeping both backends identical.
- Swap: `SwapBuffers(hdc)` with `wglSwapIntervalEXT(1)`. GL on Windows goes through the driver's ICD and
  composes through DWM, generally as well as DXGI. On NVIDIA, "OpenGL present method: prefer layered on DXGI" in the control panel.
- Total: about **250 lines of C**. It is the same size as D3D11, so it is a cheap second backend, **mainly as the Linux path**.
  On Windows it adds nothing over D3D11 + WARP + CPU. **Recommendation: D3D11 only on Windows, GL 3.3 for Linux.**

### 4.4 Vulkan and D3D12
- `vulkan-1.dll` is installed by the GPU driver installers (NVIDIA, AMD, Intel), not by Windows. It is missing on
  Microsoft Basic Display Adapter, many VMs, RDP and Windows Server. If it is ever used: `LoadLibraryA("vulkan-1.dll")`,
  `GetProcAddress(vkGetInstanceProcAddr)`, and on failure, fall back. Never link it.
- Vulkan or D3D12 backends cost about 1000-1500 lines for the same UI (sync, barriers, descriptors, pipelines,
  swapchain recreation). They bring no benefit at about 1 draw call and about 300 KB per frame. **Skip both.**

### 4.5 Linux
- Window: X11 via `dlopen("libX11.so.6")`, or Wayland via `libwayland-client.so.0` + xdg-shell protocol code
  (generated C, about 300 lines) + `libxkbcommon` for keys. SDL3 would be simpler but violates "nothing outside itself"
  unless linked statically.
- GL: `dlopen("libEGL.so.1")` + `libGL.so.1` or `libGLESv2.so.2`, EGL works on both X11 and Wayland. GLES 3.0 shaders
  are nearly identical to GL 3.3.
- Vulkan: `dlopen("libvulkan.so.1")`, which is almost always present on desktop distros (Mesa). Not needed.
- Software: XPutImage / XShm, or a Wayland `wl_shm` buffer.

---

## 5. Detecting a GPU and falling back cleanly

Order of attempts at startup (and again after a device-removed error):

```c
static const D3D_FEATURE_LEVEL fl[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
UINT f = D3D11_CREATE_DEVICE_BGRA_SUPPORT;          // + _DEBUG in debug builds
HRESULT hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, f, fl, 2,
                                D3D11_SDK_VERSION, &dev, &got, &ctx);
if (hr == E_INVALIDARG)                              // Win7-era runtime without 11_1
    hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, f, fl + 1, 1, ...);
if (FAILED(hr) || gc_gpu_is_basic(dev))              // see below
    hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, f, fl, 2, ...);  // optional
if (FAILED(hr)) use_software_renderer();
```

- **Is it a real GPU?** Get the adapter with `IDXGIDevice_GetAdapter` and call `GetDesc1`. If
  `DXGI_ADAPTER_FLAG_SOFTWARE` is set, or `VendorId == 0x1414 && DeviceId == 0x8c` (Microsoft Basic Render
  Driver), or `0x1414/0x8d` (WARP), the "hardware" device is actually software.
- **WARP versus our own software renderer**:
  - WARP is Microsoft's JIT-compiled, multithreaded, SSE/AVX software D3D11 in d3d10warp.dll (part of the OS).
    It runs the same shaders, so output is identical to the GPU and there is no extra code. It is fast for 3D, but for
    a 2D UI the per-frame overhead (full-screen pass, JIT pipeline, thread pool wake-up) is about 1-5 ms at 1080p,
    and its CPU use while active is spread over all cores.
  - Our own CPU renderer (the other agent's topic) knows the scene is rects and glyphs, and can update only damaged
    rects with `BitBlt`/`StretchDIBits` to a GDI DC. For small updates it will beat WARP easily, and at idle it
    is the same (0).
  - **Recommendation**: Hardware D3D11 then our own software renderer. Use WARP only as an option (`GUICY_RENDER=warp`) for
    testing the D3D path on machines without a GPU, and for RDP sessions where a hardware device is reported but slow.
- **RDP / remote sessions**: `GetSystemMetrics(SM_REMOTESESSION)`. Under RDP, D3D11 renders on the server GPU or WARP
  and then encodes. GDI-style partial updates are often cheaper over the wire. Prefer the software path with
  dirty rects there.
- **Runtime failure**: `Present` or `ResizeBuffers` returning `DXGI_ERROR_DEVICE_REMOVED`/`DEVICE_RESET` (driver
  update, TDR, GPU unplugged): release everything, retry the chain above once, otherwise switch to software in the same frame.
  Because the instance list is backend-neutral (2.1), switching backends needs no app involvement.
  Note: if the window was created with `WS_EX_NOREDIRECTIONBITMAP`, GDI cannot draw into it. Either don't use that style,
  or recreate the window on fallback. **Simplest: don't use the style.** The cost is a less smooth live resize.
- **Hybrid laptops**: export `NvOptimusEnablement = 1` and `AmdPowerXpressRequestHighPerformance = 1`? **No.** For a UI, the iGPU
  is the right choice: lower power, and the dGPU may sleep. Use the default adapter. (DXGI 1.6 `EnumAdapterByGpuPreference`
  `MINIMUM_POWER` if there is ever a reason to choose.)

---

## 6. Research papers on 2D vector and UI rasterization on GPU

- Loop, Blinn. **"Resolution Independent Curve Rendering using Programmable Graphics Hardware."** SIGGRAPH 2005.
  The curve-inside test in the pixel shader (the patent has now expired, as of March 2026).
- Kokojima, Sugita, Saito, Takemoto. **"Resolution Independent Rendering of Deformable Vector Objects using Graphics
  Hardware."** SIGGRAPH 2006 sketch. Stencil-then-cover.
- Green (Valve). **"Improved Alpha-Tested Magnification for Vector Textures and Special Effects."** SIGGRAPH 2007 course.
  SDF text.
- Nehab, Hoppe. **"Random-Access Rendering of General Vector Graphics."** SIGGRAPH Asia 2008. Per-cell curve lists
  (a precursor to Slug's bands).
- Kilgard, Bolz. **"GPU-accelerated Path Rendering."** SIGGRAPH Asia 2012 (NV_path_rendering).
- Ganacim, Lima, Henrique de Figueiredo, Nehab. **"Massively-Parallel Vector Graphics."** SIGGRAPH Asia 2014. Shortcut trees.
- Li, Hao, Song, Wu, Zhou, Wang. **"Efficient GPU Path Rendering Using Scanline Rasterization."** SIGGRAPH Asia 2016.
- Chlumsky. **"Shape Decomposition for Multi-channel Distance Fields."** Master's thesis, CTU Prague 2015 (MSDF).
- Lengyel. **"GPU-Centered Font Rendering Directly from Glyph Outlines."** JCGT 6(2), 2017. Slug.
- Levien. **"A sort-middle architecture for 2D graphics"** (2020) and **"Fast 2D rendering on GPU"** (2020), blog posts
  that lead to piet-gpu and Vello. Levien, Uguray, **"GPU-friendly Stroke Expansion,"** HPG 2024 (arXiv 2405.00127).
- Stampfl. **"High-performance 2D graphics rendering on the CPU using sparse strips."** ETH Zurich master's thesis, 2025.
  Best current overview. The background chapter covers GPU methods too.
- Wallace. **"Fast Rounded Rectangle Shadows"** (2017, blog). Closed-form Gaussian blur of a rounded box.
- Levien. **"Blurred rounded rectangles"** (2020, blog). A cheaper approximation.

---

## 7. The recommended GUICy GPU design (summary)

```
app code --> gc_* widget calls --> command/instance list (CPU array of gc_inst, 48 B each)
                                          |
                      +-------------------+-------------------+
                      |                                       |
              D3D11 backend (Win)                     software backend (other doc)
              GL 3.3 backend (Linux)                  same gc_inst list, GDI/XShm blit
```

1. **One primitive**: `gc_inst` (48 B) = rect + uv + 2 colours + radius + border + softness + clip index + kind.
   Rects, rounded rects, borders, circles, cheap shadows, glyphs, icons and images all go through it.
2. **One shader pair**, about 70 lines HLSL, compiled offline with `fxc /Fh` into C arrays (about 2 KB). Uses analytic SDF
   AA, premultiplied alpha, and a per-instance clip rect instead of scissor.
3. **One draw call**: `DrawInstanced(4, n)` from a StructuredBuffer (FL 11.0), with an R8 `Texture2DArray` glyph atlas
   and an image atlas. It splits only around large user images.
4. **Text**: glyphs rasterized by GDI/DirectWrite (grayscale, gamma-boosted) into the atlas on first use, hash-keyed by
   (font, size, glyph, subpixel phase), with LRU.
5. **Swap chain**: flip discard, 2 buffers, waitable, max latency 1, vsync 1.
6. **Loop**: event-driven with `MsgWaitForMultipleObjectsEx`. It renders only on change, skips frames whose instance hash
   is unchanged, and has animation timers on request. Idle = 0%.
7. **Fallback**: dynamic `LoadLibrary("d3d11.dll")`, hardware, then (basic-adapter check) our software renderer. WARP is
   an opt-in env var. On device removal, switch at runtime.
8. **Size budget**: about 225 lines C + 70 HLSL for D3D11, about 250 lines C + 70 GLSL for GL on Linux.
9. **Later, optional**: Present1 dirty rects (FLIP_SEQUENTIAL), ClearType dual-source text, an exact Gaussian shadow,
   and Slug for zoomable canvases.

---

## Sources

GPU UI renderers
- Dear ImGui backends: https://github.com/ocornut/imgui/tree/master/backends (imgui_impl_dx11.cpp, imgui_impl_opengl3.cpp)
- microui: https://github.com/rxi/microui
- rxi, "Cached Software Rendering": https://rxi.github.io/cached_software_rendering.html
- nanovg: https://github.com/memononen/nanovg
- Zed, "Leveraging Rust and the GPU to render user interfaces at 120 FPS" (Antonio Scandurra, 2023): https://zed.dev/blog/videogame
- Zed GPUI source: https://github.com/zed-industries/zed/tree/main/crates/gpui
- Zed, GPUI text via OS rasterizer + atlas: https://x.com/zeddotdev/status/1633852097039933442
- Ryan Fleury, "UI, Part 6: Rendering": https://www.dgtlgrove.com/p/ui-part-6-rendering (old: https://www.rfleury.com/p/ui-part-6-rendering)
- Ryan Fleury, UI series index: https://www.dgtlgrove.com/p/index
- RAD Debugger: https://github.com/EpicGamesExt/raddebugger
- solidalloy, "Drawing Rounded Corners and Borders with SDF" parts 1-2: https://medium.com/@solidalloy/drawing-rounded-corners-and-borders-with-sdf-part-1-rounded-corners-8017bb6ce6f9 , https://medium.com/@solidalloy/drawing-rounded-corners-and-borders-with-sdf-part-2-borders-1e7cf22bd571
- Handmade Network, "Performance of IMGUI": https://hero.handmade.network/forums/code-discussion/t/8498-performance_of_imgui_
- refterm: https://github.com/cmuratori/refterm , D3D11 file: https://github.com/cmuratori/refterm/blob/main/refterm_example_d3d11.c , glyph cache: https://github.com/cmuratori/refterm/blob/main/refterm_glyph_cache.h
- Casey Muratori on the full-screen-quad terminal: https://x.com/cmuratori/status/1405342954194051073
- m1el on refterm's glyph cache: https://m1el.github.io/
- Martins Mozeiko, minimal D3D11 in C: https://gist.github.com/mmozeiko/5e727f845db182d468a34d524508ad5f
- sokol_gfx / sokol-shdc: https://github.com/floooh/sokol , https://github.com/floooh/sokol-tools
- Clay layout library: https://github.com/nicbarker/clay
- Matthew Berger, "Build your own retained UI": https://matthewberger.dev/articles/posts/build-your-own-retained-ui-layout-interaction/

Fonts
- Slug paper (JCGT 2017): https://jcgt.org/published/0006/02/02/
- Slug i3D 2018 slides: https://terathon.com/i3d2018_lengyel.pdf
- Eric Lengyel, "A Decade of Slug" (patent to public domain, 2026-03-17): https://terathon.com/blog/decade-slug.html
- Slug reference shaders (MIT): https://github.com/EricLengyel/Slug
- Slug patent US 10,373,352: https://patents.google.com/patent/US10373352B1
- Sluggish (toy C/C++ Slug): https://github.com/mightycow/Sluggish
- AlphaPixel, "SDF vs MSDF vs Slug": https://alphapixeldev.com/sdf-vs-msdf-vs-slug-vs-rive-gpu-text-rendering/
- msdfgen: https://github.com/Chlumsky/msdfgen
- Valve SDF paper: https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
- DirectWrite glyph run analysis: https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwriteglyphrunanalysis

SDF and shadows
- Inigo Quilez, 2D distance functions: https://iquilezles.org/articles/distfunctions2d/
- Evan Wallace, "Fast Rounded Rectangle Shadows": https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/
- Raph Levien, "Blurred rounded rectangles": https://raphlinus.github.io/graphics/2020/04/21/blurred-rounded-rects.html

Compute and vector renderers
- Vello: https://github.com/linebender/vello
- Linebender monthly posts (Vello Hybrid, sparse strips): https://linebender.org/blog/tmil-14/ , https://linebender.org/blog/tmil-15/ , https://linebender.org/blog/tmil-22/ , https://linebender.org/blog/tmil-24/
- L. Stampfl, sparse strips thesis (ETH 2025): https://ethz.ch/content/dam/ethz/special-interest/infk/inst-pls/plf-dam/documents/StudentProjects/MasterTheses/2025-Laurenz-Thesis.pdf
- HN discussion of the thesis: https://news.ycombinator.com/item?id=45881568
- Raph Levien, "Fast 2D rendering on GPU": https://raphlinus.github.io/rust/graphics/gpu/2020/06/13/fast-2d-rendering.html
- Raph Levien, "A sort-middle architecture for 2D graphics": https://raphlinus.github.io/rust/graphics/gpu/2020/06/12/sort-middle.html
- Levien and Uguray, "GPU-friendly Stroke Expansion": https://arxiv.org/abs/2405.00127
- Pathfinder: https://github.com/servo/pathfinder
- Skia Graphite: https://blog.chromium.org/2025/07/introducing-skia-graphite-chromes.html
- Rive Renderer: https://rive.app/renderer , https://github.com/rive-app/rive-runtime
- Blend2D: https://blend2d.com/ , https://blend2d.com/research.html

DXGI and Direct3D
- Flip model, dirty rectangles, scrolled areas: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements
- Present1: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-present1
- DXGI_PRESENT_PARAMETERS: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/ns-dxgi1_2-dxgi_present_parameters
- For best performance, use DXGI flip model: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model
- Reduce latency with DXGI 1.3 swap chains (waitable): https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains
- GetFrameLatencyWaitableObject: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject
- Variable refresh rate / ALLOW_TEARING: https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays
- WARP: https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp
- fxc command line: https://learn.microsoft.com/en-us/windows/win32/direct3dtools/dx-graphics-tools-fxc-syntax
- D3D11 feature levels: https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro

OpenGL
- WGL_ARB_create_context: https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
- ARB_buffer_storage (persistent mapping): https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt
- Khronos wiki, creating an OpenGL context on Windows: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)

Papers (see section 6). Loop-Blinn 2005: https://www.microsoft.com/en-us/research/publication/resolution-independent-curve-rendering-using-programmable-graphics-hardware/ ;
Nehab-Hoppe 2008: https://hhoppe.com/proj/ravg/ ; Kilgard-Bolz 2012: https://developer.nvidia.com/gpu-accelerated-path-rendering ;
Ganacim et al. 2014: https://w3.impa.br/~diego/projects/GanEtAl14/ ; Li et al. 2016: https://doi.org/10.1145/2980179.2982434 .
Note: the paper links in this paragraph, the Martins Mozeiko gist and the two Levien 2020 blog URLs come from memory. They were not fetched while this was written, so check them before quoting.

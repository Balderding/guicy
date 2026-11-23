/* gyspike - G0 of PLAN-v2: a one-day GPU spike. It is not part of guicy.h.

   The question: does a D3D11 frame's fixed CPU cost (GUICy's tile hash pass, one Map of the ~500
   instances, one DrawInstanced, Present(1,0) on a FLIP_DISCARD swap chain on a child window) beat
   the CPU renderer's own frames for the running cases (typing, hover, full redraw)? The user's rule:
   the GPU becomes the default only if it wins every case after startup, and a tie loses.

   gyspike            runs itself again on a desktop of its own (never the user's), then prints a table

   Timed with QueryThreadCycleTime (this thread's own CPU cycles) and D3D11 timestamp queries for the
   GPU side, 9 runs of 200 frames each. Shaders: D3DCompile from the System32 d3dcompiler_47.dll at
   run time, which is fine for a spike outside the library (guicy.h will carry precompiled bytecode). */
#define COBJMACROS
#define GUICY_IMPLEMENTATION
#include "../guicy.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef HRESULT (WINAPI *compile_fn)(const void *, SIZE_T, const char *, const void *, void *, const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);

static const char *HLSL =
    "struct I { int4 r; uint2 uvc; uint c0, c1; float rad, bor, soft; uint a, b, c; };\n"
    "StructuredBuffer<I> inst : register(t0);\n"
    "cbuffer V : register(b0) { float2 inv; };\n"
    "struct O { float4 p : SV_Position; float4 c : COLOR; };\n"
    "O vs(uint v : SV_VertexID, uint i : SV_InstanceID) {\n"
    "  I k = inst[i]; O o;\n"
    "  int x0 = (k.r.x << 16) >> 16, y0 = k.r.x >> 16, x1 = (k.r.y << 16) >> 16, y1 = k.r.y >> 16;\n"
    "  float2 p = float2((v & 1) ? x1 : x0, (v & 2) ? y1 : y0);\n"
    "  o.p = float4(p.x * inv.x * 2 - 1, 1 - p.y * inv.y * 2, 0, 1);\n"
    "  o.c = float4(((k.c0 >> 16) & 255) / 255.0, ((k.c0 >> 8) & 255) / 255.0, (k.c0 & 255) / 255.0, (k.c0 >> 24) / 255.0);\n"
    "  return o; }\n"
    "float4 ps(O o) : SV_Target { return o.c; }\n";

static double g_cyc_per_ms;
static double cycles_ms(ULONG64 c) { return (double)c / g_cyc_per_ms; }
static ULONG64 tcycles(void) { ULONG64 c = 0; QueryThreadCycleTime(GetCurrentThread(), &c); return c; }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double median(double *v, int n) { qsort(v, (size_t)n, sizeof v[0], cmpd); return v[n / 2]; }

/* --- the scene: about 500 instances, like the typical app screen ------------------------------- */
static int s_check, s_sel;
static float s_slider = 32;
static char s_name[64] = "base";
static void scene(void) {
    static const float three[3] = { 150, 150, -1 };
    int i;
    gy_begin();
    gy_text("GUICy G0 spike");
    gy_row(3, three);
    gy_button("Build"); gy_button("Stop"); gy_button("Open folder");
    gy_row(0, NULL);
    gy_check("Pixel look", &s_check);
    gy_slider("Size", &s_slider, 8, 512);
    gy_edit("Name", s_name, sizeof s_name);
    for (i = 0; i < 12; i++) if (gy_select(i & 1 ? "Medium" : "Small", s_sel == i)) s_sel = i;
    for (i = 0; i < 14; i++) gy_text("Line %d of text so the frame has about five hundred instances to draw", i + 1);
    gy_end();
}

/* the CPU path's frames, CPU cycles of gy_end only (the UI pass is the same on both paths) */
static double cpu_case(int which) {
    double run[9], fr[200];
    int r, f;
    for (r = 0; r < 9; r++) {
        gy_offscreen(1540, 950);
        gy_input_time(1000);
        scene(); scene();
        if (which == 0) gy__c.focus = gy__hash("Name", 0);                 /* the edit box has the keys */
        for (f = 0; f < 200; f++) {
            ULONG64 c0;
            if (which == 0) { gy_input_char(f & 1 ? 'a' : 'b'); }              /* typing: one char into the focused edit */
            else if (which == 1) gy_input_mouse(f & 1 ? 80 : 1400, f & 1 ? 56 : 60);   /* hover on and off a button */
            else gy_tiles(0);                                                 /* full redraw */
            gy_begin(); /* the UI pass, not timed */
            {
                static const float three[3] = { 150, 150, -1 };
                int i;
                gy_text("GUICy G0 spike"); gy_row(3, three); gy_button("Build"); gy_button("Stop"); gy_button("Open folder");
                gy_row(0, NULL); gy_check("Pixel look", &s_check); gy_slider("Size", &s_slider, 8, 512); gy_edit("Name", s_name, sizeof s_name);
                for (i = 0; i < 12; i++) gy_select(i & 1 ? "Medium" : "Small", s_sel == i);
                for (i = 0; i < 14; i++) gy_text("Line %d of text so the frame has about five hundred instances to draw", i + 1);
            }
            c0 = tcycles(); gy_end(); fr[f] = cycles_ms(tcycles() - c0);
        }
        run[r] = median(fr, 200);
        gy_shutdown();
    }
    return median(run, 9);
}

int main(int argc, char **argv) {
    WCHAR name[64]; DWORD need = 0;
    HMODULE dc;
    compile_fn compile;
    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    IDXGISwapChain1 *sc = NULL; IDXGIFactory2 *fac = NULL; IDXGIDevice *dxd = NULL; IDXGIAdapter *ad = NULL;
    ID3D11VertexShader *vs = NULL; ID3D11PixelShader *ps = NULL;
    ID3D11Buffer *ib = NULL, *cb = NULL; ID3D11ShaderResourceView *srv = NULL; ID3D11RenderTargetView *rtv = NULL; ID3D11Texture2D *bb = NULL;
    ID3D11Query *qdis = NULL, *q0 = NULL, *q1 = NULL;
    HWND parent, child;
    D3D_FEATURE_LEVEL fl;
    int r, f, n, occluded = 0, presents = 0;
    double run_cpu[9], run_gpu[9], fr_cpu[200], fr_gpu[200], cpu_typing, cpu_hover, cpu_full, gpu_cpu, gpu_gpu, hashpass;
    char adapter[128] = "?";
    (void)argc; (void)argv;

    /* never on the user's desktop: run again on a desktop of its own */
    GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, name, sizeof name, &need);
    if (!lstrcmpiW(name, L"Default")) {
        STARTUPINFOW si; PROCESS_INFORMATION pi; DWORD code = 1; WCHAR dn[] = L"gyspike";
        HDESK d = CreateDesktopW(dn, NULL, NULL, 0, GENERIC_ALL, NULL);
        memset(&si, 0, sizeof si); si.cb = sizeof si; si.lpDesktop = dn; si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError = GetStdHandle(STD_ERROR_HANDLE); si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        fflush(stdout);
        if (!d || !CreateProcessW(NULL, GetCommandLineW(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { printf("gyspike: could not start on a test desktop\n"); return 1; }
        WaitForSingleObject(pi.hProcess, 600000); GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseDesktop(d);
        return (int)code;
    }

    { /* cycles per ms of this thread's clock, from QPC over 200 ms */
        LARGE_INTEGER q0_, q1_, fq; ULONG64 c0 = tcycles(); volatile double x = 0; int k;
        QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&q0_);
        do { for (k = 0; k < 100000; k++) x += k; QueryPerformanceCounter(&q1_); } while ((q1_.QuadPart - q0_.QuadPart) * 1000 / fq.QuadPart < 200);
        g_cyc_per_ms = (double)(tcycles() - c0) / ((double)(q1_.QuadPart - q0_.QuadPart) * 1000.0 / (double)fq.QuadPart);
    }

    cpu_typing = cpu_case(0); cpu_hover = cpu_case(1); cpu_full = cpu_case(2);

    /* --- D3D11: a parent window with a child the swap chain lives on (as G1 plans) ------------------ */
    {
        WNDCLASSW wc; RECT rc = { 0, 0, 1540, 950 };
        memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"gyspike";
        RegisterClassW(&wc);
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        parent = CreateWindowExW(0, L"gyspike", L"gyspike", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, wc.hInstance, NULL);
        child = CreateWindowExW(0, L"gyspike", L"", WS_CHILD | WS_VISIBLE, 0, 0, 1540, 950, parent, NULL, wc.hInstance, NULL);
    }
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx))) { printf("gyspike: no hardware D3D11 device\n"); return 1; }
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dxd);
    IDXGIDevice_GetAdapter(dxd, &ad);
    { DXGI_ADAPTER_DESC d; IDXGIAdapter_GetDesc(ad, &d); WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, adapter, sizeof adapter, NULL, NULL); }
    IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fac);
    {
        DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof d);
        d.Width = 1540; d.Height = 950; d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (FAILED(IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown *)dev, child, &d, NULL, NULL, &sc))) { printf("gyspike: no swap chain\n"); return 1; }
    }
    IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
    ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
    dc = LoadLibraryA("d3dcompiler_47.dll");
    compile = dc ? (compile_fn)(void *)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!compile || FAILED(compile(HLSL, strlen(HLSL), "spike", NULL, NULL, "vs", "vs_5_0", 0, 0, &vsb, &err))
        || FAILED(compile(HLSL, strlen(HLSL), "spike", NULL, NULL, "ps", "ps_5_0", 0, 0, &psb, &err))) {
        printf("gyspike: shader compile failed: %s\n", err ? (const char *)ID3D10Blob_GetBufferPointer(err) : "no d3dcompiler_47.dll"); return 1;
    }
    ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), NULL, &vs);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(psb), ID3D10Blob_GetBufferSize(psb), NULL, &ps);
    {
        D3D11_BUFFER_DESC b; D3D11_SHADER_RESOURCE_VIEW_DESC v; float inv[4] = { 1.0f / 1540, 1.0f / 950, 0, 0 }; D3D11_SUBRESOURCE_DATA sd;
        memset(&b, 0, sizeof b); b.ByteWidth = 48 * 4096; b.Usage = D3D11_USAGE_DYNAMIC; b.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; b.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; b.StructureByteStride = 48;
        ID3D11Device_CreateBuffer(dev, &b, NULL, &ib);
        memset(&v, 0, sizeof v); v.Format = DXGI_FORMAT_UNKNOWN; v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; v.Buffer.NumElements = 4096;
        ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)ib, &v, &srv);
        memset(&b, 0, sizeof b); b.ByteWidth = 16; b.Usage = D3D11_USAGE_IMMUTABLE; b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        sd.pSysMem = inv; sd.SysMemPitch = 0; sd.SysMemSlicePitch = 0;
        ID3D11Device_CreateBuffer(dev, &b, &sd, &cb);
    }
    {
        D3D11_QUERY_DESC q; q.MiscFlags = 0;
        q.Query = D3D11_QUERY_TIMESTAMP_DISJOINT; ID3D11Device_CreateQuery(dev, &q, &qdis);
        q.Query = D3D11_QUERY_TIMESTAMP; ID3D11Device_CreateQuery(dev, &q, &q0); ID3D11Device_CreateQuery(dev, &q, &q1);
    }

    /* the instance list a GPU frame would upload: the same scene, made off screen */
    gy_offscreen(1540, 950); gy_input_time(1000); scene(); scene();
    n = gy__c.ninst;
    { /* the tile hash pass a GPU frame still pays (to know whether anything changed at all) */
        double hp[200];
        for (f = 0; f < 200; f++) { ULONG64 c0; gy_begin(); gy__c.inst = gy__c.inst; scene(); c0 = tcycles(); gy_end(); hp[f] = cycles_ms(tcycles() - c0); }
        hashpass = median(hp, 200);   /* an unchanged frame's gy_end is the hash pass and nothing else */
    }
    scene();
    for (r = 0; r < 9; r++) {
        double g_ms[200];
        for (f = 0; f < 200; f++) {
            D3D11_MAPPED_SUBRESOURCE m; D3D11_VIEWPORT vp = { 0, 0, 1540, 950, 0, 1 }; float clear[4] = { 0.12f, 0.12f, 0.13f, 1 };
            UINT64 t0 = 0, t1 = 0; D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj; HRESULT hr; ULONG64 c0 = tcycles();
            ID3D11DeviceContext_Begin(ctx, (ID3D11Asynchronous *)qdis);
            ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)q0);
            ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
            memcpy(m.pData, gy__c.inst, (size_t)n * 48);
            ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)ib, 0);
            ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
            ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear);
            ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
            ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0); ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
            ID3D11DeviceContext_VSSetShaderResources(ctx, 0, 1, &srv); ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &cb);
            ID3D11DeviceContext_DrawInstanced(ctx, 4, (UINT)n, 0, 0);
            ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)q1);
            ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)qdis);
            hr = IDXGISwapChain1_Present(sc, 1, 0);
            fr_gpu[f] = cycles_ms(tcycles() - c0);   /* the CPU cost of submitting and presenting */
            presents++; if (hr == DXGI_STATUS_OCCLUDED) occluded++;
            ID3D11DeviceContext_OMSetRenderTargets(ctx, 0, NULL, NULL);
            while (ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)qdis, &dj, sizeof dj, 0) == S_FALSE) {}
            ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)q0, &t0, sizeof t0, 0);
            ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)q1, &t1, sizeof t1, 0);
            g_ms[f] = dj.Disjoint || !dj.Frequency ? 0 : (double)(t1 - t0) * 1000.0 / (double)dj.Frequency;
        }
        run_cpu[r] = median(fr_gpu, 200) + hashpass;
        run_gpu[r] = median(g_ms, 200);
        (void)fr_cpu;
    }
    gpu_cpu = median(run_cpu, 9); gpu_gpu = median(run_gpu, 9);
    {
        double lo = run_cpu[0], hi = run_cpu[8];
        printf("GPU: %s; %d instances a frame; %d presents, %d came back occluded (the test desktop is never shown)\n\n", adapter, n, presents, occluded);
        printf("| Running case | CPU path, CPU ms | GPU path, CPU ms (hash pass + Map + Draw + Present) | GPU time on the GPU | winner |\n|---|---|---|---|---|\n");
        printf("| typing one character | %.3f | %.3f | %.3f | %s |\n", cpu_typing, gpu_cpu, gpu_gpu, gpu_cpu + gpu_gpu < cpu_typing ? "GPU" : "**CPU**");
        printf("| hover on and off a button | %.3f | %.3f | %.3f | %s |\n", cpu_hover, gpu_cpu, gpu_gpu, gpu_cpu + gpu_gpu < cpu_hover ? "GPU" : "**CPU**");
        printf("| full redraw | %.3f | %.3f | %.3f | %s |\n", cpu_full, gpu_cpu, gpu_gpu, gpu_cpu + gpu_gpu < cpu_full ? "**GPU**" : "**CPU**");
        printf("\nGPU path CPU ms per run (9 runs, sorted): %.3f .. %.3f; the hash pass alone %.3f ms. 200 frames a run, medians.\n", lo, hi, hashpass);
        printf("A GPU 'win' would still have to beat its present cost on a shown window (critic r3 R1): Present on this hidden desktop may skip the flip.\n");
    }
    return 0;
}

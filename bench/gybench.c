/* gybench - step 5 of PLAN.md: the kernel race. Hand-written x86-64 assembly against GUICy's C
   (clang intrinsics) for the hot pixel kernels. An asm version stays only if it gives the same
   bytes on the same input and is at least 5 % faster at the median on this PC.

     gybench            prints the race table (markdown) and the verdicts

   Runs hidden (a console program, no window). It pins itself to one core while timing, does not
   change its priority, and needs about 10 s. */
#define GUICY_IMPLEMENTATION
#include "../guicy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================================
   The asm kernels (Windows x64: args in rcx, rdx, r8, r9; ymm0-5 are free, xmm6+ are saved).
   Each processes whole blocks and returns how many pixels it did; C finishes the tail.
   ============================================================================================ */
__asm__(
    ".intel_syntax noprefix\n"
    ".section .rdata,\"dr\"\n"
    ".p2align 5\n"
    "gyasm_c128: .rept 16\n .short 128\n .endr\n"
    "gyasm_c255: .rept 16\n .short 255\n .endr\n"
    ".text\n"

    /* int gyasm_fill_stosd(uint32_t *d, int n, uint32_t s): the string instruction (fast with ERMSB) */
    ".globl gyasm_fill_stosd\n"
    "gyasm_fill_stosd:\n"
    "    mov r9, rdi\n"
    "    mov rdi, rcx\n"
    "    mov ecx, edx\n"
    "    mov eax, r8d\n"
    "    rep stosd\n"
    "    mov rdi, r9\n"
    "    mov eax, edx\n"
    "    ret\n"

    /* int gyasm_fill_avx(uint32_t *d, int n, uint32_t s): 32 pixels a loop, 4 stores */
    ".globl gyasm_fill_avx\n"
    "gyasm_fill_avx:\n"
    "    vmovd xmm0, r8d\n"
    "    vpbroadcastd ymm0, xmm0\n"
    "    mov eax, edx\n"
    "    and eax, -32\n"
    "    lea r10, [rcx + rax*4]\n"
    "1:  cmp rcx, r10\n"
    "    jae 2f\n"
    "    vmovdqu [rcx], ymm0\n"
    "    vmovdqu [rcx+32], ymm0\n"
    "    vmovdqu [rcx+64], ymm0\n"
    "    vmovdqu [rcx+96], ymm0\n"
    "    add rcx, 128\n"
    "    jmp 1b\n"
    "2:  vzeroupper\n"
    "    ret\n"

    /* int gyasm_blend_avx(uint32_t *d, int n, uint32_t s): premultiplied s over 8 pixels a loop */
    ".globl gyasm_blend_avx\n"
    "gyasm_blend_avx:\n"
    "    vpxor ymm0, ymm0, ymm0\n"
    "    vmovd xmm4, r8d\n"
    "    vpbroadcastd ymm4, xmm4\n"
    "    vpunpcklbw ymm4, ymm4, ymm0\n"          /* s in 16-bit lanes */
    "    vpshuflw ymm5, ymm4, 0xFF\n"
    "    vpshufhw ymm5, ymm5, 0xFF\n"            /* its alpha in every lane */
    "    vmovdqa ymm3, [rip + gyasm_c255]\n"
    "    vpsubw ymm5, ymm3, ymm5\n"              /* 255 - alpha */
    "    mov eax, edx\n"
    "    and eax, -8\n"
    "    lea r10, [rcx + rax*4]\n"
    "1:  cmp rcx, r10\n"
    "    jae 2f\n"
    "    vmovdqu ymm1, [rcx]\n"
    "    vpunpcklbw ymm2, ymm1, ymm0\n"
    "    vpunpckhbw ymm3, ymm1, ymm0\n"
    "    vpmullw ymm2, ymm2, ymm5\n"
    "    vpmullw ymm3, ymm3, ymm5\n"
    "    vpaddw ymm2, ymm2, [rip + gyasm_c128]\n"
    "    vpaddw ymm3, ymm3, [rip + gyasm_c128]\n"
    "    vpsrlw ymm1, ymm2, 8\n"
    "    vpaddw ymm2, ymm2, ymm1\n"
    "    vpsrlw ymm1, ymm3, 8\n"
    "    vpaddw ymm3, ymm3, ymm1\n"
    "    vpsrlw ymm2, ymm2, 8\n"
    "    vpsrlw ymm3, ymm3, 8\n"
    "    vpaddw ymm2, ymm2, ymm4\n"
    "    vpaddw ymm3, ymm3, ymm4\n"
    "    vpackuswb ymm2, ymm2, ymm3\n"
    "    vmovdqu [rcx], ymm2\n"
    "    add rcx, 32\n"
    "    jmp 1b\n"
    "2:  vzeroupper\n"
    "    ret\n"

    /* int gyasm_row_avx(uint32_t *d, const uint32_t *s, int n): each s[i] over d[i] */
    ".globl gyasm_row_avx\n"
    "gyasm_row_avx:\n"
    "    vpxor ymm0, ymm0, ymm0\n"
    "    mov eax, r8d\n"
    "    and eax, -8\n"
    "    lea r10, [rcx + rax*4]\n"
    "1:  cmp rcx, r10\n"
    "    jae 2f\n"
    "    vmovdqu ymm1, [rdx]\n"                  /* s */
    "    vpunpcklbw ymm4, ymm1, ymm0\n"          /* s lo, 16-bit */
    "    vpunpckhbw ymm5, ymm1, ymm0\n"          /* s hi */
    "    vmovdqu ymm1, [rcx]\n"                  /* d */
    "    vpunpcklbw ymm2, ymm1, ymm0\n"
    "    vpunpckhbw ymm3, ymm1, ymm0\n"
    "    vpshuflw ymm1, ymm4, 0xFF\n"
    "    vpshufhw ymm1, ymm1, 0xFF\n"
    "    vpsubw ymm1, ymm1, [rip + gyasm_c255]\n" /* alpha - 255 = -(255 - alpha); fixed below */
    "    vpsignw ymm1, ymm1, [rip + gyasm_cneg]\n"
    "    vpmullw ymm2, ymm2, ymm1\n"
    "    vpshuflw ymm1, ymm5, 0xFF\n"
    "    vpshufhw ymm1, ymm1, 0xFF\n"
    "    vpsubw ymm1, ymm1, [rip + gyasm_c255]\n"
    "    vpsignw ymm1, ymm1, [rip + gyasm_cneg]\n"
    "    vpmullw ymm3, ymm3, ymm1\n"
    "    vpaddw ymm2, ymm2, [rip + gyasm_c128]\n"
    "    vpaddw ymm3, ymm3, [rip + gyasm_c128]\n"
    "    vpsrlw ymm1, ymm2, 8\n"
    "    vpaddw ymm2, ymm2, ymm1\n"
    "    vpsrlw ymm1, ymm3, 8\n"
    "    vpaddw ymm3, ymm3, ymm1\n"
    "    vpsrlw ymm2, ymm2, 8\n"
    "    vpsrlw ymm3, ymm3, 8\n"
    "    vpaddw ymm2, ymm2, ymm4\n"
    "    vpaddw ymm3, ymm3, ymm5\n"
    "    vpackuswb ymm2, ymm2, ymm3\n"
    "    vmovdqu [rcx], ymm2\n"
    "    add rcx, 32\n"
    "    add rdx, 32\n"
    "    jmp 1b\n"
    "2:  vzeroupper\n"
    "    ret\n"

    /* int gyasm_glyph_avx(uint32_t *d, const unsigned char *cov, int n, uint32_t col) */
    ".globl gyasm_glyph_avx\n"
    "gyasm_glyph_avx:\n"
    "    sub rsp, 56\n"
    "    vmovdqu [rsp], xmm6\n"
    "    vmovdqu [rsp+16], xmm7\n"
    "    vmovdqu [rsp+32], xmm8\n"
    "    vpxor ymm0, ymm0, ymm0\n"
    "    vmovd xmm6, r9d\n"
    "    vpbroadcastd ymm6, xmm6\n"
    "    vpunpcklbw ymm6, ymm6, ymm0\n"          /* the colour in 16-bit lanes */
    "    vmovdqa ymm7, [rip + gyasm_c128]\n"
    "    vmovdqa ymm8, [rip + gyasm_c255]\n"
    "    mov eax, r8d\n"
    "    and eax, -8\n"
    "    lea r10, [rcx + rax*4]\n"
    "1:  cmp rcx, r10\n"
    "    jae 3f\n"
    "    mov r11, [rdx]\n"
    "    test r11, r11\n"
    "    jz 2f\n"
    "    vmovq xmm1, r11\n"
    "    vpunpcklbw xmm1, xmm1, xmm1\n"
    "    vpunpcklwd xmm2, xmm1, xmm1\n"          /* pixels 0-3, each coverage byte 4 times */
    "    vpunpckhwd xmm1, xmm1, xmm1\n"          /* pixels 4-7 */
    "    vinserti128 ymm2, ymm2, xmm1, 1\n"
    "    vpunpcklbw ymm4, ymm2, ymm0\n"          /* coverage lo */
    "    vpunpckhbw ymm5, ymm2, ymm0\n"          /* coverage hi */
    "    vpmullw ymm4, ymm4, ymm6\n"
    "    vpmullw ymm5, ymm5, ymm6\n"
    "    vpaddw ymm4, ymm4, ymm7\n"
    "    vpaddw ymm5, ymm5, ymm7\n"
    "    vpsrlw ymm1, ymm4, 8\n"
    "    vpaddw ymm4, ymm4, ymm1\n"
    "    vpsrlw ymm1, ymm5, 8\n"
    "    vpaddw ymm5, ymm5, ymm1\n"
    "    vpsrlw ymm4, ymm4, 8\n"                 /* s lo = colour * coverage / 255 */
    "    vpsrlw ymm5, ymm5, 8\n"                 /* s hi */
    "    vmovdqu ymm1, [rcx]\n"
    "    vpunpcklbw ymm2, ymm1, ymm0\n"
    "    vpunpckhbw ymm3, ymm1, ymm0\n"
    "    vpshuflw ymm1, ymm4, 0xFF\n"
    "    vpshufhw ymm1, ymm1, 0xFF\n"
    "    vpsubw ymm1, ymm8, ymm1\n"
    "    vpmullw ymm2, ymm2, ymm1\n"
    "    vpshuflw ymm1, ymm5, 0xFF\n"
    "    vpshufhw ymm1, ymm1, 0xFF\n"
    "    vpsubw ymm1, ymm8, ymm1\n"
    "    vpmullw ymm3, ymm3, ymm1\n"
    "    vpaddw ymm2, ymm2, ymm7\n"
    "    vpaddw ymm3, ymm3, ymm7\n"
    "    vpsrlw ymm1, ymm2, 8\n"
    "    vpaddw ymm2, ymm2, ymm1\n"
    "    vpsrlw ymm1, ymm3, 8\n"
    "    vpaddw ymm3, ymm3, ymm1\n"
    "    vpsrlw ymm2, ymm2, 8\n"
    "    vpsrlw ymm3, ymm3, 8\n"
    "    vpaddw ymm2, ymm2, ymm4\n"
    "    vpaddw ymm3, ymm3, ymm5\n"
    "    vpackuswb ymm2, ymm2, ymm3\n"
    "    vmovdqu [rcx], ymm2\n"
    "2:  add rcx, 32\n"
    "    add rdx, 8\n"
    "    jmp 1b\n"
    "3:  vmovdqu xmm6, [rsp]\n"
    "    vmovdqu xmm7, [rsp+16]\n"
    "    vmovdqu xmm8, [rsp+32]\n"
    "    add rsp, 56\n"
    "    vzeroupper\n"
    "    ret\n"

    ".section .rdata,\"dr\"\n"
    ".p2align 5\n"
    "gyasm_cneg: .rept 16\n .short -1\n .endr\n"
    ".text\n"
    ".att_syntax prefix\n");

int gyasm_fill_stosd(uint32_t *d, int n, uint32_t s);
int gyasm_fill_avx(uint32_t *d, int n, uint32_t s);
int gyasm_blend_avx(uint32_t *d, int n, uint32_t s);
int gyasm_row_avx(uint32_t *d, const uint32_t *s, int n);
int gyasm_glyph_avx(uint32_t *d, const unsigned char *cov, int n, uint32_t col);

/* C wrappers with the kernels' own signatures: asm for the blocks, GUICy's scalar for the tail */
static void a_fill_stosd(uint32_t *d, int n, uint32_t s) { gyasm_fill_stosd(d, n, s); }
static void a_fill_avx(uint32_t *d, int n, uint32_t s) { int k = gyasm_fill_avx(d, n, s); gy__fill_scalar(d + k, n - k, s); }
static void a_blend_avx(uint32_t *d, int n, uint32_t s) { int k = gyasm_blend_avx(d, n, s); gy__fill_scalar(d + k, n - k, s); }
static void a_row_avx(uint32_t *d, const uint32_t *s, int n) { int k = gyasm_row_avx(d, s, n); gy__row_scalar(d + k, s + k, n - k); }
static void a_glyph_avx(uint32_t *d, const unsigned char *c, int n, uint32_t col) { int k = gyasm_glyph_avx(d, c, n, col); gy__glyph_scalar(d + k, c + k, n - k, col); }

/* ============================================================================================ */

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static uint32_t premul(uint32_t c) { uint32_t a = c >> 24; return gy__scale(c & 0xFFFFFF, a) | a << 24; }

#define MAXN 4096
static uint32_t *g_dst, *g_ref, *g_src, *g_orig;
static unsigned char *g_cov;

static double now_ns(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1e9 / (double)f.QuadPart;
}

static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

typedef struct { const char *name; int kind; void *c_fn, *asm_fn; const char *asm_name; uint32_t s; } race;
enum { K_FILL, K_ROW, K_GLYPH };

static void call(int kind, void *fn, uint32_t *d, int n, uint32_t s) {
    if (kind == K_FILL) ((void (*)(uint32_t *, int, uint32_t))fn)(d, n, s);
    else if (kind == K_ROW) ((void (*)(uint32_t *, const uint32_t *, int))fn)(d, g_src, n);
    else ((void (*)(uint32_t *, const unsigned char *, int, uint32_t))fn)(d, g_cov, n, s);
}

/* median ns per pixel over 31 runs; each run repeats the call until about 1 ms has passed */
static double time_kernel(int kind, void *fn, int n, uint32_t s) {
    double t[31];
    int r, reps = 1;
    for (;;) {                                 /* how many calls make about 1 ms */
        double t0 = now_ns(), t1;
        int k;
        for (k = 0; k < reps; k++) { memcpy(g_dst, g_orig, (size_t)n * 4); call(kind, fn, g_dst, n, s); }
        t1 = now_ns();
        if (t1 - t0 > 1e6 || reps > (1 << 24)) break;
        reps *= 2;
    }
    for (r = 0; r < 31; r++) {
        double t0 = now_ns();
        int k;
        for (k = 0; k < reps; k++) { memcpy(g_dst, g_orig, (size_t)n * 4); call(kind, fn, g_dst, n, s); }
        t[r] = (now_ns() - t0) / reps;
    }
    qsort(t, 31, sizeof t[0], cmp_d);
    return t[15];
}

static double time_copy(int n) {               /* the memcpy that resets the buffer, timed alone */
    double t[31];
    int r, reps = 4096, k;
    for (r = 0; r < 31; r++) {
        double t0 = now_ns();
        for (k = 0; k < reps; k++) { memcpy(g_dst, g_orig, (size_t)n * 4); __asm__ volatile("" ::: "memory"); }
        t[r] = (now_ns() - t0) / reps;
    }
    qsort(t, 31, sizeof t[0], cmp_d);
    return t[15];
}

int main(void) {
    static const int sizes[6] = { 16, 128, 256, 512, 1024, 1920 };
    race R[] = {
        { "fill, opaque", K_FILL, (void *)gy__fill_avx2, (void *)a_fill_stosd, "rep stosd", 0 },
        { "fill, opaque", K_FILL, (void *)gy__fill_avx2, (void *)a_fill_avx, "AVX2 asm, 4 stores a loop", 0 },
        { "fill, blended", K_FILL, (void *)gy__fill_avx2, (void *)a_blend_avx, "AVX2 asm", 0 },
        { "row blend", K_ROW, (void *)gy__row_avx2, (void *)a_row_avx, "AVX2 asm", 0 },
        { "glyph blend", K_GLYPH, (void *)gy__glyph_avx2, (void *)a_glyph_avx, "AVX2 asm", 0 },
    };
    int i, j, k, cpu_ok;
    char brand[49] = { 0 };
    unsigned regs[4];
    DWORD_PTR old_mask;
    if (!gy_offscreen(16, 16)) return 1;
    cpu_ok = gy_isa_set(GY_ISA_AVX2) == GY_ISA_AVX2;
    if (!cpu_ok) { printf("this CPU has no AVX2: the asm kernels here need it\n"); return 1; }
    for (i = 0; i < 3; i++) { __get_cpuid(0x80000002u + (unsigned)i, &regs[0], &regs[1], &regs[2], &regs[3]); memcpy(brand + 16 * i, regs, 16); }
    g_dst = (uint32_t *)_aligned_malloc(MAXN * 4 + 64, 64) + 1;   /* +1: unaligned on purpose, like real rows */
    g_ref = (uint32_t *)_aligned_malloc(MAXN * 4 + 64, 64);
    g_src = (uint32_t *)_aligned_malloc(MAXN * 4 + 64, 64);
    g_orig = (uint32_t *)_aligned_malloc(MAXN * 4 + 64, 64);
    g_cov = (unsigned char *)_aligned_malloc(MAXN + 64, 64);
    for (i = 0; i < MAXN; i++) {
        g_orig[i] = premul(rnd() | (rnd() & 0xFF) << 24);
        g_src[i] = premul(rnd() | (i % 7 ? rnd() & 0xFF : 255u) << 24);
        g_cov[i] = (unsigned char)(i % 11 < 3 ? 0 : i % 11 < 5 ? 255 : rnd() & 255);   /* glyph-like: runs of 0 and 255 */
    }
    R[0].s = R[1].s = GY_RGB(0, 150, 150);
    R[2].s = premul(0x80406080u);
    R[4].s = GY_RGB(224, 226, 230);
    old_mask = SetThreadAffinityMask(GetCurrentThread(), 4);       /* one core while timing */

    printf("CPU: %s\n\n", brand);
    printf("| Kernel | asm version | pixels | C (intrinsics) ns/px | asm ns/px | asm vs C | bit-exact | verdict |\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (j = 0; j < (int)(sizeof R / sizeof R[0]); j++) {
        int exact = 1, wins = 0;
        double cm[6], am[6];
        for (k = 0; k < 6; k++) {              /* the same bytes from the same input, at every size and offset */
            int n = sizes[k], off;
            for (off = 0; off < 8; off++) {
                memcpy(g_ref, g_orig, MAXN * 4); memcpy(g_dst, g_orig, MAXN * 4);
                call(R[j].kind, R[j].c_fn, g_ref + off, n - off, R[j].s);
                call(R[j].kind, R[j].asm_fn, g_dst + off, n - off, R[j].s);
                if (memcmp(g_ref, g_dst, MAXN * 4)) exact = 0;
            }
        }
        for (k = 0; k < 6; k++) {
            double cp = time_copy(sizes[k]);
            cm[k] = (time_kernel(R[j].kind, R[j].c_fn, sizes[k], R[j].s) - cp) / sizes[k];
            am[k] = (time_kernel(R[j].kind, R[j].asm_fn, sizes[k], R[j].s) - cp) / sizes[k];
            if (am[k] < 0.001) am[k] = 0.001;
            if (cm[k] < 0.001) cm[k] = 0.001;
        }
        wins = exact && am[5] <= cm[5] * 0.95; /* the full-row size decides */
        for (k = 0; k < 6; k++)
            printf("| %s | %s | %d | %.3f | %.3f | %+.0f %% | %s | %s |\n", R[j].name, R[j].asm_name, sizes[k], cm[k], am[k],
                   (am[k] / cm[k] - 1) * 100, exact ? "yes" : "NO", k < 5 ? "" : wins ? "**asm kept**" : "C kept, asm dropped");
    }
    SetThreadAffinityMask(GetCurrentThread(), old_mask);
    printf("\nns/px excludes the buffer reset (timed alone and taken off). Median of 31 runs of about 1 ms each, one core.\n");
    printf("An asm version stays only if bit-exact and at least 5 %% faster at 1920 px (one full row).\n");
    return 0;
}

/* gytest - GUICy's own test host. Runs hidden: everything is drawn off screen, no window, no focus.

     gytest              run every test; print PASS/FAIL per test; exit code = number of failures
     gytest --update     write the current pictures as the new goldens (only after looking at them)
     gytest NAME...      run only the named tests

   Each test draws a frame and writes it to work\out\NAME.bmp. The golden is tests\golden\NAME.bmp,
   and the two must be equal byte for byte. */
#define GUICY_IMPLEMENTATION
#include "../guicy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*test_fn)(void);                  /* draws its frame; returns 1 if it could */

static int t_blank(void) {                     /* step 1: an empty frame is the theme's bg, exactly */
    return gy_offscreen(256, 160);
}

static const struct { const char *name; test_fn fn; } TESTS[] = {
    { "blank", t_blank },
};

static char g_root[MAX_PATH];                  /* the repo folder: the parent of the exe's build\ */

static void root_from_exe(void) {
    char *s;
    GetModuleFileNameA(NULL, g_root, MAX_PATH);
    if ((s = strrchr(g_root, '\\'))) *s = 0;   /* ...\build */
    if ((s = strrchr(g_root, '\\'))) *s = 0;   /* ...\guicy */
}

static unsigned char *read_all(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc((size_t)*n + 1);
    if (b && fread(b, 1, (size_t)*n, f) != (size_t)*n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

static int copy_file(const char *from, const char *to) { return CopyFileA(from, to, FALSE) != 0; }

int main(int argc, char **argv) {
    char out[MAX_PATH], gold[MAX_PATH];
    int update = 0, fails = 0, ran = 0, i, a;
    root_from_exe();
    for (a = 1; a < argc; a++) if (!strcmp(argv[a], "--update")) update = 1;
    snprintf(out, sizeof out, "%s\\work", g_root); CreateDirectoryA(out, NULL);
    snprintf(out, sizeof out, "%s\\work\\out", g_root); CreateDirectoryA(out, NULL);

    for (i = 0; i < (int)(sizeof TESTS / sizeof TESTS[0]); i++) {
        long na = 0, nb = 0;
        unsigned char *pa, *pb;
        int want = argc == 1 || (argc == 2 && update);
        for (a = 1; a < argc && !want; a++) if (!strcmp(argv[a], TESTS[i].name)) want = 1;
        if (!want) continue;
        ran++;
        snprintf(out, sizeof out, "%s\\work\\out\\%s.bmp", g_root, TESTS[i].name);
        snprintf(gold, sizeof gold, "%s\\tests\\golden\\%s.bmp", g_root, TESTS[i].name);
        if (!TESTS[i].fn() || !gy_shot(out)) {
            printf("FAIL %s: %s\n", TESTS[i].name, gy_last_error()); fails++; gy_shutdown(); continue;
        }
        gy_shutdown();
        if (update) {
            if (copy_file(out, gold)) printf("UPDATED %s\n", TESTS[i].name);
            else { printf("FAIL %s: could not write %s\n", TESTS[i].name, gold); fails++; }
            continue;
        }
        pa = read_all(out, &na); pb = read_all(gold, &nb);
        if (!pb) { printf("FAIL %s: no golden at %s (look at %s, then run --update)\n", TESTS[i].name, gold, out); fails++; }
        else if (!pa || na != nb || memcmp(pa, pb, (size_t)na)) {
            long diff = 0, k;
            for (k = 0; pa && k < na && k < nb; k++) diff += pa[k] != pb[k];
            printf("FAIL %s: differs from the golden (%ld of %ld bytes; sizes %ld vs %ld)\n", TESTS[i].name, diff, nb, na, nb);
            fails++;
        } else printf("PASS %s\n", TESTS[i].name);
        free(pa); free(pb);
    }
    printf("%d test(s), %d failed\n", ran, fails);
    return fails;
}

/* probe_dwrite - the open question from research/cpu.md: can plain C call DirectWrite through the
   MinGW sysroot's dwrite.h?  It loads dwrite.dll at run time (never linked), makes a factory, and
   counts the system font families through the C vtables. Prints one line; exit 0 = yes. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>

typedef HRESULT (WINAPI *create_fn)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);

/* IID_IDWriteFactory {b859ee5a-d838-4b5b-a2e8-1adc7d93db48}, written here so no GUID library is linked */
static const IID gy_iid_dwrite_factory = { 0xb859ee5a, 0xd838, 0x4b5b, { 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48 } };

int main(void) {
    HMODULE dll = LoadLibraryA("dwrite.dll");
    create_fn create = dll ? (create_fn)(void *)GetProcAddress(dll, "DWriteCreateFactory") : NULL;
    IDWriteFactory *f = NULL;
    IDWriteFontCollection *fonts = NULL;
    UINT32 families, index = 0;
    BOOL has_segoe = FALSE;
    HRESULT hr;
    if (!create) { printf("FAIL probe_dwrite: dwrite.dll or DWriteCreateFactory not found\n"); return 1; }
    hr = create(DWRITE_FACTORY_TYPE_SHARED, &gy_iid_dwrite_factory, (IUnknown **)&f);
    if (FAILED(hr) || !f) { printf("FAIL probe_dwrite: DWriteCreateFactory 0x%08lx\n", (unsigned long)hr); return 1; }
    hr = IDWriteFactory_GetSystemFontCollection(f, &fonts, FALSE);
    if (FAILED(hr)) { printf("FAIL probe_dwrite: GetSystemFontCollection 0x%08lx\n", (unsigned long)hr); return 1; }
    families = IDWriteFontCollection_GetFontFamilyCount(fonts);
    IDWriteFontCollection_FindFamilyName(fonts, L"Segoe UI", &index, &has_segoe);
    printf("PASS probe_dwrite: plain C through dwrite.h works; %u font families, Segoe UI %s\n",
           (unsigned)families, has_segoe ? "found" : "not found");
    IDWriteFontCollection_Release(fonts);
    IDWriteFactory_Release(f);
    return 0;
}

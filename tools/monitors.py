"""List the monitors with their real pixel size, DPI, and the scale GUICy's D1 rule would pick:
max(DPI/96, height/1080), rounded to 25 % steps. Opens no window."""
import ctypes, ctypes.wintypes as W

u32 = ctypes.WinDLL("user32")
shcore = ctypes.WinDLL("shcore")
u32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))  # per-monitor v2: real pixels, real DPI


class MONITORINFOEX(ctypes.Structure):
    _fields_ = [("cbSize", W.DWORD), ("rcMonitor", W.RECT), ("rcWork", W.RECT), ("dwFlags", W.DWORD), ("szDevice", W.WCHAR * 32)]


rows = []
PROC = ctypes.WINFUNCTYPE(W.BOOL, W.HMONITOR, W.HDC, ctypes.POINTER(W.RECT), W.LPARAM)


def cb(hmon, hdc, rect, lp):
    mi = MONITORINFOEX(); mi.cbSize = ctypes.sizeof(mi)
    u32.GetMonitorInfoW(hmon, ctypes.byref(mi))
    dx, dy = W.UINT(), W.UINT()
    shcore.GetDpiForMonitor(hmon, 0, ctypes.byref(dx), ctypes.byref(dy))  # MDT_EFFECTIVE_DPI
    w = mi.rcMonitor.right - mi.rcMonitor.left
    h = mi.rcMonitor.bottom - mi.rcMonitor.top
    f = max(dx.value / 96, h / 1080)
    f = round(f * 4) / 4
    rows.append((mi.szDevice, w, h, dx.value, round(dx.value / 96 * 100), round(f * 100), bool(mi.dwFlags & 1)))
    return True


u32.EnumDisplayMonitors(None, None, PROC(cb), 0)
print("| Monitor | Pixels | Windows DPI | Windows scale | GUICy D1 scale | Main |")
print("|---|---|---|---|---|---|")
for d, w, h, dpi, ws, gs, main in rows:
    print(f"| {d} | {w}x{h} | {dpi} | {ws} % | {gs} % | {'yes' if main else ''} |")

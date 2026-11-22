"""GUICy step 0: measure an existing GUI exe from the outside, hidden.

The exe is copied into X:\\guicy\\work\\baseline\\<name>\\ (with any extra files given) and run
there with SW_HIDE, so the app's own folder is never touched and no window is shown.

Measures:
  start_ms   launch -> the process owns a top-level window
  idle_cpu   process CPU time over IDLE_S seconds with no input, as % of one core and ms
  move_ms    CPU ms per WM_MOUSEMOVE, from N moves posted to the hidden window
  ws_mb      peak working set

usage: python -I baseline.py NAME EXE [EXTRA_FILE ...]
prints one JSON line.
"""
import ctypes, ctypes.wintypes as W, json, os, shutil, subprocess, sys, time

IDLE_S = int(os.environ.get("GY_IDLE_S", "60"))
MOVES = 600
MOVE_GAP = 1 / 60

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
u32 = ctypes.WinDLL("user32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)


class PMC(ctypes.Structure):
    _fields_ = [("cb", W.DWORD), ("PageFaultCount", W.DWORD), ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t)]


def cpu_ms(h):
    c, e, k, u = W.FILETIME(), W.FILETIME(), W.FILETIME(), W.FILETIME()
    if not k32.GetProcessTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(k), ctypes.byref(u)):
        raise OSError(ctypes.get_last_error(), "GetProcessTimes")
    f = lambda t: (t.dwHighDateTime << 32 | t.dwLowDateTime) / 10000.0
    return f(k) + f(u)


DESK = "guicy_bench"


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [("cb", W.DWORD), ("lpReserved", W.LPWSTR), ("lpDesktop", W.LPWSTR), ("lpTitle", W.LPWSTR),
                ("dwX", W.DWORD), ("dwY", W.DWORD), ("dwXSize", W.DWORD), ("dwYSize", W.DWORD),
                ("dwXCountChars", W.DWORD), ("dwYCountChars", W.DWORD), ("dwFillAttribute", W.DWORD),
                ("dwFlags", W.DWORD), ("wShowWindow", W.WORD), ("cbReserved2", W.WORD),
                ("lpReserved2", ctypes.c_void_p), ("hStdInput", W.HANDLE), ("hStdOutput", W.HANDLE),
                ("hStdError", W.HANDLE)]


class PROCINFO(ctypes.Structure):
    _fields_ = [("hProcess", W.HANDLE), ("hThread", W.HANDLE), ("dwProcessId", W.DWORD), ("dwThreadId", W.DWORD)]


u32.CreateDesktopW.restype = W.HANDLE
k32.CreateProcessW.argtypes = [W.LPCWSTR, W.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, W.BOOL, W.DWORD,
                               ctypes.c_wchar_p, W.LPCWSTR, ctypes.c_void_p, ctypes.c_void_p]
u32.EnumDesktopWindows.argtypes = [W.HANDLE, ctypes.c_void_p, W.LPARAM]


def windows_of(pid, desk):
    found = []
    PROC = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)

    def cb(hwnd, _):
        p = W.DWORD()
        u32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        r = W.RECT()
        # top-level, unowned (GW_OWNER == 4), visible on its desktop, bigger than a helper window
        if p.value == pid and not u32.GetWindow(hwnd, 4) and u32.IsWindowVisible(hwnd) \
                and u32.GetClientRect(hwnd, ctypes.byref(r)) and r.right * r.bottom > 10000:
            found.append((r.right * r.bottom, hwnd))
        return True
    cbp = PROC(cb)
    u32.EnumDesktopWindows(desk, cbp, 0)
    return [h for _, h in sorted(found, reverse=True)]


def run_hidden(name, args, timeout_s=180):
    """--run NAME EXE ARG...: run an exe already copied into work/baseline/NAME on the hidden desktop
    and wait for it to exit (for the apps' own --selftest picture runs)."""
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    run_dir = os.path.join(root, "work", "baseline", name)
    tmp = os.path.join(root, "work", "tmp")
    env = dict(os.environ, TEMP=tmp, TMP=tmp)
    desk = u32.CreateDesktopW(DESK, None, None, 0, 0x10000000, None)
    si = STARTUPINFOW(); si.cb = ctypes.sizeof(si); si.lpDesktop = DESK
    pi = PROCINFO()
    block = "".join("%s=%s\0" % kv for kv in env.items()) + "\0"
    exe = os.path.join(run_dir, args[0])
    cmd = subprocess.list2cmdline([exe] + args[1:])
    if not k32.CreateProcessW(exe, cmd, None, None, False, 0x00000400, block, run_dir, ctypes.byref(si), ctypes.byref(pi)):
        raise OSError(ctypes.get_last_error(), "CreateProcessW")
    k32.CloseHandle(pi.hThread)
    t0 = time.perf_counter()
    r = k32.WaitForSingleObject(pi.hProcess, timeout_s * 1000)
    code = W.DWORD(); k32.GetExitCodeProcess(pi.hProcess, ctypes.byref(code))
    if r != 0:
        k32.TerminateProcess(pi.hProcess, 1)
    k32.CloseHandle(pi.hProcess); u32.CloseDesktop(desk)
    return {"app": name, "run": args, "exit": code.value, "timed_out": r != 0, "s": round(time.perf_counter() - t0, 1)}


def main():
    if sys.argv[1] == "--run":
        return run_hidden(sys.argv[2], sys.argv[3:])
    name, exe, extras = sys.argv[1], sys.argv[2], sys.argv[3:]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    run_dir = os.path.join(root, "work", "baseline", name)
    os.makedirs(run_dir, exist_ok=True)
    dst = os.path.join(run_dir, os.path.basename(exe))
    shutil.copy2(exe, dst)
    for x in extras:
        shutil.copy2(x, os.path.join(run_dir, os.path.basename(x)))
    tmp = os.path.join(root, "work", "tmp")
    os.makedirs(tmp, exist_ok=True)
    env = dict(os.environ, TEMP=tmp, TMP=tmp)

    # The app runs on its own desktop, which is never switched to, so even a window created with
    # WS_VISIBLE never reaches the user's screen and never takes focus.
    desk = u32.CreateDesktopW(DESK, None, None, 0, 0x10000000, None)  # GENERIC_ALL
    if not desk:
        raise OSError(ctypes.get_last_error(), "CreateDesktopW")
    si = STARTUPINFOW(); si.cb = ctypes.sizeof(si); si.lpDesktop = DESK
    pi = PROCINFO()
    block = "".join("%s=%s\0" % kv for kv in env.items()) + "\0"
    t0 = time.perf_counter()
    if not k32.CreateProcessW(dst, None, None, None, False, 0x00000400, block, run_dir,
                              ctypes.byref(si), ctypes.byref(pi)):  # CREATE_UNICODE_ENVIRONMENT
        raise OSError(ctypes.get_last_error(), "CreateProcessW")
    h, pid = pi.hProcess, pi.dwProcessId
    k32.CloseHandle(pi.hThread)
    alive = lambda: k32.WaitForSingleObject(h, 0) == 0x102  # WAIT_TIMEOUT
    out = {"app": name, "exe_kb": os.path.getsize(exe) // 1024, "desktop": DESK}
    try:
        hw = []
        while time.perf_counter() - t0 < 30 and alive():
            hw = windows_of(pid, desk)
            if hw:
                break
            time.sleep(0.005)
        if not hw:
            out["error"] = "no window within 30 s" if alive() else "exited early"
            return out
        out["start_ms"] = round((time.perf_counter() - t0) * 1000)
        out["visible"] = bool(u32.IsWindowVisible(hw[0]))
        time.sleep(5)  # let it settle

        c0 = cpu_ms(h)
        time.sleep(IDLE_S)
        idle = cpu_ms(h) - c0
        out["idle_cpu_ms_per_60s"] = round(idle * 60 / IDLE_S, 1)
        out["idle_cpu_pct_core"] = round(idle / (IDLE_S * 1000) * 100, 3)

        r = W.RECT()
        u32.GetClientRect(hw[0], ctypes.byref(r))
        cw, ch = max(r.right, 200), max(r.bottom, 200)
        c0 = cpu_ms(h)
        for i in range(MOVES):
            x = int(cw * (0.1 + 0.8 * ((i * 37) % 100) / 100))
            y = int(ch * (0.1 + 0.8 * ((i * 53) % 100) / 100))
            u32.PostMessageW(hw[0], 0x0200, 0, (y << 16) | (x & 0xFFFF))  # WM_MOUSEMOVE
            time.sleep(MOVE_GAP)
        time.sleep(1)
        busy = cpu_ms(h) - c0 - idle / IDLE_S * (MOVES * MOVE_GAP + 1)
        out["move_cpu_ms_each"] = round(busy / MOVES, 3)
        out["client"] = [r.right, r.bottom]

        pmc = PMC(); pmc.cb = ctypes.sizeof(PMC)
        psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb)
        out["peak_ws_mb"] = round(pmc.PeakWorkingSetSize / 1048576, 1)
        return out
    finally:
        if alive():
            k32.TerminateProcess(h, 1)
            k32.WaitForSingleObject(h, 10000)
        k32.CloseHandle(h)
        u32.CloseDesktop(desk)


if __name__ == "__main__":
    print(json.dumps(main()))

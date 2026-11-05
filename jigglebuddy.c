/*
 * jigglebuddy.c - a tiny tray-icon mouse jiggler for Windows.
 *
 * Keeps the PC awake and your status "active" by nudging the mouse when you
 * have been idle. No window, no CRT, no dependencies beyond user32/kernel32/
 * shell32/gdi32. Builds to a few KB.
 *
 *   Left-click tray icon : toggle on/off
 *   Right-click          : menu (Enabled / Exit)
 *   Command line         : jigglebuddy.exe [seconds]   (idle interval, default 60)
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>

#define WM_TRAY      (WM_APP + 1)
#define ID_TIMER     1
#define ID_TOGGLE    100
#define ID_EXIT      101

static NOTIFYICONDATAW g_nid;
static HICON g_iconOn, g_iconOff;
static UINT  g_taskbarCreated;
static BOOL  g_enabled = TRUE;
static DWORD g_intervalMs = 60 * 1000;

/* Built without the CRT, so the compiler may still emit calls to these. */
void *memset(void *d, int c, size_t n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

static void copyStr(WCHAR *dst, const WCHAR *src, int max)
{
    int i = 0;
    for (; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

/* Draw a small round icon at runtime: green when on, grey when off. */
static HICON makeIcon(COLORREF rgb)
{
    int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, cx, cy);
    HBITMAP mask  = CreateBitmap(cx, cy, 1, 1, NULL);
    HBRUSH  brush = CreateSolidBrush(rgb);
    ICONINFO ii = { TRUE, 0, 0, mask, color };
    HICON icon;

    /* Mask: white = transparent, black = opaque. */
    SelectObject(dc, mask);
    PatBlt(dc, 0, 0, cx, cy, WHITENESS);
    SelectObject(dc, GetStockObject(BLACK_BRUSH));
    SelectObject(dc, GetStockObject(BLACK_PEN));
    Ellipse(dc, 1, 1, cx - 1, cy - 1);

    /* Colour plane. */
    SelectObject(dc, color);
    PatBlt(dc, 0, 0, cx, cy, BLACKNESS);
    SelectObject(dc, brush);
    Ellipse(dc, 1, 1, cx - 1, cy - 1);

    SelectObject(dc, GetStockObject(NULL_BRUSH));
    icon = CreateIconIndirect(&ii);

    DeleteObject(brush);
    DeleteObject(mask);
    DeleteObject(color);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return icon;
}

static void applyState(void)
{
    /* Ask Windows not to sleep / blank the display while enabled. */
    SetThreadExecutionState(g_enabled
        ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
        : ES_CONTINUOUS);

    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon  = g_enabled ? g_iconOn : g_iconOff;
    copyStr(g_nid.szTip, g_enabled ? L"JiggleBuddy: on" : L"JiggleBuddy: off",
            ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void addTrayIcon(void)
{
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAY;
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    applyState();
}

/* Nudge the pointer 1px and back, then pin it to where it was. */
static void jiggle(void)
{
    INPUT in[2];
    POINT pt;
    GetCursorPos(&pt);

    memset(in, 0, sizeof in);
    in[0].type = in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = in[1].mi.dwFlags = MOUSEEVENTF_MOVE;
    in[0].mi.dx = 1;
    in[1].mi.dx = -1;
    SendInput(2, in, sizeof(INPUT));

    SetCursorPos(pt.x, pt.y);
}

static DWORD idleMs(void)
{
    LASTINPUTINFO li = { sizeof li, 0 };
    GetLastInputInfo(&li);
    return GetTickCount() - li.dwTime;
}

static void showMenu(HWND hwnd)
{
    POINT pt;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_enabled ? MF_CHECKED : 0), ID_TOGGLE, L"&Enabled");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"E&xit");

    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);   /* so the menu closes when clicking away */
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        if (g_enabled && idleMs() >= g_intervalMs - 1000)
            jiggle();
        return 0;

    case WM_TRAY:
        if (lp == WM_LBUTTONUP) {
            g_enabled = !g_enabled;
            applyState();
        } else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            showMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wp) == ID_TOGGLE) {
            g_enabled = !g_enabled;
            applyState();
        } else if (LOWORD(wp) == ID_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        SetThreadExecutionState(ES_CONTINUOUS);
        PostQuitMessage(0);
        return 0;
    }

    /* Explorer restarted: put our icon back. */
    if (msg == g_taskbarCreated && msg != 0) {
        addTrayIcon();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* Optional first argument: idle interval in seconds. */
static DWORD parseSeconds(const WCHAR *cmd)
{
    DWORD n = 0;
    BOOL quoted = FALSE;

    /* Skip program name (possibly quoted). */
    while (*cmd && (quoted || (*cmd != L' ' && *cmd != L'\t'))) {
        if (*cmd == L'"') quoted = !quoted;
        cmd++;
    }
    while (*cmd == L' ' || *cmd == L'\t') cmd++;
    while (*cmd >= L'0' && *cmd <= L'9' && n < 86400)
        n = n * 10 + (DWORD)(*cmd++ - L'0');
    return n;
}

void WINAPI entry(void)
{
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    DWORD secs;
    HANDLE mutex;

    /* One instance only. */
    mutex = CreateMutexW(NULL, TRUE, L"Local\\JiggleBuddy");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(0);

    secs = parseSeconds(GetCommandLineW());
    if (secs >= 5)
        g_intervalMs = secs * 1000;

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_iconOn  = makeIcon(RGB(0x2e, 0xcc, 0x71));
    g_iconOff = makeIcon(RGB(0x80, 0x80, 0x80));

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"JiggleBuddy";
    RegisterClassW(&wc);

    /* Never-shown window: no taskbar button, no UI. (Not HWND_MESSAGE, since
       message-only windows miss the TaskbarCreated broadcast.) */
    hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"JiggleBuddy",
                           0, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

    memset(&g_nid, 0, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd   = hwnd;
    g_nid.uID    = 1;
    addTrayIcon();

    /* Check twice per interval so we jiggle at most ~interval after going idle. */
    SetTimer(hwnd, ID_TIMER, g_intervalMs / 2, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyIcon(g_iconOn);
    DestroyIcon(g_iconOff);
    ReleaseMutex(mutex);
    ExitProcess(0);
}

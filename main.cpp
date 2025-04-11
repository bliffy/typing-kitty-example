// No copyright claimed.
// Released under the MIT License.
// Author: Michael E. Jolley

#include <Windows.h>
#include <windowsx.h> // for GET_X_LPARAM and GET_Y_LPARAM
#include <iomanip>

#define MIN_WIDTH  350
#define MIN_HEIGHT 245
#define BAR_GAP    176
#define APP_NAME   L"PkittyWidget"

enum CTX_MENU_ITEM_IDs {
    ID_CTX_EXIT            = 2000,
    ID_CTX_RESET,
    ID_CTX_LOCK_POSITION,
    ID_CTX_UNLOCK_POSITION,
    ID_TRAY_ICON,
};

enum KITTY_FRAME : int {
    NO_PAWS_DOWN    = 0,
    FIRST_PAW_DOWN  = 1,
    SECOND_PAW_DOWN = 2
};
int g_iFrame = NO_PAWS_DOWN;

HWND       g_hwApp;
HINSTANCE  g_hInstance;
HBITMAP    g_hbmCat;
BITMAP     g_bm;

BOOL g_bShouldQuit    = FALSE;
INT  g_iWindowX       = 0;
INT  g_iWindowY       = 0;
INT  g_iWindowWidth   = MIN_WIDTH;
INT  g_iWindowHeight  = MIN_HEIGHT;
INT  g_iStartBarTop   = 0;
BOOL g_bPositionLock  = FALSE;
BOOL g_bDragging      = FALSE;


constexpr BYTE g_KEYS[30] = {
    0x41,
    0x42,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47,
    0x48,
    0x49,
    0x4A,
    0x4B,
    0x4C,
    0x4D,
    0x4E,
    0x4F,
    0x50,
    0x51,
    0x52,
    0x53,
    0x54,
    0x55,
    0x56,
    0x57,
    0x58,
    0x59,
    0x5A,
    VK_SPACE,
    VK_TAB,
    VK_RETURN,
};
constexpr int g_KEYS_COUNT = 30;
bool g_keyStates[256] = { 0 };

LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

int  Initialize(HINSTANCE& hThisInstance);
void AddTrayIcon(LPCWSTR pszToolTip);
void RemoveTrayIcon(void);
void DrawCat(void);
void MouseDragHelper(long lWindowX, long lWindowY, long lScreenX, long lScreenY, bool reset);
void OnMouseDrag(long lPointerX, long lPointerY, long lDeltaX, long lDeltaY);
void OnRightClick(long lPointerX, long lPointerY);
void ShowPopupMenu(long lCursorX, long lCursorY);
void OnReset(void);
bool CheckKeys(void);


int APIENTRY wWinMain(HINSTANCE _In_ hThisInstance, HINSTANCE _In_opt_ hPrevInstance, PWSTR _In_ lpCmdLine, int _In_ nCmdShow)
{
    if (!Initialize(hThisInstance))
        return 1;

    MSG msg = { 0 };
    ULONGLONG timer = 0;
    ULONGLONG timerDelay = CLOCKS_PER_SEC / 5;
    
    while (!g_bShouldQuit) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_bShouldQuit = TRUE;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else if ( CheckKeys() || GetTickCount64() > (timer + timerDelay)) {
            timer = GetTickCount64();
            DrawCat();
        }
        else {
            Sleep(0); // on Windows, this is the same concept as sched_yield()
        }
    }
    RemoveTrayIcon();
    return msg.wParam;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    LONG x, y;
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        DrawCat();
        break;
    case WM_SIZE:
        break;
    case WM_DPICHANGED:
        g_iWindowWidth  = ((RECT*)lParam)->right  - ((RECT*)lParam)->left;
        g_iWindowHeight = ((RECT*)lParam)->bottom - ((RECT*)lParam)->top;
        MoveWindow(hwnd, g_iWindowX, g_iWindowY, g_iWindowWidth, g_iWindowHeight, true);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_CTX_RESET:
            OnReset();
            break;
        case ID_CTX_LOCK_POSITION:
            g_bPositionLock = TRUE;
            break;
        case ID_CTX_UNLOCK_POSITION:
            g_bPositionLock = FALSE;
            break;
        case ID_CTX_EXIT:
            PostQuitMessage(0);
            break;
        default:
            break;
        }
        break;
    case WM_APP:
        if (lParam == WM_RBUTTONUP && hwnd == g_hwApp) {
            GetCursorPos(&pt);
            OnRightClick(pt.x, pt.y);
        }
        break;
    case WM_LBUTTONDOWN:
        if (!g_bPositionLock) g_bDragging = TRUE;
        x = GET_X_LPARAM(lParam);
        y = GET_Y_LPARAM(lParam);
        GetCursorPos(&pt);
        MouseDragHelper(x, y, pt.x, pt.y, true);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONUP:
        if (hwnd == g_hwApp && g_bDragging) {
            g_bDragging = false;
            ReleaseCapture();
        }
        break;
    case WM_MOUSEMOVE:
        if (hwnd == g_hwApp)
            SetFocus(g_hwApp);
        else if (GetFocus() == g_hwApp)
            SetFocus(g_hwApp);
        if (hwnd == g_hwApp) {
            x = GET_X_LPARAM(lParam);
            y = GET_Y_LPARAM(lParam);
            if (g_bDragging) {
                GetCursorPos(&pt);
                MouseDragHelper(x, y, pt.x, pt.y, false);
            }
        }
        break;
    case WM_RBUTTONDOWN:
        if (hwnd == g_hwApp) {
            SetForegroundWindow(g_hwApp);
            GetCursorPos(&pt);
            OnRightClick(pt.x, pt.y);
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

void DrawCat(void)
{
    HDC hRealDC   = GetDC(g_hwApp);
    HDC hdcMemory = CreateCompatibleDC(hRealDC);
    SelectObject(hdcMemory, g_hbmCat);

    BLENDFUNCTION bfunc = { 0 };
    bfunc.BlendOp = AC_SRC_OVER;
    bfunc.BlendFlags = 0;
    bfunc.SourceConstantAlpha = 0;
    bfunc.AlphaFormat = 0;
    
    POINT pos = { g_iWindowX , g_iWindowY };
    SIZE size = { g_iWindowWidth, g_iWindowHeight };
    POINT bmpOffset = { 350 * g_iFrame, 0 };
    UpdateLayeredWindow(
        g_hwApp,
        hRealDC,
        &pos,
        &size,
        hdcMemory,
        &bmpOffset,
        0x00ff0000,
        &bfunc,
        ULW_COLORKEY);
    MoveWindow(g_hwApp, g_iWindowX, g_iWindowY, g_iWindowWidth, g_iWindowHeight, true);

    ReleaseDC(g_hwApp, hRealDC);
    DeleteDC(hdcMemory);
}

bool CheckKeys(void)
{
    static int prev_paw = SECOND_PAW_DOWN;
    bool changed = false;
    bool any = false;
    bool state;
    for (int i = 0; i < g_KEYS_COUNT; i++) {
        // turn the most-significant-bit returned into a bool state
        state = !!(GetAsyncKeyState(g_KEYS[i]) & (1 << 15));
        changed = changed || (g_keyStates[i] != state);
        any = any || state;
        g_keyStates[i] = state;
    }
    if (!any)
        // no paws on keys
        g_iFrame = NO_PAWS_DOWN;
    else if (changed) {
        if ( (rand() & 0xff) < 200)
            // usually alternate paw
            g_iFrame = 3 - prev_paw;
        else
            // rarely repeat paws
            g_iFrame = prev_paw;
        prev_paw = g_iFrame;
    }
    return changed;
}

int Initialize(HINSTANCE& hThisInstance)
{
    RECT rtAppRect;
    WNDCLASSEX winclApp = { 0 };

    WCHAR szAppClassName[] = APP_NAME;

    winclApp.hInstance = hThisInstance;
    winclApp.lpszClassName = szAppClassName;
    winclApp.lpfnWndProc = WindowProcedure;
    winclApp.style = 0;
    winclApp.cbSize = sizeof(WNDCLASSEX);
    winclApp.hIcon = LoadIcon(hThisInstance, L"APP_ICON_LARGE");
    winclApp.hIconSm = LoadIcon(hThisInstance, L"APP_ICON_SMALL");
    winclApp.hCursor = LoadCursor(NULL, IDC_ARROW);
    winclApp.lpszMenuName = NULL;
    winclApp.cbClsExtra = 0;
    winclApp.cbWndExtra = 0;
    winclApp.hbrBackground = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);

    if (!RegisterClassEx(&winclApp))
        return false;
    
    RECT rtWorkArea;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rtWorkArea, 0)) {
        g_iStartBarTop = rtWorkArea.bottom;
        g_iWindowX = rtWorkArea.right - g_iWindowWidth;
    }
    else
        g_iStartBarTop = GetSystemMetrics(SM_CYMAXIMIZED);
    if (g_iStartBarTop != 0)
        g_iWindowY = g_iStartBarTop - BAR_GAP;
            
    g_hwApp = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // | WS_EX_TRANSPARENT ,
        szAppClassName,
        APP_NAME,
        WS_POPUP | WS_VISIBLE,
        g_iWindowX,
        g_iWindowY,
        g_iWindowWidth,
        g_iWindowHeight,
        HWND_DESKTOP,
        NULL,
        hThisInstance,
        NULL);
    GetClientRect(g_hwApp, &rtAppRect);

    g_hInstance = hThisInstance;

    g_hbmCat = LoadBitmap(hThisInstance, L"CATSPRITE");
    GetObject(g_hbmCat, sizeof(g_bm), &g_bm);

    AddTrayIcon(L"Kitty");

    return true;
}

void MouseDragHelper(long lWindowX, long lWindowY, long lScreenX, long lScreenY, bool reset)
{
    static POINT last_xy = { 0, 0 };
    if (reset) {
        OnMouseDrag(lWindowX, lWindowY, 0, 0);
        last_xy.x = lScreenX;
        last_xy.y = lScreenY;
    } else {
        OnMouseDrag(lWindowX, lWindowY, (lScreenX - last_xy.x), (lScreenY - last_xy.y));
        last_xy.x = lScreenX;
        last_xy.y = lScreenY;
    }
}

void OnMouseDrag(long lPointerX, long lPointerY, long lDeltaX, long lDeltaY)
{
    if (!g_bDragging)
        return;
    RECT rtCurrentRect;
    if (!GetWindowRect(g_hwApp, &rtCurrentRect))
        return;
    g_iWindowX = rtCurrentRect.left + lDeltaX;
    g_iWindowY = rtCurrentRect.top + lDeltaY;
    MoveWindow(g_hwApp, g_iWindowX, g_iWindowY, g_iWindowWidth, g_iWindowHeight, false);
}

void OnRightClick(long lPointerX, long lPointerY)
{
    ShowPopupMenu(lPointerX, lPointerY);
}

void ShowPopupMenu(long lCursorX, long lCursorY)
{
    HMENU hContextMenu = CreatePopupMenu();

    int i = 0;
    InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_RESET, L"Reset Position");
    if ( g_bPositionLock )
        InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_UNLOCK_POSITION, L"Unlock Position");
    else
        InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_LOCK_POSITION, L"Lock Position");
    InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_EXIT, L"Exit");

    SetMenuDefaultItem(hContextMenu, ID_CTX_EXIT, FALSE);

    SetFocus(g_hwApp);
    SendMessage(g_hwApp, WM_INITMENUPOPUP, (WPARAM)hContextMenu, 0);

    DWORD dwFlags = TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY;
    WORD cmd = TrackPopupMenu(
        hContextMenu, dwFlags,
        lCursorX, lCursorY,
        0, g_hwApp, NULL);

    SendMessage(g_hwApp, WM_COMMAND, cmd, 0);
    DestroyMenu(hContextMenu);
}

void OnReset(void)
{
    RECT rtWorkArea;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rtWorkArea, 0)) {
        g_iStartBarTop = rtWorkArea.bottom;
        g_iWindowX = rtWorkArea.right - g_iWindowWidth;
    }
    else
        g_iStartBarTop = GetSystemMetrics(SM_CYMAXIMIZED);
    if (g_iStartBarTop != 0)
        g_iWindowY = g_iStartBarTop - BAR_GAP;

    DrawCat();
}

void AddTrayIcon(LPCWSTR pszToolTip)
{
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = g_hwApp;
    nid.uID              = ID_TRAY_ICON;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP;
    nid.hIcon            = LoadIcon(g_hInstance, L"APP_ICON_SMALL");
    wcscpy_s(nid.szTip, pszToolTip);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(void)
{
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd   = g_hwApp;
    nid.uID    = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

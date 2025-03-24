
#include <Windows.h>
#include <windowsx.h> // for GET_X_LPARAM and GET_Y_LPARAM
#include <iomanip>

#define MIN_WIDTH  350
#define MIN_HEIGHT 245
#define BAR_GAP 176
#define APP_NAME L"PkittyWidget"

enum CTX_MENU_ITEM_IDs {
    ID_CTX_EXIT = 2000,
    ID_CTX_RESET,
    ID_CTX_LOCK_POSITION,
    ID_CTX_UNLOCK_POSITION,
    ID_TRAY_ICON,
};

enum KITTY_FRAME {
    NO_PAWS_DOWN    = 0,
    FIRST_PAW_DOWN  = 1,
    SECOND_PAW_DOWN = 2
};
int frame = NO_PAWS_DOWN;

HWND  hwApp;
HINSTANCE g_hInstance;

HBITMAP hbmCat;
BITMAP bm;

BOOL bShouldQuit   = FALSE;
INT iWindowX       = 0;
INT iWindowY       = 0;
INT iWindowWidth   = MIN_WIDTH;
INT iWindowHeight  = MIN_HEIGHT;
INT iStartBarTop   = 0;
BOOL bPositionLock = FALSE;
BOOL bDragging     = FALSE;


BYTE keys[30] = {
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
constexpr int KEYS_COUNT = 30;
bool keyStates[256] = { 0 };

LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

INT Initialize(HINSTANCE& hThisInstance);
void AddTrayIcon(LPCWSTR pszToolTip);
void RemoveTrayIcon(void);
VOID DrawCat(VOID);
void MouseDragHelper(long lWindowX, long lWindowY, long lScreenX, long lScreenY, bool reset);
void OnMouseDrag(long lPointerX, long lPointerY, long lDeltaX, long lDeltaY);
void OnRightClick(long lPointerX, long lPointerY);
void ShowPopupMenu(LONG lCursorX, LONG lCursorY);
void OnReset(void);
bool CheckKeys(void);


int APIENTRY wWinMain(HINSTANCE _In_ hThisInstance, HINSTANCE _In_opt_ hPrevInstance, PWSTR _In_ lpCmdLine, int _In_ nCmdShow)
{
    if (!Initialize(hThisInstance))
        return 1;

    MSG msg = { 0 };
    ULONGLONG timer = 0;
    ULONGLONG timerDelay = CLOCKS_PER_SEC / 5;
    
    while (!bShouldQuit) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                bShouldQuit = TRUE;
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
        iWindowWidth  = ((RECT*)lParam)->right  - ((RECT*)lParam)->left;
        iWindowHeight = ((RECT*)lParam)->bottom - ((RECT*)lParam)->top;
        MoveWindow(hwnd, iWindowX, iWindowY, iWindowWidth, iWindowHeight, true);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_CTX_RESET:
            OnReset();
            break;
        case ID_CTX_LOCK_POSITION:
            bPositionLock = TRUE;
            break;
        case ID_CTX_UNLOCK_POSITION:
            bPositionLock = FALSE;
            break;
        case ID_CTX_EXIT:
            PostQuitMessage(0);
            break;
        default:
            break;
        }
        break;
    case WM_APP:
        if (lParam == WM_RBUTTONUP && hwnd == hwApp) {
            GetCursorPos(&pt);
            OnRightClick(pt.x, pt.y);
        }
        break;
    case WM_LBUTTONDOWN:
        if (!bPositionLock) bDragging = TRUE;
        x = GET_X_LPARAM(lParam);
        y = GET_Y_LPARAM(lParam);
        GetCursorPos(&pt);
        MouseDragHelper(x, y, pt.x, pt.y, true);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONUP:
        if (hwnd == hwApp && bDragging) {
            bDragging = false;
            ReleaseCapture();
        }
        break;
    case WM_MOUSEMOVE:
        if (hwnd == hwApp)
            SetFocus(hwApp);
        else if (GetFocus() == hwApp)
            SetFocus(hwApp);
        if (hwnd == hwApp) {
            x = GET_X_LPARAM(lParam);
            y = GET_Y_LPARAM(lParam);
            if (bDragging) {
                GetCursorPos(&pt);
                MouseDragHelper(x, y, pt.x, pt.y, false);
            }
        }
        break;
    case WM_RBUTTONDOWN:
        if (hwnd == hwApp) {
            SetForegroundWindow(hwApp);
            GetCursorPos(&pt);
            OnRightClick(pt.x, pt.y);
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

VOID DrawCat(VOID)
{
    HDC hRealDC   = GetDC(hwApp);
    HDC hdcMemory = CreateCompatibleDC(hRealDC);
    SelectObject(hdcMemory, hbmCat);

    BLENDFUNCTION bfunc = { 0 };
    bfunc.BlendOp = AC_SRC_OVER;
    bfunc.BlendFlags = 0;
    bfunc.SourceConstantAlpha = 0;
    bfunc.AlphaFormat = 0;
    
    POINT pos = { iWindowX , iWindowY };
    SIZE size = { iWindowWidth, iWindowHeight };
    POINT bmpOffset = { 350 * frame, 0 };
    UpdateLayeredWindow(
        hwApp,
        hRealDC,
        &pos,
        &size,
        hdcMemory,
        &bmpOffset,
        0x00ff0000,
        &bfunc,
        ULW_COLORKEY);
    MoveWindow(hwApp, iWindowX, iWindowY, iWindowWidth, iWindowHeight, true);

    ReleaseDC(hwApp, hRealDC);
    DeleteDC(hdcMemory);
}

bool CheckKeys(void)
{
    static int prev_paw = SECOND_PAW_DOWN;
    bool changed = false;
    bool any = false;
    bool state;
    for (int i = 0; i < KEYS_COUNT; i++) {
        // turn the most-significant-bit returned into a bool state
        state = !!(GetAsyncKeyState(keys[i]) & (1 << 15));
        changed = changed || (keyStates[i] != state);
        any = any || state;
        keyStates[i] = state;
    }
    if (!any)
        // no paws on keys
        frame = NO_PAWS_DOWN;
    else if (changed) {
        if ( (rand() & 0xff) < 200)
            // usually alternate paw
            frame = 3 - prev_paw;
        else
            // rarely repeat paws
            frame = prev_paw;
        prev_paw = frame;
    }
    return changed;
}

INT Initialize(HINSTANCE& hThisInstance)
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
    //winclApp.hCursor = LoadCursor(NULL, IDC_SIZEALL);
    winclApp.hCursor = LoadCursor(NULL, IDC_ARROW);
    winclApp.lpszMenuName = NULL;
    winclApp.cbClsExtra = 0;
    winclApp.cbWndExtra = 0;
    winclApp.hbrBackground = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);

    if (!RegisterClassEx(&winclApp))
        return FALSE;
    
    RECT rtWorkArea;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rtWorkArea, 0)) {
        iStartBarTop = rtWorkArea.bottom;
        iWindowX = rtWorkArea.right - iWindowWidth;
    }
    else
        iStartBarTop = GetSystemMetrics(SM_CYMAXIMIZED);
    if (iStartBarTop != 0)
        iWindowY = iStartBarTop - BAR_GAP;
            
    hwApp = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // | WS_EX_TRANSPARENT ,
        szAppClassName,
        APP_NAME,
        WS_POPUP | WS_VISIBLE,
        iWindowX,
        iWindowY,
        iWindowWidth,
        iWindowHeight,
        HWND_DESKTOP,
        NULL,
        hThisInstance,
        NULL);
    GetClientRect(hwApp, &rtAppRect);

    g_hInstance = hThisInstance;

    hbmCat = LoadBitmap(hThisInstance, L"CATSPRITE");
    GetObject(hbmCat, sizeof(bm), &bm);

    AddTrayIcon(L"Kitty");

    return TRUE;
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
    if (!bDragging)
        return;
    RECT rtCurrentRect;
    if (!GetWindowRect(hwApp, &rtCurrentRect))
        return;
    iWindowX = rtCurrentRect.left + lDeltaX;
    iWindowY = rtCurrentRect.top + lDeltaY;
    MoveWindow(hwApp, iWindowX, iWindowY, iWindowWidth, iWindowHeight, false);
}

void OnRightClick(long lPointerX, long lPointerY)
{
    ShowPopupMenu(lPointerX, lPointerY);
}

void ShowPopupMenu(LONG lCursorX, LONG lCursorY)
{
    HMENU hContextMenu = CreatePopupMenu();

    int i = 0;
    InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_RESET, L"Reset Position");
    if ( bPositionLock )
        InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_UNLOCK_POSITION, L"Unlock Position");
    else
        InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_LOCK_POSITION, L"Lock Position");
    InsertMenu(hContextMenu, i++, MF_BYPOSITION | MF_STRING, ID_CTX_EXIT, L"Exit");

    SetMenuDefaultItem(hContextMenu, ID_CTX_EXIT, FALSE);

    SetFocus(hwApp);
    SendMessage(hwApp, WM_INITMENUPOPUP, (WPARAM)hContextMenu, 0);

    DWORD dwFlags = TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY;
    WORD cmd = TrackPopupMenu(
        hContextMenu, dwFlags,
        lCursorX, lCursorY,
        0, hwApp, NULL);

    SendMessage(hwApp, WM_COMMAND, cmd, 0);
    DestroyMenu(hContextMenu);
}

void OnReset(void)
{
    RECT rtWorkArea;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rtWorkArea, 0)) {
        iStartBarTop = rtWorkArea.bottom;
        iWindowX = rtWorkArea.right - iWindowWidth;
    }
    else
        iStartBarTop = GetSystemMetrics(SM_CYMAXIMIZED);
    if (iStartBarTop != 0)
        iWindowY = iStartBarTop - BAR_GAP;

    DrawCat();
}

void AddTrayIcon(LPCWSTR pszToolTip)
{
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hwApp;
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
    nid.hWnd   = hwApp;
    nid.uID    = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

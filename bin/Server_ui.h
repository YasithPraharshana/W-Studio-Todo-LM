#ifndef SERVER_UI_H
#define SERVER_UI_H

#include <windows.h>
#include <shellapi.h>
#include <winsock2.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <ctime>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

// ─── Message / Control IDs ─────────────────────────────────────────────────
#define WM_TRAYICON        (WM_USER + 1)
#define ID_TRAY_EXIT        1001
#define ID_OPEN_WEB_BTN     2001
#define ID_INDICATOR_REQ    3001
#define ID_INDICATOR_RES    3002
#define IDI_ICON1           101
#define TIMER_UPTIME        1

// ─── Colour Palette ────────────────────────────────────────────────────────
// Background layers
#define CLR_BG          RGB(10,  10,  10)   // Solid opaque background
#define CLR_PANEL       RGB(18,  18,  28)   // raised panel
#define CLR_HEADER      RGB(14,  14,  22)   // header strip
#define CLR_DIVIDER     RGB(35,  35,  55)   // subtle separator

// Accent / Status
#define CLR_ACCENT      RGB(0,   210, 180)  // teal-cyan primary
#define CLR_ACCENT2     RGB(80,  120, 255)  // indigo secondary
#define CLR_REQ_ON      RGB(0,   230, 160)  // request active  (green)
#define CLR_REQ_OFF     RGB(20,  55,  45)   // request idle
#define CLR_RES_ON      RGB(255, 80,  100)  // response active (red)
#define CLR_RES_OFF     RGB(55,  20,  28)   // response idle
#define CLR_STATUS_DOT  RGB(0,   210, 100)  // server-running dot

// Text
#define CLR_TEXT_HI     RGB(240, 240, 250)  // headlines
#define CLR_TEXT_MID    RGB(160, 165, 190)  // body / labels
#define CLR_TEXT_DIM    RGB(70,  75,  100)  // muted

// Buttons
#define CLR_BTN_FACE       RGB(0,   210, 180)
#define CLR_BTN_HOV        RGB(30,  230, 200)
#define CLR_BTN_TEXT       RGB(8,   12,  20)

// Exit Button
#define CLR_BTN_EXIT_FACE  RGB(220, 50,  60)
#define CLR_BTN_EXIT_HOV   RGB(255, 80,  90)

// Window geometry
#define WIN_W  400
#define WIN_H  470

#define ID_TRAY_OPEN_UI    1002
#define ID_TRAY_OPEN_WEB   1003

// ─── Globals ───────────────────────────────────────────────────────────────
extern bool           reqActive;
extern bool           resActive;
extern bool           running;
extern SOCKET         server_fd;
extern HWND           hwndPopup;
extern HWND           hReqLight;
extern HWND           hResLight;
extern NOTIFYICONDATA nid;

// Defined in this header (once per translation unit via the header guard)
HWND   hLogBox      = NULL;
HWND   hUptimeLabel = NULL;
time_t startTime    = 0;

// ─── Helper: Draw text without a child control ─────────────────────────────
static void GDIText(HDC hdc, const char* txt, RECT r,
                    COLORREF fg, HFONT hf,
                    UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
{
    SetTextColor(hdc, fg);
    SetBkMode(hdc, TRANSPARENT);
    if (hf) SelectObject(hdc, hf);
    DrawText(hdc, txt, -1, &r, fmt);
}

// ─── Helper: Filled rounded rectangle via polygon ──────────────────────────
static void FillRoundRect(HDC hdc, RECT r, int rx, COLORREF clr)
{
    HBRUSH hBr = CreateSolidBrush(clr);
    HBRUSH hOld = (HBRUSH)SelectObject(hdc, hBr);
    HPEN   hPn  = CreatePen(PS_NULL, 0, 0);
    HPEN   hPOld = (HPEN)SelectObject(hdc, hPn);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rx, rx);
    SelectObject(hdc, hOld);
    SelectObject(hdc, hPOld);
    DeleteObject(hBr);
    DeleteObject(hPn);
}

// ─── Helper: Filled circle (indicator) ────────────────────────────────────
static void FillCircle(HDC hdc, int cx, int cy, int r, COLORREF clr)
{
    HBRUSH hBr = CreateSolidBrush(clr);
    HPEN   hPn = CreatePen(PS_NULL, 0, 0);
    SelectObject(hdc, hBr);
    SelectObject(hdc, hPn);
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    DeleteObject(hBr);
    DeleteObject(hPn);
}

// ─── Helper: Horizontal divider ───────────────────────────────────────────
static void HLine(HDC hdc, int x1, int x2, int y, COLORREF clr)
{
    HPEN hp = CreatePen(PS_SOLID, 1, clr);
    HPEN old = (HPEN)SelectObject(hdc, hp);
    MoveToEx(hdc, x1, y, NULL);
    LineTo(hdc, x2, y);
    SelectObject(hdc, old);
    DeleteObject(hp);
}

// ─── Forward declaration ───────────────────────────────────────────────────
DWORD WINAPI runServer(LPVOID lpParam);

// ─── Font cache ────────────────────────────────────────────────────────────
static HFONT hFontTitle  = NULL;
static HFONT hFontSub    = NULL;
static HFONT hFontMono   = NULL;
static HFONT hFontLabel  = NULL;
static HFONT hFontBtn    = NULL;

static void InitFonts()
{
    if (hFontTitle) return;
    hFontTitle = CreateFont(20, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontSub   = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontMono  = CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FIXED_PITCH  | FF_MODERN, "Consolas");
    hFontLabel = CreateFont(11, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hFontBtn   = CreateFont(14, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE,
                            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

// ─── Button hover tracking ─────────────────────────────────────────────────
static bool btnWebHover = false;
static bool btnExitHover = false;

// ─── Window Procedure ──────────────────────────────────────────────────────
LRESULT CALLBACK StatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // ── Colour entire background + child controls ─────────────────────────
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC   hdc  = (HDC)wParam;
        HWND  hCtl = (HWND)lParam;
        int   id   = GetDlgCtrlID(hCtl);

        // Indicator lights
        if (id == ID_INDICATOR_REQ)
            return (LRESULT)CreateSolidBrush(reqActive ? CLR_REQ_ON : CLR_REQ_OFF);
        if (id == ID_INDICATOR_RES)
            return (LRESULT)CreateSolidBrush(resActive ? CLR_RES_ON : CLR_RES_OFF);

        // Log box
        if (hCtl == hLogBox) {
            SetTextColor(hdc, RGB(130, 255, 180));
            SetBkColor(hdc, RGB(8, 10, 18));
            return (LRESULT)CreateSolidBrush(RGB(8, 10, 18));
        }

        // Uptime label
        if (hCtl == hUptimeLabel) {
            SetTextColor(hdc, CLR_ACCENT);
            SetBkColor(hdc, CLR_PANEL);
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)CreateSolidBrush(CLR_PANEL);
        }

        // Everything else → transparent on panel bg
        SetTextColor(hdc, CLR_TEXT_MID);
        SetBkColor(hdc, CLR_PANEL);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)CreateSolidBrush(CLR_PANEL);
    }

    // ── Erase background ──────────────────────────────────────────────────
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hBr = CreateSolidBrush(CLR_BG);
        FillRect(hdc, &rc, hBr);
        DeleteObject(hBr);
        return 1;
    }

    // ── Custom paint (all static decoration) ──────────────────────────────
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // ── 1. Header panel ──
        RECT hdr = {0, 0, WIN_W, 68};
        HBRUSH hBrHdr = CreateSolidBrush(CLR_HEADER);
        FillRect(hdc, &hdr, hBrHdr);
        DeleteObject(hBrHdr);

        // Accent left bar on header
        RECT acBar = {0, 0, 4, 68};
        HBRUSH hBrAc = CreateSolidBrush(CLR_ACCENT);
        FillRect(hdc, &acBar, hBrAc);
        DeleteObject(hBrAc);

        // Title
        RECT rTitle = {18, 10, WIN_W - 10, 38};
        GDIText(hdc, "SERVER DASHBOARD", rTitle, CLR_TEXT_HI, hFontTitle);

        // Sub-title line
        RECT rSub = {19, 38, WIN_W - 10, 58};
        GDIText(hdc, "Personal Todo list Manager  \xB7  localhost:8080", rSub, CLR_TEXT_DIM, hFontSub);

        // Running indicator dot + text (top-right)
        FillCircle(hdc, WIN_W - 22, 18, 5, CLR_STATUS_DOT);
        RECT rRun = {WIN_W - 80, 10, WIN_W - 30, 28};
        GDIText(hdc, "RUNNING", rRun, CLR_STATUS_DOT, hFontLabel, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // ── 2. Uptime card ──
        FillRoundRect(hdc, {14, 78, WIN_W - 14, 116}, 8, CLR_PANEL);
        RECT rUpLbl = {26, 80, 130, 96};
        GDIText(hdc, "UPTIME", rUpLbl, CLR_TEXT_DIM, hFontLabel);

        // ── 3. Indicator cards ──
        // REQ card
        FillRoundRect(hdc, {14, 126, 193, 172}, 8, CLR_PANEL);
        RECT rReqLbl = {54, 128, 185, 143};
        GDIText(hdc, "REQ IN", rReqLbl, CLR_TEXT_DIM, hFontLabel);

        // RES card
        FillRoundRect(hdc, {207, 126, WIN_W - 14, 172}, 8, CLR_PANEL);
        RECT rResLbl = {247, 128, WIN_W - 18, 143};
        GDIText(hdc, "RES OUT", rResLbl, CLR_TEXT_DIM, hFontLabel);

        // REQ status text
        RECT rReqTxt = {46, 147, 185, 166};
        GDIText(hdc, reqActive ? "ACTIVE" : "IDLE",
                rReqTxt, reqActive ? CLR_REQ_ON : CLR_TEXT_DIM, hFontSub);

        // RES status text
        RECT rResTxt = {239, 147, WIN_W - 18, 166};
        GDIText(hdc, resActive ? "SENDING" : "IDLE",
                rResTxt, resActive ? CLR_RES_ON : CLR_TEXT_DIM, hFontSub);

        // ── 4. Log section header ──
        HLine(hdc, 14, WIN_W - 14, 182, CLR_DIVIDER);
        RECT rLogLbl = {14, 184, 200, 200};
        GDIText(hdc, "  ACTIVITY LOG", rLogLbl, CLR_ACCENT, hFontLabel);

        // ── 5. Buttons (Split width 50/50) ──
        
        // Open Web Button
        RECT btnWebR = {14, 320, 193, 362}; // WIN_W/2 - 7 = 193
        COLORREF webClr = btnWebHover ? CLR_BTN_HOV : CLR_BTN_FACE;
        FillRoundRect(hdc, btnWebR, 8, webClr);
        GDIText(hdc, "^ OPEN WEB", btnWebR, CLR_BTN_TEXT, hFontBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Exit Server Button
        RECT btnExitR = {207, 320, WIN_W - 14, 362}; // WIN_W/2 + 7 = 207
        COLORREF exitClr = btnExitHover ? CLR_BTN_EXIT_HOV : CLR_BTN_EXIT_FACE;
        FillRoundRect(hdc, btnExitR, 8, exitClr);
        GDIText(hdc, "X EXIT SERVER", btnExitR, RGB(255,255,255), hFontBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // ── 6. Divider above footer ──
        HLine(hdc, 14, WIN_W - 14, 374, CLR_DIVIDER);

        // ── 7. Footer ──
        RECT rFoot = {14, 376, WIN_W - 14, WIN_H - 6};
        GDIText(hdc, "\xA9 2026 YP Creation Studio", rFoot, CLR_TEXT_DIM, hFontSub, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    // ── Button Clicks ─────────────────────────────────────────────────────
    case WM_LBUTTONUP: {
        if (btnWebHover) {
            ShellExecute(NULL, "open", "http://localhost:8080", NULL, NULL, SW_SHOWNORMAL);
        } else if (btnExitHover) {
            running = false;
            closesocket(server_fd);
            PostQuitMessage(0); // Tell program to terminate entirely
        }
        break;
    }

    // ── Button hover states ───────────────────────────────────────────────
    case WM_MOUSEMOVE: {
        int mx = LOWORD(lParam), my = HIWORD(lParam);
        
        bool newWebHov = (mx >= 14 && mx <= 193 && my >= 320 && my <= 362);
        bool newExitHov = (mx >= 207 && mx <= WIN_W - 14 && my >= 320 && my <= 362);
        
        if (newWebHov != btnWebHover || newExitHov != btnExitHover) {
            btnWebHover = newWebHov;
            btnExitHover = newExitHov;
            RECT inv = {14, 320, WIN_W - 14, 362};
            InvalidateRect(hwnd, &inv, FALSE);
        }
        break;
    }

    // ── Uptime + indicator refresh ─────────────────────────────────────────
    case WM_TIMER: {
        if (wParam == TIMER_UPTIME) {
            time_t now   = time(0);
            double secs  = difftime(now, startTime);
            int h = (int)secs / 3600;
            int m = ((int)secs % 3600) / 60;
            int s = (int)secs % 60;
            char buf[64];
            sprintf(buf, "%02dh  %02dm  %02ds", h, m, s);
            SetWindowText(hUptimeLabel, buf);

            // Refresh indicator cards + status text
            RECT indR = {14, 126, WIN_W - 14, 172};
            InvalidateRect(hwnd, &indR, FALSE);

            if (reqActive) { reqActive = false; InvalidateRect(hReqLight, NULL, TRUE); }
            if (resActive) { resActive = false; InvalidateRect(hResLight, NULL, TRUE); }
        }
        break;
    }

    // ── Window Close / Hide ───────────────────────────────────────────────
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ─── Helper: center window on screen ──────────────────────────────────────
void CenterWindow(HWND hwnd)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right  - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top))  / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

// ─── Append a timestamped line to the log box ──────────────
void AddLog(const char* info)
{
    if (!hLogBox) return;

    // 1. වත්මන් වේලාව ලබා ගැනීම
    time_t now = time(0);
    tm* lt = localtime(&now);
    char ts[12];
    strftime(ts, sizeof(ts), "[%H:%M:%S] ", lt);
    std::string line = std::string(ts) + info + "\r\n";

    // 2. පේළි ගණන පරීක්ෂා කිරීම (Line Count)
    int lineCount = (int)SendMessage(hLogBox, EM_GETLINECOUNT, 0, 0);

    // පේළි 100 ට වඩා තිබේ නම්, පළමු පේළිය ඉවත් කරන්න
    if (lineCount >= 50) {
        int startPos = (int)SendMessage(hLogBox, EM_LINEINDEX, 0, 0);
        int endPos   = (int)SendMessage(hLogBox, EM_LINEINDEX, 1, 0);

        SendMessage(hLogBox, EM_SETSEL, startPos, endPos);
        SendMessage(hLogBox, EM_REPLACESEL, FALSE, (LPARAM)"");
    }

    // 3. අලුත් පේළිය අවසානයට එකතු කිරීම
    int len = GetWindowTextLength(hLogBox);
    SendMessage(hLogBox, EM_SETSEL, len, len);
    SendMessage(hLogBox, EM_REPLACESEL, 0, (LPARAM)line.c_str());
    
    // 4. Auto scroll down (Warning එක ඉවත් කරන ලද පේළිය)
    SendMessage(hLogBox, WM_VSCROLL, SB_BOTTOM, 0); 
}

// ─── Create the dashboard window ──────────────────────────────────────────
void CreateStatusUI(HWND /*hOwner*/, HINSTANCE hInstance)
{
    InitFonts();

    const char* CLS = "YPDashboardClass";
    static bool reg = false;
    if (!reg) {
        WNDCLASS wc  = {0};
        wc.lpfnWndProc   = StatusWndProc;
        wc.hInstance     = hInstance;
        wc.hbrBackground = CreateSolidBrush(CLR_BG);
        wc.lpszClassName = CLS;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        RegisterClass(&wc);
        reg = true;
    }

    // Bring existing window to front if already open
    if (hwndPopup && IsWindow(hwndPopup)) {
        ShowWindow(hwndPopup, SW_RESTORE);
        SetForegroundWindow(hwndPopup);
        return;
    }

    // ── Main window (Standard Opaque Window) ──
    hwndPopup = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        CLS, "YP Creation Studio - W Studio 1.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        0, 0, WIN_W, WIN_H,
        NULL, NULL, hInstance, NULL);

    // ── DWM dark title bar ──
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwndPopup,
        20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

    // ── App icon ──
    HICON hIco = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hwndPopup, WM_SETICON, ICON_SMALL, (LPARAM)hIco);
    SendMessage(hwndPopup, WM_SETICON, ICON_BIG,   (LPARAM)hIco);

    // ── Uptime label ──
    hUptimeLabel = CreateWindow("STATIC", "00h  00m  00s",
        WS_VISIBLE | WS_CHILD | SS_RIGHT,
        100, 85, WIN_W - 125, 24,
        hwndPopup, NULL, hInstance, NULL);
    SendMessage(hUptimeLabel, WM_SETFONT, (WPARAM)hFontBtn, TRUE);

    // ── Indicator dots ──
    hReqLight = CreateWindow("STATIC", "",
        WS_VISIBLE | WS_CHILD,
        26, 138, 16, 16,
        hwndPopup, (HMENU)ID_INDICATOR_REQ, hInstance, NULL);

    hResLight = CreateWindow("STATIC", "",
        WS_VISIBLE | WS_CHILD,
        219, 138, 16, 16,
        hwndPopup, (HMENU)ID_INDICATOR_RES, hInstance, NULL);

    // ── Log box ──
    hLogBox = CreateWindowEx(WS_EX_CLIENTEDGE,
        "EDIT", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        14, 202, WIN_W - 28, 108,
        hwndPopup, NULL, hInstance, NULL);
    SendMessage(hLogBox, WM_SETFONT, (WPARAM)hFontMono, TRUE);

    SetTimer(hwndPopup, TIMER_UPTIME, 1000, NULL);
    CenterWindow(hwndPopup);

    ShowWindow(hwndPopup, SW_SHOW);
    UpdateWindow(hwndPopup);
}

// ─── Tray icon window procedure ────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON) {
        // වම් ක්ලික් කළ විට dashboard එක පෙන්වන්න
        if (lParam == WM_LBUTTONUP) {
            CreateStatusUI(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
        } 
        // දකුණු ක්ලික් කළ විට මෙනුව පෙන්වන්න
        else if (lParam == WM_RBUTTONUP) {
            POINT pt; 
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            
            // Menu Items එකතු කිරීම
            // 1. Open Dashboard (Bold Text - ප්‍රධාන option එක ලෙස)
            InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_UI, "Open Dashboard");
            // Dashboard එක default option එක ලෙස Bold කරන්න
            SetMenuDefaultItem(hMenu, ID_TRAY_OPEN_UI, FALSE);
            
            // 2. Open Web App
            InsertMenu(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_WEB, "Open Web Interface");
            
            // 3. Separator Line එකක්
            InsertMenu(hMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            
            // 4. Exit Option
            InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, "Exit Server");

            // මෙනුව පෙන්වීමට පෙර foreground window එක සකස් කළ යුතුය (නැතිනම් මෙනුව අතුරුදහන් නොවේ)
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
    } 
    else if (msg == WM_COMMAND) {
        int wmId = LOWORD(wParam);
        switch (wmId) {
            case ID_TRAY_OPEN_UI:
                CreateStatusUI(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
                break;
                
            case ID_TRAY_OPEN_WEB:
                ShellExecute(NULL, "open", "http://localhost:8080", NULL, NULL, SW_SHOWNORMAL);
                break;
                
            case ID_TRAY_EXIT:
                running = false;
                closesocket(server_fd);
                PostQuitMessage(0);
                break;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ─── WinMain ───────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    startTime = time(0);
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);

    HICON hMainIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "TrayIconClass";
    wc.hIcon         = hMainIcon;
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("TrayIconClass", "", 0, 0, 0, 0, 0,
                             NULL, NULL, hInstance, NULL);

    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hwnd;
    nid.uID              = 1;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = hMainIcon;
    strcpy(nid.szTip, "W Studio Server - Running");
    Shell_NotifyIcon(NIM_ADD, &nid);

    CreateThread(NULL, 0, runServer, NULL, 0, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    return 0;
}

#endif // SERVER_UI_H
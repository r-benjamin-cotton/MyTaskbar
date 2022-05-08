// MyTaskbar.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "framework.h"
#include "MyTaskbar.h"

#define MAX_LOADSTRING 100

using namespace Gdiplus;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

#if defined(_DEBUG)
void DebugPrintf(const wchar_t* frm, ...);
#else
#define DebugPrintf     (void)
#endif

void FillRoundRect(Gdiplus::Graphics* graphics, const Gdiplus::Brush* brush, const Gdiplus::RectF& rect, Gdiplus::REAL percent);

class MyTaskWindow;

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名

static HANDLE hMutex;
static MyTaskWindow* taskWindowInProgress = nullptr;

static std::map<HWND, MyTaskWindow*> taskWindows;

constexpr DWORD styleEx = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP;
constexpr DWORD style = WS_POPUP;


class MayTaskList
{
public:
    struct TaskInfo
    {
        HWND hWnd;
        HICON hIcon;
        std::wstring title;
    };
    std::list<TaskInfo> taskList;

private:
    bool dirty;
    std::list<TaskInfo> taskListUpdating;
    static bool IsTaskWindow(HWND hWnd)
    {
        auto dwStyle = (DWORD)GetWindowLongW(hWnd, GWL_STYLE);
        auto dwExStyle = (DWORD)GetWindowLongW(hWnd, GWL_EXSTYLE);
        if ((dwStyle & WS_VISIBLE) == 0)
        {
            return false;
        }
        if ((dwExStyle & WS_EX_TOOLWINDOW) != 0)
        {
            return false;
        }
        if ((dwStyle & WS_SYSMENU) == 0)
        {
            return false;
        }
        auto hOwner = GetWindow(hWnd, GW_OWNER);
        if (hOwner != NULL)
        {
            if (((dwExStyle & WS_EX_APPWINDOW) == 0) || !IsIconic(hWnd))
            {
                return false;
            }
        }
        return true;
    }

    static HICON GetWindowIcon(HWND hWnd)
    {
#if 0
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        auto hIcon = LoadIcon(hInstance, IDI_APPLICATION);
#endif
#if 1
        DWORD dwProcessId;
        GetWindowThreadProcessId(hWnd, &dwProcessId);
        auto hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);

        DWORD len = 1024;
        auto str = std::vector<wchar_t>(len, '\0');
        QueryFullProcessImageName(hProcess, 0, str.data(), &len);
        SHFILEINFOW si = {};
        SHGetFileInfoW(str.data(), 0, &si, sizeof(si), SHGFI_ICON | SHGFI_LARGEICON);
        auto hIcon = si.hIcon;
        CloseHandle(hProcess);
#endif
        return hIcon;
        //return (HICON)GetClassLongPtr(hWnd, GCLP_HICON);
        //return (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
    }
    static std::wstring GetWindowTitle(HWND hWnd)
    {
        auto length = GetWindowTextLengthW(hWnd) + 1;
        auto str = std::vector<wchar_t>(length, '\0');
        GetWindowTextW(hWnd, str.data(), length);
        return std::wstring(str.data());
    }
    bool OnEnumWindow(HWND hWnd)
    {
        for (auto ti = taskList.begin(); ti != taskList.end(); ti++)
        {
            if (ti->hWnd == hWnd)
            {
#if 1
                ti->title = GetWindowTitle(hWnd);
#endif
                taskListUpdating.push_back(*ti);
                taskList.erase(ti);
                return true;
            }
        }
        if (IsTaskWindow(hWnd))
        {
            auto hIcon = GetWindowIcon(hWnd);
            auto title = GetWindowTitle(hWnd);
            if (!title.empty() || (GetClassLongPtr(hWnd, GCLP_HICON) != NULL))
            {
                taskListUpdating.push_back(TaskInfo{ hWnd, hIcon, title });
                dirty = true;
                //DebugPrintf(L"%s\n", title.c_str());
            }
        }
        return true;
    }
    static BOOL CALLBACK EnumFunc(HWND hWnd, LPARAM lParam)
    {
        auto pThis = reinterpret_cast<MayTaskList*>(lParam);
        return pThis->OnEnumWindow(hWnd) ? TRUE : FALSE;
    }
public:
    void Clear()
    {
        for (auto& task : taskList)
        {
            if (task.hIcon != NULL)
            {
                DestroyIcon(task.hIcon);
                task.hIcon = NULL;
            }
        }
        taskList.clear();
    }
    bool Update()
    {
        dirty = false;
        EnumWindows(EnumFunc, (LPARAM)this);
        if (!taskList.empty())
        {
            Clear();
            dirty = true;
        }
        taskList = taskListUpdating;
        taskListUpdating.clear();
        return dirty;
    }
};

class MyTaskWindow
{
private:
    HWND hWnd;
    HMENU hMenu = NULL;
    RECT windowRect;

    HDC hMemDC = NULL;
    HBITMAP hBitmap = NULL;
    BITMAPINFO bitmapInfo{};
    COLORREF* bitmapData = nullptr;
    Gdiplus::Graphics* gp = nullptr;
    LinearGradientBrush* brushBg = nullptr;
    SolidBrush* brushActive = nullptr;
    SolidBrush* brushInactive = nullptr;
    SolidBrush* brushText = nullptr;
    SolidBrush* brushGhost = nullptr;
    Font* font = nullptr;

    MayTaskList taskList;
    int cycaption = 0;
    int cxmin = 0;

    UINT dpi = 0;

    UINT_PTR timerId = 0;
    DWORD previousTickCount = 0;
    HWND selection = NULL;
    bool visible = true;
    int leaving = 0;
    int scanDelay = 0;
    float viewMargin = 0;
    float targetPos = 0;
    float scrollPos = 0;
    float lineHeight = 0;
    float viewHeight = 0;
    float border = 0;

    static constexpr int LeaveCount = 300;
    static constexpr int ScanInterval = 300;

private:
    void SetupWindow()
    {
        hMenu = LoadMenuW(hInst, MAKEINTRESOURCE(IDC_MYTASKBAR));
        dpi = GetDpiForWindow(hWnd);
        auto hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info = { sizeof(MONITORINFO) };
        GetMonitorInfo(hMonitor, &info);
        windowRect = info.rcWork;
    }
    void ReleaseBitmap()
    {
        if (hBitmap != NULL)
        {
            DeleteObject(hBitmap);
            hBitmap = NULL;
            bitmapData = nullptr;
        }
        if (hMemDC != NULL)
        {
            DeleteDC(hMemDC);
            hMemDC = NULL;
        }
    }
    void SetupBitmap()
    {
        ReleaseBitmap();
        HDC hdc = GetDC(hWnd);
        hMemDC = CreateCompatibleDC(hdc);

        SIZE sz = { windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biWidth = sz.cx;
        bitmapInfo.bmiHeader.biHeight = sz.cy;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        bitmapInfo.bmiHeader.biSizeImage = sz.cx * sz.cy * 4;

        hBitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, (void**)&bitmapData, NULL, NULL);
        if (hBitmap == NULL)
        {
            return;
        }
        SelectObject(hMemDC, hBitmap);
        ReleaseDC(hWnd, hdc);
    }
    void ReleaseGraphics()
    {
        delete gp;
        gp = nullptr;
        delete brushBg;
        brushBg = nullptr;
        delete brushActive;
        brushActive = nullptr;
        delete brushInactive;
        brushInactive = nullptr;
        delete brushText;
        brushText = nullptr;
        delete brushGhost;
        brushGhost = nullptr;
        delete font;
        font = nullptr;
    }
    void SetupGraphics()
    {
        ReleaseGraphics();
        if (hMemDC == NULL)
        {
            return;
        }
        cycaption = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
        cxmin = GetSystemMetricsForDpi(SM_CXMIN, dpi);

        gp = new Gdiplus::Graphics(hMemDC);
        auto colorBg = GetThemeSysColor(NULL, COLOR_BACKGROUND);
        auto bkc0 = Color(255, GetRValue(colorBg), GetGValue(colorBg), GetBValue(colorBg));
        auto bkc1 = Color(0, GetRValue(colorBg), GetGValue(colorBg), GetBValue(colorBg));
        brushBg = new LinearGradientBrush(Point(0, 0), Point(cxmin + 1, 0), bkc0, bkc1);
        brushBg->SetGammaCorrection(TRUE);

        auto colorActive = GetThemeSysColor(NULL, COLOR_ACTIVECAPTION);
        brushActive = new SolidBrush(Color(255, GetRValue(colorActive), GetGValue(colorActive), GetBValue(colorActive)));
        auto colorInactive = GetThemeSysColor(NULL, COLOR_INACTIVECAPTION);
        brushInactive = new SolidBrush(Color(255, GetRValue(colorInactive), GetGValue(colorInactive), GetBValue(colorInactive)));
        auto colorText = GetThemeSysColor(NULL, COLOR_CAPTIONTEXT);
        brushText = new SolidBrush(Color(255, GetRValue(colorText), GetGValue(colorText), GetBValue(colorText)));
        brushGhost = new SolidBrush(Color(1, 127, 127, 127));

        LOGFONTW logfont;
        GetThemeSysFont(NULL, TMT_CAPTIONFONT, &logfont);
        font = new Font(hMemDC, &logfont);
        auto fontHeight = font->GetHeight((REAL)dpi);
        border = (cycaption - fontHeight) / 2;
        lineHeight = (fontHeight + border * 2) * 1.5f;
        viewMargin = (windowRect.bottom - windowRect.top) * (1.0f / 8.0f);
        viewHeight = 0;
    }

    void Render()
    {
        selection = NULL;
        if (!visible)
        {
            auto colorBorder = GetThemeSysColor(NULL, COLOR_INACTIVEBORDER);
            gp->Clear(Color(128, GetRValue(colorBorder), GetGValue(colorBorder), GetBValue(colorBorder)));
            return;
        }
        PointF cursolPos;
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            cursolPos.X = (REAL)pt.x;
            cursolPos.Y = (REAL)pt.y;
        }
        {
            Rect r(windowRect.left, windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
            r.Width = cxmin;
            gp->Clear(Color(0, 0, 0, 0));
            gp->FillRectangle(brushBg, r);
            gp->SetTextRenderingHint(TextRenderingHintAntiAlias);
            gp->SetSmoothingMode(SmoothingModeAntiAlias);
            PointF pointF(cycaption * 2.0f, viewMargin - scrollPos);
#if 1
            for (const auto& taskInfo : taskList.taskList)
            {
                auto title = taskInfo.title;
                if (title.empty())
                {
                    title = L" ";
                }
                RectF r = {};
                auto status = gp->MeasureString(title.c_str(), -1, font, pointF, &r);
                r.X -= cycaption + border * 2 + lineHeight * 0.5f;
                r.Y -= border + lineHeight * 0.5f;
                r.Width += cycaption * 2 + border * 2 + lineHeight;
                r.Height += border * 2 + lineHeight;
                gp->FillRectangle(brushGhost, r);
                pointF.Y += lineHeight;
            }
            pointF.Y = viewMargin - scrollPos;
#endif
            for (const auto& taskInfo : taskList.taskList)
            {
                auto title = taskInfo.title;
                if (title.empty())
                {
                    title = L" ";
                }
                RectF r = {};
                auto status = gp->MeasureString(title.c_str(), -1, font, pointF, &r);
                r.X -= cycaption + border * 2;
                r.Y -= border;
                r.Width += cycaption * 2 + border * 2;
                r.Height += border * 2;
                auto active = r.Contains(cursolPos);
                if (active)
                {
                    selection = taskInfo.hWnd;
                }
                auto bg = active ? brushActive : brushInactive;
                FillRoundRect(gp, bg, r, 0.6f);
                //g.Flush();
                DrawIconEx(hMemDC, (int)(r.X + border), (int)(r.Y + cycaption * (1.0f / 16.0f)), taskInfo.hIcon, cycaption, cycaption, 0, NULL, DI_NORMAL);
                //g.Flush();
                //auto bitmap = Bitmap::FromHICON(taskInfo.hIcon);
                //g.DrawImage(bitmap, RectF(r.X, r.Y, height, height));
                gp->DrawString(title.c_str(), -1, font, pointF, brushText);
                pointF.Y += lineHeight;
            }
            viewHeight = pointF.Y + scrollPos + viewMargin;
        }
    }
    void Apply()
    {
#if 1
        RECT rect {};
        GetWindowRect(hWnd, &rect);
        SIZE sz { rect.right - rect.left, rect.bottom - rect.top };
#else
        SIZE sz = { windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
#endif
        POINT org = { 0, 0 };
        BLENDFUNCTION blend;
        blend.AlphaFormat = AC_SRC_ALPHA;
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.BlendFlags = 0;
        COLORREF key = 0;
        DWORD flags = ULW_ALPHA;
        UpdateLayeredWindow(hWnd, NULL, NULL, &sz, hMemDC, &org, key, &blend, flags);
    }
    void Enter(bool apply = true)
    {
        //DebugPrintf(L"Enter\n");
        if (visible)
        {
            return;
        }
        visible = true;
        auto cx = windowRect.right - windowRect.left;
        auto cy = windowRect.bottom - windowRect.top;
        SetWindowPos(hWnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        if (apply)
        {
            Apply();
        }
        BeginTimer();
        scanDelay = ScanInterval;
        leaving = 0;
        taskList.Update();
    }
    void Leave(bool apply = true)
    {
        //DebugPrintf(L"Leave\n");
        if (!visible)
        {
            return;
        }
        visible = false;
        auto cx = 2;// windowRect.right - windowRect.left;
        auto cy = windowRect.bottom - windowRect.top;
        SetWindowPos(hWnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        if (apply)
        {
            Apply();
        }
        selection = NULL;
        EndTimer();
    }
    void ValidateViewPos()
    {
        if (viewHeight < windowRect.bottom)
        {
            if ((viewHeight - targetPos) > windowRect.bottom)
            {
                targetPos = viewHeight - windowRect.bottom;
            }
            if (targetPos > 0)
            {
                targetPos = 0;
            }
        }
        else
        {
            if ((viewHeight - targetPos) < windowRect.bottom)
            {
                targetPos = viewHeight - windowRect.bottom;
            }
            if (targetPos < 0)
            {
                targetPos = 0;
            }
        }
    }
    void Update()
    {
        ValidateViewPos();
        Render();
        Apply();
    }
    void OnTimer(DWORD tickCount)
    {
        auto deltaTime = tickCount - previousTickCount;
        previousTickCount = tickCount;
        //DebugPrintf(L"%d\n", deltaTime);
        auto dirty = false;
        {
            auto dt = scrollPos - targetPos;
            if (abs(dt) < 0.1f)
            {
                scrollPos = targetPos;
            }
            else
            {
                scrollPos = dt * powf(0.001f, deltaTime * (1.0f / 1000.0f)) + targetPos;
                dirty = true;
            }
        }
        {
            scanDelay -= (int)deltaTime;
            if (scanDelay <= 0)
            {
                scanDelay = ScanInterval;
                dirty = taskList.Update();
            }
        }
        if (leaving != 0)
        {
            leaving -= (int)deltaTime;
            if (leaving <= 0)
            {
                leaving = 0;
                Leave();
            }
        }
        if (dirty)
        {
            Update();
        }
    }
    void BeginTimer()
    {
        if (timerId != 0)
        {
            return;
        }
        previousTickCount = (DWORD)(GetTickCount64() & 0xffffffffU);
        timerId = SetTimer(hWnd, (UINT_PTR)this, 10, Timerproc);
        //timerId = SetCoalescableTimer(hWnd, (UINT_PTR)this, 10, Timerproc, 10);
    }
    void EndTimer()
    {
        if (timerId == 0)
        {
            return;
        }
        KillTimer(hWnd, timerId);
        timerId = 0;
    }
    static void Timerproc(HWND hWnd, UINT uMsg, UINT_PTR uID, DWORD tickCount)
    {
        auto pThis = reinterpret_cast<MyTaskWindow*>(uID);
        pThis->OnTimer(tickCount);
    }

public:
    void OnDestroy()
    {
        EndTimer();
        taskList.Clear();
        ReleaseGraphics();
        ReleaseBitmap();
        if (hMenu != NULL)
        {
            //DestroyMenu(hMenu);
            hMenu = NULL;
        }
        hWnd = NULL;
    }
    void OnCreate(HWND hWnd)
    {
        this->hWnd = hWnd;
        OnSettingsChanged();
    }
    void OnSettingsChanged()
    {
        //DebugPrintf(L"OnSettingsChanged\n");
        SetupWindow();
        Leave(false);
        SetupBitmap();
        SetupGraphics();
        Update();
    }
    void OnKillFocus()
    {
        Leave();
    }
    void OnLButtonUp(int x, int y)
    {
        if (selection ==NULL)
        {
            return;
        }
        auto hWnd = selection;
        Leave();
        SetForegroundWindow(hWnd);
        BringWindowToTop(hWnd);
    }
    void OnCloseTask()
    {
        if (selection == NULL)
        {
            return;
        }
        PostMessage(selection, WM_CLOSE, 0, 0);
    }
    void OnContextMenu(int x, int y)
    {
#if 1
        if (selection == NULL)
        {
            auto hCtxtMenu = GetSubMenu(hMenu, 0);
            TrackPopupMenu(hCtxtMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hWnd, NULL);
        }
        else
        {
            auto hCtxtMenu = GetSubMenu(hMenu, 1);
            TrackPopupMenu(hCtxtMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hWnd, NULL);
        }
#else
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Close");
        TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
#endif
    }
    void OnMouseWheel(int x, int y, int zd)
    {
        if (!visible)
        {
            return;
        }
        //DebugPrintf(L"%d\n", zd);
        targetPos -= lineHeight * zd * (1.0f / 120.0f);
        Update();
    }
    void OnMouseMove(int x, int y)
    {
        leaving = 0;
        if ((x < windowRect.left) || (x >= windowRect.right) || (y < windowRect.top) || (y >= windowRect.bottom))
        {
            leaving = LeaveCount;
        }
        else
        {
            //auto color = GetPixel(hmemdc, x, y);
            auto color = bitmapData[x + bitmapInfo.bmiHeader.biWidth * (bitmapInfo.bmiHeader.biHeight - 1 - y)];
            //DebugPrintf(L"%08x %d %d %d %d\n", color, x, y, bitmapInfo.bmiHeader.biWidth, bitmapInfo.bmiHeader.biHeight);
            if (((color >> 24) & 0xffU) == 0)
            {
                leaving = LeaveCount;
            }
        }
        if (GetCapture() != hWnd)
        {
            SetCapture(hWnd);
        }
        Enter(false);
        Update();
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。
    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MYTASKBAR, szWindowClass, MAX_LOADSTRING);

    hMutex = CreateMutex(NULL, FALSE, szTitle);
    if (hMutex == NULL)
    {
        return 0;
    }
    if (WAIT_OBJECT_0 != WaitForSingleObject(hMutex, 0))
    {
        return 0;
    }

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    {
        //timeBeginPeriod(1);
        BOOL info = FALSE;
        SetUserObjectInformationW(GetCurrentProcess(), UOI_TIMERPROC_EXCEPTION_SUPPRESSION, &info, sizeof(info));
    }

    MyRegisterClass(hInstance);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    //HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MYTASKBAR));

    BOOL bRet;
    MSG msg;

    // メイン メッセージ ループ:
    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            // error
            break;
        }
#if false
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#else
        TranslateMessage(&msg);
        DispatchMessage(&msg);
#endif
    }
    GdiplusShutdown(gdiplusToken);
    CloseHandle(hMutex);
    return (int) msg.wParam;
}


//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYTASKBAR));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;// (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;// MAKEINTRESOURCEW(IDC_MYTASKBAR);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   taskWindowInProgress = new MyTaskWindow();
   HWND hWnd = CreateWindowEx(styleEx, szWindowClass, szTitle, style, 0, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
   taskWindowInProgress = nullptr;
   if (hWnd == NULL)
   {
      return FALSE;
   }
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   return TRUE;
}


static MyTaskWindow* FindTaskWindow(HWND hWnd)
{
    auto it = taskWindows.find(hWnd);
    if (it == taskWindows.end())
    {
        return nullptr;
    }
    return it->second;
}
static POINT GetClientPos(HWND hWnd, LPARAM lParam)
{
    auto x = GET_X_LPARAM(lParam);
    auto y = GET_Y_LPARAM(lParam);
    POINT pt{ x, y };
    ScreenToClient(hWnd, &pt);
    return pt;
}
//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (taskWindowInProgress != nullptr)
    {
        taskWindows.emplace(hWnd, taskWindowInProgress);
        taskWindowInProgress = nullptr;
    }
    auto taskWindow = FindTaskWindow(hWnd);
    if (taskWindow == nullptr)
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    switch (message)
    {
    case WM_CREATE:
    {
        taskWindow->OnCreate(hWnd);
        break;
    }
    case WM_MOUSEMOVE:
    {
        auto pt = GetClientPos(hWnd, lParam);
        taskWindow->OnMouseMove(pt.x, pt.y);
        break;
    }
    case WM_MOUSEWHEEL:
    {
        auto pt = GetClientPos(hWnd, lParam);
        auto zd = GET_WHEEL_DELTA_WPARAM(wParam);
        taskWindow->OnMouseWheel(pt.x, pt.y, zd);
        break;
    }
    case WM_LBUTTONUP:
    {
        auto pt = GetClientPos(hWnd, lParam);
        taskWindow->OnLButtonUp(pt.x, pt.y);
        break;
    }
    case WM_KILLFOCUS:
    {
        taskWindow->OnKillFocus();
        //DebugPrintf(L"WM_KILLFOCUS");
        break;
    }
    case WM_CONTEXTMENU:
    {
        auto pt = GetClientPos(hWnd, lParam);
        taskWindow->OnContextMenu(pt.x, pt.y);
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 選択されたメニューの解析:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case ID_CLOSE_TASK:
            taskWindow->OnCloseTask();
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: HDC を使用する描画コードをここに追加してください...
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
    case WM_FONTCHANGE:
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
    case WM_THEMECHANGED:
    {
        taskWindow->OnSettingsChanged();
        break;
    }
    case WM_DESTROY:
    {
        taskWindow->OnDestroy();
        taskWindows.erase(hWnd);
        delete taskWindow;
        if (taskWindows.empty())
        {
            PostQuitMessage(0);
        }
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        POINT pos;
        GetCursorPos(&pos);
        SetWindowPos(hwndDlg, HWND_TOP, pos.x, pos.y, 0, 0, SWP_NOSIZE);
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hwndDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
#if defined(_DEBUG)
void DebugPrintf(const wchar_t* frm, ...)
{
    const int count = 256;
    wchar_t work[count];
    va_list list;
    va_start(list, frm);
    _vsnwprintf_s(work, count, frm, list);
    OutputDebugString(work);
}
#endif
void FillRoundRect(Gdiplus::Graphics* gp, const Gdiplus::Brush* brush, const Gdiplus::RectF& rect, Gdiplus::REAL percent)
{
    auto left = rect.X;
    auto top = rect.Y;
    auto right = rect.GetRight();
    auto bottom = rect.GetBottom();
    auto radius = (bottom - top) * percent;
    auto dx = min(radius * 2, rect.Width);
    auto dy = min(radius * 2, rect.Height);

    Gdiplus::GraphicsPath path;

    path.AddArc(right - dx, top, dx, dy, 270.0f, 90.0f);
    path.AddArc(right - dx, bottom - dy, dx, dy, 0.0f, 90.0f);
    path.AddArc(left, bottom - dy, dx, dy, 90.0f, 90.0f);
    path.AddArc(left, top, dx, dy, 180.0f, 90.0f);
    path.CloseAllFigures();

    gp->FillPath(brush, &path);
}


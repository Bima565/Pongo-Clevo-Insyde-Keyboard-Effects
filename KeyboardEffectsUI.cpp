// Native Win32 launcher for keyboard effects.
// Modernized ultra-lightweight dark-mode UI with zero-overhead GDI rendering.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define WM_TRAYICON   (WM_APP + 1)
#define IDC_EFFECTS   1001
#define IDC_APPLY     1002
#define IDC_STOP      1003
#define IDC_EXIT      1004
#define IDC_STARTUP   1005

namespace
{
    PROCESS_INFORMATION g_child{};
    std::wstring g_runningTheme{};

    constexpr wchar_t kRegPath[] = L"Software\\KeyboardEffects\\Settings";
    constexpr wchar_t kRegValueLastTheme[] = L"LastTheme";
    constexpr wchar_t kRegValueRunStartup[] = L"RunAtStartup";

    // Modern Dark Slate Palette
    constexpr COLORREF kClrWindowBg      = RGB(18, 20, 26);   // #12141A (Deep dark background)
    constexpr COLORREF kClrHeaderBg      = RGB(24, 28, 38);   // #181C26 (Header banner)
    constexpr COLORREF kClrCardBorder    = RGB(38, 44, 58);   // #262C3A (Subtle borders)
    constexpr COLORREF kClrItemBg        = RGB(25, 29, 39);   // #191D27 (List item card)
    constexpr COLORREF kClrItemBorder    = RGB(36, 42, 56);   // #242A38 (List item border)
    constexpr COLORREF kClrItemHover     = RGB(35, 42, 58);   // #232A3A (Item hover)
    constexpr COLORREF kClrItemSelected  = RGB(30, 48, 78);   // #1E304E (Item selected)
    constexpr COLORREF kClrItemSelBorder = RGB(59, 130, 246); // #3B82F6 (Selected border)
    constexpr COLORREF kClrAccentCyan    = RGB(6, 182, 212);  // #06B6D4 (Accent strip)
    constexpr COLORREF kClrTextPrimary   = RGB(243, 244, 246);// #F3F4F6 (Primary text)
    constexpr COLORREF kClrTextSecondary = RGB(156, 163, 175);// #9CA3AF (Description text)
    constexpr COLORREF kClrTextMuted     = RGB(107, 114, 128);// #6B7280 (Muted hints)
    constexpr COLORREF kClrStatusActive  = RGB(16, 185, 129); // #10B981 (Active status green)
    constexpr COLORREF kClrStatusIdle    = RGB(100, 116, 139);// #64748B (Idle status slate)

    // Button Colors
    constexpr COLORREF kClrBtnApply      = RGB(37, 99, 235);  // #2563EB
    constexpr COLORREF kClrBtnApplyHover = RGB(59, 130, 246); // #3B82F6
    constexpr COLORREF kClrBtnApplyPress = RGB(29, 78, 216);  // #1D4ED8

    constexpr COLORREF kClrBtnStop       = RGB(220, 38, 38);  // #DC2626
    constexpr COLORREF kClrBtnStopHover  = RGB(239, 68, 68);  // #EF4444
    constexpr COLORREF kClrBtnStopPress  = RGB(185, 28, 28);  // #B91C1C

    constexpr COLORREF kClrBtnExit       = RGB(31, 36, 48);   // #1F2430
    constexpr COLORREF kClrBtnExitHover  = RGB(45, 53, 70);   // #2D3546
    constexpr COLORREF kClrBtnExitPress  = RGB(22, 26, 36);   // #161A24
    constexpr COLORREF kClrBtnExitBorder = RGB(55, 65, 81);   // #374151

    struct EffectItem
    {
        std::wstring label;
        std::wstring themeArg;
        std::wstring description;
    };

    const std::vector<EffectItem> kEffects = {
        {L"1. Breathe", L"breathe", L"Classic breathing pulse through RGB colors."},
        {L"2. New Colour Breathe", L"newcolourbreathe", L"Dynamic multi-color breathing cycle."},
        {L"3. Colour Transform", L"colourtransform", L"Smooth and continuous RGB color wave."},
        {L"4. Strobe Transform", L"strobe", L"Dynamic strobe with smooth fade-in & fade-out."},
        {L"5. Music Adaptive Transform", L"musicadaptive", L"Real-time audio visualizer reacting to PC music."},
        {L"6. RGB Zone Test (R/G/B)", L"rgbzones", L"Diagnostic test cycling hardware color zones."},
        {L"7. Colour Transform Boost", L"fireflicker", L"Typing speed reactive: cycles colors faster the quicker you type!"},
        {L"8. Screen Reactive", L"screenreactive", L"Ambilight ambient sync matching on-screen colors."}
    };

    struct Settings
    {
        std::wstring lastTheme = L"breathe";
        bool runAtStartup = false;
    };

    HWND g_hList = nullptr;
    HWND g_hApplyBtn = nullptr;
    HWND g_hStopBtn = nullptr;
    HWND g_hExitBtn = nullptr;
    HWND g_hStartup = nullptr;

    HFONT g_hFontTitle = nullptr;
    HFONT g_hFontSub = nullptr;
    HFONT g_hFontItemTitle = nullptr;
    HFONT g_hFontItemDesc = nullptr;
    HFONT g_hFontButton = nullptr;
    HFONT g_hFontStatus = nullptr;
    HFONT g_hFontBadge = nullptr;
    HBRUSH g_hBrushWindowBg = nullptr;

    int g_hoveredButtonId = 0;
    int g_hoveredListItem = -1;

    void EnableDarkMode(HWND hWnd)
    {
        BOOL darkMode = TRUE;
        if (FAILED(DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode))))
        {
            DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));
        }
    }

    void InitGdiResources()
    {
        g_hBrushWindowBg = CreateSolidBrush(kClrWindowBg);

        g_hFontTitle = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontSub = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontItemTitle = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontItemDesc = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontButton = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontStatus = CreateFontW(-12, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontBadge = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    void FreeGdiResources()
    {
        auto safeDelete = [](HGDIOBJ& obj) {
            if (obj) { DeleteObject(obj); obj = nullptr; }
        };

        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hBrushWindowBg));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontTitle));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontSub));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontItemTitle));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontItemDesc));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontButton));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontStatus));
        safeDelete(reinterpret_cast<HGDIOBJ&>(g_hFontBadge));
    }

    void DrawRoundedCard(HDC hdc, const RECT& rc, int radius, COLORREF fill, COLORREF border, int borderWidth = 1)
    {
        HPEN hPen = CreatePen(PS_SOLID, borderWidth, border);
        HBRUSH hBrush = CreateSolidBrush(fill);

        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        HGDIOBJ oldBrush = SelectObject(hdc, hBrush);

        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        DeleteObject(hPen);
        DeleteObject(hBrush);
    }

    Settings LoadSettings()
    {
        Settings s;
        HKEY hKey{};
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            wchar_t buf[128]{};
            DWORD bufSize = sizeof(buf);
            if (RegQueryValueExW(hKey, kRegValueLastTheme, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS)
            {
                s.lastTheme.assign(buf);
            }

            DWORD dwRun = 0;
            DWORD dwSize = sizeof(dwRun);
            if (RegQueryValueExW(hKey, kRegValueRunStartup, nullptr, nullptr, reinterpret_cast<LPBYTE>(&dwRun), &dwSize) == ERROR_SUCCESS)
            {
                s.runAtStartup = (dwRun != 0);
            }

            RegCloseKey(hKey);
        }
        return s;
    }

    void SaveSettings(const Settings& s)
    {
        HKEY hKey{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, kRegValueLastTheme, 0, REG_SZ,
                reinterpret_cast<const BYTE*>(s.lastTheme.c_str()),
                static_cast<DWORD>((s.lastTheme.size() + 1) * sizeof(wchar_t)));

            const DWORD dwRun = s.runAtStartup ? 1u : 0u;
            RegSetValueExW(hKey, kRegValueRunStartup, 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&dwRun), sizeof(dwRun));

            RegCloseKey(hKey);
        }
    }

    std::wstring GetModuleDir()
    {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        const auto pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
        {
            dir.resize(pos);
        }
        return dir;
    }

    std::wstring ParentDir(std::wstring path)
    {
        const auto pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
        {
            path.resize(pos);
        }
        return path;
    }

    bool FileExists(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    std::wstring FindKeyboardExe()
    {
        const std::wstring moduleDir = GetModuleDir();
        const std::wstring parent = ParentDir(moduleDir);
        const std::wstring grandParent = ParentDir(parent);

        const std::vector<std::wstring> candidates = {
            moduleDir + L"\\CppKeyboardColour.exe",
            moduleDir + L"\\KBInsydeColours.exe",
            parent + L"\\CppKeyboardColour.exe",
            parent + L"\\KBInsydeColours.exe",
            grandParent + L"\\x64\\Debug\\CppKeyboardColour.exe",
            grandParent + L"\\x64\\Release\\CppKeyboardColour.exe",
            grandParent + L"\\Debug\\CppKeyboardColour.exe",
            grandParent + L"\\Release\\CppKeyboardColour.exe",
            grandParent + L"\\KBInsydeColours.exe"
        };

        for (const auto& path : candidates)
        {
            if (FileExists(path))
            {
                return path;
            }
        }

        return L"";
    }

    std::wstring GetWorkingDirForKeyboardExe(const std::wstring& exePath)
    {
        auto dir = exePath;
        const auto pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
        {
            dir.resize(pos);
        }

        if (FileExists(dir + L"\\InsydeDCHU.dll"))
        {
            return dir;
        }

        std::wstring parent = ParentDir(dir);
        if (FileExists(parent + L"\\InsydeDCHU.dll"))
        {
            return parent;
        }

        std::wstring grandParent = ParentDir(parent);
        if (FileExists(grandParent + L"\\InsydeDCHU.dll"))
        {
            return grandParent;
        }

        return dir;
    }

    const EffectItem* FindEffectByTheme(const std::wstring& themeArg)
    {
        for (const auto& effect : kEffects)
        {
            if (effect.themeArg == themeArg)
            {
                return &effect;
            }
        }
        return nullptr;
    }

    int FindEffectIndex(const std::wstring& themeArg)
    {
        for (size_t i = 0; i < kEffects.size(); ++i)
        {
            if (kEffects[i].themeArg == themeArg)
            {
                return static_cast<int>(i);
            }
        }
        return 0;
    }

    void StopCurrentEffect()
    {
        if (g_child.hProcess)
        {
            TerminateProcess(g_child.hProcess, 0);
            CloseHandle(g_child.hProcess);
            g_child.hProcess = nullptr;
        }

        if (g_child.hThread)
        {
            CloseHandle(g_child.hThread);
            g_child.hThread = nullptr;
        }

        g_runningTheme.clear();
    }

    bool LaunchEffect(HWND hWnd, const std::wstring& theme)
    {
        StopCurrentEffect();

        const auto exe = FindKeyboardExe();
        if (exe.empty())
        {
            MessageBoxW(hWnd,
                L"Could not locate CppKeyboardColour.exe in build folders.",
                L"Keyboard Effects",
                MB_ICONERROR | MB_OK);
            return false;
        }

        std::wstring cmd = L"\"" + exe + L"\" theme " + theme;
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        const std::wstring workingDir = GetWorkingDirForKeyboardExe(exe);
        ZeroMemory(&g_child, sizeof(g_child));

        const BOOL ok = CreateProcessW(
            nullptr,
            cmdBuf.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDir.c_str(),
            &si,
            &g_child);

        if (!ok)
        {
            MessageBoxW(hWnd, L"Failed to start the keyboard effect process.", L"Keyboard Effects", MB_ICONERROR | MB_OK);
            return false;
        }

        g_runningTheme = theme;
        InvalidateRect(hWnd, nullptr, FALSE);
        if (g_hList) InvalidateRect(g_hList, nullptr, TRUE);
        return true;
    }

    void SetRunAtStartup(bool enable)
    {
        HKEY hKey{};
        constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

        if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            if (enable)
            {
                const std::wstring exeCommand = L"\"" + GetModuleDir() + L"\\KeyboardEffectsUI.exe\" --autoplay";
                RegSetValueExW(hKey, L"KeyboardEffectsUI", 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(exeCommand.c_str()),
                    static_cast<DWORD>((exeCommand.size() + 1) * sizeof(wchar_t)));
            }
            else
            {
                RegDeleteValueW(hKey, L"KeyboardEffectsUI");
            }

            RegCloseKey(hKey);
        }
    }

    std::wstring GetSelectedTheme()
    {
        if (!g_hList)
        {
            return L"";
        }

        const int sel = static_cast<int>(SendMessageW(g_hList, LB_GETCURSEL, 0, 0));
        if (sel < 0 || sel >= static_cast<int>(kEffects.size()))
        {
            return L"";
        }

        return kEffects[sel].themeArg;
    }

    void ApplySelectedEffect(HWND hWnd, Settings& settings)
    {
        const auto theme = GetSelectedTheme();
        if (theme.empty())
        {
            return;
        }

        settings.lastTheme = theme;
        SaveSettings(settings);
        LaunchEffect(hWnd, theme);
    }

    // Subclass procedure for custom Owner-Drawn Buttons
    LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        switch (uMsg)
        {
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            int btnId = static_cast<int>(uIdSubclass);
            if (g_hoveredButtonId != btnId)
            {
                g_hoveredButtonId = btnId;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE:
            g_hoveredButtonId = 0;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ButtonSubclassProc, uIdSubclass);
            break;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Settings settings{};
    static NOTIFYICONDATAW nid{};

    switch (msg)
    {
    case WM_CREATE:
    {
        InitGdiResources();
        EnableDarkMode(hWnd);
        settings = LoadSettings();

        // Custom Owner-Drawn ListBox
        g_hList = CreateWindowExW(
            0,
            L"LISTBOX",
            nullptr,
            WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            16, 96, 412, 332,
            hWnd,
            reinterpret_cast<HMENU>(IDC_EFFECTS),
            reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance,
            nullptr
        );

        SendMessageW(g_hList, LB_SETITEMHEIGHT, 0, 40);

        for (const auto& effect : kEffects)
        {
            SendMessageW(g_hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(effect.label.c_str()));
        }
        SendMessageW(g_hList, LB_SETCURSEL, FindEffectIndex(settings.lastTheme), 0);

        // Buttons
        g_hApplyBtn = CreateWindowW(
            L"BUTTON", L"Apply Effect",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_TABSTOP,
            16, 442, 198, 38,
            hWnd, reinterpret_cast<HMENU>(IDC_APPLY), nullptr, nullptr
        );
        SetWindowSubclass(g_hApplyBtn, ButtonSubclassProc, IDC_APPLY, 0);

        g_hStopBtn = CreateWindowW(
            L"BUTTON", L"Stop",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_TABSTOP,
            230, 442, 198, 38,
            hWnd, reinterpret_cast<HMENU>(IDC_STOP), nullptr, nullptr
        );
        SetWindowSubclass(g_hStopBtn, ButtonSubclassProc, IDC_STOP, 0);

        g_hStartup = CreateWindowW(
            L"BUTTON", L"Run at Windows startup",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            18, 496, 250, 26,
            hWnd, reinterpret_cast<HMENU>(IDC_STARTUP), nullptr, nullptr
        );
        SendMessageW(g_hStartup, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFontSub), TRUE);
        SendMessageW(g_hStartup, BM_SETCHECK, settings.runAtStartup ? BST_CHECKED : BST_UNCHECKED, 0);

        g_hExitBtn = CreateWindowW(
            L"BUTTON", L"Exit App",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_TABSTOP,
            330, 492, 98, 30,
            hWnd, reinterpret_cast<HMENU>(IDC_EXIT), nullptr, nullptr
        );
        SetWindowSubclass(g_hExitBtn, ButtonSubclassProc, IDC_EXIT, 0);

        // Tray Icon
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"CLEVO Keyboard Effects");
        Shell_NotifyIconW(NIM_ADD, &nid);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT cr;
        GetClientRect(hWnd, &cr);

        // Memory DC for double-buffered window painting
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // 1. Fill Dark Window Background
        FillRect(memDC, &cr, g_hBrushWindowBg);

        // 2. Draw Modern Header Banner
        RECT rcHeader = { 16, 14, 428, 82 };
        DrawRoundedCard(memDC, rcHeader, 8, kClrHeaderBg, kClrCardBorder, 1);

        // Header Title
        SetBkMode(memDC, TRANSPARENT);
        SelectObject(memDC, g_hFontTitle);
        SetTextColor(memDC, kClrTextPrimary);
        RECT rcTitle = { 30, 22, 280, 44 };
        DrawTextW(memDC, L"CLEVO RGB Keyboard", -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

        // Header Subtitle
        SelectObject(memDC, g_hFontSub);
        SetTextColor(memDC, kClrTextSecondary);
        RECT rcSub = { 30, 44, 280, 62 };
        DrawTextW(memDC, L"Ultra-Lightweight Effects Controller", -1, &rcSub, DT_LEFT | DT_SINGLELINE);

        // Status Indicator Pill in Header
        const bool isRunning = !g_runningTheme.empty();
        COLORREF statusDotClr = isRunning ? kClrStatusActive : kClrStatusIdle;
        COLORREF statusCardBg = isRunning ? RGB(20, 42, 34) : RGB(30, 35, 46);
        COLORREF statusBorder = isRunning ? RGB(34, 90, 65) : RGB(46, 54, 70);

        RECT rcStatusCard = { 270, 24, 416, 56 };
        DrawRoundedCard(memDC, rcStatusCard, 6, statusCardBg, statusBorder, 1);

        // Status Dot
        HBRUSH hDotBrush = CreateSolidBrush(statusDotClr);
        HGDIOBJ oldDot = SelectObject(memDC, hDotBrush);
        SelectObject(memDC, GetStockObject(NULL_PEN));
        Ellipse(memDC, 282, 36, 292, 46);
        SelectObject(memDC, oldDot);
        DeleteObject(hDotBrush);

        // Status Text
        SelectObject(memDC, g_hFontStatus);
        SetTextColor(memDC, isRunning ? kClrStatusActive : kClrTextSecondary);
        RECT rcStatusText = { 298, 24, 410, 56 };
        std::wstring statusStr = isRunning ? L"Active" : L"Standby";
        if (isRunning)
        {
            if (const auto* eff = FindEffectByTheme(g_runningTheme))
            {
                statusStr = eff->label.substr(eff->label.find(L" ") + 1);
            }
        }
        DrawTextW(memDC, statusStr.c_str(), -1, &rcStatusText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Copy from memory DC to screen DC
        BitBlt(hdc, 0, 0, cr.right, cr.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = reinterpret_cast<HDC>(wParam);
        HWND hwndStatic = reinterpret_cast<HWND>(lParam);

        if (hwndStatic == g_hStartup)
        {
            SetTextColor(hdcStatic, kClrTextPrimary);
            SetBkColor(hdcStatic, kClrWindowBg);
            return reinterpret_cast<INT_PTR>(g_hBrushWindowBg);
        }
        break;
    }

    case WM_DRAWITEM:
    {
        auto* lpDrawItem = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (!lpDrawItem) return FALSE;

        // Custom Owner-Drawn ListBox Items
        if (lpDrawItem->CtlID == IDC_EFFECTS)
        {
            const int idx = lpDrawItem->itemID;
            if (idx < 0 || idx >= static_cast<int>(kEffects.size()))
            {
                return TRUE;
            }

            const bool isSelected = (lpDrawItem->itemState & ODS_SELECTED) != 0;
            const bool isRunningThis = (!g_runningTheme.empty() && kEffects[idx].themeArg == g_runningTheme);

            HDC hdc = lpDrawItem->hDC;
            RECT rc = lpDrawItem->rcItem;

            // Memory DC for flicker-free item drawing
            HDC memDC = CreateCompatibleDC(hdc);
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            RECT localRc = { 0, 0, w, h };
            RECT cardRc = { 2, 2, w - 2, h - 2 };

            // Fill background gap
            HBRUSH hGapBrush = CreateSolidBrush(kClrWindowBg);
            FillRect(memDC, &localRc, hGapBrush);
            DeleteObject(hGapBrush);

            // Item Card Background
            COLORREF bg = isSelected ? kClrItemSelected : kClrItemBg;
            COLORREF border = isSelected ? kClrItemSelBorder : kClrItemBorder;
            DrawRoundedCard(memDC, cardRc, 6, bg, border, 1);

            // Left Accent Strip for Selected item
            if (isSelected)
            {
                RECT accentRc = { cardRc.left, cardRc.top + 3, cardRc.left + 4, cardRc.bottom - 3 };
                HBRUSH hAccent = CreateSolidBrush(kClrAccentCyan);
                FillRect(memDC, &accentRc, hAccent);
                DeleteObject(hAccent);
            }

            SetBkMode(memDC, TRANSPARENT);

            // Title Line
            SelectObject(memDC, g_hFontItemTitle);
            SetTextColor(memDC, isSelected ? RGB(255, 255, 255) : kClrTextPrimary);
            RECT textRc = { cardRc.left + 12, cardRc.top + 4, cardRc.right - (isRunningThis ? 72 : 12), cardRc.top + 20 };
            DrawTextW(memDC, kEffects[idx].label.c_str(), -1, &textRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            // Description Line
            SelectObject(memDC, g_hFontItemDesc);
            SetTextColor(memDC, isSelected ? RGB(165, 205, 255) : kClrTextSecondary);
            RECT descRc = { cardRc.left + 12, cardRc.top + 20, cardRc.right - (isRunningThis ? 72 : 12), cardRc.bottom - 3 };
            DrawTextW(memDC, kEffects[idx].description.c_str(), -1, &descRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            // ACTIVE Badge on the right
            if (isRunningThis)
            {
                RECT badgeRc = { cardRc.right - 64, cardRc.top + 10, cardRc.right - 8, cardRc.bottom - 10 };
                DrawRoundedCard(memDC, badgeRc, 4, RGB(16, 185, 129), RGB(34, 197, 94), 1);
                SelectObject(memDC, g_hFontBadge);
                SetTextColor(memDC, RGB(255, 255, 255));
                DrawTextW(memDC, L"ACTIVE", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Blit to screen
            BitBlt(hdc, rc.left, rc.top, w, h, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            return TRUE;
        }

        // Custom Owner-Drawn Buttons (Apply / Stop / Exit)
        if (lpDrawItem->CtlType == ODT_BUTTON)
        {
            HDC hdc = lpDrawItem->hDC;
            RECT rc = lpDrawItem->rcItem;
            const bool isPressed = (lpDrawItem->itemState & ODS_SELECTED) != 0;
            const bool isHovered = (g_hoveredButtonId == static_cast<int>(lpDrawItem->CtlID));

            COLORREF bg = RGB(40, 45, 60);
            COLORREF border = RGB(60, 70, 90);
            COLORREF textClr = RGB(255, 255, 255);
            std::wstring text{};

            if (lpDrawItem->CtlID == IDC_APPLY)
            {
                text = L"Apply Effect";
                bg = isPressed ? kClrBtnApplyPress : (isHovered ? kClrBtnApplyHover : kClrBtnApply);
                border = bg;
            }
            else if (lpDrawItem->CtlID == IDC_STOP)
            {
                text = L"Stop";
                bg = isPressed ? kClrBtnStopPress : (isHovered ? kClrBtnStopHover : kClrBtnStop);
                border = bg;
            }
            else if (lpDrawItem->CtlID == IDC_EXIT)
            {
                text = L"Exit App";
                bg = isPressed ? kClrBtnExitPress : (isHovered ? kClrBtnExitHover : kClrBtnExit);
                border = isHovered ? RGB(90, 105, 130) : kClrBtnExitBorder;
                textClr = isHovered ? RGB(255, 255, 255) : RGB(209, 213, 219);
            }

            DrawRoundedCard(hdc, rc, 6, bg, border, 1);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hFontButton);
            SetTextColor(hdc, textClr);
            DrawTextW(hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            return TRUE;
        }

        break;
    }

    case WM_COMMAND:
    {
        const WORD id = LOWORD(wParam);
        const WORD code = HIWORD(wParam);

        if (id == IDC_EFFECTS && (code == LBN_SELCHANGE || code == LBN_DBLCLK))
        {
            if (code == LBN_DBLCLK)
            {
                ApplySelectedEffect(hWnd, settings);
            }
            else
            {
                InvalidateRect(g_hList, nullptr, FALSE);
            }
            return 0;
        }

        if (id == IDC_APPLY)
        {
            ApplySelectedEffect(hWnd, settings);
            return 0;
        }

        if (id == IDC_STOP)
        {
            StopCurrentEffect();
            InvalidateRect(hWnd, nullptr, FALSE);
            if (g_hList) InvalidateRect(g_hList, nullptr, TRUE);
            return 0;
        }

        if (id == IDC_EXIT)
        {
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == IDC_STARTUP)
        {
            settings.runAtStartup = (SendMessageW(g_hStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveSettings(settings);
            SetRunAtStartup(settings.runAtStartup);
            return 0;
        }

        return 0;
    }

    case WM_TRAYICON:
    {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"Show Window");
            AppendMenuW(hMenu, MF_STRING, 2, L"Stop Effect");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 3, L"Exit");

            POINT pt{};
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            const UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == 1)
            {
                ShowWindow(hWnd, SW_SHOWNORMAL);
                SetForegroundWindow(hWnd);
            }
            else if (cmd == 2)
            {
                StopCurrentEffect();
                InvalidateRect(hWnd, nullptr, FALSE);
                if (g_hList) InvalidateRect(g_hList, nullptr, TRUE);
            }
            else if (cmd == 3)
            {
                DestroyWindow(hWnd);
            }
        }
        else if (lParam == WM_LBUTTONDBLCLK)
        {
            ShowWindow(hWnd, SW_SHOWNORMAL);
            SetForegroundWindow(hWnd);
        }
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        StopCurrentEffect();
        FreeGdiResources();
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow)
{
    const bool autoPlay = (lpCmdLine && wcsstr(lpCmdLine, L"--autoplay"));

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"KeyboardEffectsUI_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex)
        {
            CloseHandle(hMutex);
        }
        return 0;
    }

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr; // Handled in WM_PAINT
    wc.lpszClassName = L"KeyboardEffectsUIClass";

    RegisterClassW(&wc);

    const int winW = 460;
    const int winH = 580;
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = std::max(0, (screenW - winW) / 2);
    const int posY = std::max(0, (screenH - winH) / 2);

    HWND hWnd = CreateWindowW(
        wc.lpszClassName,
        L"CLEVO Keyboard Effects",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
    {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    ShowWindow(hWnd, autoPlay ? SW_HIDE : nCmdShow);
    UpdateWindow(hWnd);

    if (autoPlay)
    {
        Settings settings = LoadSettings();
        LaunchEffect(hWnd, settings.lastTheme);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex)
    {
        CloseHandle(hMutex);
    }

    return static_cast<int>(msg.wParam);
}

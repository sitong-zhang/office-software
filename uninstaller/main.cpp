// Chenyi Office（胡辰逸定制版） uninstaller

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include "app/ui/common.h"
#include "app/ui/icons.h"
#include "app/ui/theme.h"

using namespace Gdiplus;

#include <string>
#include <vector>

static const wchar_t* kProduct = L"Chenyi Office（胡辰逸定制版）";

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static volatile LONG g_done = 0;
static volatile LONG g_incomplete = 0;
static volatile LONG g_stage = 0;
static volatile LONG g_finishedShown = 0;
static int g_dpi = 96;
static int g_width = 760;
static int g_height = 480;
static const int kBaseWidth = 760;
static const int kBaseHeight = 480;
static ULONG_PTR g_gdiplusToken = 0;
static int g_hover = 0;
static bool g_enginePreExisting = false;
static std::wstring g_installDir;
static std::wstring g_status = L"正在移除办公内核与程序文件…";

#define S(x) MulDiv((x), g_dpi, 96)
#define SIDEBAR S(220)
#define TITLEBAR S(56)

enum UninstallAction { UA_NONE = 0, UA_CLOSE, UA_MINIMIZE };

static ui::HitMap g_hits;

static void AddHit(const RectF& rect, int id) { g_hits.Add(rect, id); }

static int HitTest(POINT point) { return g_hits.Test(point, UA_NONE); }

static bool IsProcessRunning(const wchar_t* name) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, name) == 0) {
        found = true;
        break;
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
}

static bool DeleteTree(const std::wstring& path) {
  if (path.empty()) return true;
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return true;
  std::wstring from = path;
  from.push_back(L'\0');
  from.push_back(L'\0');
  SHFILEOPSTRUCTW op{};
  op.wFunc = FO_DELETE;
  op.pFrom = from.c_str();
  op.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_SILENT;
  return SHFileOperationW(&op) == 0;
}

static DWORD ReadDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD fallback = 0) {
  HKEY key = nullptr;
  DWORD value = fallback;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&value, &size) != ERROR_SUCCESS ||
        type != REG_DWORD) {
      value = fallback;
    }
    RegCloseKey(key);
  }
  return value;
}

static void DrawTextLine(HDC dc, const std::wstring& text, RECT rect, int size, int weight,
                         COLORREF color, const wchar_t* face, UINT flags) {
  HFONT font = CreateFontW(-S(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, face);
  HGDIOBJ old = SelectObject(dc, font);
  SetTextColor(dc, color);
  SetBkMode(dc, TRANSPARENT);
  DrawTextW(dc, text.c_str(), (int)text.size(), &rect, flags);
  SelectObject(dc, old);
  DeleteObject(font);
}

static bool RunAndWait(const std::wstring& command) {
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> buffer(command.begin(), command.end());
  buffer.push_back(0);
  if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &si, &pi))
    return false;
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

static DWORD WINAPI UninstallThread(LPVOID) {
  InterlockedExchange(&g_stage, 1);
  std::wstring productCode;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", 0, KEY_READ, &key) ==
      ERROR_SUCCESS) {
    wchar_t value[128] = {0};
    DWORD size = sizeof(value) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(key, L"ProductCode", nullptr, &type, (LPBYTE)value, &size) ==
            ERROR_SUCCESS &&
        type == REG_SZ)
      productCode = value;
    RegCloseKey(key);
  }

  // If the engine was already on the machine before Chenyi Office, it belongs
  // to the user: never uninstall it through our ProductCode.
  DWORD enginePreExisting =
      ReadDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePreExisting");
  if (!productCode.empty() && enginePreExisting == 0) {
    wchar_t systemDir[MAX_PATH] = {0};
    GetSystemDirectoryW(systemDir, MAX_PATH);
    RunAndWait(std::wstring(L"\"") + systemDir + L"\\msiexec.exe\" /x " + productCode +
               L" /qn /norestart");
  }
  InterlockedExchange(&g_stage, 2);

  wchar_t* publicDesktop = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, nullptr, &publicDesktop))) {
    DeleteFileW((std::wstring(publicDesktop) + L"\\" + kProduct + L".lnk").c_str());
    CoTaskMemFree(publicDesktop);
  }
  wchar_t* commonPrograms = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &commonPrograms))) {
    std::wstring dir = std::wstring(commonPrograms) + L"\\" + kProduct;
    DeleteFileW((dir + L"\\" + kProduct + L".lnk").c_str());
    RemoveDirectoryW(dir.c_str());
    CoTaskMemFree(commonPrograms);
  }

  // Remove the local profile: engine profile, seen announcements and logs
  // must not come back after a reinstall.
  InterlockedExchange(&g_stage, 3);
  bool profileDeleted = true;
  wchar_t* localAppData = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
    profileDeleted = DeleteTree(std::wstring(localAppData) + L"\\ChenyiOffice");
    CoTaskMemFree(localAppData);
  }

  RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ChenyiOffice");
  RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice");

  // Rename the running executable so the install directory can be removed.
  wchar_t temp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, temp);
  std::wstring self = temp;
  self += L"ChenyiOfficeUninstall.exe";
  wchar_t selfPath[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
  bool moved = MoveFileExW(selfPath, self.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
                   ? true
                   : false;

  InterlockedExchange(&g_stage, 4);
  bool installDeleted = DeleteTree(g_installDir);
  if (!profileDeleted || !installDeleted) InterlockedExchange(&g_incomplete, 1);

  if (moved) {
    MoveFileExW(self.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  } else {
    MoveFileExW(selfPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  }

  InterlockedExchange(&g_stage, 5);
  InterlockedExchange(&g_done, 1);
  return 0;
}

static void DrawWindowButton(Graphics& g, const RectF& rect, int icon, int action, bool dark) {
  ui::WindowButton(g, {g_dpi, g_hover}, g_hits, rect, icon, action, dark);
}

static void DrawSidebar(Graphics& g) {
  RectF side(0, 0, SIDEBAR, g_height);
  LinearGradientBrush background(side, ui::rgba(22, 50, 39), ui::rgba(10, 24, 19),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, side);

  ui::Image(g, 111, RectF(S(24), S(24), S(40), S(40)));
  ui::TextLatin(g, L"Chenyi Office", RectF(S(76), S(22), S(130), S(24)), S(14),
                FontStyleBold, ui::SidebarText());
  ui::Text(g, L"胡辰逸定制版", RectF(S(76), S(46), S(130), S(18)), S(9.5), FontStyleRegular,
           ui::SidebarMuted());
  ui::FillRound(g, RectF(S(24), S(86), S(172), 1), 0, ui::rgba(255, 255, 255, 18));

  ui::Text(g, L"卸载", RectF(S(24), S(122), S(170), S(34)), S(24), FontStyleBold,
           ui::SidebarText());
  ui::Text(g, L"移除 Chenyi Office 的程序文件、本地配置和快捷方式。",
           RectF(S(24), S(162), S(170), S(60)), S(9.5), FontStyleRegular, ui::SidebarMuted(),
           StringAlignmentNear, StringAlignmentNear, true);

  ui::Text(g, L"Chenyi Office · Windows x64", RectF(S(24), g_height - S(36), S(180), S(18)),
           S(8.5), FontStyleRegular, ui::SidebarMuted());
}

static void DrawScreen(Graphics& g) {
  g.Clear(ui::Paper());
  DrawSidebar(g);

  ui::Text(g, L"卸载 Chenyi Office", RectF(SIDEBAR + S(40), S(12), S(360), S(32)), S(17),
           FontStyleBold, ui::Text());
  ui::Text(g, L"Chenyi Office · 胡辰逸定制版", RectF(SIDEBAR + S(40), S(34), S(360), S(16)),
           S(10), FontStyleRegular, ui::TextMuted());
  ui::FillRound(g, RectF(SIDEBAR, TITLEBAR - 1, g_width - SIDEBAR, 1), 0, ui::Border());
  DrawWindowButton(g, RectF(g_width - S(86), S(14), S(28), S(28)), UI_ICON_MINIMIZE, UA_MINIMIZE,
                   false);

  bool done = InterlockedCompareExchange(&g_done, 0, 0) == 1;
  if (done) {
    bool incomplete = InterlockedCompareExchange(&g_incomplete, 0, 0) == 1;
    DrawWindowButton(g, RectF(g_width - S(52), S(14), S(28), S(28)), UI_ICON_CLOSE, UA_CLOSE,
                     false);
    RectF tile(SIDEBAR + S(40), S(96), S(64), S(64));
    ui::FillRound(g, tile, S(18), incomplete ? ui::DangerSoft() : ui::rgba(232, 244, 238));
    ui::Icon(g, incomplete ? UI_ICON_INFO : UI_ICON_CHECK_CIRCLE,
             incomplete ? ui::ICON_MUTED : ui::ICON_GREEN,
             RectF(tile.X + S(16), tile.Y + S(16), S(32), S(32)));
    ui::Text(g, incomplete ? L"卸载完成，但有残留" : L"卸载完成",
             RectF(tile.X + S(84), S(104), g_width - tile.X - S(124), S(40)), S(24),
             FontStyleBold, incomplete ? ui::Danger() : ui::Text());
    ui::Text(g,
             incomplete
                 ? L"部分文件正被占用，未能删除。请重启电脑后手动清理 ChenyiOffice 目录。"
                 : L"Chenyi Office 及其本地配置已从本机移除。",
             RectF(SIDEBAR + S(40), S(188), g_width - SIDEBAR - S(80), S(70)), S(11.5),
             FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
    if (g_enginePreExisting) {
      ui::Text(g, L"系统中原有的办公内核已保留。",
               RectF(SIDEBAR + S(40), S(268), g_width - SIDEBAR - S(80), S(22)), S(10),
               FontStyleRegular, ui::Green());
    }
    RectF closeButton(g_width - S(196), g_height - S(68), S(156), S(40));
    ui::FillRound(g, closeButton, S(10),
                  g_hover == UA_CLOSE ? ui::GreenLight() : ui::Green());
    ui::Text(g, L"关闭", closeButton, S(12.5), FontStyleBold, ui::Surface(),
             StringAlignmentCenter, StringAlignmentCenter);
    AddHit(closeButton, UA_CLOSE);
    return;
  }

  RectF tile(SIDEBAR + S(40), S(96), S(56), S(56));
  ui::FillRound(g, tile, S(16), ui::DangerSoft());
  ui::Icon(g, UI_ICON_TRASH, ui::ICON_MUTED, RectF(tile.X + S(14), tile.Y + S(14), S(28), S(28)));
  ui::Text(g, L"正在卸载", RectF(tile.X + S(76), S(100), g_width - tile.X - S(116), S(36)),
           S(24), FontStyleBold, ui::Text());
  ui::Text(g, g_status, RectF(SIDEBAR + S(40), S(164), g_width - SIDEBAR - S(80), S(22)), S(10.5),
           FontStyleRegular, ui::TextMuted());

  int stage = (int)InterlockedCompareExchange(&g_stage, 0, 0);
  const wchar_t* steps[] = {L"移除办公内核", L"清理本地配置", L"删除程序文件", L"完成"};
  float y = S(212);
  for (int i = 0; i < 4; ++i) {
    bool stepDone = stage > i + 1 || stage >= 5;
    bool active = stage == i + 1;
    RectF row(SIDEBAR + S(40), y, g_width - SIDEBAR - S(80), S(36));
    ui::FillRound(g, row, S(10), active ? ui::rgba(232, 244, 238) : ui::rgba(0, 0, 0, 0));
    ui::Icon(g, stepDone ? UI_ICON_CHECK_CIRCLE : UI_ICON_CLOCK,
             stepDone ? ui::ICON_GREEN : (active ? ui::ICON_GREEN : ui::ICON_SOFT),
             RectF(row.X + S(10), y + S(10), S(16), S(16)));
    ui::Text(g, steps[i], RectF(row.X + S(36), y, row.Width - S(36), S(36)), S(10.5),
             active ? FontStyleBold : FontStyleRegular,
             active ? ui::Text() : ui::TextMuted(), StringAlignmentNear, StringAlignmentCenter);
    y += S(40);
  }

  RectF note(SIDEBAR + S(40), S(386), g_width - SIDEBAR - S(80), S(50));
  ui::FillRound(g, note, S(12), ui::SurfaceMuted());
  ui::Icon(g, UI_ICON_INFO, ui::ICON_GREEN, RectF(note.X + S(14), note.Y + S(15), S(20), S(20)));
  ui::Text(g,
           g_enginePreExisting ? L"系统原有的办公内核会被保留，不会随本次卸载移除。"
                               : L"请勿关闭此窗口。完成后会提示是否还有残留文件。",
           RectF(note.X + S(44), note.Y + S(10), note.Width - S(58), S(30)), S(9.5),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_DPICHANGED: {
      g_dpi = HIWORD(wParam);
      if (g_dpi <= 0) g_dpi = 96;
      g_width = MulDiv(kBaseWidth, g_dpi, 96);
      g_height = MulDiv(kBaseHeight, g_dpi, 96);
      RECT* suggested = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, g_width, g_height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      ui::ApplyWindowChrome(hwnd, g_width, g_height, S(18), false);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      g_hits.Clear();
      Bitmap buffer(g_width, g_height, PixelFormat32bppPARGB);
      Graphics g(&buffer);
      g.SetSmoothingMode(SmoothingModeAntiAlias);
      g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
      DrawScreen(g);
      Graphics target(dc);
      target.DrawImage(&buffer, 0, 0, g_width, g_height);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER:
      if (InterlockedCompareExchange(&g_done, 0, 0) == 1 &&
          InterlockedCompareExchange(&g_finishedShown, 0, 0) == 0) {
        InterlockedExchange(&g_finishedShown, 1);
        KillTimer(hwnd, 1);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int hover = HitTest(point);
      if (hover != g_hover) {
        g_hover = hover;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      TRACKMOUSEEVENT track{};
      track.cbSize = sizeof(track);
      track.dwFlags = TME_LEAVE;
      track.hwndTrack = hwnd;
      TrackMouseEvent(&track);
      return 0;
    }
    case WM_MOUSELEAVE:
      if (g_hover != 0) {
        g_hover = 0;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int action = HitTest(point);
      if (action == UA_CLOSE) {
        PostQuitMessage(0);
        return 0;
      }
      if (action == UA_MINIMIZE) {
        ShowWindow(hwnd, SW_MINIMIZE);
        return 0;
      }
      if (point.y < TITLEBAR) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      }
      return 0;
    }
    case WM_CLOSE:
      if (InterlockedCompareExchange(&g_done, 0, 0) == 1) PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  g_instance = instance;
  SetProcessDPIAware();
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  HDC screen = GetDC(nullptr);
  g_dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen) ReleaseDC(nullptr, screen);
  if (g_dpi <= 0) g_dpi = 96;

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", 0, KEY_READ, &key) ==
      ERROR_SUCCESS) {
    wchar_t value[MAX_PATH] = {0};
    DWORD size = sizeof(value) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(key, L"InstallDir", nullptr, &type, (LPBYTE)value, &size) ==
            ERROR_SUCCESS &&
        type == REG_SZ)
      g_installDir = value;
    RegCloseKey(key);
  }

  DWORD enginePreExisting =
      ReadDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePreExisting");
  g_enginePreExisting = enginePreExisting != 0;
  const wchar_t* question =
      enginePreExisting == 0
          ? L"确定要卸载 Chenyi Office（胡辰逸定制版）及其办公内核吗？"
          : L"确定要卸载 Chenyi Office（胡辰逸定制版）吗？\n\n"
            L"系统中原有的办公内核会被保留。";
  int answer = MessageBoxW(nullptr, question, kProduct,
                           MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
  if (answer != IDYES) return 0;

  if (IsProcessRunning(L"soffice.bin") || IsProcessRunning(L"soffice.exe") ||
      IsProcessRunning(L"ChenyiOffice.exe")) {
    MessageBoxW(nullptr,
                L"Chenyi Office 或其办公内核仍在运行。\n\n"
                L"请先保存并关闭所有文档和程序，然后重新运行卸载器。",
                kProduct, MB_ICONWARNING | MB_OK);
    return 0;
  }

  ui::InitGdiplus(g_gdiplusToken);
  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ChenyiOfficeUninstaller";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);

  g_width = MulDiv(kBaseWidth, g_dpi, 96);
  g_height = MulDiv(kBaseHeight, g_dpi, 96);
  int x = (GetSystemMetrics(SM_CXSCREEN) - g_width) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - g_height) / 2;
  g_window = CreateWindowExW(0, wc.lpszClassName, kProduct, WS_POPUP, x, y, g_width, g_height,
                             nullptr, nullptr, instance, nullptr);
  if (!g_window) {
    ui::ShutdownGdiplus(g_gdiplusToken);
    return 1;
  }
  ui::ApplyWindowChrome(g_window, g_width, g_height, S(18), false);

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 250, nullptr);
  CreateThread(nullptr, 0, UninstallThread, nullptr, 0, nullptr);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  ui::ShutdownGdiplus(g_gdiplusToken);
  return 0;
}

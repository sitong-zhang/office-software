// Chenyi Office（胡辰逸定制版） uninstaller

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <string>
#include <vector>

static const wchar_t* kProduct = L"Chenyi Office（胡辰逸定制版）";

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static volatile LONG g_done = 0;
static volatile LONG g_incomplete = 0;
static int g_dpi = 96;
static std::wstring g_installDir;
static std::wstring g_status = L"正在移除办公内核与程序文件…";

#define S(x) MulDiv((x), g_dpi, 96)

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

  // Remove the local profile: LibreOffice profile, seen announcements and logs
  // must not come back after a reinstall.
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

  bool installDeleted = DeleteTree(g_installDir);
  if (!profileDeleted || !installDeleted) InterlockedExchange(&g_incomplete, 1);

  if (moved) {
    MoveFileExW(self.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  } else {
    MoveFileExW(selfPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  }

  InterlockedExchange(&g_done, 1);
  return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      RECT full{0, 0, S(600), S(300)};
      HBRUSH bg = CreateSolidBrush(RGB(247, 245, 239));
      FillRect(dc, &full, bg);
      DeleteObject(bg);

      RECT brand{S(48), S(42), S(540), S(66)};
      DrawTextLine(dc, L"CHENYI OFFICE · 胡辰逸定制版", brand, 12, FW_SEMIBOLD, RGB(31, 92, 70),
                   L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
      RECT title{S(48), S(92), S(550), S(132)};
      DrawTextLine(dc, L"正在卸载", title, 28, FW_SEMIBOLD, RGB(23, 23, 15),
                   L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
      RECT status{S(48), S(146), S(550), S(210)};
      DrawTextLine(dc, g_status, status, 13, FW_NORMAL, RGB(70, 70, 60),
                   L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);
      RECT foot{S(48), S(250), S(550), S(274)};
      DrawTextLine(dc, L"请勿关闭此窗口。", foot, 11, FW_NORMAL, RGB(120, 120, 106),
                   L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER:
      if (InterlockedCompareExchange(&g_done, 0, 0) == 1) {
        KillTimer(hwnd, 1);
        if (InterlockedCompareExchange(&g_incomplete, 0, 0) == 1) {
          MessageBoxW(hwnd,
                      L"Chenyi Office 已卸载，但部分文件正被占用，未能删除。\n\n"
                      L"请重启电脑后手动删除残留的 ChenyiOffice 目录，"
                      L"或关闭所有办公程序后重新运行卸载器。",
                      kProduct, MB_ICONWARNING | MB_OK);
        } else {
          MessageBoxW(hwnd, L"Chenyi Office（胡辰逸定制版）已卸载完成。", kProduct,
                      MB_ICONINFORMATION | MB_OK);
        }
        DestroyWindow(hwnd);
      }
      return 0;
    case WM_CLOSE:
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
  const wchar_t* question =
      enginePreExisting == 0
          ? L"确定要卸载 Chenyi Office（胡辰逸定制版）及其办公内核吗？"
          : L"确定要卸载 Chenyi Office（胡辰逸定制版）吗？\n\n"
            L"系统中原有的 LibreOffice 内核会被保留。";
  int answer = MessageBoxW(nullptr, question, kProduct,
                           MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
  if (answer != IDYES) return 0;

  if (IsProcessRunning(L"soffice.bin") || IsProcessRunning(L"soffice.exe") ||
      IsProcessRunning(L"ChenyiOffice.exe")) {
    MessageBoxW(nullptr,
                L"Chenyi Office 或 LibreOffice 仍在运行。\n\n"
                L"请先保存并关闭所有文档和程序，然后重新运行卸载器。",
                kProduct, MB_ICONWARNING | MB_OK);
    return 0;
  }

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ChenyiOfficeUninstaller";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);

  int width = S(600), height = S(300);
  int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
  g_window = CreateWindowExW(0, wc.lpszClassName, kProduct, WS_POPUP, x, y, width, height,
                             nullptr, nullptr, instance, nullptr);
  if (!g_window) return 1;

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 250, nullptr);
  CreateThread(nullptr, 0, UninstallThread, nullptr, 0, nullptr);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

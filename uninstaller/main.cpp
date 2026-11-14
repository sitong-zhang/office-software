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

#include <string>
#include <vector>

static const wchar_t* kProduct = L"Chenyi Office（胡辰逸定制版）";

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static volatile LONG g_done = 0;
static std::wstring g_installDir;
static std::wstring g_status = L"正在移除办公内核与程序文件…";

#define S(x) MulDiv((x), 96, 96)

static void DrawTextLine(HDC dc, const std::wstring& text, RECT rect, int size, int weight,
                         COLORREF color, const wchar_t* face, UINT flags) {
  HFONT font = CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
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

  if (!productCode.empty()) {
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

  if (!g_installDir.empty()) {
    DeleteFileW((g_installDir + L"\\ChenyiOffice.exe").c_str());
    DeleteFileW((g_installDir + L"\\Uninstall.exe").c_str());
    DeleteFileW((g_installDir + L"\\LICENSE").c_str());
    DeleteFileW((g_installDir + L"\\NOTICE").c_str());
    RemoveDirectoryW(g_installDir.c_str());
  }

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
      RECT full{0, 0, 600, 300};
      HBRUSH bg = CreateSolidBrush(RGB(247, 245, 239));
      FillRect(dc, &full, bg);
      DeleteObject(bg);

      RECT brand{48, 42, 540, 66};
      DrawTextLine(dc, L"CHENYI OFFICE · 胡辰逸定制版", brand, 12, FW_SEMIBOLD, RGB(31, 92, 70),
                   L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
      RECT title{48, 92, 550, 132};
      DrawTextLine(dc, L"正在卸载", title, 28, FW_SEMIBOLD, RGB(23, 23, 15),
                   L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
      RECT status{48, 146, 550, 210};
      DrawTextLine(dc, g_status, status, 13, FW_NORMAL, RGB(70, 70, 60),
                   L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);
      RECT foot{48, 250, 550, 274};
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
        MessageBoxW(hwnd, L"Chenyi Office（胡辰逸定制版）已卸载完成。", kProduct,
                    MB_ICONINFORMATION | MB_OK);
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
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  int answer = MessageBoxW(nullptr, L"确定要卸载 Chenyi Office（胡辰逸定制版）及其办公内核吗？",
                           kProduct, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
  if (answer != IDYES) return 0;

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

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ChenyiOfficeUninstaller";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);

  int width = 600, height = 300;
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

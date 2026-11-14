// Chenyi Office（胡辰逸定制版） installer
// Welcome gate with password, payload extraction, engine installation.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objidl.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if __has_include("product_code.h")
#include "product_code.h"
#endif
#ifndef MSI_PRODUCT_CODE
#define MSI_PRODUCT_CODE L""
#endif

static const wchar_t* kProduct = L"Chenyi Office（胡辰逸定制版）";
static const wchar_t* kVersion = L"0.1.5";
// SHA-256 of the install password; the plain password is not stored in source.
// This is a distribution gate, not a security boundary: the installer payload
// can always be extracted by someone who obtains the file.
static const wchar_t* kPasswordHash =
    L"d19e5dbfd3d3afe04f12e4f5c12085065e973246970d42608186ab4d20d4a9c5";

enum Screen { WELCOME = 0, INSTALLING = 1, FINISH = 2, FAILED = 3 };

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static HWND g_edit = nullptr;
static WNDPROC g_editProc = nullptr;
static HBITMAP g_background = nullptr;
static HICON g_icon = nullptr;
static int g_dpi = 96;
static int g_width = 900;
static int g_height = 600;
static Screen g_screen = WELCOME;
static bool g_passwordError = false;
static bool g_enginePreExisting = false;
static bool g_updateMode = false;
static std::wstring g_error;
static std::wstring g_installDir;

static volatile LONG g_stage = 0;
static volatile LONG g_state = 0;  // 0 idle, 1 working, 2 done, 3 failed
static volatile LONG g_extractPercent = 0;
static volatile LONG g_marquee = 0;

#define S(x) MulDiv((x), g_dpi, 96)

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void FillColor(HDC dc, const RECT& r, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &r, brush);
  DeleteObject(brush);
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

static RECT ButtonRect() {
  RECT r;
  r.right = S(840);
  r.left = S(640);
  r.bottom = S(562);
  r.top = S(516);
  return r;
}

static bool ButtonVisible() {
  return g_screen != INSTALLING;
}

static void DrawButton(HDC dc, const wchar_t* label) {
  RECT r = ButtonRect();
  FillColor(dc, r, RGB(244, 241, 232));
  DrawTextLine(dc, label, r, 15, FW_SEMIBOLD, RGB(16, 32, 26), L"Microsoft YaHei UI",
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawBrand(HDC dc) {
  if (g_icon)
    DrawIconEx(dc, S(56), S(38), g_icon, S(52), S(52), 0, nullptr, DI_NORMAL);
  RECT word{S(120), S(42), S(420), S(66)};
  DrawTextLine(dc, L"CHENYI OFFICE", word, 14, FW_SEMIBOLD, RGB(23, 23, 15), L"Segoe UI",
               DT_LEFT | DT_SINGLELINE);
  RECT edition{S(120), S(68), S(420), S(90)};
  DrawTextLine(dc, L"胡辰逸定制版", edition, 11, FW_NORMAL, RGB(31, 92, 70),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
}

static void DrawWelcome(HDC dc) {
  DrawBrand(dc);

  RECT eyebrow{S(56), S(112), S(600), S(136)};
  DrawTextLine(dc, L"欢迎使用", eyebrow, 15, FW_NORMAL, RGB(95, 95, 82), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  RECT title{S(56), S(138), S(600), S(192)};
  DrawTextLine(dc, L"Chenyi Office", title, 40, FW_SEMIBOLD, RGB(23, 23, 15), L"Segoe UI",
               DT_LEFT | DT_SINGLELINE);

  RECT edition{S(56), S(194), S(600), S(222)};
  DrawTextLine(dc, L"胡辰逸定制版 · Windows 10 / 11 64 位", edition, 16, FW_NORMAL,
               RGB(31, 92, 70), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT intro{S(56), S(240), S(580), S(300)};
  DrawTextLine(dc,
               L"一套以界面为先的办公套件：重新设计的启动中心与工作区、专属开屏动画、"
               L"应用内公告，以及发布即生效的强制更新。文档、表格、演示与 PDF 都在同一个安静的工作空间里完成。",
               intro, 14, FW_NORMAL, RGB(70, 70, 60), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);

  struct Feature {
    const wchar_t* title;
    const wchar_t* text;
  };
  Feature features[3] = {
      {L"界面重构", L"统一视觉语言，减少干扰，文档优先。"},
      {L"开屏动画", L"启动同时检查新版本与公告。"},
      {L"强制更新", L"发布新版本后，始终运行最新版。"},
  };
  int y = 316;
  for (const auto& f : features) {
    RECT dot{S(58), S(y + 9), S(64), S(y + 15)};
    FillColor(dc, dot, RGB(88, 184, 146));
    RECT line{S(76), S(y), S(580), S(y + 28)};
    DrawTextLine(dc, std::wstring(f.title) + L" — " + f.text, line, 13, FW_NORMAL,
                 RGB(70, 70, 60), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
    y += 34;
  }

  if (g_updateMode) {
    RECT updateHint{S(56), S(430), S(600), S(486)};
    DrawTextLine(dc, L"检测到已安装的 Chenyi Office，本次为覆盖更新：无需密码，"
                     L"直接点击右下角即可开始。",
                 updateHint, 13, FW_NORMAL, RGB(31, 92, 70), L"Microsoft YaHei UI",
                 DT_LEFT | DT_WORDBREAK);
  } else {
    RECT label{S(56), S(430), S(400), S(452)};
    DrawTextLine(dc, L"安装密码", label, 13, FW_SEMIBOLD, RGB(23, 23, 15),
                 L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

    RECT hint{S(56), S(504), S(580), S(530)};
    if (g_passwordError) {
      DrawTextLine(dc, L"密码错误，无法继续安装。请重新输入。", hint, 12, FW_NORMAL,
                   RGB(184, 72, 58), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
    } else {
      DrawTextLine(dc, L"请输入安装密码后点击右下角继续。", hint, 12, FW_NORMAL,
                   RGB(110, 110, 96), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);
    }
  }

  if (g_enginePreExisting && !g_updateMode) {
    RECT engineHint{S(56), S(532), S(600), S(554)};
    DrawTextLine(dc, L"检测到系统已有 LibreOffice：将复用它作为内核，卸载时不会移除它。",
                 engineHint, 10, FW_NORMAL, RGB(150, 96, 44), L"Microsoft YaHei UI",
                 DT_LEFT | DT_SINGLELINE);
  }

  RECT footer{S(56), S(560), S(600), S(584)};
  DrawTextLine(dc, L"基于 LibreOffice 技术构建 · Mozilla Public License 2.0", footer, 10,
               FW_NORMAL, RGB(120, 120, 106), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  DrawButton(dc, g_updateMode ? L"立即更新" : L"下一步确认");
}

static void DrawInstalling(HDC dc) {
  DrawBrand(dc);

  RECT title{S(56), S(120), S(820), S(168)};
  DrawTextLine(dc, L"正在安装 Chenyi Office", title, 30, FW_SEMIBOLD, RGB(23, 23, 15),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  const wchar_t* stage = L"正在准备…";
  switch (InterlockedCompareExchange(&g_stage, 0, 0)) {
    case 1: stage = L"正在解压办公内核…"; break;
    case 2: stage = L"正在安装办公内核，请稍候…"; break;
    case 3: stage = L"正在写入程序文件…"; break;
    case 4: stage = L"正在创建快捷方式…"; break;
    case 5: stage = L"正在完成安装…"; break;
    default: break;
  }
  RECT stageText{S(56), S(184), S(820), S(212)};
  DrawTextLine(dc, stage, stageText, 15, FW_NORMAL, RGB(70, 70, 60), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  int percent = (int)InterlockedCompareExchange(&g_extractPercent, 0, 0);
  RECT track{S(56), S(246), S(580), S(254)};
  FillColor(dc, track, RGB(220, 215, 201));
  if (InterlockedCompareExchange(&g_stage, 0, 0) == 1) {
    int width = (track.right - track.left) * percent / 100;
    if (width > 0) {
      RECT fill{track.left, track.top, track.left + width, track.bottom};
      FillColor(dc, fill, RGB(31, 92, 70));
    }
    wchar_t pct[16];
    swprintf(pct, 16, L"%d%%", percent);
    RECT pctRect{S(596), S(232), S(700), S(268)};
    DrawTextLine(dc, pct, pctRect, 13, FW_SEMIBOLD, RGB(31, 92, 70), L"Consolas",
                 DT_LEFT | DT_SINGLELINE);
  } else {
    int offset = (int)InterlockedCompareExchange(&g_marquee, 0, 0);
    int width = S(120);
    int start = S(56) + (offset % (S(464) + width));
    int left = std::max((int)S(56), start - width);
    int right = std::min((int)S(580), start);
    if (right > left) {
      RECT fill{left, track.top, right, track.bottom};
      FillColor(dc, fill, RGB(31, 92, 70));
    }
  }

  RECT note{S(56), S(300), S(580), S(380)};
  DrawTextLine(dc,
               L"安装过程中请勿关闭窗口。办公内核体积较大，首次安装通常需要几分钟。",
               note, 12, FW_NORMAL, RGB(110, 110, 96), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);
}

static void DrawFinish(HDC dc) {
  DrawBrand(dc);

  RECT circle{S(56), S(118), S(104), S(166)};
  FillColor(dc, circle, RGB(31, 92, 70));
  RECT checkRect{S(62), S(130), S(98), S(160)};
  DrawTextLine(dc, L"✓", checkRect, 20, FW_BOLD, RGB(244, 241, 232), L"Segoe UI",
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  RECT title{S(120), S(120), S(820), S(164)};
  DrawTextLine(dc, L"安装完成", title, 30, FW_SEMIBOLD, RGB(23, 23, 15),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT body{S(56), S(200), S(580), S(290)};
  DrawTextLine(dc,
               L"Chenyi Office（胡辰逸定制版）已安装到本机。首次启动会执行开屏动画、"
               L"检查最新版本并读取应用内公告，完成后进入办公套件。",
               body, 14, FW_NORMAL, RGB(70, 70, 60), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);

  RECT footer{S(56), S(560), S(600), S(584)};
  DrawTextLine(dc, L"安装位置：" + g_installDir, footer, 10, FW_NORMAL, RGB(120, 120, 106),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

  DrawButton(dc, L"打开应用");
}

static void DrawFailed(HDC dc) {
  DrawBrand(dc);

  RECT title{S(56), S(120), S(820), S(168)};
  DrawTextLine(dc, L"安装失败", title, 30, FW_SEMIBOLD, RGB(150, 56, 44),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT msg{S(56), S(186), S(600), S(320)};
  DrawTextLine(dc, g_error, msg, 14, FW_NORMAL, RGB(70, 70, 60), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);

  RECT hint{S(56), S(340), S(600), S(400)};
  DrawTextLine(dc, L"可以修正问题后重试；已安装的部分会被重新配置。", hint, 12, FW_NORMAL,
               RGB(110, 110, 96), L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);

  DrawButton(dc, L"重试");
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

static std::wstring SelfPath() {
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return path;
}

static std::string WideToUtf8(const std::wstring& input) {
  int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), nullptr, 0, nullptr,
                                 nullptr);
  std::string bytes((size_t)size, '\0');
  if (size > 0) {
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), bytes.data(), size, nullptr,
                        nullptr);
  }
  return bytes;
}

static bool PasswordMatches(const std::wstring& input) {
  std::string bytes;
  int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), nullptr, 0, nullptr,
                                 nullptr);
  bytes.resize((size_t)size);
  WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), &bytes[0], size, nullptr,
                      nullptr);

  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectSize = 0, result = 0;
  std::vector<BYTE> object, digest(32);
  bool ok = false;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
  if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectSize, sizeof(objectSize),
                        &result, 0) < 0)
    goto cleanup;
  object.resize(objectSize);
  if (BCryptCreateHash(alg, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) goto cleanup;
  if (BCryptHashData(hash, (PUCHAR)bytes.data(), (ULONG)bytes.size(), 0) < 0) goto cleanup;
  if (BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) < 0) goto cleanup;
  {
    static const wchar_t* digits = L"0123456789abcdef";
    std::wstring hex;
    for (BYTE b : digest) {
      hex.push_back(digits[b >> 4]);
      hex.push_back(digits[b & 0xF]);
    }
    ok = _wcsicmp(hex.c_str(), kPasswordHash) == 0;
  }

cleanup:
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  return ok;
}

static std::wstring TempDirectory() {
  wchar_t temp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, temp);
  std::wstring dir = std::wstring(temp) + L"ChenyiOfficeSetup";
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  return dir;
}

static bool WriteResourceFile(int id, const std::wstring& path) {
  HRSRC res = FindResourceW(g_instance, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!res) return false;
  HGLOBAL data = LoadResource(g_instance, res);
  DWORD size = SizeofResource(g_instance, res);
  void* bytes = LockResource(data);
  if (!bytes) return false;
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  bool ok = WriteFile(file, bytes, size, &written, nullptr) && written == size;
  CloseHandle(file);
  return ok;
}

static bool RunAndWait(const std::wstring& command, DWORD& exitCode) {
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> buffer(command.begin(), command.end());
  buffer.push_back(0);
  if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &si, &pi))
    return false;
  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

static bool CreateShortcut(const std::wstring& linkPath, const std::wstring& target,
                           const std::wstring& description) {
  IShellLinkW* link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                              (void**)&link)))
    return false;
  link->SetPath(target.c_str());
  link->SetDescription(description.c_str());
  std::wstring dir = target.substr(0, target.find_last_of(L'\\'));
  link->SetWorkingDirectory(dir.c_str());
  link->SetIconLocation(target.c_str(), 0);

  IPersistFile* file = nullptr;
  bool ok = false;
  if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
    ok = SUCCEEDED(file->Save(linkPath.c_str(), TRUE));
    file->Release();
  }
  link->Release();
  return ok;
}

static void SetString(HKEY root, const wchar_t* subkey, const wchar_t* name,
                      const std::wstring& value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) ==
      ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)value.c_str(),
                   (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
  }
}

static void SetDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) ==
      ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    RegCloseKey(key);
  }
}

static std::wstring GetString(HKEY root, const wchar_t* subkey, const wchar_t* name) {
  HKEY key = nullptr;
  std::wstring value;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    wchar_t buffer[1024] = {0};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS &&
        type == REG_SZ) {
      value = buffer;
    }
    RegCloseKey(key);
  }
  return value;
}

static DWORD GetDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD fallback = 0) {
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

// ---------------------------------------------------------------------------
// Installation worker
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct PayloadFooter {
  char magic[8];
  uint64_t offset;
  uint64_t size;
};
#pragma pack(pop)

static void ApplyEngineBranding(const std::wstring& programFiles) {
  std::wstring program = programFiles + L"\\LibreOffice\\program";
  std::wstring shell = program + L"\\shell";
  if (GetFileAttributesW(program.c_str()) == INVALID_FILE_ATTRIBUTES) return;

  WriteResourceFile(150, program + L"\\intro.png");
  WriteResourceFile(151, program + L"\\intro-highres.png");
  WriteResourceFile(152, shell + L"\\logo.svg");
  WriteResourceFile(153, shell + L"\\logo-sc.svg");
  WriteResourceFile(154, shell + L"\\about.svg");
}

static void Fail(const std::wstring& message) {
  g_error = message;
  InterlockedExchange(&g_state, 3);
}

static bool EngineAlreadyInstalled() {
  std::wstring dir = GetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"InstallDir");
  if (dir.empty()) return false;
  // A stale registry value alone must not turn a fresh install into a
  // passwordless update.
  return GetFileAttributesW((dir + L"\\ChenyiOffice.exe").c_str()) != INVALID_FILE_ATTRIBUTES;
}

static std::wstring DefaultEnginePath() {
  wchar_t programFiles[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
  if (!programFiles[0]) return L"";
  return std::wstring(programFiles) + L"\\LibreOffice\\program\\soffice.exe";
}

// A "foreign" engine is a LibreOffice installation that existed before Chenyi
// Office was installed. We reuse it, never brand it, and never uninstall it.
static bool DetectForeignEngine() {
  if (EngineAlreadyInstalled()) {
    return GetDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePreExisting") != 0;
  }
  std::wstring probe = DefaultEnginePath();
  return !probe.empty() && GetFileAttributesW(probe.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static DWORD WINAPI InstallThreadProc(LPVOID);

static int RunSilentUpdate() {
  // Silent mode is reserved for updates. A fresh install still requires the
  // distribution password in the normal UI.
  if (!EngineAlreadyInstalled()) return 2;

  DWORD threadId = 0;
  HANDLE thread = CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, &threadId);
  if (!thread) return 3;
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);

  if (InterlockedCompareExchange(&g_state, 0, 0) != 2) {
    wchar_t temp[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, temp);
    std::wstring log = std::wstring(temp) + L"ChenyiOffice-update-error.txt";
    FILE* file = _wfopen(log.c_str(), L"wb");
    if (file) {
      std::string text = WideToUtf8(g_error.empty() ? L"更新失败" : g_error);
      fwrite(text.data(), 1, text.size(), file);
      fclose(file);
    }
    return 1;
  }
  return 0;
}

static DWORD WINAPI InstallThreadProc(LPVOID) {
  std::wstring temp = TempDirectory();
  std::wstring msi = temp + L"\\LibreOffice.msi";

  // Extract the appended MSI payload.
  InterlockedExchange(&g_stage, 1);
  InterlockedExchange(&g_extractPercent, 0);

  HANDLE self = CreateFileW(SelfPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (self == INVALID_HANDLE_VALUE) {
    Fail(L"无法读取安装包自身。");
    return 0;
  }
  LARGE_INTEGER fileSize{};
  GetFileSizeEx(self, &fileSize);
  uint64_t totalSize = (uint64_t)fileSize.QuadPart;
  if (totalSize < sizeof(PayloadFooter) + 1) {
    CloseHandle(self);
    Fail(L"安装包不完整，请重新下载。");
    return 0;
  }

  // The payload footer sits before any Authenticode certificate table.
  // Scan the tail for the magic instead of assuming the last 24 bytes.
  const uint64_t scanSize = std::min<uint64_t>(totalSize, 1024 * 1024);
  std::vector<char> tail((size_t)scanSize);
  LARGE_INTEGER tailPos{};
  tailPos.QuadPart = (LONGLONG)(totalSize - scanSize);
  SetFilePointerEx(self, tailPos, nullptr, FILE_BEGIN);
  DWORD tailRead = 0;
  if (!ReadFile(self, tail.data(), (DWORD)scanSize, &tailRead, nullptr) ||
      tailRead != scanSize) {
    CloseHandle(self);
    Fail(L"安装包读取失败，请重新下载。");
    return 0;
  }

  PayloadFooter footer{};
  bool found = false;
  for (size_t i = 0; i + sizeof(PayloadFooter) <= tail.size(); ++i) {
    if (memcmp(tail.data() + i, "CHNYSET1", 8) != 0) continue;
    PayloadFooter candidate{};
    memcpy(&candidate, tail.data() + i, sizeof(candidate));
    uint64_t footerPos = totalSize - scanSize + i;
    if (candidate.size > 0 && candidate.offset > 0 &&
        candidate.offset + candidate.size == footerPos) {
      footer = candidate;
      found = true;
      break;
    }
  }
  if (!found) {
    CloseHandle(self);
    Fail(L"安装包数据损坏，请重新下载。");
    return 0;
  }

  LARGE_INTEGER payloadPos{};
  payloadPos.QuadPart = (LONGLONG)footer.offset;
  SetFilePointerEx(self, payloadPos, nullptr, FILE_BEGIN);
  HANDLE out = CreateFileW(msi.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    CloseHandle(self);
    Fail(L"无法写入临时文件，请检查磁盘空间。");
    return 0;
  }

  std::vector<BYTE> buffer(1024 * 1024);
  uint64_t remaining = footer.size;
  uint64_t done = 0;
  bool copyOk = true;
  while (remaining > 0) {
    DWORD chunk = (DWORD)std::min<uint64_t>(remaining, buffer.size());
    DWORD got = 0, wrote = 0;
    if (!ReadFile(self, buffer.data(), chunk, &got, nullptr) || got == 0 ||
        !WriteFile(out, buffer.data(), got, &wrote, nullptr) || wrote != got) {
      copyOk = false;
      break;
    }
    remaining -= got;
    done += got;
    InterlockedExchange(&g_extractPercent, (LONG)((done * 100) / footer.size));
  }
  CloseHandle(out);
  CloseHandle(self);
  if (!copyOk) {
    DeleteFileW(msi.c_str());
    Fail(L"解压安装数据失败，请检查磁盘空间。");
    return 0;
  }

  // Install the office engine.
  InterlockedExchange(&g_stage, 2);
  wchar_t programFiles[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
  bool enginePreExisting = DetectForeignEngine();
  g_enginePreExisting = enginePreExisting;

  // Never run the MSI against a LibreOffice the user already had: with the
  // same ProductCode that becomes a repair/upgrade of their installation.
  if (!enginePreExisting) {
    wchar_t systemDir[MAX_PATH] = {0};
    GetSystemDirectoryW(systemDir, MAX_PATH);
    std::wstring msiexec = std::wstring(systemDir) + L"\\msiexec.exe";
    std::wstring command = L"\"" + msiexec + L"\" /i \"" + msi + L"\" /qn /norestart";
    DWORD exitCode = 1;
    if (!RunAndWait(command, exitCode)) {
      Fail(L"无法启动办公内核安装程序。");
      return 0;
    }
    if (exitCode != 0 && exitCode != 3010) {
      wchar_t buf[256];
      swprintf(buf, 256, L"办公内核安装失败（错误代码 %lu）。", exitCode);
      Fail(buf);
      return 0;
    }
  }

  // Copy application files.
  InterlockedExchange(&g_stage, 3);
  g_installDir = std::wstring(programFiles) + L"\\ChenyiOffice";
  SHCreateDirectoryExW(nullptr, g_installDir.c_str(), nullptr);

  bool filesOk = WriteResourceFile(102, g_installDir + L"\\ChenyiOffice.exe") &&
                 WriteResourceFile(103, g_installDir + L"\\Uninstall.exe") &&
                 WriteResourceFile(104, g_installDir + L"\\LICENSE") &&
                 WriteResourceFile(105, g_installDir + L"\\NOTICE");
  if (!filesOk) {
    Fail(L"无法写入程序文件，请以管理员身份重试。");
    return 0;
  }

  // Only brand an engine we installed ourselves; a pre-existing LibreOffice
  // keeps its original splash and About artwork.
  if (!enginePreExisting) ApplyEngineBranding(std::wstring(programFiles));

  // Shell integration.
  InterlockedExchange(&g_stage, 4);
  wchar_t* publicDesktop = nullptr;
  wchar_t* commonPrograms = nullptr;
  std::wstring desktopLink, startLink, startDir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, nullptr, &publicDesktop))) {
    desktopLink = std::wstring(publicDesktop) + L"\\" + kProduct + L".lnk";
    CoTaskMemFree(publicDesktop);
  }
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &commonPrograms))) {
    startDir = std::wstring(commonPrograms) + L"\\" + kProduct;
    SHCreateDirectoryExW(nullptr, startDir.c_str(), nullptr);
    startLink = startDir + L"\\" + kProduct + L".lnk";
    CoTaskMemFree(commonPrograms);
  }

  std::wstring launcher = g_installDir + L"\\ChenyiOffice.exe";
  if (!desktopLink.empty()) CreateShortcut(desktopLink, launcher, kProduct);
  if (!startLink.empty()) CreateShortcut(startLink, launcher, kProduct);

  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"InstallDir", g_installDir);
  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"Version", kVersion);
  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"ProductCode", MSI_PRODUCT_CODE);
  SetDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePreExisting",
           enginePreExisting ? 1 : 0);
  std::wstring engine = DefaultEnginePath();
  if (!engine.empty() && GetFileAttributesW(engine.c_str()) != INVALID_FILE_ATTRIBUTES)
    SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePath", engine);

  const wchar_t* uninstallKey =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ChenyiOffice";
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayName", kProduct);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayVersion", kVersion);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"Publisher", L"Chenyi Office contributors");
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"InstallLocation", g_installDir);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayIcon", launcher);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"UninstallString",
            L"\"" + g_installDir + L"\\Uninstall.exe\"");
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"EstimatedSize", 1200000);
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"NoModify", 1);
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"NoRepair", 1);

  InterlockedExchange(&g_stage, 5);
  DeleteFileW(msi.c_str());
  InterlockedExchange(&g_state, 2);
  return 0;
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

static void Submit() {
  if (g_screen != WELCOME) return;
  // The password only guards the first installation. Once Chenyi Office is
  // installed, updates (manual or silent) never ask for it again.
  if (!g_updateMode) {
    wchar_t text[64] = {0};
    if (g_edit) GetWindowTextW(g_edit, text, 64);
    if (!PasswordMatches(text)) {
      g_passwordError = true;
      if (g_edit) {
        SetWindowTextW(g_edit, L"");
        SetFocus(g_edit);
      }
      InvalidateRect(g_window, nullptr, FALSE);
      return;
    }
  }
  g_passwordError = false;
  g_screen = INSTALLING;
  if (g_edit) ShowWindow(g_edit, SW_HIDE);
  SetFocus(g_window);
  InterlockedExchange(&g_stage, 0);
  InterlockedExchange(&g_state, 1);
  CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, nullptr);
  InvalidateRect(g_window, nullptr, FALSE);
}

static void HandleButton() {
  switch (g_screen) {
    case WELCOME:
      Submit();
      break;
    case FINISH: {
      std::wstring launcher = g_installDir + L"\\ChenyiOffice.exe";
      ShellExecuteW(nullptr, L"open", launcher.c_str(), nullptr, g_installDir.c_str(),
                    SW_SHOWNORMAL);
      PostQuitMessage(0);
      break;
    }
    case FAILED:
      g_error.clear();
      g_screen = INSTALLING;
      InterlockedExchange(&g_stage, 0);
      InterlockedExchange(&g_state, 1);
      InterlockedExchange(&g_extractPercent, 0);
      CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, nullptr);
      InvalidateRect(g_window, nullptr, FALSE);
      break;
    default:
      break;
  }
}

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
    Submit();
    return 0;
  }
  if (msg == WM_CHAR && (wParam == L'\r' || wParam == L'\n')) return 0;
  return CallWindowProcW(g_editProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return 0;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      HDC mem = CreateCompatibleDC(dc);
      HBITMAP bitmap = CreateCompatibleBitmap(dc, g_width, g_height);
      HGDIOBJ old = SelectObject(mem, bitmap);

      FillColor(mem, RECT{0, 0, g_width, g_height}, RGB(247, 245, 239));

      if (g_background) {
        HDC bgDc = CreateCompatibleDC(mem);
        HGDIOBJ oldBg = SelectObject(bgDc, g_background);
        BITMAP bm{};
        GetObjectW(g_background, sizeof(bm), &bm);
        SetStretchBltMode(mem, HALFTONE);
        StretchBlt(mem, 0, 0, g_width, g_height, bgDc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        SelectObject(bgDc, oldBg);
        DeleteDC(bgDc);
      }

      switch (g_screen) {
        case WELCOME: DrawWelcome(mem); break;
        case INSTALLING: DrawInstalling(mem); break;
        case FINISH: DrawFinish(mem); break;
        case FAILED: DrawFailed(mem); break;
      }

      BitBlt(dc, 0, 0, g_width, g_height, mem, 0, 0, SRCCOPY);
      SelectObject(mem, old);
      DeleteObject(bitmap);
      DeleteDC(mem);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_TIMER: {
      if (g_screen == INSTALLING) {
        LONG state = InterlockedCompareExchange(&g_state, 0, 0);
        if (state == 2) {
          g_screen = FINISH;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (state == 3) {
          g_screen = FAILED;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        LONG m = InterlockedCompareExchange(&g_marquee, 0, 0);
        InterlockedExchange(&g_marquee, m + 8);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }

    case WM_LBUTTONDOWN: {
      if (!ButtonVisible()) return 0;
      POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      RECT button = ButtonRect();
      if (PtInRect(&button, p)) HandleButton();
      return 0;
    }

    case WM_KEYDOWN:
      if (wParam == VK_RETURN && ButtonVisible()) HandleButton();
      return 0;

    case WM_CLOSE:
      if (g_screen == INSTALLING) return 0;
      PostQuitMessage(0);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
  g_instance = instance;
  SetProcessDPIAware();
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  HDC screen = GetDC(nullptr);
  g_dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen) ReleaseDC(nullptr, screen);
  if (g_dpi <= 0) g_dpi = 96;
  g_width = S(900);
  g_height = S(600);

  // Silent mode is the update path used by the launcher: no UI, no password.
  bool silent = commandLine && (wcsstr(commandLine, L"/S") || wcsstr(commandLine, L"--silent"));
  if (silent) return RunSilentUpdate();

  // Re-running the installer over an existing installation is an update, not a
  // fresh install, so the distribution password is not requested.
  g_updateMode = EngineAlreadyInstalled();
  g_enginePreExisting = DetectForeignEngine();

  g_background = (HBITMAP)LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_BITMAP, 0, 0,
                                     LR_CREATEDIBSECTION);
  g_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, S(64), S(64),
                             LR_DEFAULTCOLOR);

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ChenyiOfficeSetup";
  wc.hIcon = g_icon;
  RegisterClassW(&wc);

  int x = (GetSystemMetrics(SM_CXSCREEN) - g_width) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - g_height) / 2;
  g_window = CreateWindowExW(0, wc.lpszClassName, kProduct, WS_POPUP, x, y, g_width, g_height,
                             nullptr, nullptr, instance, nullptr);
  if (!g_window) return 1;

  if (!g_updateMode) {
    g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL | ES_LEFT,
                             S(56), S(454), S(280), S(40), g_window, (HMENU)1001, instance,
                             nullptr);
    HFONT editFont =
        CreateFontW(-S(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    SendMessageW(g_edit, WM_SETFONT, (WPARAM)editFont, TRUE);
    SendMessageW(g_edit, EM_SETLIMITTEXT, 64, 0);
    g_editProc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)EditProc);
  }

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 30, nullptr);
  SetFocus(g_edit ? g_edit : g_window);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

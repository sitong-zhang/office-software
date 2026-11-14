// Chenyi Office（胡辰逸定制版） launcher
// Splash, mandatory update gate, in-app announcements, engine start.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Product configuration
// ---------------------------------------------------------------------------

static const wchar_t* kProductName = L"Chenyi Office（胡辰逸定制版）";
static const wchar_t* kVersion = L"0.1.1";
static const wchar_t* kUserAgent = L"ChenyiOffice/0.1.1";

static const char* kManifestUrl =
    "https://github.com/sitong-zhang/office-software/releases/latest/download/latest.json";
static const char* kAnnouncementUrl =
    "https://raw.githubusercontent.com/sitong-zhang/office-software/main/announcements.json";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring out((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
  return out;
}

static std::string WideToUtf8(const std::wstring& s) {
  if (s.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
  std::string out((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
  return out;
}

static std::wstring FormatBytes(uint64_t bytes) {
  wchar_t buf[64];
  if (bytes >= 1024ull * 1024ull)
    swprintf(buf, 64, L"%.0f MB", (double)bytes / (1024.0 * 1024.0));
  else
    swprintf(buf, 64, L"%.0f KB", (double)bytes / 1024.0);
  return buf;
}

static bool FileExists(const std::wstring& path) {
  DWORD a = GetFileAttributesW(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---------------------------------------------------------------------------
// Minimal JSON parser (objects, arrays, strings, numbers, bool, null)
// ---------------------------------------------------------------------------

namespace json {

struct Value {
  enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };
  Type type = NUL;
  bool boolean = false;
  double number = 0;
  std::string text;
  std::vector<Value> items;
  std::map<std::string, Value> fields;

  const Value* Get(const std::string& key) const {
    if (type != OBJECT) return nullptr;
    auto it = fields.find(key);
    return it == fields.end() ? nullptr : &it->second;
  }
  std::string Str(const std::string& key, const std::string& def = "") const {
    const Value* v = Get(key);
    return (v && v->type == STRING) ? v->text : def;
  }
  double Num(const std::string& key, double def = 0) const {
    const Value* v = Get(key);
    return (v && v->type == NUMBER) ? v->number : def;
  }
  bool Bool(const std::string& key, bool def = false) const {
    const Value* v = Get(key);
    return (v && v->type == BOOL) ? v->boolean : def;
  }
};

static void AppendUtf8(std::string& out, unsigned cp) {
  if (cp < 0x80) {
    out.push_back((char)cp);
  } else if (cp < 0x800) {
    out.push_back((char)(0xC0 | (cp >> 6)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back((char)(0xE0 | (cp >> 12)));
    out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  } else {
    out.push_back((char)(0xF0 | (cp >> 18)));
    out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  }
}

class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}

  bool Parse(Value& out, std::string& err) {
    Skip();
    if (!ParseValue(out, err)) return false;
    Skip();
    if (pos_ != s_.size()) {
      err = "trailing characters";
      return false;
    }
    return true;
  }

 private:
  const std::string& s_;
  size_t pos_ = 0;

  void Skip() {
    while (pos_ < s_.size() &&
           (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
      ++pos_;
  }

  bool Literal(const char* lit) {
    size_t n = strlen(lit);
    if (s_.compare(pos_, n, lit) != 0) return false;
    pos_ += n;
    return true;
  }

  bool ParseString(std::string& out, std::string& err) {
    if (pos_ >= s_.size() || s_[pos_] != '"') {
      err = "expected string";
      return false;
    }
    ++pos_;
    out.clear();
    while (pos_ < s_.size()) {
      unsigned char c = (unsigned char)s_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        out.push_back((char)c);
        continue;
      }
      if (pos_ >= s_.size()) {
        err = "unterminated escape";
        return false;
      }
      char e = s_[pos_++];
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          auto hex4 = [&](unsigned& v) -> bool {
            if (pos_ + 4 > s_.size()) return false;
            v = 0;
            for (int i = 0; i < 4; ++i) {
              char h = s_[pos_++];
              v <<= 4;
              if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
              else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
              else return false;
            }
            return true;
          };
          unsigned cp = 0;
          if (!hex4(cp)) {
            err = "bad unicode escape";
            return false;
          }
          if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= s_.size() && s_[pos_] == '\\' &&
              s_[pos_ + 1] == 'u') {
            pos_ += 2;
            unsigned low = 0;
            if (!hex4(low)) {
              err = "bad unicode escape";
              return false;
            }
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          }
          AppendUtf8(out, cp);
          break;
        }
        default:
          err = "bad escape";
          return false;
      }
    }
    err = "unterminated string";
    return false;
  }

  bool ParseValue(Value& out, std::string& err) {
    Skip();
    if (pos_ >= s_.size()) {
      err = "unexpected end";
      return false;
    }
    char c = s_[pos_];
    if (c == '{') {
      ++pos_;
      out.type = Value::OBJECT;
      Skip();
      if (pos_ < s_.size() && s_[pos_] == '}') {
        ++pos_;
        return true;
      }
      while (true) {
        Skip();
        std::string key;
        if (!ParseString(key, err)) return false;
        Skip();
        if (pos_ >= s_.size() || s_[pos_] != ':') {
          err = "expected colon";
          return false;
        }
        ++pos_;
        Value v;
        if (!ParseValue(v, err)) return false;
        out.fields[key] = v;
        Skip();
        if (pos_ < s_.size() && s_[pos_] == ',') {
          ++pos_;
          continue;
        }
        if (pos_ < s_.size() && s_[pos_] == '}') {
          ++pos_;
          return true;
        }
        err = "expected , or }";
        return false;
      }
    }
    if (c == '[') {
      ++pos_;
      out.type = Value::ARRAY;
      Skip();
      if (pos_ < s_.size() && s_[pos_] == ']') {
        ++pos_;
        return true;
      }
      while (true) {
        Value v;
        if (!ParseValue(v, err)) return false;
        out.items.push_back(v);
        Skip();
        if (pos_ < s_.size() && s_[pos_] == ',') {
          ++pos_;
          continue;
        }
        if (pos_ < s_.size() && s_[pos_] == ']') {
          ++pos_;
          return true;
        }
        err = "expected , or ]";
        return false;
      }
    }
    if (c == '"') {
      out.type = Value::STRING;
      return ParseString(out.text, err);
    }
    if (c == 't' && Literal("true")) {
      out.type = Value::BOOL;
      out.boolean = true;
      return true;
    }
    if (c == 'f' && Literal("false")) {
      out.type = Value::BOOL;
      out.boolean = false;
      return true;
    }
    if (c == 'n' && Literal("null")) {
      out.type = Value::NUL;
      return true;
    }
    char* end = nullptr;
    double num = strtod(s_.c_str() + pos_, &end);
    if (end && end != s_.c_str() + pos_) {
      pos_ = (size_t)(end - s_.c_str());
      out.type = Value::NUMBER;
      out.number = num;
      return true;
    }
    err = "unexpected token";
    return false;
  }
};

}  // namespace json

// ---------------------------------------------------------------------------
// HTTP (WinHTTP)
// ---------------------------------------------------------------------------

struct HttpHandle {
  HINTERNET h = nullptr;
  ~HttpHandle() {
    if (h) WinHttpCloseHandle(h);
  }
};

static bool HttpRequest(const std::wstring& url, std::string* body, const std::wstring& filePath,
                        uint64_t expectedSize, std::function<void(uint64_t, uint64_t)> progress,
                        std::wstring& err) {
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = (DWORD)-1;
  parts.dwHostNameLength = (DWORD)-1;
  parts.dwUrlPathLength = (DWORD)-1;
  parts.dwExtraInfoLength = (DWORD)-1;
  if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &parts)) {
    err = L"更新地址无效";
    return false;
  }

  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength)
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

  HttpHandle session, connection, request;
  session.h = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h) {
    err = L"无法初始化网络";
    return false;
  }
  WinHttpSetTimeouts(session.h, 8000, 8000, 8000, 12000);

  connection.h = WinHttpConnect(session.h, host.c_str(), parts.nPort, 0);
  if (!connection.h) {
    err = L"无法连接服务器";
    return false;
  }

  request.h = WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 secure ? WINHTTP_FLAG_SECURE : 0);
  if (!request.h) {
    err = L"无法创建请求";
    return false;
  }

  WinHttpAddRequestHeaders(request.h, L"Cache-Control: no-cache\r\n", (DWORD)-1,
                           WINHTTP_ADDREQ_FLAG_ADD);

  if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                          0, 0) ||
      !WinHttpReceiveResponse(request.h, nullptr)) {
    err = L"网络连接失败";
    return false;
  }

  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                           WINHTTP_NO_HEADER_INDEX)) {
    err = L"无法读取响应";
    return false;
  }
  if (status != 200) {
    wchar_t buf[64];
    swprintf(buf, 64, L"服务器返回 HTTP %lu", status);
    err = buf;
    return false;
  }

  DWORD contentLength = 0;
  DWORD lengthSize = sizeof(contentLength);
  WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &lengthSize,
                      WINHTTP_NO_HEADER_INDEX);
  uint64_t total = contentLength ? contentLength : expectedSize;

  HANDLE file = INVALID_HANDLE_VALUE;
  if (body == nullptr) {
    file = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      err = L"无法写入临时文件";
      return false;
    }
  }

  std::vector<char> buffer(64 * 1024);
  uint64_t done = 0;
  while (true) {
    DWORD read = 0;
    if (!WinHttpReadData(request.h, buffer.data(), (DWORD)buffer.size(), &read)) {
      err = L"下载中断";
      if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
      if (body == nullptr) DeleteFileW(filePath.c_str());
      return false;
    }
    if (read == 0) break;
    if (body) {
      body->append(buffer.data(), read);
      if (body->size() > 4 * 1024 * 1024) {
        err = L"响应内容过大";
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        return false;
      }
    } else {
      DWORD written = 0;
      if (!WriteFile(file, buffer.data(), read, &written, nullptr) || written != read) {
        err = L"写入失败";
        CloseHandle(file);
        DeleteFileW(filePath.c_str());
        return false;
      }
    }
    done += read;
    if (progress) progress(done, total);
  }

  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  if (body == nullptr && expectedSize > 0 && done != expectedSize) {
    // Size mismatch is tolerated here; the SHA-256 check is authoritative.
  }
  return true;
}

static bool HttpGet(const std::wstring& url, std::string& body, std::wstring& err) {
  body.clear();
  return HttpRequest(url, &body, L"", 0, nullptr, err);
}

// ---------------------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------------------

static bool Sha256File(const std::wstring& path, std::wstring& hex) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objSize = 0, result = 0;
  std::vector<BYTE> object;
  bool ok = false;

  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;

  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) goto cleanup;
  if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objSize, sizeof(objSize), &result, 0) <
      0)
    goto cleanup;
  object.resize(objSize);
  if (BCryptCreateHash(alg, &hash, object.data(), objSize, nullptr, 0, 0) < 0) goto cleanup;

  {
    std::vector<BYTE> buffer(1024 * 1024);
    DWORD read = 0;
    while (ReadFile(file, buffer.data(), (DWORD)buffer.size(), &read, nullptr) && read > 0) {
      if (BCryptHashData(hash, buffer.data(), read, 0) < 0) goto cleanup;
    }
  }

  {
    BYTE digest[32];
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) goto cleanup;
    static const wchar_t* digits = L"0123456789abcdef";
    hex.clear();
    for (int i = 0; i < 32; ++i) {
      hex.push_back(digits[digest[i] >> 4]);
      hex.push_back(digits[digest[i] & 0xF]);
    }
    ok = true;
  }

cleanup:
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  CloseHandle(file);
  return ok;
}

// ---------------------------------------------------------------------------
// Version and time helpers
// ---------------------------------------------------------------------------

static bool VersionGreater(const std::wstring& a, const std::wstring& b) {
  int av[4] = {0, 0, 0, 0}, bv[4] = {0, 0, 0, 0};
  auto parse = [](const std::wstring& s, int* out) {
    int idx = 0;
    size_t i = 0;
    while (i < s.size() && idx < 4) {
      while (i < s.size() && (s[i] < L'0' || s[i] > L'9')) ++i;
      if (i >= s.size()) break;
      int v = 0;
      while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') {
        v = v * 10 + (s[i] - L'0');
        ++i;
      }
      out[idx++] = v;
      if (i < s.size() && s[i] == L'-') break;
    }
  };
  parse(a, av);
  parse(b, bv);
  for (int i = 0; i < 4; ++i) {
    if (av[i] != bv[i]) return av[i] > bv[i];
  }
  return false;
}

static bool IsoToTime(const std::string& s, time_t& out) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6) return false;

  // Days-from-civil (Howard Hinnant), independent of the process timezone.
  int yy = y - (mo <= 2 ? 1 : 0);
  const int era = (yy >= 0 ? yy : yy - 399) / 400;
  const unsigned yoe = (unsigned)(yy - era * 400);
  const unsigned doy = (153u * (unsigned)(mo + (mo > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  long long days = (long long)era * 146097LL + (long long)doe - 719468LL;
  long long base = days * 86400LL + h * 3600LL + mi * 60LL + sec;

  int offset = 0;
  size_t tpos = s.find('T');
  size_t z = std::string::npos;
  if (tpos != std::string::npos) z = s.find_first_of("+-Zz", tpos + 1);
  if (z != std::string::npos && s[z] != 'Z' && s[z] != 'z') {
    int oh = 0, om = 0;
    if (sscanf(s.c_str() + z + 1, "%d:%d", &oh, &om) == 2) {
      offset = oh * 3600 + om * 60;
      if (s[z] == '-') offset = -offset;
    }
  }
  out = (time_t)(base - offset);
  return true;
}

// ---------------------------------------------------------------------------
// Local state and engine discovery
// ---------------------------------------------------------------------------

static std::wstring StateDirectory() {
  wchar_t* base = nullptr;
  std::wstring dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) {
    dir = base;
    CoTaskMemFree(base);
  } else {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    dir = tmp;
  }
  dir += L"\\ChenyiOffice";
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  return dir;
}

static void LogLine(const std::wstring& text) {
  std::wstring path = StateDirectory() + L"\\launcher.log";
  FILE* f = _wfopen(path.c_str(), L"ab");
  if (!f) return;
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char stamp[64];
  std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d ", st.wYear, st.wMonth,
                st.wDay, st.wHour, st.wMinute, st.wSecond);
  std::string line = std::string(stamp) + WideToUtf8(text) + "\r\n";
  fwrite(line.data(), 1, line.size(), f);
  fclose(f);
}

static std::set<std::string> LoadSeen() {
  std::set<std::string> seen;
  std::wstring path = StateDirectory() + L"\\state.ini";
  FILE* f = _wfopen(path.c_str(), L"rb");
  if (!f) return seen;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    std::string id(line);
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r')) id.pop_back();
    if (!id.empty()) seen.insert(id);
  }
  fclose(f);
  return seen;
}

static void AppendSeen(const std::vector<std::string>& ids) {
  std::wstring path = StateDirectory() + L"\\state.ini";
  FILE* f = _wfopen(path.c_str(), L"ab");
  if (!f) return;
  for (const auto& id : ids) {
    fwrite(id.data(), 1, id.size(), f);
    fwrite("\n", 1, 1, f);
  }
  fclose(f);
}

static std::wstring ReadRegistryString(HKEY root, const wchar_t* subkey, const wchar_t* name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return L"";
  wchar_t value[MAX_PATH * 2] = {0};
  DWORD size = sizeof(value) - sizeof(wchar_t);
  DWORD type = 0;
  std::wstring result;
  if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)value, &size) == ERROR_SUCCESS &&
      (type == REG_SZ || type == REG_EXPAND_SZ)) {
    result = value;
  }
  RegCloseKey(key);
  return result;
}

static std::wstring FindEngine() {
  std::vector<std::wstring> candidates;
  std::wstring configured = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ChenyiOffice",
                                               L"EnginePath");
  if (!configured.empty()) candidates.push_back(configured);

  wchar_t programFiles[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
  if (programFiles[0]) candidates.push_back(std::wstring(programFiles) + L"\\LibreOffice\\program\\soffice.exe");

  candidates.push_back(L"C:\\Program Files\\LibreOffice\\program\\soffice.exe");

  std::wstring uno = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\LibreOffice\\UNO",
                                        L"InstallPath");
  if (!uno.empty()) {
    if (!uno.empty() && uno.back() != L'\\') uno += L'\\';
    candidates.push_back(uno + L"program\\soffice.exe");
  }

  for (const auto& path : candidates) {
    if (FileExists(path)) return path;
  }
  return L"";
}

// ---------------------------------------------------------------------------
// Data gathered by the worker thread
// ---------------------------------------------------------------------------

struct UpdateInfo {
  std::wstring version;
  std::wstring url;
  std::wstring sha256;
  uint64_t size = 0;
  bool mandatory = true;
};

struct Announcement {
  std::string id;
  std::wstring title;
  std::wstring body;
};

static UpdateInfo g_update;
static std::vector<Announcement> g_announcements;

static volatile LONG g_checkState = 0;  // 0 pending, 1 ready, 2 failed
static volatile LONG g_downloadState = 0;  // 0 none, 1 running, 2 verified, 3 failed
static volatile LONG g_downloadPercent = 0;
static std::wstring g_downloadError;
static std::wstring g_installerPath;

static void LoadAnnouncements(const std::set<std::string>& seen) {
  wchar_t stamp[32];
  swprintf(stamp, 32, L"?t=%llu", (unsigned long long)GetTickCount64());

  static const char* sources[] = {
      kAnnouncementUrl,
      "https://cdn.jsdelivr.net/gh/sitong-zhang/office-software@main/announcements.json",
  };

  std::string body;
  for (const char* source : sources) {
    std::wstring err;
    body.clear();
    if (HttpGet(Utf8ToWide(source) + stamp, body, err) && !body.empty()) {
      LogLine(std::wstring(L"announcements fetched from ") + Utf8ToWide(source));
      break;
    }
    LogLine(std::wstring(L"announcement fetch failed: ") + err);
  }
  if (body.empty()) return;

  json::Value root;
  std::string parseError;
  if (!json::Parser(body).Parse(root, parseError)) return;

  time_t now = time(nullptr);
  std::wstring current = kVersion;

  const json::Value* list = root.Get("announcements");
  if (!list || list->type != json::Value::ARRAY) return;

  for (const auto& entry : list->items) {
    if (entry.type != json::Value::OBJECT) continue;
    std::string id = entry.Str("id");
    if (id.empty() || seen.count(id)) {
      if (!id.empty()) LogLine(L"announcement already seen: " + Utf8ToWide(id));
      continue;
    }

    time_t published = 0;
    if (!IsoToTime(entry.Str("published_at"), published)) {
      LogLine(L"announcement time parse failed: " + Utf8ToWide(entry.Str("published_at")));
      continue;
    }
    LogLine(L"announcement " + Utf8ToWide(id) + L" published=" +
            std::to_wstring((long long)published) + L" now=" +
            std::to_wstring((long long)now));
    if (published > now) {
      LogLine(L"announcement skipped: not published yet");
      continue;
    }

    std::string expiresText = entry.Str("expires_at");
    if (!expiresText.empty()) {
      time_t expires = 0;
      if (IsoToTime(expiresText, expires) && expires <= now) continue;
    }

    std::string minVer = entry.Str("min_version");
    if (!minVer.empty() && VersionGreater(Utf8ToWide(minVer), current)) continue;
    std::string maxVer = entry.Str("max_version");
    if (!maxVer.empty() && VersionGreater(current, Utf8ToWide(maxVer))) continue;

    Announcement a;
    a.id = id;
    a.title = Utf8ToWide(entry.Str("title"));
    a.body = Utf8ToWide(entry.Str("body"));
    if (a.title.empty()) continue;
    g_announcements.push_back(a);
  }

  std::sort(g_announcements.begin(), g_announcements.end(),
            [](const Announcement& a, const Announcement& b) { return a.id < b.id; });
  LogLine(L"unseen announcements: " + std::to_wstring(g_announcements.size()));
}

static DWORD WINAPI CheckThreadProc(LPVOID) {
  LogLine(std::wstring(L"launcher start, version ") + kVersion);
  std::string body;
  std::wstring err;
  bool manifestOk = HttpGet(Utf8ToWide(kManifestUrl), body, err);
  if (!manifestOk) LogLine(L"manifest fetch failed: " + err);

  if (manifestOk) {
    json::Value root;
    std::string parseError;
    if (json::Parser(body).Parse(root, parseError)) {
      std::string remote = root.Str("version");
      LogLine(L"manifest version: " + Utf8ToWide(remote));
      if (!remote.empty() && VersionGreater(Utf8ToWide(remote), kVersion)) {
        g_update.version = Utf8ToWide(remote);
        g_update.mandatory = true;
        const json::Value* installer = root.Get("installer");
        if (installer) {
          g_update.url = Utf8ToWide(installer->Str("url"));
          g_update.sha256 = Utf8ToWide(installer->Str("sha256"));
          g_update.size = (uint64_t)installer->Num("size", 0);
        }
        if (!g_update.url.empty()) {
          InterlockedExchange(&g_checkState, 1);
          return 0;
        }
        g_update = UpdateInfo();
      }
    }
  }

  if (manifestOk) {
    LoadAnnouncements(LoadSeen());
  }

  InterlockedExchange(&g_checkState, manifestOk ? 1 : 2);
  return 0;
}

// ---------------------------------------------------------------------------
// Download worker
// ---------------------------------------------------------------------------

static std::wstring UpdateFilePath() {
  wchar_t temp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, temp);
  std::wstring version = g_update.version;
  for (auto& c : version)
    if (c == L'/' || c == L'\\' || c == L':') c = L'_';
  return std::wstring(temp) + L"ChenyiOffice-" + version + L"-x64.exe";
}

static DWORD WINAPI DownloadThreadProc(LPVOID) {
  std::wstring path = UpdateFilePath();
  std::wstring err;
  DeleteFileW(path.c_str());

  bool ok = HttpRequest(g_update.url, nullptr, path, g_update.size,
                        [](uint64_t done, uint64_t total) {
                          LONG pct = 0;
                          if (total > 0) pct = (LONG)((done * 100) / total);
                          if (pct > 99) pct = 99;
                          InterlockedExchange(&g_downloadPercent, pct);
                        },
                        err);
  if (!ok) {
    g_downloadError = err.empty() ? L"下载失败" : err;
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  std::wstring digest;
  if (!Sha256File(path, digest)) {
    DeleteFileW(path.c_str());
    g_downloadError = L"无法校验安装包";
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  if (!g_update.sha256.empty() && _wcsicmp(digest.c_str(), g_update.sha256.c_str()) != 0) {
    DeleteFileW(path.c_str());
    g_downloadError = L"安装包校验失败，已删除下载文件";
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  g_installerPath = path;
  InterlockedExchange(&g_downloadState, 2);
  return 0;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static int g_dpi = 96;
static int g_width = 860;
static int g_height = 540;
static ULONGLONG g_startTick = 0;
static int g_screen = 0;  // 0 splash, 1 announcement, 2 update, 3 downloading, 4 error
static size_t g_announcementIndex = 0;
static int g_scroll = 0;
static int g_scrollMax = 0;
static std::wstring g_status = L"正在检查更新…";

#define S(x) MulDiv((x), g_dpi, 96)

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
  r.right = g_width - S(56);
  r.left = r.right - S(190);
  r.bottom = g_height - S(56);
  r.top = r.bottom - S(46);
  return r;
}

static void DrawButton(HDC dc, const wchar_t* label, bool busy) {
  RECT r = ButtonRect();
  FillColor(dc, r, busy ? RGB(38, 62, 52) : RGB(244, 241, 232));
  DrawTextLine(dc, label, r, 15, FW_SEMIBOLD, busy ? RGB(150, 170, 158) : RGB(16, 32, 26),
               L"Microsoft YaHei UI", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawProgress(HDC dc, int percent) {
  int left = S(56), right = g_width - S(56);
  int top = g_height - S(130);
  RECT track{left, top, right, top + S(8)};
  FillColor(dc, track, RGB(38, 62, 52));
  int width = (right - left) * percent / 100;
  if (width > 0) {
    RECT fill{left, top, left + width, top + S(8)};
    FillColor(dc, fill, RGB(88, 184, 146));
  }
}

static void DrawSplash(HDC dc) {
  ULONGLONG elapsed = GetTickCount64() - g_startTick;
  int pad = S(56);

  RECT label{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, L"OFFICE SUITE · WINDOWS", label, 11, FW_NORMAL, RGB(110, 142, 127),
               L"Consolas", DT_LEFT | DT_SINGLELINE);

  int lineTop = S(168);
  int lineWidth = (int)(S(240) * std::min(1.0, (double)elapsed / 900.0));
  RECT accent{pad, lineTop, pad + lineWidth, lineTop + S(3)};
  FillColor(dc, accent, RGB(88, 184, 146));

  RECT word{pad, S(196), g_width - pad, S(268)};
  DrawTextLine(dc, L"CHENYI OFFICE", word, 44, FW_SEMIBOLD, RGB(244, 241, 232),
               L"Segoe UI", DT_LEFT | DT_SINGLELINE);

  RECT edition{pad, S(272), g_width - pad, S(304)};
  DrawTextLine(dc, L"胡辰逸定制版", edition, 17, FW_NORMAL, RGB(136, 196, 168),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT tagline{pad, S(330), g_width - pad, S(360)};
  DrawTextLine(dc, L"为 Windows 10 / 11 64 位重新设计的办公套件", tagline, 14, FW_NORMAL,
               RGB(150, 170, 158), L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT status{pad, g_height - S(74), g_width - S(220), g_height - S(46)};
  DrawTextLine(dc, g_status, status, 12, FW_NORMAL, RGB(150, 170, 158), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

  RECT version{g_width - S(220), g_height - S(74), g_width - pad, g_height - S(46)};
  DrawTextLine(dc, std::wstring(L"v") + kVersion, version, 12, FW_NORMAL, RGB(110, 142, 127),
               L"Consolas", DT_RIGHT | DT_SINGLELINE);
}

static void DrawAnnouncement(HDC dc) {
  int pad = S(56);
  const Announcement& a = g_announcements[g_announcementIndex];

  RECT label{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, L"公告", label, 11, FW_NORMAL, RGB(88, 184, 146), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  wchar_t counter[32];
  swprintf(counter, 32, L"%d / %d", (int)g_announcementIndex + 1, (int)g_announcements.size());
  RECT count{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, counter, count, 11, FW_NORMAL, RGB(110, 142, 127), L"Consolas",
               DT_RIGHT | DT_SINGLELINE);

  RECT title{pad, S(84), g_width - pad, S(136)};
  DrawTextLine(dc, a.title, title, 26, FW_SEMIBOLD, RGB(244, 241, 232), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

  RECT bodyArea{pad, S(152), g_width - pad, g_height - S(130)};
  RECT measure = bodyArea;
  DrawTextLine(dc, a.body, measure, 15, FW_NORMAL, RGB(210, 216, 208), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
  int bodyHeight = measure.bottom - measure.top;
  int visible = bodyArea.bottom - bodyArea.top;
  g_scrollMax = std::max(0, bodyHeight - visible);
  if (g_scroll > g_scrollMax) g_scroll = g_scrollMax;

  int saved = SaveDC(dc);
  IntersectClipRect(dc, bodyArea.left, bodyArea.top, bodyArea.right, bodyArea.bottom);
  RECT body = bodyArea;
  body.top -= g_scroll;
  body.bottom = body.top + bodyHeight + S(4);
  DrawTextLine(dc, a.body, body, 15, FW_NORMAL, RGB(210, 216, 208), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);
  RestoreDC(dc, saved);

  DrawButton(dc, L"继续", false);
}

static void DrawUpdate(HDC dc) {
  int pad = S(56);

  RECT label{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, L"强制更新", label, 11, FW_NORMAL, RGB(232, 166, 120), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  RECT title{pad, S(88), g_width - pad, S(140)};
  DrawTextLine(dc, L"发现新版本", title, 30, FW_SEMIBOLD, RGB(244, 241, 232),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT ver{pad, S(148), g_width - pad, S(186)};
  DrawTextLine(dc, std::wstring(L"v") + g_update.version, ver, 24, FW_SEMIBOLD,
               RGB(136, 196, 168), L"Consolas", DT_LEFT | DT_SINGLELINE);

  RECT meta{pad, S(194), g_width - pad, S(222)};
  wchar_t metaText[512];
  swprintf(metaText, 512, L"当前版本 v%ls · 安装包 %ls", kVersion,
           FormatBytes(g_update.size).c_str());
  DrawTextLine(dc, metaText, meta, 13, FW_NORMAL, RGB(150, 170, 158), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  RECT body{pad, S(248), g_width - pad, S(330)};
  DrawTextLine(dc,
               L"此更新为强制更新。完成安装前，旧版本无法继续使用；更新页面不提供退出或跳过。",
               body, 15, FW_NORMAL, RGB(210, 216, 208), L"Microsoft YaHei UI",
               DT_LEFT | DT_WORDBREAK);

  RECT note{pad, S(348), g_width - pad, g_height - S(150)};
  DrawTextLine(dc, L"下载完成后会自动校验安装包并启动安装程序，安装结束后自动重新打开。", note, 13,
               FW_NORMAL, RGB(150, 170, 158), L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);

  DrawButton(dc, L"下载并安装", false);
}

static void DrawDownloading(HDC dc) {
  int pad = S(56);
  int percent = (int)InterlockedCompareExchange(&g_downloadPercent, 0, 0);

  RECT label{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, L"强制更新", label, 11, FW_NORMAL, RGB(232, 166, 120), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  RECT title{pad, S(96), g_width - pad, S(148)};
  DrawTextLine(dc, L"正在下载更新", title, 30, FW_SEMIBOLD, RGB(244, 241, 232),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  wchar_t pct[32];
  swprintf(pct, 32, L"%d%%", percent);
  RECT number{pad, S(170), g_width - pad, S(240)};
  DrawTextLine(dc, pct, number, 44, FW_SEMIBOLD, RGB(136, 196, 168), L"Consolas",
               DT_LEFT | DT_SINGLELINE);

  DrawProgress(dc, percent);

  RECT hint{pad, g_height - S(190), g_width - pad, g_height - S(140)};
  DrawTextLine(dc, L"下载期间请保持网络连接。完成前无法关闭或跳过更新。", hint, 13, FW_NORMAL,
               RGB(150, 170, 158), L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);
}

static void DrawError(HDC dc) {
  int pad = S(56);

  RECT label{pad, S(46), g_width - pad, S(70)};
  DrawTextLine(dc, L"更新失败", label, 11, FW_NORMAL, RGB(232, 133, 120), L"Microsoft YaHei UI",
               DT_LEFT | DT_SINGLELINE);

  RECT title{pad, S(96), g_width - pad, S(148)};
  DrawTextLine(dc, L"更新未能完成", title, 30, FW_SEMIBOLD, RGB(244, 241, 232),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT msg{pad, S(168), g_width - pad, S(230)};
  DrawTextLine(dc, g_downloadError, msg, 15, FW_NORMAL, RGB(226, 166, 158),
               L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);

  RECT link{pad, S(250), g_width - pad, S(320)};
  DrawTextLine(dc, std::wstring(L"手动下载地址：") + g_update.url, link, 12, FW_NORMAL,
               RGB(136, 196, 168), L"Consolas",
               DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

  DrawButton(dc, L"重试", false);
}

static void LaunchEngine() {
  std::wstring engine = FindEngine();
  LogLine(engine.empty() ? L"engine not found" : L"launch engine: " + engine);
  if (engine.empty()) {
    MessageBoxW(g_window,
                L"没有找到办公套件内核。请重新运行安装包，或检查 LibreOffice 是否已正确安装。",
                kProductName, MB_ICONERROR | MB_OK);
    PostQuitMessage(0);
    return;
  }
  std::wstring dir = engine;
  size_t slash = dir.find_last_of(L'\\');
  if (slash != std::wstring::npos) dir = dir.substr(0, slash);

  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"open";
  info.lpFile = engine.c_str();
  info.lpDirectory = dir.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&info)) {
    MessageBoxW(g_window, L"无法启动办公套件。", kProductName, MB_ICONERROR | MB_OK);
  } else if (info.hProcess) {
    CloseHandle(info.hProcess);
  }
  PostQuitMessage(0);
}

static void FinishAnnouncements() {
  std::vector<std::string> ids;
  for (const auto& a : g_announcements) ids.push_back(a.id);
  AppendSeen(ids);
  LaunchEngine();
}

static void StartDownload() {
  g_downloadError.clear();
  g_installerPath.clear();
  InterlockedExchange(&g_downloadPercent, 0);
  InterlockedExchange(&g_downloadState, 1);
  g_screen = 3;
  CreateThread(nullptr, 0, DownloadThreadProc, nullptr, 0, nullptr);
  InvalidateRect(g_window, nullptr, FALSE);
}

static void RunInstallerAndRestart() {
  std::wstring params = L"/S";
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = g_installerPath.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteExW(&info)) {
    g_downloadError = L"无法启动安装程序，请允许管理员权限后重试。";
    g_screen = 4;
    InvalidateRect(g_window, nullptr, FALSE);
    return;
  }
  if (info.hProcess) {
    WaitForSingleObject(info.hProcess, INFINITE);
    CloseHandle(info.hProcess);
  }

  wchar_t self[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  ShellExecuteW(nullptr, L"open", self, nullptr, nullptr, SW_SHOWNORMAL);
  PostQuitMessage(0);
}

static void HandleButton() {
  if (g_screen == 1) {
    if (g_announcementIndex + 1 < g_announcements.size()) {
      ++g_announcementIndex;
      g_scroll = 0;
      InvalidateRect(g_window, nullptr, FALSE);
    } else {
      FinishAnnouncements();
    }
  } else if (g_screen == 2 || g_screen == 4) {
    if (!g_update.url.empty()) StartDownload();
  }
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

      RECT full{0, 0, g_width, g_height};
      FillColor(mem, full, RGB(16, 32, 26));

      switch (g_screen) {
        case 0: DrawSplash(mem); break;
        case 1: DrawAnnouncement(mem); break;
        case 2: DrawUpdate(mem); break;
        case 3: DrawDownloading(mem); break;
        case 4: DrawError(mem); break;
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
      if (g_screen == 0) {
        ULONGLONG elapsed = GetTickCount64() - g_startTick;
        LONG state = InterlockedCompareExchange(&g_checkState, 0, 0);
        if (state == 1 && elapsed >= 1500) {
          if (!g_update.version.empty()) {
            g_screen = 2;
          } else if (!g_announcements.empty()) {
            g_screen = 1;
            g_announcementIndex = 0;
            g_scroll = 0;
          } else {
            LaunchEngine();
            return 0;
          }
        } else if (state == 2 && elapsed >= 1200) {
          LaunchEngine();
          return 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      } else if (g_screen == 3) {
        LONG state = InterlockedCompareExchange(&g_downloadState, 0, 0);
        if (state == 2) {
          g_screen = 3;
          SetWindowTextW(hwnd, L"正在安装更新");
          InvalidateRect(hwnd, nullptr, FALSE);
          RunInstallerAndRestart();
          return 0;
        }
        if (state == 3) {
          g_screen = 4;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }

    case WM_LBUTTONDOWN: {
      POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      RECT button = ButtonRect();
      if (PtInRect(&button, p)) HandleButton();
      return 0;
    }

    case WM_KEYDOWN:
      if (wParam == VK_RETURN) HandleButton();
      return 0;

    case WM_MOUSEWHEEL:
      if (g_screen == 1) {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scroll -= (delta / WHEEL_DELTA) * S(48);
        if (g_scroll < 0) g_scroll = 0;
        if (g_scroll > g_scrollMax) g_scroll = g_scrollMax;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;

    case WM_CLOSE:
      // No close path while an update is pending or being downloaded.
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

  HDC screen = GetDC(nullptr);
  g_dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen) ReleaseDC(nullptr, screen);
  if (g_dpi <= 0) g_dpi = 96;

  g_width = S(860);
  g_height = S(540);
  g_startTick = GetTickCount64();

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = L"ChenyiOfficeLauncher";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);

  int x = (GetSystemMetrics(SM_CXSCREEN) - g_width) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - g_height) / 2;

  g_window = CreateWindowExW(0, wc.lpszClassName, kProductName, WS_POPUP, x, y, g_width, g_height,
                            nullptr, nullptr, instance, nullptr);
  if (!g_window) return 1;

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 30, nullptr);

  CreateThread(nullptr, 0, CheckThreadProc, nullptr, 0, nullptr);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

// Chenyi Office（胡辰逸定制版） launcher
// Splash, optional update prompt, in-app announcements, engine start.

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
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>

#include "app/ui/icons.h"
#include "app/ui/theme.h"

using namespace Gdiplus;

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
static const wchar_t* kVersion = L"0.4.1";
static const wchar_t* kUserAgent = L"ChenyiOffice/0.4.1";

// SHA-1 thumbprints of accepted code-signing certificates. Updates are accepted
// only from an installer signed by one of these certificates, even if the
// manifest is tampered with. During a certificate rotation, ship a launcher
// containing both the old and new thumbprints before switching the signer.
static const wchar_t* kExpectedThumbprints[] = {
    L"FDD3A2D15971DCBAE6CE40C5BAE76605A89062E4",
};
static const wchar_t* kReleasesUrl =
    L"https://github.com/sitong-zhang/office-software/releases/latest";

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

static bool ReadTextFile(const std::wstring& path, std::string& out) {
  out.clear();
  FILE* f = _wfopen(path.c_str(), L"rb");
  if (!f) return false;
  char buffer[512];
  size_t n = fread(buffer, 1, sizeof(buffer), f);
  fclose(f);
  out.assign(buffer, n);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return true;
}

static bool EndsWithNoCase(const std::wstring& value, const wchar_t* suffix) {
  size_t length = wcslen(suffix);
  if (value.size() < length) return false;
  return _wcsicmp(value.c_str() + value.size() - length, suffix) == 0;
}

static bool IsAllowedHost(const std::wstring& host) {
  static const wchar_t* exact[] = {
      L"github.com",
      L"cdn.jsdelivr.net",
  };
  for (const wchar_t* allowed : exact) {
    if (_wcsicmp(host.c_str(), allowed) == 0) return true;
  }
  // GitHub rotates asset hosts (objects., release-assets., raw., ...), so
  // accept any subdomain of githubusercontent.com instead of a fixed list.
  if (_wcsicmp(host.c_str(), L"githubusercontent.com") == 0) return true;
  return EndsWithNoCase(host, L".githubusercontent.com");
}

static bool IsAllowedUrl(const std::wstring& url) {
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = (DWORD)-1;
  if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &parts)) return false;
  if (parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
  if (parts.nPort != INTERNET_DEFAULT_HTTPS_PORT) return false;
  return IsAllowedHost(std::wstring(parts.lpszHostName, parts.dwHostNameLength));
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

static bool QueryHeaderString(HINTERNET request, DWORD level, std::wstring& out) {
  DWORD size = 0;
  if (!WinHttpQueryHeaders(request, level, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                           WINHTTP_NO_HEADER_INDEX) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }
  if (size == 0) return false;
  std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
  if (!WinHttpQueryHeaders(request, level, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    return false;
  }
  out.assign(buffer.data());
  return true;
}

static bool HttpRequest(const std::wstring& initialUrl, std::string* body,
                        const std::wstring& filePath, uint64_t expectedSize,
                        std::function<void(uint64_t, uint64_t)> progress, std::wstring& err) {
  HttpHandle session;
  session.h = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h) {
    err = L"无法初始化网络";
    return false;
  }
  WinHttpSetTimeouts(session.h, 8000, 8000, 8000, 12000);

  std::wstring url = initialUrl;
  // Follow redirects ourselves so every hop is validated before it is
  // requested. WinHTTP's automatic redirects do not expose that guarantee.
  for (int hop = 0; hop <= 5; ++hop) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
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
    if (parts.nScheme != INTERNET_SCHEME_HTTPS ||
        parts.nPort != INTERNET_DEFAULT_HTTPS_PORT || !IsAllowedHost(host)) {
      err = L"不安全的更新地址";
      return false;
    }

    HttpHandle connection, request;
    connection.h = WinHttpConnect(session.h, host.c_str(), parts.nPort, 0);
    if (!connection.h) {
      err = L"无法连接服务器";
      return false;
    }

    request.h = WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr,
                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE);
    if (!request.h) {
      err = L"无法创建请求";
      return false;
    }

    WinHttpAddRequestHeaders(request.h, L"Cache-Control: no-cache\r\n", (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
                     sizeof(redirectPolicy));

    if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
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

    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
      std::wstring location;
      if (!QueryHeaderString(request.h, WINHTTP_QUERY_LOCATION, location) ||
          location.empty()) {
        err = L"更新重定向缺少目标地址";
        return false;
      }
      wchar_t resolved[4096] = {0};
      DWORD resolvedSize = 4096;
      if (UrlCombineW(url.c_str(), location.c_str(), resolved, &resolvedSize, 0) != S_OK) {
        err = L"更新重定向地址无效";
        return false;
      }
      url = resolved;
      continue;
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
      err = L"下载文件大小不匹配";
      DeleteFileW(filePath.c_str());
      return false;
    }
    return true;
  }

  err = L"更新地址重定向次数过多";
  return false;
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
// Authenticode verification
// ---------------------------------------------------------------------------

static std::wstring SignerThumbprint(const std::wstring& path) {
  HCERTSTORE store = nullptr;
  HCRYPTMSG message = nullptr;
  std::wstring result;

  if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path.c_str(),
                        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                        CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &store,
                        &message, nullptr)) {
    return result;
  }

  DWORD size = 0;
  if (CryptMsgGetParam(message, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &size) && size > 0) {
    std::vector<BYTE> buffer(size);
    if (CryptMsgGetParam(message, CMSG_SIGNER_INFO_PARAM, 0, buffer.data(), &size)) {
      auto* signer = reinterpret_cast<CMSG_SIGNER_INFO*>(buffer.data());
      CERT_INFO certInfo{};
      certInfo.Issuer = signer->Issuer;
      certInfo.SerialNumber = signer->SerialNumber;
      PCCERT_CONTEXT cert = CertFindCertificateInStore(
          store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &certInfo,
          nullptr);
      if (cert) {
        BYTE hash[20] = {0};
        DWORD hashSize = sizeof(hash);
        if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &hashSize)) {
          static const wchar_t* digits = L"0123456789ABCDEF";
          for (DWORD i = 0; i < hashSize; ++i) {
            result.push_back(digits[hash[i] >> 4]);
            result.push_back(digits[hash[i] & 0xF]);
          }
        }
        CertFreeCertificateContext(cert);
      }
    }
  }

  if (message) CryptMsgClose(message);
  if (store) CertCloseStore(store, 0);
  return result;
}

static bool IsTrustedThumbprint(const std::wstring& thumbprint) {
  if (thumbprint.empty()) return false;
  for (const wchar_t* expected : kExpectedThumbprints) {
    if (_wcsicmp(thumbprint.c_str(), expected) == 0) return true;
  }
  return false;
}

static bool VerifyInstallerSignature(const std::wstring& path) {
  if (!FileExists(path)) return false;

  if (!IsTrustedThumbprint(SignerThumbprint(path))) return false;

  WINTRUST_FILE_INFO fileInfo{};
  fileInfo.cbStruct = sizeof(fileInfo);
  fileInfo.pcwszFilePath = path.c_str();

  WINTRUST_DATA data{};
  data.cbStruct = sizeof(data);
  data.dwUIChoice = WTD_UI_NONE;
  data.fdwRevocationChecks = WTD_REVOKE_NONE;
  data.dwUnionChoice = WTD_CHOICE_FILE;
  data.pFile = &fileInfo;
  data.dwStateAction = WTD_STATEACTION_VERIFY;

  GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG status = WinVerifyTrust(nullptr, &action, &data);
  data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(nullptr, &action, &data);

  // The publisher certificate is self-signed and therefore not in the Windows
  // trust store. The pinned thumbprint above is the identity check. Expired
  // certificates are also accepted: with a pinned self-signed identity,
  // expiry would otherwise stall the update prompt forever.
  return status == ERROR_SUCCESS || status == CERT_E_UNTRUSTEDROOT ||
         status == CERT_E_EXPIRED;
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
    if (sscanf(s.c_str() + z + 1, "%2d:%2d", &oh, &om) != 2 &&
        sscanf(s.c_str() + z + 1, "%2d%2d", &oh, &om) != 2) {
      oh = 0;
      om = 0;
    }
    offset = oh * 3600 + om * 60;
    if (s[z] == '-') offset = -offset;
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
  if (programFiles[0]) {
    candidates.push_back(std::wstring(programFiles) +
                         L"\\ChenyiOffice\\Engine\\program\\ChenyiEngine.exe");
    candidates.push_back(std::wstring(programFiles) +
                         L"\\ChenyiOffice\\Engine\\program\\soffice.exe");
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
  // Retained for manifest compatibility. Updates are always optional;
  // this flag never blocks the user from continuing on the current build.
  bool mandatory = false;
};

struct Announcement {
  std::string id;
  std::wstring title;
  std::wstring body;
  time_t published = 0;
};

static UpdateInfo g_update;
static std::vector<Announcement> g_announcements;

static volatile LONG g_checkState = 0;  // 0 pending, 1 ready, 2 failed
static volatile LONG g_downloadState = 0;  // 0 none, 1 running, 2 verified, 3 failed
static volatile LONG g_downloadPercent = 0;
static std::wstring g_downloadError;
static std::wstring g_installerPath;
static CRITICAL_SECTION g_dataLock;
static std::wstring g_status = L"正在检查更新…";

static void SetDownloadError(const std::wstring& text) {
  EnterCriticalSection(&g_dataLock);
  g_downloadError = text;
  LeaveCriticalSection(&g_dataLock);
}

static std::wstring GetDownloadError() {
  EnterCriticalSection(&g_dataLock);
  std::wstring text = g_downloadError;
  LeaveCriticalSection(&g_dataLock);
  return text;
}

static void SetInstallerPath(const std::wstring& path) {
  EnterCriticalSection(&g_dataLock);
  g_installerPath = path;
  LeaveCriticalSection(&g_dataLock);
}

static std::wstring GetInstallerPath() {
  EnterCriticalSection(&g_dataLock);
  std::wstring path = g_installerPath;
  LeaveCriticalSection(&g_dataLock);
  return path;
}

static void SetStatus(const std::wstring& text) {
  EnterCriticalSection(&g_dataLock);
  g_status = text;
  LeaveCriticalSection(&g_dataLock);
}

static std::wstring GetStatus() {
  EnterCriticalSection(&g_dataLock);
  std::wstring text = g_status;
  LeaveCriticalSection(&g_dataLock);
  return text;
}

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
    a.published = published;
    if (a.title.empty()) continue;
    g_announcements.push_back(a);
  }

  std::sort(g_announcements.begin(), g_announcements.end(),
            [](const Announcement& a, const Announcement& b) {
              if (a.published != b.published) return a.published < b.published;
              return a.id < b.id;
            });
  LogLine(L"unseen announcements: " + std::to_wstring(g_announcements.size()));
}

static DWORD WINAPI CheckThreadProc(LPVOID) {
  LogLine(std::wstring(L"launcher start, version ") + kVersion);
  std::string body;
  std::wstring err;
  bool manifestOk = HttpGet(Utf8ToWide(kManifestUrl), body, err);
  if (!manifestOk) LogLine(L"manifest fetch failed: " + err);

  bool updateAvailable = false;
  if (manifestOk) {
    json::Value root;
    std::string parseError;
    if (json::Parser(body).Parse(root, parseError)) {
      std::string remote = root.Str("version");
      std::string minSupported = root.Str("min_supported_version");
      LogLine(L"manifest version: " + Utf8ToWide(remote));

      bool newer = !remote.empty() && VersionGreater(Utf8ToWide(remote), kVersion);
      bool belowMinimum =
          !minSupported.empty() && VersionGreater(Utf8ToWide(minSupported), kVersion);

      if (newer || belowMinimum) {
        const json::Value* installer = root.Get("installer");
        if (installer) {
          std::string url = installer->Str("url");
          std::string sha = installer->Str("sha256");
          uint64_t size = (uint64_t)installer->Num("size", 0);
          bool manifestConsistent =
              minSupported.empty() || !VersionGreater(Utf8ToWide(minSupported), Utf8ToWide(remote));
          bool valid = !remote.empty() && !url.empty() && url.rfind("https://", 0) == 0 &&
                       sha.size() == 64 && size > 0 && manifestConsistent;
          if (valid) {
            g_update.version = Utf8ToWide(remote);
            g_update.mandatory = root.Bool("mandatory", false);
            g_update.url = Utf8ToWide(url);
            g_update.sha256 = Utf8ToWide(sha);
            g_update.size = size;
            updateAvailable = true;
            SetStatus(L"发现新版本");
          }
          LogLine(valid ? L"update available (optional)" : L"manifest update rejected: missing url, sha256, or size");
        } else {
          LogLine(L"manifest update rejected: installer object missing");
        }
      }
    }
  }

  // Announcements are always fetched, including when an update is available,
  // so a user who defers the update still receives the release notice.
  if (manifestOk) {
    LoadAnnouncements(LoadSeen());
  }

  if (!updateAvailable) {
    SetStatus(manifestOk ? L"已是最新版本" : L"无法检查更新");
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
    SetDownloadError(err.empty() ? L"下载失败" : err);
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  std::wstring digest;
  if (!Sha256File(path, digest)) {
    DeleteFileW(path.c_str());
    SetDownloadError(L"无法校验安装包");
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  if (g_update.sha256.size() != 64 ||
      _wcsicmp(digest.c_str(), g_update.sha256.c_str()) != 0) {
    DeleteFileW(path.c_str());
    SetDownloadError(L"安装包校验失败，已删除下载文件");
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  if (!VerifyInstallerSignature(path)) {
    DeleteFileW(path.c_str());
    SetDownloadError(L"安装包发布者签名无效，已拒绝更新");
    InterlockedExchange(&g_downloadState, 3);
    return 0;
  }

  SetInstallerPath(path);
  InterlockedExchange(&g_downloadState, 2);
  return 0;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static int g_dpi = 96;
static int g_width = 1080;
static int g_height = 700;
static const int kBaseWidth = 1080;
static const int kBaseHeight = 700;
static ULONGLONG g_startTick = 0;
static ULONG_PTR g_gdiplusToken = 0;
static int g_screen = 0;  // 0 splash, 1 announcement, 2 update, 3 downloading, 4 error, 5 home
static int g_homeSection = 0;  // 0 dashboard, 1 new, 2 recent, 3 settings
static size_t g_announcementIndex = 0;
static int g_scroll = 0;
static int g_scrollMax = 0;
static int g_hover = 0;
static std::vector<std::wstring> g_recent;

#define S(x) MulDiv((x), g_dpi, 96)
#define SIDEBAR S(232)
#define TITLEBAR S(56)

enum Action {
  A_NONE = 0,
  A_NAV_HOME,
  A_NAV_NEW,
  A_NAV_OPEN,
  A_NAV_RECENT,
  A_NAV_ANNOUNCE,
  A_NAV_SETTINGS,
  A_CREATE_WRITER,
  A_CREATE_CALC,
  A_CREATE_IMPRESS,
  A_CREATE_DRAW,
  A_OPEN_FILE,
  A_CHECK_UPDATE,
  A_DOWNLOAD,
  A_VIEW_ANNOUNCE,
  A_CONTINUE,
  A_RETRY,
  A_SKIP_UPDATE,
  A_MINIMIZE,
  A_CLOSE,
  A_OPEN_PROFILE,
  A_ABOUT,
  A_OPEN_RELEASES,
  A_CLEAR_RECENT,
  A_RECENT_BASE = 100,
};

struct HitTarget {
  RectF rect;
  int id;
};

static std::vector<HitTarget> g_hits;

static void AddHit(const RectF& rect, int id) {
  if (rect.Width > 0 && rect.Height > 0) g_hits.push_back({rect, id});
}

static int HitTest(POINT point) {
  for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it) {
    if (ui::Hit(it->rect, point)) return it->id;
  }
  return A_NONE;
}

static void LaunchEngine(const std::wstring& extraArgs = L"");
static void StartDownload();
static void FinishAnnouncements();
static void RefreshRecent();

static std::wstring BaseName(const std::wstring& path) {
  size_t slash = path.find_last_of(L"\\/");
  return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

static std::vector<std::wstring> LoadRecent() {
  std::vector<std::wstring> items;
  FILE* file = _wfopen((StateDirectory() + L"\\recent.ini").c_str(), L"rb");
  if (!file) return items;
  char line[1024];
  while (fgets(line, sizeof(line), file) && items.size() < 6) {
    std::string text(line);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    if (!text.empty()) items.push_back(Utf8ToWide(text));
  }
  fclose(file);
  return items;
}

static void AddRecent(const std::wstring& path) {
  std::vector<std::wstring> items = LoadRecent();
  items.erase(std::remove_if(items.begin(), items.end(), [&](const std::wstring& item) {
                return _wcsicmp(item.c_str(), path.c_str()) == 0;
              }),
              items.end());
  items.insert(items.begin(), path);
  if (items.size() > 6) items.resize(6);
  FILE* file = _wfopen((StateDirectory() + L"\\recent.ini").c_str(), L"wb");
  if (!file) return;
  for (const auto& item : items) {
    std::string text = WideToUtf8(item);
    fwrite(text.data(), 1, text.size(), file);
    fwrite("\n", 1, 1, file);
  }
  fclose(file);
  RefreshRecent();
}

static void RefreshRecent() {
  g_recent = LoadRecent();
}

static void OpenFileDialog() {
  wchar_t path[MAX_PATH] = {0};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = g_window;
  dialog.lpstrFile = path;
  dialog.nMaxFile = MAX_PATH;
  dialog.lpstrFilter =
      L"Office documents\0*.odt;*.ods;*.odp;*.odg;*.docx;*.xlsx;*.pptx;*.pdf\0"
      L"All files\0*.*\0\0";
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&dialog)) return;
  AddRecent(path);
  LaunchEngine(L"\"" + std::wstring(path) + L"\"");
}

static void StartUpdateCheck() {
  g_update = UpdateInfo{};
  g_announcements.clear();
  g_announcementIndex = 0;
  SetStatus(L"正在检查更新…");
  InterlockedExchange(&g_checkState, 0);
  g_screen = 0;
  g_startTick = GetTickCount64();
  CreateThread(nullptr, 0, CheckThreadProc, nullptr, 0, nullptr);
  InvalidateRect(g_window, nullptr, FALSE);
}

static void ShowAnnouncements() {
  if (g_announcements.empty()) {
    MessageBoxW(g_window, L"当前没有新公告。", kProductName, MB_ICONINFORMATION | MB_OK);
    return;
  }
  g_screen = 1;
  g_announcementIndex = 0;
  g_scroll = 0;
  InvalidateRect(g_window, nullptr, FALSE);
}

static void DrawButton(Graphics& g, const RectF& rect, const std::wstring& label, int icon,
                       bool primary, int action, bool dark = false) {
  bool hover = g_hover == action;
  if (primary) {
    ui::FillRound(g, rect, S(10), hover ? ui::GreenLight() : ui::Green());
  } else {
    ui::FillRound(g, rect, S(10), hover ? ui::SurfaceMuted() : ui::Surface());
    ui::StrokeRound(g, rect, S(10), ui::Border(), 1.0f);
  }
  float iconSize = S(17);
  float textX = rect.X + S(16);
  if (icon) {
    ui::Icon(g, icon, primary || dark ? ui::ICON_CREAM : ui::ICON_GREEN,
             RectF(rect.X + S(14), rect.Y + (rect.Height - iconSize) / 2, iconSize, iconSize));
    textX += iconSize + S(8);
  }
  ui::Text(g, label, RectF(textX, rect.Y, rect.Width - (textX - rect.X) - S(12), rect.Height),
           S(13), FontStyleBold, primary ? ui::Surface() : ui::Text(), StringAlignmentNear,
           StringAlignmentCenter);
  AddHit(rect, action);
}

static void DrawWindowButton(Graphics& g, const RectF& rect, int icon, int action, bool dark) {
  bool hover = g_hover == action;
  if (hover) {
    ui::FillRound(g, rect, S(8), dark ? ui::rgba(255, 255, 255, 24) : ui::rgba(16, 32, 26, 12));
  }
  ui::Icon(g, icon, dark ? ui::ICON_CREAM : ui::ICON_MUTED,
           RectF(rect.X + S(6), rect.Y + S(6), S(16), S(16)));
  AddHit(rect, action);
}

static void DrawNavItem(Graphics& g, int y, int icon, const std::wstring& label, int action,
                        bool active) {
  RectF rect(S(16), (float)y, S(200), S(44));
  bool hover = g_hover == action;
  if (active) {
    ui::FillRound(g, rect, S(12), ui::SidebarActive());
    ui::FillRound(g, RectF(S(17), y + S(14), S(3), S(16)), S(2), ui::Accent());
  } else if (hover) {
    ui::FillRound(g, rect, S(12), ui::rgba(255, 255, 255, 14));
  }
  ui::Icon(g, icon, active ? ui::ICON_CREAM : ui::ICON_SOFT,
           RectF(S(30), y + S(13), S(18), S(18)));
  ui::Text(g, label, RectF(S(60), y, S(140), S(44)), S(13),
           active ? FontStyleBold : FontStyleRegular,
           active ? ui::SidebarText() : ui::SidebarMuted(), StringAlignmentNear,
           StringAlignmentCenter);
  AddHit(rect, action);
}

static void DrawSidebar(Graphics& g) {
  RectF side(0, 0, SIDEBAR, g_height);
  LinearGradientBrush background(side, ui::rgba(22, 50, 39), ui::rgba(10, 24, 19),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, side);

  ui::Image(g, 111, RectF(S(24), S(24), S(38), S(38)));
  ui::TextLatin(g, L"Chenyi Office", RectF(S(74), S(22), S(140), S(24)), S(15),
                FontStyleBold, ui::SidebarText());
  ui::Text(g, L"胡辰逸定制版", RectF(S(74), S(47), S(140), S(18)), S(10),
           FontStyleRegular, ui::SidebarMuted());
  ui::FillRound(g, RectF(S(24), S(86), S(184), 1), 0, ui::rgba(255, 255, 255, 18));

  int y = S(104);
  DrawNavItem(g, y, UI_ICON_HOUSE, L"首页", A_NAV_HOME, g_homeSection == 0 && g_screen == 5);
  y += S(48);
  DrawNavItem(g, y, UI_ICON_FILE_PLUS, L"新建", A_NAV_NEW, g_homeSection == 1 && g_screen == 5);
  y += S(48);
  DrawNavItem(g, y, UI_ICON_FOLDER_OPEN, L"打开", A_NAV_OPEN, false);
  y += S(48);
  DrawNavItem(g, y, UI_ICON_CLOCK, L"最近", A_NAV_RECENT, g_homeSection == 2 && g_screen == 5);
  y += S(48);
  DrawNavItem(g, y, UI_ICON_MEGAPHONE, L"公告", A_NAV_ANNOUNCE, false);
  y += S(48);
  DrawNavItem(g, y, UI_ICON_SETTINGS, L"设置", A_NAV_SETTINGS, g_homeSection == 3 && g_screen == 5);

  RectF statusCard(S(16), g_height - S(96), S(200), S(72));
  ui::FillRound(g, statusCard, S(12), ui::rgba(255, 255, 255, 8));
  ui::Icon(g, UI_ICON_SHIELD_CHECK, ui::ICON_SOFT, RectF(S(30), g_height - S(80), S(18), S(18)));
  ui::TextLatin(g, std::wstring(L"v") + kVersion, RectF(S(58), g_height - S(82), S(120), S(20)),
                S(12), FontStyleBold, ui::SidebarText());
  std::wstring status = g_update.version.empty() ? GetStatus() : L"发现新版本";
  ui::Text(g, status, RectF(S(30), g_height - S(54), S(170), S(18)), S(9.5),
           FontStyleRegular, g_update.version.empty() ? ui::SidebarMuted() : ui::Accent(),
           StringAlignmentNear, StringAlignmentNear, false);
}

static void DrawTitleBar(Graphics& g, const std::wstring& title, bool allowClose) {
  ui::Text(g, title, RectF(SIDEBAR + S(36), S(12), S(480), S(32)), S(17), FontStyleBold,
           ui::Text());
  ui::Text(g, L"Chenyi Office · 胡辰逸定制版", RectF(SIDEBAR + S(36), S(34), S(480), S(16)),
           S(10), FontStyleRegular, ui::TextMuted());
  ui::FillRound(g, RectF(SIDEBAR, TITLEBAR - 1, g_width - SIDEBAR, 1), 0, ui::Border());

  DrawWindowButton(g, RectF(g_width - S(86), S(14), S(28), S(28)), UI_ICON_MINIMIZE, A_MINIMIZE,
                   false);
  if (allowClose) {
    DrawWindowButton(g, RectF(g_width - S(52), S(14), S(28), S(28)), UI_ICON_CLOSE, A_CLOSE,
                     false);
  }
}

static void DrawQuickCard(Graphics& g, const RectF& rect, int icon, const std::wstring& title,
                          const std::wstring& subtitle, int action) {
  bool hover = g_hover == action;
  ui::SoftShadow(g, rect, S(14));
  ui::FillRound(g, rect, S(14), ui::Surface());
  ui::StrokeRound(g, rect, S(14), hover ? ui::Accent() : ui::Border(), hover ? 1.5f : 1.0f);
  RectF tile(rect.X + S(18), rect.Y + S(18), S(42), S(42));
  ui::FillRound(g, tile, S(12), hover ? ui::rgba(232, 244, 238) : ui::SurfaceMuted());
  ui::Icon(g, icon, ui::ICON_GREEN, RectF(tile.X + S(9), tile.Y + S(9), S(24), S(24)));
  ui::Text(g, title, RectF(rect.X + S(74), rect.Y + S(18), rect.Width - S(90), S(24)), S(14),
           FontStyleBold, ui::Text());
  ui::Text(g, subtitle, RectF(rect.X + S(74), rect.Y + S(44), rect.Width - S(90), S(36)),
           S(10.5), FontStyleRegular, ui::TextMuted(), StringAlignmentNear,
           StringAlignmentNear, true);
  ui::Icon(g, UI_ICON_CHEVRON_RIGHT, ui::ICON_MUTED,
           RectF(rect.X + rect.Width - S(30), rect.Y + rect.Height - S(30), S(14), S(14)));
  AddHit(rect, action);
}

static void DrawVersionCard(Graphics& g, const RectF& rect) {
  ui::SoftShadow(g, rect, S(16));
  ui::FillRound(g, rect, S(16), ui::Surface());
  ui::StrokeRound(g, rect, S(16), ui::Border());
  ui::Text(g, L"版本与更新", RectF(rect.X + S(18), rect.Y + S(16), rect.Width - S(60), S(22)),
           S(13), FontStyleBold, ui::Text());
  ui::Icon(g, UI_ICON_SHIELD_CHECK, ui::ICON_GREEN,
           RectF(rect.X + rect.Width - S(38), rect.Y + S(16), S(20), S(20)));
  ui::TextLatin(g, std::wstring(L"v") + kVersion,
                RectF(rect.X + S(18), rect.Y + S(46), rect.Width - S(36), S(34)), S(24),
                FontStyleBold, ui::Text());
  std::wstring status = g_update.version.empty() ? GetStatus() : L"有新版本可用";
  ui::Text(g, status, RectF(rect.X + S(18), rect.Y + S(80), rect.Width - S(36), S(34)),
           S(10.5), FontStyleRegular,
           g_update.version.empty() ? ui::TextMuted() : ui::Green(), StringAlignmentNear,
           StringAlignmentNear, true);
  if (g_update.version.empty()) {
    DrawButton(g, RectF(rect.X + S(18), rect.Y + rect.Height - S(48), rect.Width - S(36), S(34)),
               L"检查更新", UI_ICON_REFRESH, false, A_CHECK_UPDATE);
  } else {
    DrawButton(g, RectF(rect.X + S(18), rect.Y + rect.Height - S(48), rect.Width - S(36), S(34)),
               L"立即更新", UI_ICON_DOWNLOAD, true, A_DOWNLOAD);
  }
}

static void DrawAnnouncementCard(Graphics& g, const RectF& rect) {
  ui::SoftShadow(g, rect, S(16));
  ui::FillRound(g, rect, S(16), ui::Surface());
  ui::StrokeRound(g, rect, S(16), ui::Border());
  ui::Text(g, L"最新公告", RectF(rect.X + S(18), rect.Y + S(16), rect.Width - S(36), S(22)),
           S(13), FontStyleBold, ui::Text());
  if (g_announcements.empty()) {
    ui::Text(g, L"暂无公告", RectF(rect.X + S(18), rect.Y + S(52), rect.Width - S(36), S(22)),
             S(11), FontStyleRegular, ui::TextMuted());
    return;
  }
  const Announcement& latest = g_announcements.back();
  ui::Text(g, latest.title, RectF(rect.X + S(18), rect.Y + S(48), rect.Width - S(36), S(40)),
           S(12), FontStyleBold, ui::Text(), StringAlignmentNear, StringAlignmentNear, true);
  std::wstring body = latest.body;
  if (body.size() > 120) body = body.substr(0, 120) + L"…";
  ui::Text(g, body, RectF(rect.X + S(18), rect.Y + S(90), rect.Width - S(36), S(56)), S(10),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
  DrawButton(g, RectF(rect.X + S(18), rect.Y + rect.Height - S(48), rect.Width - S(36), S(34)),
             L"查看公告", UI_ICON_MEGAPHONE, false, A_VIEW_ANNOUNCE);
}

static void DrawEngineCard(Graphics& g, const RectF& rect) {
  ui::SoftShadow(g, rect, S(16));
  ui::FillRound(g, rect, S(16), ui::Surface());
  ui::StrokeRound(g, rect, S(16), ui::Border());
  ui::Text(g, L"办公内核", RectF(rect.X + S(18), rect.Y + S(16), rect.Width - S(36), S(22)),
           S(13), FontStyleBold, ui::Text());
  std::wstring engine = FindEngine();
  ui::Text(g, engine.empty() ? L"未找到" : L"已就绪",
           RectF(rect.X + S(18), rect.Y + S(46), rect.Width - S(36), S(22)), S(12),
           FontStyleBold, engine.empty() ? ui::Danger() : ui::Green());
  ui::Text(g, engine.empty() ? L"请重新安装内核" : BaseName(engine),
           RectF(rect.X + S(18), rect.Y + S(72), rect.Width - S(36), S(20)), S(9.5),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, false);
  if (engine.empty()) {
    RectF repair(rect.X + S(18), rect.Y + rect.Height - S(44), rect.Width - S(36), S(32));
    ui::FillRound(g, repair, S(9),
                  g_hover == A_OPEN_RELEASES ? ui::Surface() : ui::SurfaceMuted());
    ui::Icon(g, UI_ICON_DOWNLOAD, ui::ICON_GREEN,
             RectF(repair.X + S(10), repair.Y + S(8), S(16), S(16)));
    ui::Text(g, L"重新安装内核", RectF(repair.X + S(32), repair.Y, repair.Width - S(40), S(32)),
             S(10.5), FontStyleBold, ui::Green(), StringAlignmentNear, StringAlignmentCenter);
    AddHit(repair, A_OPEN_RELEASES);
  }
}

static void DrawDashboard(Graphics& g) {
  float left = S(272), top = S(80), leftWidth = S(520);
  RectF hero(left, top, leftWidth, S(160));
  ui::FillRoundGradient(g, hero, S(18), ui::GreenLight(), ui::Ink());
  ui::Text(g, L"下午好，胡辰逸", RectF(left + S(24), top + S(20), leftWidth - S(48), S(34)),
           S(24), FontStyleBold, ui::SidebarText());
  ui::Text(g, L"把文档、表格、演示和绘图放在一个安静的工作空间里。",
           RectF(left + S(24), top + S(62), leftWidth - S(48), S(40)), S(11.5),
           FontStyleRegular, ui::SidebarMuted(), StringAlignmentNear, StringAlignmentNear, true);
  RectF openButton(left + S(24), top + S(108), S(148), S(36));
  ui::FillRound(g, openButton, S(10), g_hover == A_OPEN_FILE ? ui::SurfaceMuted() : ui::Surface());
  ui::Icon(g, UI_ICON_FOLDER_OPEN, ui::ICON_DARK,
           RectF(openButton.X + S(14), openButton.Y + S(10), S(16), S(16)));
  ui::Text(g, L"打开文件", RectF(openButton.X + S(38), openButton.Y, S(100), S(36)), S(12),
           FontStyleBold, ui::Text(), StringAlignmentNear, StringAlignmentCenter);
  AddHit(openButton, A_OPEN_FILE);

  ui::Text(g, L"新建", RectF(left, top + S(180), S(160), S(24)), S(16), FontStyleBold,
           ui::Text());
  ui::Text(g, L"快速开始", RectF(left + S(64), top + S(184), S(200), S(18)), S(10),
           FontStyleRegular, ui::TextMuted());

  float cardWidth = S(250), cardHeight = S(112), gap = S(20);
  DrawQuickCard(g, RectF(left, top + S(214), cardWidth, cardHeight), UI_ICON_FILE_TEXT,
                L"Writer 文档", L"排版、写作与长文档", A_CREATE_WRITER);
  DrawQuickCard(g, RectF(left + cardWidth + gap, top + S(214), cardWidth, cardHeight),
                UI_ICON_TABLE, L"Calc 表格", L"数据、公式与图表", A_CREATE_CALC);
  DrawQuickCard(g, RectF(left, top + S(214) + cardHeight + gap, cardWidth, cardHeight),
                UI_ICON_PRESENTATION, L"Impress 演示", L"幻灯片与放映", A_CREATE_IMPRESS);
  DrawQuickCard(g, RectF(left + cardWidth + gap, top + S(214) + cardHeight + gap, cardWidth,
                         cardHeight),
                UI_ICON_SHAPES, L"Draw 绘图", L"图形、流程图与海报", A_CREATE_DRAW);

  float recentTop = top + S(474);
  ui::Text(g, L"最近使用", RectF(left, recentTop, S(200), S(24)), S(16), FontStyleBold,
           ui::Text());
  RectF recentCard(left, recentTop + S(30), leftWidth, S(84));
  ui::SoftShadow(g, recentCard, S(14));
  ui::FillRound(g, recentCard, S(14), ui::Surface());
  ui::StrokeRound(g, recentCard, S(14), ui::Border());
  if (g_recent.empty()) {
    ui::Icon(g, UI_ICON_CLOCK, ui::ICON_MUTED,
             RectF(recentCard.X + S(20), recentCard.Y + S(28), S(24), S(24)));
    ui::Text(g, L"还没有最近文档", RectF(recentCard.X + S(56), recentCard.Y + S(18), S(280), S(24)),
             S(12.5), FontStyleBold, ui::Text());
    ui::Text(g, L"通过“打开文件”打开文档后会出现在这里。",
             RectF(recentCard.X + S(56), recentCard.Y + S(44), S(320), S(20)), S(10),
             FontStyleRegular, ui::TextMuted());
    AddHit(recentCard, A_OPEN_FILE);
  } else {
    const std::wstring& path = g_recent.front();
    ui::Icon(g, UI_ICON_FILE_TEXT, ui::ICON_GREEN,
             RectF(recentCard.X + S(20), recentCard.Y + S(28), S(24), S(24)));
    ui::Text(g, BaseName(path), RectF(recentCard.X + S(56), recentCard.Y + S(16), S(380), S(24)),
             S(12.5), FontStyleBold, ui::Text(), StringAlignmentNear, StringAlignmentNear, false);
    ui::Text(g, path, RectF(recentCard.X + S(56), recentCard.Y + S(42), S(400), S(20)), S(9.5),
             FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, false);
    AddHit(recentCard, A_RECENT_BASE);
  }

  float rail = left + leftWidth + S(20);
  DrawVersionCard(g, RectF(rail, top, S(228), S(160)));
  DrawAnnouncementCard(g, RectF(rail, top + S(176), S(228), S(190)));
  DrawEngineCard(g, RectF(rail, top + S(382), S(228), S(110)));
}

static void DrawNewPage(Graphics& g) {
  float left = S(272), top = S(88);
  ui::Text(g, L"选择一个新文档", RectF(left, top, S(520), S(34)), S(22), FontStyleBold,
           ui::Text());
  ui::Text(g, L"从空白文档开始，或直接打开已有文件继续工作。",
           RectF(left, top + S(36), S(520), S(22)), S(11), FontStyleRegular, ui::TextMuted());
  float cardWidth = S(250), cardHeight = S(132), gap = S(20);
  DrawQuickCard(g, RectF(left, top + S(78), cardWidth, cardHeight), UI_ICON_FILE_TEXT,
                L"Writer 文档", L"排版、写作与长文档", A_CREATE_WRITER);
  DrawQuickCard(g, RectF(left + cardWidth + gap, top + S(78), cardWidth, cardHeight),
                UI_ICON_TABLE, L"Calc 表格", L"数据、公式与图表", A_CREATE_CALC);
  DrawQuickCard(g, RectF(left, top + S(78) + cardHeight + gap, cardWidth, cardHeight),
                UI_ICON_PRESENTATION, L"Impress 演示", L"幻灯片与放映", A_CREATE_IMPRESS);
  DrawQuickCard(g, RectF(left + cardWidth + gap, top + S(78) + cardHeight + gap, cardWidth,
                         cardHeight),
                UI_ICON_SHAPES, L"Draw 绘图", L"图形、流程图与海报", A_CREATE_DRAW);
  RectF openCard(left, top + S(78) + (cardHeight + gap) * 2 + S(12), S(520), S(64));
  ui::SoftShadow(g, openCard, S(14));
  ui::FillRound(g, openCard, S(14), ui::Surface());
  ui::StrokeRound(g, openCard, S(14), ui::Border());
  ui::Icon(g, UI_ICON_FOLDER_OPEN, ui::ICON_GREEN,
           RectF(openCard.X + S(20), openCard.Y + S(20), S(24), S(24)));
  ui::Text(g, L"打开已有文件", RectF(openCard.X + S(58), openCard.Y + S(14), S(300), S(22)),
           S(12.5), FontStyleBold, ui::Text());
  ui::Text(g, L"支持 ODF、Office Open XML 和 PDF", RectF(openCard.X + S(58), openCard.Y + S(36),
                                                        S(360), S(18)),
           S(9.5), FontStyleRegular, ui::TextMuted());
  AddHit(openCard, A_OPEN_FILE);
}

static void DrawRecentPage(Graphics& g) {
  float left = S(272), top = S(88);
  ui::Text(g, L"最近使用", RectF(left, top, S(520), S(34)), S(22), FontStyleBold, ui::Text());
  ui::Text(g, L"这里记录通过 Chenyi Office 打开过的文件。",
           RectF(left, top + S(36), S(520), S(22)), S(11), FontStyleRegular, ui::TextMuted());
  if (g_recent.empty()) {
    RectF empty(left, top + S(82), S(520), S(180));
    ui::SoftShadow(g, empty, S(16));
    ui::FillRound(g, empty, S(16), ui::Surface());
    ui::StrokeRound(g, empty, S(16), ui::Border());
    ui::Icon(g, UI_ICON_CLOCK, ui::ICON_MUTED, RectF(left + S(24), top + S(110), S(32), S(32)));
    ui::Text(g, L"还没有最近文档", RectF(left + S(72), top + S(108), S(360), S(26)), S(15),
             FontStyleBold, ui::Text());
    ui::Text(g, L"打开一个文档后，它会出现在这里。", RectF(left + S(72), top + S(138), S(360), S(22)),
             S(10.5), FontStyleRegular, ui::TextMuted());
    DrawButton(g, RectF(left + S(24), top + S(184), S(132), S(34)), L"打开文件",
               UI_ICON_FOLDER_OPEN, false, A_OPEN_FILE);
    return;
  }
  DrawButton(g, RectF(left + S(388), top - S(2), S(132), S(32)), L"清空记录", UI_ICON_TRASH,
             false, A_CLEAR_RECENT);
  float y = top + S(74);
  for (size_t i = 0; i < g_recent.size() && i < 5; ++i) {
    RectF row(left, y, S(520), S(64));
    ui::FillRound(g, row, S(12), ui::Surface());
    ui::StrokeRound(g, row, S(12), ui::Border());
    ui::Icon(g, UI_ICON_FILE_TEXT, ui::ICON_GREEN, RectF(row.X + S(18), row.Y + S(18), S(24), S(24)));
    ui::Text(g, BaseName(g_recent[i]), RectF(row.X + S(54), row.Y + S(12), S(400), S(22)), S(12),
             FontStyleBold, ui::Text());
    ui::Text(g, g_recent[i], RectF(row.X + S(54), row.Y + S(36), S(420), S(18)), S(9.5),
             FontStyleRegular, ui::TextMuted());
    ui::Icon(g, UI_ICON_CHEVRON_RIGHT, ui::ICON_MUTED,
             RectF(row.X + row.Width - S(30), row.Y + S(24), S(14), S(14)));
    AddHit(row, A_RECENT_BASE + (int)i);
    y += S(74);
  }
}

static void DrawSettingsPage(Graphics& g) {
  float left = S(272), top = S(88);
  ui::Text(g, L"设置", RectF(left, top, S(520), S(34)), S(22), FontStyleBold, ui::Text());
  ui::Text(g, L"更新、本地数据和产品信息。", RectF(left, top + S(36), S(520), S(22)), S(11),
           FontStyleRegular, ui::TextMuted());

  RectF updateCard(left, top + S(80), S(520), S(132));
  ui::SoftShadow(g, updateCard, S(14));
  ui::FillRound(g, updateCard, S(14), ui::Surface());
  ui::StrokeRound(g, updateCard, S(14), ui::Border());
  ui::Icon(g, UI_ICON_SHIELD_CHECK, ui::ICON_GREEN,
           RectF(updateCard.X + S(20), updateCard.Y + S(20), S(24), S(24)));
  ui::Text(g, L"版本与更新", RectF(updateCard.X + S(58), updateCard.Y + S(16), S(300), S(22)),
           S(13), FontStyleBold, ui::Text());
  ui::TextLatin(g, std::wstring(L"v") + kVersion,
                RectF(updateCard.X + S(58), updateCard.Y + S(42), S(200), S(28)), S(18),
                FontStyleBold, ui::Text());
  ui::Text(g, GetStatus(), RectF(updateCard.X + S(58), updateCard.Y + S(74), S(300), S(20)),
           S(10), FontStyleRegular, ui::TextMuted());
  DrawButton(g, RectF(updateCard.X + S(360), updateCard.Y + S(82), S(140), S(34)),
             g_update.version.empty() ? L"检查更新" : L"立即更新",
             g_update.version.empty() ? UI_ICON_REFRESH : UI_ICON_DOWNLOAD, true,
             g_update.version.empty() ? A_CHECK_UPDATE : A_DOWNLOAD);

  RectF dataCard(left, top + S(228), S(520), S(132));
  ui::FillRound(g, dataCard, S(14), ui::Surface());
  ui::StrokeRound(g, dataCard, S(14), ui::Border());
  ui::Icon(g, UI_ICON_HARD_DRIVE, ui::ICON_GREEN,
           RectF(dataCard.X + S(20), dataCard.Y + S(20), S(24), S(24)));
  ui::Text(g, L"本地数据", RectF(dataCard.X + S(58), dataCard.Y + S(16), S(300), S(22)), S(13),
           FontStyleBold, ui::Text());
  ui::Text(g, L"配置、日志和已读公告保存在本地；卸载时会一并清理。",
           RectF(dataCard.X + S(58), dataCard.Y + S(44), S(420), S(40)), S(10.5),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
  DrawButton(g, RectF(dataCard.X + S(20), dataCard.Y + S(86), S(150), S(34)), L"打开配置目录",
             UI_ICON_FOLDER_OPEN, false, A_OPEN_PROFILE);
  DrawButton(g, RectF(dataCard.X + S(184), dataCard.Y + S(86), S(140), S(34)), L"清空最近记录",
             UI_ICON_TRASH, false, A_CLEAR_RECENT);

  RectF aboutCard(left, top + S(376), S(520), S(132));
  ui::FillRound(g, aboutCard, S(14), ui::Surface());
  ui::StrokeRound(g, aboutCard, S(14), ui::Border());
  ui::Image(g, 110, RectF(aboutCard.X + S(20), aboutCard.Y + S(20), S(28), S(28)));
  ui::Text(g, L"Chenyi Office（胡辰逸定制版）",
           RectF(aboutCard.X + S(60), aboutCard.Y + S(16), S(420), S(22)), S(13),
           FontStyleBold, ui::Text());
  ui::Text(g, L"独立打造的桌面办公空间，界面、更新与公告由本项目维护。",
           RectF(aboutCard.X + S(60), aboutCard.Y + S(44), S(420), S(22)), S(10.5),
           FontStyleRegular, ui::TextMuted());
  ui::Text(g, L"Apache 2.0 图标 · Material Design Icons",
           RectF(aboutCard.X + S(20), aboutCard.Y + S(82), S(420), S(20)), S(9.5),
           FontStyleRegular, ui::TextMuted());
  DrawButton(g, RectF(aboutCard.X + S(360), aboutCard.Y + S(84), S(140), S(32)), L"关于",
             UI_ICON_INFO, false, A_ABOUT);
}

static void DrawHome(Graphics& g) {
  DrawSidebar(g);
  switch (g_homeSection) {
    case 1:
      DrawTitleBar(g, L"新建", true);
      DrawNewPage(g);
      break;
    case 2:
      DrawTitleBar(g, L"最近", true);
      DrawRecentPage(g);
      break;
    case 3:
      DrawTitleBar(g, L"设置", true);
      DrawSettingsPage(g);
      break;
    default:
      DrawTitleBar(g, L"首页", true);
      DrawDashboard(g);
      break;
  }
}

static void DrawSplash(Graphics& g) {
  RectF full(0, 0, g_width, g_height);
  LinearGradientBrush background(full, ui::rgba(20, 46, 36), ui::rgba(8, 20, 16),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, full);

  ULONGLONG elapsed = GetTickCount64() - g_startTick;
  float center = g_width / 2.0f;
  ui::Image(g, 111, RectF(center - S(48), g_height / 2.0f - S(150), S(96), S(96)));
  ui::TextLatin(g, L"CHENYI OFFICE", RectF(0, g_height / 2.0f - S(34), g_width, S(44)), S(34),
                FontStyleBold, ui::SidebarText(), StringAlignmentCenter);
  ui::Text(g, L"胡辰逸定制版", RectF(0, g_height / 2.0f + S(14), g_width, S(24)), S(14),
           FontStyleRegular, ui::Accent(), StringAlignmentCenter);
  float lineWidth = (float)S(220) * std::min(1.0, (double)elapsed / 900.0);
  ui::FillRound(g, RectF(center - lineWidth / 2, g_height / 2.0f + S(54), lineWidth, S(3)), S(2),
                ui::Accent());
  ui::Text(g, GetStatus(), RectF(S(56), g_height - S(74), g_width - S(220), S(24)), S(11),
           FontStyleRegular, ui::SidebarMuted());
  ui::TextLatin(g, std::wstring(L"v") + kVersion,
                RectF(g_width - S(220), g_height - S(74), S(164), S(24)), S(11),
                FontStyleRegular, ui::TextSoft(), StringAlignmentFar);
}

static void DrawAnnouncement(Graphics& g) {
  RectF full(0, 0, g_width, g_height);
  LinearGradientBrush background(full, ui::rgba(20, 46, 36), ui::rgba(8, 20, 16),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, full);

  const Announcement& announcement = g_announcements[g_announcementIndex];
  float cardWidth = std::min((float)S(760), (float)g_width - S(112));
  float cardHeight = std::min((float)S(540), (float)g_height - S(112));
  RectF card((g_width - cardWidth) / 2, (g_height - cardHeight) / 2, cardWidth, cardHeight);
  ui::SoftShadow(g, card, S(20));
  ui::FillRound(g, card, S(20), ui::Paper());
  DrawWindowButton(g, RectF(g_width - S(52), S(16), S(28), S(28)), UI_ICON_CLOSE, A_CLOSE, true);

  wchar_t counter[64];
  swprintf(counter, 64, L"公告  %d / %d", (int)g_announcementIndex + 1,
           (int)g_announcements.size());
  ui::Text(g, counter, RectF(card.X + S(36), card.Y + S(28), card.Width - S(72), S(20)), S(10),
           FontStyleBold, ui::Green());
  ui::Text(g, announcement.title,
           RectF(card.X + S(36), card.Y + S(58), card.Width - S(72), S(78)), S(24),
           FontStyleBold, ui::Text(), StringAlignmentNear, StringAlignmentNear, true);

  RectF bodyArea(card.X + S(36), card.Y + S(150), card.Width - S(72), card.Height - S(242));
  RectF measured = ui::MeasureText(g, announcement.body,
                                   RectF(0, 0, bodyArea.Width, S(2000)), S(12.5),
                                   FontStyleRegular, true);
  g_scrollMax = std::max(0, (int)measured.Height - (int)bodyArea.Height);
  if (g_scroll < 0) g_scroll = 0;
  if (g_scroll > g_scrollMax) g_scroll = g_scrollMax;

  GraphicsState state = g.Save();
  g.SetClip(bodyArea);
  ui::Text(g, announcement.body,
           RectF(bodyArea.X, bodyArea.Y - g_scroll, bodyArea.Width, measured.Height + S(20)),
           S(12.5), FontStyleRegular, ui::Text(), StringAlignmentNear, StringAlignmentNear, true);
  g.Restore(state);

  if (g_scrollMax > 0) {
    float trackX = card.X + card.Width - S(20);
    float thumbHeight = std::max((float)S(32), bodyArea.Height * bodyArea.Height / measured.Height);
    float thumbY = bodyArea.Y + (bodyArea.Height - thumbHeight) * g_scroll / g_scrollMax;
    ui::FillRound(g, RectF(trackX, bodyArea.Y, S(4), bodyArea.Height), S(2),
                  ui::rgba(16, 32, 26, 18));
    ui::FillRound(g, RectF(trackX, thumbY, S(4), thumbHeight), S(2), ui::rgba(31, 92, 70, 120));
  }

  bool last = g_announcementIndex + 1 >= g_announcements.size();
  DrawButton(g, RectF(card.X + card.Width - S(172), card.Y + card.Height - S(64), S(136), S(40)),
             last ? L"继续" : L"下一条", last ? UI_ICON_ARROW_RIGHT : UI_ICON_CHEVRON_RIGHT,
             true, A_CONTINUE);
}

static void DrawUpdate(Graphics& g) {
  RectF full(0, 0, g_width, g_height);
  LinearGradientBrush background(full, ui::rgba(20, 46, 36), ui::rgba(8, 20, 16),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, full);

  float cardWidth = std::min((float)S(720), (float)g_width - S(112));
  float cardHeight = std::min((float)S(520), (float)g_height - S(112));
  RectF card((g_width - cardWidth) / 2, (g_height - cardHeight) / 2, cardWidth, cardHeight);
  ui::SoftShadow(g, card, S(20));
  ui::FillRound(g, card, S(20), ui::Paper());
  DrawWindowButton(g, RectF(g_width - S(86), S(16), S(28), S(28)), UI_ICON_MINIMIZE, A_MINIMIZE,
                   true);
  DrawWindowButton(g, RectF(g_width - S(52), S(16), S(28), S(28)), UI_ICON_CLOSE, A_SKIP_UPDATE,
                   true);

  ui::FillRoundGradient(g, RectF(card.X, card.Y, card.Width, S(160)), S(20), ui::GreenLight(),
                        ui::Green());
  ui::FillRound(g, RectF(card.X, card.Y + S(120), card.Width, S(40)), 0, ui::Green());
  ui::Icon(g, UI_ICON_SHIELD_CHECK, ui::ICON_CREAM,
           RectF(card.X + S(32), card.Y + S(30), S(44), S(44)));
  ui::Text(g, L"可用更新", RectF(card.X + S(92), card.Y + S(28), S(300), S(20)), S(10),
           FontStyleBold, ui::rgba(220, 240, 230));
  ui::Text(g, L"发现新版本", RectF(card.X + S(92), card.Y + S(48), S(400), S(36)), S(24),
           FontStyleBold, ui::SidebarText());
  ui::TextLatin(g, std::wstring(L"v") + g_update.version,
                RectF(card.X + S(32), card.Y + S(102), S(400), S(40)), S(28), FontStyleBold,
                ui::Surface());

  wchar_t meta[512];
  swprintf(meta, 512, L"当前版本 v%ls  ·  安装包 %ls", kVersion,
           FormatBytes(g_update.size).c_str());
  ui::Text(g, meta, RectF(card.X + S(32), card.Y + S(184), card.Width - S(64), S(22)), S(10.5),
           FontStyleRegular, ui::TextMuted());
  ui::Text(g, L"有新版本可用。你可以立即更新，也可以稍后提醒；在此期间当前版本可继续正常使用。",
           RectF(card.X + S(32), card.Y + S(220), card.Width - S(64), S(48)), S(11.5),
           FontStyleRegular, ui::Text(), StringAlignmentNear, StringAlignmentNear, true);

  RectF note(card.X + S(32), card.Y + S(286), card.Width - S(64), S(64));
  ui::FillRound(g, note, S(12), ui::SurfaceMuted());
  ui::Icon(g, UI_ICON_INFO, ui::ICON_GREEN, RectF(note.X + S(14), note.Y + S(20), S(20), S(20)));
  ui::Text(g, L"下载完成后会自动校验 SHA-256 与发布者签名，再静默安装并重新打开。",
           RectF(note.X + S(44), note.Y + S(14), note.Width - S(58), S(40)), S(10.5),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);

  DrawButton(g, RectF(card.X + S(32), card.Y + card.Height - S(64), S(180), S(40)),
             L"立即更新", UI_ICON_DOWNLOAD, true, A_DOWNLOAD);
  DrawButton(g, RectF(card.X + S(228), card.Y + card.Height - S(64), S(180), S(40)),
             L"稍后提醒", 0, false, A_SKIP_UPDATE);
}

static void DrawDownloading(Graphics& g) {
  RectF full(0, 0, g_width, g_height);
  LinearGradientBrush background(full, ui::rgba(20, 46, 36), ui::rgba(8, 20, 16),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, full);
  float cardWidth = std::min((float)S(720), (float)g_width - S(112));
  float cardHeight = std::min((float)S(500), (float)g_height - S(112));
  RectF card((g_width - cardWidth) / 2, (g_height - cardHeight) / 2, cardWidth, cardHeight);
  ui::SoftShadow(g, card, S(20));
  ui::FillRound(g, card, S(20), ui::Paper());
  DrawWindowButton(g, RectF(g_width - S(86), S(16), S(28), S(28)), UI_ICON_MINIMIZE, A_MINIMIZE,
                   true);
  DrawWindowButton(g, RectF(g_width - S(52), S(16), S(28), S(28)), UI_ICON_CLOSE, A_CLOSE, true);

  int percent = (int)InterlockedCompareExchange(&g_downloadPercent, 0, 0);
  ui::FillRound(g, RectF(card.X + S(32), card.Y + S(32), S(56), S(56)), S(16),
                ui::rgba(232, 244, 238));
  ui::Icon(g, UI_ICON_DOWNLOAD, ui::ICON_GREEN,
           RectF(card.X + S(46), card.Y + S(46), S(28), S(28)));
  ui::Text(g, L"软件更新", RectF(card.X + S(104), card.Y + S(34), S(300), S(18)), S(10),
           FontStyleBold, ui::Green());
  ui::Text(g, L"正在下载更新", RectF(card.X + S(104), card.Y + S(54), S(400), S(32)), S(22),
           FontStyleBold, ui::Text());
  wchar_t pct[32];
  swprintf(pct, 32, L"%d%%", percent);
  ui::TextLatin(g, pct, RectF(card.X + S(32), card.Y + S(120), card.Width - S(64), S(64)), S(48),
                FontStyleBold, ui::Green());
  RectF track(card.X + S(32), card.Y + S(196), card.Width - S(64), S(10));
  ui::FillRound(g, track, S(5), ui::SurfaceMuted());
  if (percent > 0) {
    ui::FillRound(g, RectF(track.X, track.Y, track.Width * percent / 100.0f, track.Height), S(5),
                  ui::Green());
  }
  ui::Text(g, L"下载期间请保持网络连接；你随时可以关闭窗口，稍后再更新。",
           RectF(card.X + S(32), card.Y + S(228), card.Width - S(64), S(44)), S(11),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
  ui::Text(g, L"完成后会自动校验安装包并启动安装程序。",
           RectF(card.X + S(32), card.Y + S(286), card.Width - S(64), S(22)), S(10),
           FontStyleRegular, ui::TextMuted());
}

static void DrawError(Graphics& g) {
  RectF full(0, 0, g_width, g_height);
  LinearGradientBrush background(full, ui::rgba(34, 34, 30), ui::rgba(16, 20, 18),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, full);
  float cardWidth = std::min((float)S(760), (float)g_width - S(112));
  float cardHeight = std::min((float)S(520), (float)g_height - S(112));
  RectF card((g_width - cardWidth) / 2, (g_height - cardHeight) / 2, cardWidth, cardHeight);
  ui::SoftShadow(g, card, S(20));
  ui::FillRound(g, card, S(20), ui::Paper());
  DrawWindowButton(g, RectF(g_width - S(86), S(16), S(28), S(28)), UI_ICON_MINIMIZE, A_MINIMIZE,
                   true);
  DrawWindowButton(g, RectF(g_width - S(52), S(16), S(28), S(28)), UI_ICON_CLOSE, A_CLOSE, true);

  ui::FillRound(g, RectF(card.X + S(32), card.Y + S(32), S(56), S(56)), S(16), ui::DangerSoft());
  ui::Icon(g, UI_ICON_INFO, ui::ICON_MUTED, RectF(card.X + S(46), card.Y + S(46), S(28), S(28)));
  ui::Text(g, L"更新失败", RectF(card.X + S(104), card.Y + S(34), S(300), S(18)), S(10),
           FontStyleBold, ui::Danger());
  ui::Text(g, L"更新未能完成", RectF(card.X + S(104), card.Y + S(54), S(400), S(32)), S(22),
           FontStyleBold, ui::Text());
  ui::Text(g, GetDownloadError(), RectF(card.X + S(32), card.Y + S(112), card.Width - S(64), S(150)),
           S(11.5), FontStyleRegular, ui::Text(), StringAlignmentNear, StringAlignmentNear, true);
  ui::Text(g, g_update.url, RectF(card.X + S(32), card.Y + S(272), card.Width - S(64), S(44)),
           S(9.5), FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear,
           true);
  DrawButton(g, RectF(card.X + S(32), card.Y + card.Height - S(64), S(136), S(40)), L"重试",
             UI_ICON_REFRESH, true, A_RETRY);
  DrawButton(g, RectF(card.X + S(184), card.Y + card.Height - S(64), S(168), S(40)),
             L"打开下载页", UI_ICON_EXTERNAL_LINK, false, A_OPEN_RELEASES);
  DrawButton(g, RectF(card.X + card.Width - S(172), card.Y + card.Height - S(64), S(136), S(40)),
             L"稍后提醒", 0, false, A_SKIP_UPDATE);
}

static void OnAction(int action) {
  if (action >= A_RECENT_BASE) {
    size_t index = (size_t)(action - A_RECENT_BASE);
    if (index < g_recent.size()) LaunchEngine(L"\"" + g_recent[index] + L"\"");
    return;
  }
  switch (action) {
    case A_NAV_HOME:
      g_homeSection = 0;
      break;
    case A_NAV_NEW:
      g_homeSection = 1;
      break;
    case A_NAV_OPEN:
      OpenFileDialog();
      return;
    case A_NAV_RECENT:
      g_homeSection = 2;
      break;
    case A_NAV_ANNOUNCE:
      ShowAnnouncements();
      return;
    case A_NAV_SETTINGS:
      g_homeSection = 3;
      break;
    case A_CREATE_WRITER:
      LaunchEngine(L"--writer");
      return;
    case A_CREATE_CALC:
      LaunchEngine(L"--calc");
      return;
    case A_CREATE_IMPRESS:
      LaunchEngine(L"--impress");
      return;
    case A_CREATE_DRAW:
      LaunchEngine(L"--draw");
      return;
    case A_OPEN_FILE:
      OpenFileDialog();
      return;
    case A_CHECK_UPDATE:
      StartUpdateCheck();
      return;
    case A_DOWNLOAD:
      if (!g_update.url.empty()) StartDownload();
      return;
    case A_VIEW_ANNOUNCE:
      ShowAnnouncements();
      return;
    case A_CONTINUE:
      if (g_screen == 1) {
        if (g_announcementIndex + 1 < g_announcements.size()) {
          ++g_announcementIndex;
          g_scroll = 0;
        } else {
          FinishAnnouncements();
          return;
        }
      }
      break;
    case A_RETRY:
      if (!g_update.url.empty()) StartDownload();
      return;
    case A_SKIP_UPDATE:
      // Deferring an update leaves the current build fully usable. Unseen
      // release notices are still delivered before the home screen.
      if (!g_announcements.empty()) {
        g_screen = 1;
        g_announcementIndex = 0;
        g_scroll = 0;
      } else {
        g_screen = 5;
        g_homeSection = 0;
      }
      LogLine(L"update deferred by user");
      break;
    case A_MINIMIZE:
      ShowWindow(g_window, SW_MINIMIZE);
      return;
    case A_CLOSE:
      if (g_screen == 1) {
        FinishAnnouncements();
      } else {
        // Updates are optional: closing the window never blocks the user.
        PostQuitMessage(0);
      }
      return;
    case A_OPEN_PROFILE:
      ShellExecuteW(nullptr, L"open", StateDirectory().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      return;
    case A_ABOUT:
      MessageBoxW(g_window,
                  L"Chenyi Office（胡辰逸定制版）\n\n"
                  L"Windows 10 / 11 · 64 位\n"
                  L"产品图标来自 Google Material Design Icons（Apache 2.0）。",
                  kProductName, MB_ICONINFORMATION | MB_OK);
      return;
    case A_OPEN_RELEASES:
      ShellExecuteW(nullptr, L"open", kReleasesUrl, nullptr, nullptr, SW_SHOWNORMAL);
      return;
    case A_CLEAR_RECENT:
      DeleteFileW((StateDirectory() + L"\\recent.ini").c_str());
      RefreshRecent();
      break;
    default:
      return;
  }
  InvalidateRect(g_window, nullptr, FALSE);
}

static std::wstring ProfileDirectory() {
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
  dir += L"\\ChenyiOffice\\lo-profile";
  SHCreateDirectoryExW(nullptr, (dir + L"\\user").c_str(), nullptr);
  return dir;
}

static std::wstring FileUrlFromPath(const std::wstring& path) {
  std::string utf8 = WideToUtf8(path);
  std::string url = "file:///";
  char encoded[8];
  for (unsigned char c : utf8) {
    if (c == '\\') {
      url.push_back('/');
    } else if (c == ' ' || c == '%' || c == '#' || c == '?' || c >= 0x80) {
      std::snprintf(encoded, sizeof(encoded), "%%%02X", c);
      url += encoded;
    } else {
      url.push_back((char)c);
    }
  }
  return Utf8ToWide(url);
}

static bool ReadWholeFile(const std::wstring& path, std::string& out) {
  out.clear();
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
      size.QuadPart > 32LL * 1024 * 1024) {
    CloseHandle(file);
    return false;
  }
  out.resize((size_t)size.QuadPart);
  DWORD read = 0;
  bool ok = out.empty() || (ReadFile(file, &out[0], (DWORD)out.size(), &read, nullptr) &&
                            read == out.size());
  CloseHandle(file);
  return ok;
}

static bool WriteWholeFile(const std::wstring& path, const std::string& data) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  bool ok = WriteFile(file, data.data(), (DWORD)data.size(), &written, nullptr) &&
            written == data.size();
  CloseHandle(file);
  return ok;
}

static bool LoadResourceString(int id, std::string& out) {
  HRSRC res = FindResourceW(g_instance, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!res) return false;
  HGLOBAL data = LoadResource(g_instance, res);
  DWORD size = SizeofResource(g_instance, res);
  void* bytes = LockResource(data);
  if (!bytes) return false;
  out.assign((const char*)bytes, size);
  return true;
}

static std::string XmlAttribute(const std::string& block, const char* attribute) {
  std::string needle = std::string(attribute) + "=\"";
  size_t start = block.find(needle);
  if (start == std::string::npos) return "";
  start += needle.size();
  size_t end = block.find('"', start);
  if (end == std::string::npos) return "";
  return block.substr(start, end - start);
}

static std::string ProfileKey(const std::string& path, const std::string& name) {
  return path + '\x1f' + name;
}

static void CollectProfileItems(const std::string& xml, std::set<std::string>& keys,
                                std::vector<std::string>* blocks) {
  size_t pos = 0;
  while ((pos = xml.find("<item ", pos)) != std::string::npos) {
    size_t end = xml.find("</item>", pos);
    if (end == std::string::npos) break;
    end += 7;
    std::string block = xml.substr(pos, end - pos);
    std::string path = XmlAttribute(block, "oor:path");
    std::string name = XmlAttribute(block, "oor:name");
    if (!path.empty() && !name.empty()) {
      keys.insert(ProfileKey(path, name));
      if (blocks) blocks->push_back(block);
    }
    pos = end;
  }
}

// Add brand defaults that the user's profile does not have yet. Existing
// values are kept, so upgrades never wipe user toolbar/icon/option changes.
static std::string MergeProfile(const std::string& existing, const std::string& tmpl) {
  std::set<std::string> existingKeys;
  CollectProfileItems(existing, existingKeys, nullptr);

  std::set<std::string> templateKeys;
  std::vector<std::string> templateBlocks;
  CollectProfileItems(tmpl, templateKeys, &templateBlocks);

  std::string additions;
  for (const auto& block : templateBlocks) {
    std::string path = XmlAttribute(block, "oor:path");
    std::string name = XmlAttribute(block, "oor:name");
    if (path.empty() || name.empty()) continue;
    if (existingKeys.count(ProfileKey(path, name))) continue;
    additions += block;
    additions += "\n";
  }
  if (additions.empty()) return existing;

  std::string merged = existing;
  size_t close = merged.rfind("</oor:items>");
  if (close == std::string::npos) {
    merged += "\n" + additions;
  } else {
    merged.insert(close, additions);
  }
  return merged;
}

static void EnsureProfile() {
  std::wstring dir = ProfileDirectory();
  std::wstring path = dir + L"\\user\\registrymodifications.xcu";
  std::wstring marker = dir + L"\\brand-version.txt";

  std::string installedVersion;
  if (FileExists(path) && ReadTextFile(marker, installedVersion) &&
      installedVersion == WideToUtf8(kVersion)) {
    return;
  }

  std::string tmpl;
  if (!LoadResourceString(106, tmpl) || tmpl.empty()) {
    LogLine(L"brand profile resource missing");
    return;
  }

  bool created = !FileExists(path);
  if (created) {
    if (!WriteWholeFile(path, tmpl)) {
      LogLine(L"cannot create brand profile");
      return;
    }
  } else {
    CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE);
    std::string existing;
    if (!ReadWholeFile(path, existing)) {
      LogLine(L"cannot read existing brand profile");
      return;
    }
    std::string merged = MergeProfile(existing, tmpl);
    if (merged != existing && !WriteWholeFile(path, merged)) {
      LogLine(L"cannot merge brand profile");
      return;
    }
  }

  FILE* versionFile = _wfopen(marker.c_str(), L"wb");
  if (versionFile) {
    std::string version = WideToUtf8(kVersion);
    fwrite(version.data(), 1, version.size(), versionFile);
    fclose(versionFile);
  }
  LogLine(std::wstring(created ? L"brand profile created for " : L"brand profile merged for ") +
          kVersion);
}

static void LaunchEngine(const std::wstring& extraArgs) {
  std::wstring engine = FindEngine();
  LogLine(engine.empty() ? L"engine not found" : L"launch engine: " + engine);
  if (engine.empty()) {
    LogLine(L"engine missing; EnginePath and standard locations were checked");
    int answer = MessageBoxW(
        g_window,
        L"没有找到办公套件内核。它可能已被卸载，或被系统中的其他办公软件替换。\n\n"
        L"是否打开下载页，重新安装 Chenyi Office 内核？",
        kProductName, MB_ICONERROR | MB_YESNO);
    if (answer == IDYES) {
      ShellExecuteW(nullptr, L"open", kReleasesUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
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
  EnsureProfile();
  std::wstring params =
      L"-env:UserInstallation=\"" + FileUrlFromPath(ProfileDirectory()) + L"\"";
  if (!extraArgs.empty()) params += L" " + extraArgs;
  info.lpParameters = params.c_str();
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
  g_screen = 5;
  g_homeSection = 0;
  InvalidateRect(g_window, nullptr, FALSE);
}

static void StartDownload() {
  SetDownloadError(L"");
  SetInstallerPath(L"");
  InterlockedExchange(&g_downloadPercent, 0);
  InterlockedExchange(&g_downloadState, 1);
  g_screen = 3;
  CreateThread(nullptr, 0, DownloadThreadProc, nullptr, 0, nullptr);
  InvalidateRect(g_window, nullptr, FALSE);
}

static void RunInstallerAndRestart() {
  std::wstring installer = GetInstallerPath();
  if (installer.empty()) {
    SetDownloadError(L"没有可用的安装包，请重新下载。");
    g_screen = 4;
    InvalidateRect(g_window, nullptr, FALSE);
    return;
  }
  std::wstring params = L"/S";
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = installer.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteExW(&info)) {
    SetDownloadError(L"无法启动安装程序，请允许管理员权限后重试。");
    g_screen = 4;
    InvalidateRect(g_window, nullptr, FALSE);
    return;
  }
  if (info.hProcess) {
    DWORD code = 1;
    WaitForSingleObject(info.hProcess, INFINITE);
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (code != 0) {
      wchar_t temp[MAX_PATH] = {0};
      GetTempPathW(MAX_PATH, temp);
      std::wstring log = std::wstring(temp) + L"ChenyiOffice-update-error.txt";
      if (code == 2) {
        SetDownloadError(
            L"静默更新已取消：没有检测到已安装的 Chenyi Office。\n\n"
            L"请手动运行下载的安装包，并输入首次安装密码。\n诊断日志：" + log);
      } else {
        SetDownloadError(L"更新安装失败。\n\n诊断日志：" + log +
                         L"\n\n请重试，或打开下载页手动安装。");
      }
      g_screen = 4;
      InvalidateRect(g_window, nullptr, FALSE);
      return;
    }
  }

  wchar_t self[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  ShellExecuteW(nullptr, L"open", self, nullptr, nullptr, SW_SHOWNORMAL);
  PostQuitMessage(0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return 0;

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
      g_hits.clear();

      Bitmap buffer(g_width, g_height, PixelFormat32bppPARGB);
      Graphics g(&buffer);
      g.SetSmoothingMode(SmoothingModeAntiAlias);
      g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
      g.Clear(ui::Paper());

      switch (g_screen) {
        case 0: DrawSplash(g); break;
        case 1: DrawAnnouncement(g); break;
        case 2: DrawUpdate(g); break;
        case 3: DrawDownloading(g); break;
        case 4: DrawError(g); break;
        case 5: DrawHome(g); break;
        default: DrawSplash(g); break;
      }

      Graphics target(dc);
      target.DrawImage(&buffer, 0, 0, g_width, g_height);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_TIMER: {
      if (g_screen == 0) {
        ULONGLONG elapsed = GetTickCount64() - g_startTick;
        LONG state = InterlockedCompareExchange(&g_checkState, 0, 0);
        if (state == 1 && elapsed >= 1400) {
          if (!g_update.version.empty()) {
            g_screen = 2;
          } else if (!g_announcements.empty()) {
            g_screen = 1;
            g_announcementIndex = 0;
            g_scroll = 0;
          } else {
            g_screen = 5;
            g_homeSection = 0;
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (state == 2 && elapsed >= 1000) {
          g_screen = 5;
          g_homeSection = 0;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      } else if (g_screen == 3) {
        LONG state = InterlockedCompareExchange(&g_downloadState, 0, 0);
        if (state == 2) {
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
      if (action != A_NONE) {
        OnAction(action);
        return 0;
      }
      if (point.y < TITLEBAR) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      }
      return 0;
    }

    case WM_KEYDOWN:
      if (wParam == VK_RETURN) {
        if (g_screen == 1) {
          OnAction(A_CONTINUE);
        } else if (g_screen == 2) {
          OnAction(A_DOWNLOAD);
        } else if (g_screen == 4) {
          OnAction(A_RETRY);
        } else if (g_screen == 5) {
          OnAction(A_OPEN_FILE);
        }
      } else if (wParam == VK_ESCAPE) {
        if (g_screen == 1) {
          OnAction(A_CONTINUE);
        } else if (g_screen == 2 || g_screen == 4) {
          OnAction(A_SKIP_UPDATE);
        } else {
          OnAction(A_CLOSE);
        }
      }
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
      // Updates are optional: every screen may be closed. Doing so simply
      // defers a pending update to the next launch.
      if (g_screen == 1) {
        FinishAnnouncements();
        return 0;
      }
      PostQuitMessage(0);
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
  ui::InitGdiplus(g_gdiplusToken);
  InitializeCriticalSection(&g_dataLock);

  HANDLE singleInstance = CreateMutexW(nullptr, TRUE, L"Local\\ChenyiOfficeLauncher");
  if (singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(singleInstance);
    return 0;
  }

  HDC screen = GetDC(nullptr);
  g_dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen) ReleaseDC(nullptr, screen);
  if (g_dpi <= 0) g_dpi = 96;

  g_width = MulDiv(kBaseWidth, g_dpi, 96);
  g_height = MulDiv(kBaseHeight, g_dpi, 96);
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
  if (!g_window) {
    ui::ShutdownGdiplus(g_gdiplusToken);
    return 1;
  }
  ui::ApplyWindowChrome(g_window, g_width, g_height, S(18), false);
  RefreshRecent();

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 30, nullptr);

  CreateThread(nullptr, 0, CheckThreadProc, nullptr, 0, nullptr);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  DeleteCriticalSection(&g_dataLock);
  if (singleInstance) CloseHandle(singleInstance);
  ui::ShutdownGdiplus(g_gdiplusToken);
  return 0;
}

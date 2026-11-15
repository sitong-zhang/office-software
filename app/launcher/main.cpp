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
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>

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
static const wchar_t* kVersion = L"0.1.7";
static const wchar_t* kUserAgent = L"ChenyiOffice/0.1.7";

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
  // expiry would otherwise deadlock the mandatory update screen forever.
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
            g_update.mandatory = true;
            g_update.url = Utf8ToWide(url);
            g_update.sha256 = Utf8ToWide(sha);
            g_update.size = size;
            SetStatus(L"发现新版本");
            InterlockedExchange(&g_checkState, 1);
            return 0;
          }
          LogLine(L"manifest update rejected: missing url, sha256, or size");
        } else {
          LogLine(L"manifest update rejected: installer object missing");
        }
      } else {
        SetStatus(L"已是最新版本");
      }
    }
  }

  if (manifestOk) {
    LoadAnnouncements(LoadSeen());
  }

  SetStatus(manifestOk ? L"已是最新版本" : L"无法检查更新");
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
static int g_width = 860;
static int g_height = 540;
static ULONGLONG g_startTick = 0;
static int g_screen = 0;  // 0 splash, 1 announcement, 2 update, 3 downloading, 4 error
static size_t g_announcementIndex = 0;
static int g_scroll = 0;
static int g_scrollMax = 0;

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
  DrawTextLine(dc, GetStatus(), status, 12, FW_NORMAL, RGB(150, 170, 158), L"Microsoft YaHei UI",
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

  RECT msg{pad, S(168), g_width - pad, S(318)};
  DrawTextLine(dc, GetDownloadError(), msg, 15, FW_NORMAL, RGB(226, 166, 158),
               L"Microsoft YaHei UI", DT_LEFT | DT_WORDBREAK);

  RECT linkLabel{pad, S(336), g_width - pad, S(360)};
  DrawTextLine(dc, L"手动下载地址：", linkLabel, 12, FW_NORMAL, RGB(150, 170, 158),
               L"Microsoft YaHei UI", DT_LEFT | DT_SINGLELINE);

  RECT link{pad, S(362), g_width - pad, S(410)};
  DrawTextLine(dc, g_update.url, link, 11, FW_NORMAL, RGB(136, 196, 168), L"Consolas",
               DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

  DrawButton(dc, L"重试", false);
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

static void LaunchEngine() {
  std::wstring engine = FindEngine();
  LogLine(engine.empty() ? L"engine not found" : L"launch engine: " + engine);
  if (engine.empty()) {
    LogLine(L"engine missing; EnginePath and standard locations were checked");
    int answer = MessageBoxW(
        g_window,
        L"没有找到办公套件内核。它可能已被卸载，或被系统中的 LibreOffice 升级替换。\n\n"
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
  std::wstring profileArg =
      L"-env:UserInstallation=\"" + FileUrlFromPath(ProfileDirectory()) + L"\"";
  info.lpParameters = profileArg.c_str();
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
      // The splash can be closed; the announcement gate behaves like the
      // "continue" button. Mandatory updates and downloads cannot be closed.
      if (g_screen == 0) {
        PostQuitMessage(0);
        return 0;
      }
      if (g_screen == 1) {
        FinishAnnouncements();
        return 0;
      }
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
  DeleteCriticalSection(&g_dataLock);
  if (singleInstance) CloseHandle(singleInstance);
  return 0;
}

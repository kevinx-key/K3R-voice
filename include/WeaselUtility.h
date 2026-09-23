#pragma once
#include <filesystem>
#include <string>
#include <sstream>
#include <wrl/client.h>
#include <shellapi.h>
using namespace Microsoft::WRL;

namespace fs = std::filesystem;

inline int utf8towcslen(const char* utf8_str, int utf8_len) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

inline std::wstring getUsername() {
  DWORD len = 0;
  GetUserName(NULL, &len);

  if (len <= 0) {
    return L"";
  }

  wchar_t* username = new wchar_t[len + 1];

  GetUserName(username, &len);
  if (len <= 0) {
    delete[] username;
    return L"";
  }
  auto res = std::wstring(username);
  delete[] username;
  return res;
}

// data directories
std::filesystem::path WeaselSharedDataPath();
std::filesystem::path WeaselUserDataPath();
inline fs::path WeaselLogPath() {
  WCHAR _path[MAX_PATH] = {0};
  // default location
  ExpandEnvironmentStringsW(L"%TEMP%\\rime.weasel", _path, _countof(_path));
  fs::path path = fs::path(_path);
  if (!fs::exists(path)) {
    fs::create_directories(path);
  }
  return path;
}

inline BOOL IsUserDarkMode() {
  constexpr const LPCWSTR key =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
  constexpr const LPCWSTR value = L"AppsUseLightTheme";

  DWORD type;
  DWORD data;
  DWORD size = sizeof(DWORD);
  LSTATUS st = RegGetValue(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD,
                           &type, &data, &size);

  if (st == ERROR_SUCCESS && type == REG_DWORD)
    return data == 0;
  return false;
}

inline std::wstring string_to_wstring(const std::string& str,
                                      int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != 0 && code_page != CP_UTF8)
    return L"";
  // calc len
  int len =
      MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), NULL, 0);
  if (len <= 0)
    return L"";
  std::wstring res;
  TCHAR* buffer = new TCHAR[len + 1];
  MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer, len);
  buffer[len] = '\0';
  res.append(buffer);
  delete[] buffer;
  return res;
}

inline std::string wstring_to_string(const std::wstring& wstr,
                                     int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != 0 && code_page != CP_UTF8)
    return "";
  int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                                NULL, 0, NULL, NULL);
  if (len <= 0)
    return "";
  std::string res;
  char* buffer = new char[len + 1];
  WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(), buffer, len,
                      NULL, NULL);
  buffer[len] = '\0';
  res.append(buffer);
  delete[] buffer;
  return res;
}

inline BOOL is_wow64() {
  DWORD errorCode;
  if (GetSystemWow64DirectoryW(NULL, 0) == 0)
    if ((errorCode = GetLastError()) == ERROR_CALL_NOT_IMPLEMENTED)
      return FALSE;
    else
      ExitProcess((UINT)errorCode);
  else
    return TRUE;
}

template <typename CharT>
struct EscapeChar {
  static const CharT escape;
  static const CharT linefeed;
  static const CharT tab;
  static const CharT linefeed_escape;
  static const CharT tab_escape;
};

template <>
const char EscapeChar<char>::escape = '\\';
template <>
const char EscapeChar<char>::linefeed = '\n';
template <>
const char EscapeChar<char>::tab = '\t';
template <>
const char EscapeChar<char>::linefeed_escape = 'n';
template <>
const char EscapeChar<char>::tab_escape = 't';

template <>
const wchar_t EscapeChar<wchar_t>::escape = L'\\';
template <>
const wchar_t EscapeChar<wchar_t>::linefeed = L'\n';
template <>
const wchar_t EscapeChar<wchar_t>::tab = L'\t';
template <>
const wchar_t EscapeChar<wchar_t>::linefeed_escape = L'n';
template <>
const wchar_t EscapeChar<wchar_t>::tab_escape = L't';

template <typename CharT>
inline std::basic_string<CharT> escape_string(
    const std::basic_string<CharT> input) {
  using Esc = EscapeChar<CharT>;
  std::basic_stringstream<CharT> res;
  for (auto p = input.begin(); p != input.end(); ++p) {
    if (*p == Esc::escape) {
      res << Esc::escape << Esc::escape;
    } else if (*p == Esc::linefeed) {
      res << Esc::escape << Esc::linefeed_escape;
    } else if (*p == Esc::tab) {
      res << Esc::escape << Esc::tab_escape;
    } else {
      res << *p;
    }
  }
  return res.str();
}

template <typename CharT>
inline std::basic_string<CharT> unescape_string(
    const std::basic_string<CharT>& input) {
  using Esc = EscapeChar<CharT>;
  std::basic_stringstream<CharT> res;
  for (auto p = input.begin(); p != input.end(); ++p) {
    if (*p == Esc::escape) {
      if (++p == input.end()) {
        break;
      } else if (*p == Esc::linefeed_escape) {
        res << Esc::linefeed;
      } else if (*p == Esc::tab_escape) {
        res << Esc::tab;
      } else {  // \a => a
        res << *p;
      }
    } else {
      res << *p;
    }
  }
  return res.str();
}

// resource
std::string GetCustomResource(const char* name, const char* type);

// K3R-voice fork: unified entry point -- launch the Listen app settings window.
//
// 解析顺序（自己查注册表，而不是把裸文件名交给 ShellExecute：实测在提权进程里
// ShellExecuteW 对裸文件名的 App Paths 解析会失败，返回 SE_ERR_FNF）：
//   1) App Paths 注册表：HKLM 64/32 视图 + HKCU 64/32 视图
//   2) 裸文件名（交给系统按 PATH / App Paths 解析）
//   3) 常见安装位置兜底
inline bool LaunchListenSettings(HWND hParent = NULL) {
  const wchar_t* kAppPaths =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\listen.exe";

  std::wstring exe;
  const HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  const REGSAM views[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
  for (HKEY root : roots) {
    for (REGSAM view : views) {
      HKEY key = NULL;
      if (RegOpenKeyExW(root, kAppPaths, 0, KEY_READ | view, &key) !=
          ERROR_SUCCESS)
        continue;
      wchar_t buf[MAX_PATH] = {0};
      DWORD cb = sizeof(buf);
      DWORD type = 0;
      LONG r = RegQueryValueExW(key, NULL, NULL, &type, (LPBYTE)buf, &cb);
      RegCloseKey(key);
      if (r == ERROR_SUCCESS && buf[0]) {
        exe = buf;
        if (exe.size() >= 2 && exe.front() == L'"' && exe.back() == L'"')
          exe = exe.substr(1, exe.size() - 2);
        break;
      }
    }
    if (!exe.empty())
      break;
  }

  auto launch = [](const std::wstring& path) -> bool {
    if (path.empty())
      return false;
    return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), L"--settings",
                                    NULL, SW_SHOWNORMAL) > 32;
  };

  if (launch(exe) || launch(L"listen.exe"))
    return true;

  wchar_t dir[MAX_PATH] = {0};
  if (GetEnvironmentVariableW(L"ProgramFiles", dir, MAX_PATH) &&
      launch(std::wstring(dir) + L"\\Listen\\listen.exe"))
    return true;
  if (GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH) &&
      launch(std::wstring(dir) + L"\\Programs\\Listen\\listen.exe"))
    return true;

  MessageBoxW(hParent, L"未找到倾听输入法，请先安装。", L"倾听输入法",
              MB_ICONINFORMATION | MB_OK);
  return false;
}

inline std::wstring get_weasel_ime_name() {
  LANGID langId = GetUserDefaultUILanguage();

  if (langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_HONGKONG) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_MACAU)) {
    return L"傾聽輸入法";
  } else if (langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) ||
             langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE)) {
    return L"倾听输入法";
  } else {
    return L"Listen";
  }
}

inline LONG RegGetStringValue(HKEY key,
                              LPCWSTR lpSubKey,
                              LPCWSTR lpValue,
                              std::wstring& value) {
  TCHAR szValue[MAX_PATH];
  DWORD dwBufLen = MAX_PATH;

  LONG lRes = RegGetValue(key, lpSubKey, lpValue, RRF_RT_REG_SZ, NULL, szValue,
                          &dwBufLen);
  if (lRes == ERROR_SUCCESS) {
    value = std::wstring(szValue);
  }
  return lRes;
}

inline LANGID get_language_id() {
  std::wstring lang{};
  if (RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel",
                        L"Language", lang) == ERROR_SUCCESS) {
    if (lang == L"chs")
      return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    else if (lang == L"cht")
      return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
    else if (lang == L"eng")
      return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  }
  LANGID langId = GetUserDefaultUILanguage();
  if (langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) ||
      langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE)) {
    langId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  } else if (langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL) ||
             langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_HONGKONG) ||
             langId == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_MACAU)) {
    langId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
  } else {
    langId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  }
  return langId;
}

#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x, CP_ACP)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define acptow(x) string_to_wstring(x, CP_ACP)
#define u8toacp(x) wtoacp(u8tow(x))

class DebugStream {
 public:
  DebugStream() = default;
  ~DebugStream() { OutputDebugString(ss.str().c_str()); }
  template <typename T>
  DebugStream& operator<<(const T& value) {
    ss << value;
    return *this;
  }
  DebugStream& operator<<(const char* value) {
    if (value) {
      std::wstring wvalue(u8tow(value));  // utf-8
      ss << wvalue;
    }
    return *this;
  }
  DebugStream& operator<<(const std::string value) {
    std::wstring wvalue(acptow(value));  // utf-8
    ss << wvalue;
    return *this;
  }

 private:
  std::wstringstream ss;
};
inline std::string current_time() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto time_point = system_clock::to_time_t(now);
  auto ns = duration_cast<microseconds>(now.time_since_epoch());  // 转换为微秒
  std::tm tm = *std::localtime(&time_point);
  std::ostringstream oss;
  oss << std::put_time(&tm,
                       "%Y%m%d %H:%M:%S");  // 日期时间格式：20241113 08:54:34
  oss << "." << std::setw(6) << std::setfill('0')
      << ns.count() % 1000000;  // 微秒部分
  return oss.str();
}
#define DEBUG                                                       \
  (DebugStream() << "[" << current_time() << " " << __FILE__ << ":" \
                 << __LINE__ << "] ")

using wstring = std::wstring;
using string = std::string;
template <typename T>
using vector = std::vector<T>;

inline string HRESULTToString(HRESULT hr) {
  if (SUCCEEDED(hr))
    return "Success";
  char buffer[512];
  DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD dwSize =
      FormatMessageA(dwFlags, nullptr, hr, 0, buffer, sizeof(buffer), nullptr);
  if (dwSize == 0)
    return "Unknown HRESULT error";
  return string(buffer);
}

struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};

#define HR(result) HR_Impl(result, __FILE__, __LINE__)

inline void HR_Impl(HRESULT const result, const char* file, int line) {
  if (S_OK != result) {
    DebugStream() << "[" << current_time() << " " << file << ":" << line << "] "
                  << HRESULTToString(result);
    throw ComException(result);
  }
}

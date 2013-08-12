#include "anchocommons.h"
#include "Exceptions.h"

#include <ShlObj.h>
#include <map>

#pragma comment(lib, "Version.lib")

#include <boost/format.hpp>
//----------------------------------------------------------------------------
//
bool isExtensionPage(const std::wstring &aUrl)
{
  return std::wstring::npos != aUrl.find(s_AnchoProtocolHandlerPrefix);
}
//----------------------------------------------------------------------------
//
std::wstring getDomainName(const std::wstring &aUrl)
{
  static const std::wstring protocolDelimiter = L"://";
  size_t prefix = aUrl.find(protocolDelimiter);
  if (prefix == std::wstring::npos) {
    throw std::runtime_error("Wrong URL format");
  }
  size_t domainStart = prefix + protocolDelimiter.size();

  size_t domainEnd = aUrl.find_first_of(L'/', domainStart);

  return aUrl.substr(domainStart, domainEnd-domainStart);
}
//----------------------------------------------------------------------------
//
std::wstring stringFromCLSID(const CLSID &aCLSID)
{
  LPOLESTR lpolestr;
  IF_FAILED_THROW(::StringFromCLSID(aCLSID, &lpolestr));
  std::wstring tmp(lpolestr);
  CoTaskMemFree(lpolestr);
  return tmp;
};
//----------------------------------------------------------------------------
//
std::wstring stringFromGUID2(const GUID &aGUID)
{
  wchar_t guid[1024] = {0};
  IF_FAILED_THROW(::StringFromGUID2( aGUID, (OLECHAR*)guid, sizeof(guid)));
  return std::wstring(guid);
};

//----------------------------------------------------------------------------
//
typedef HRESULT (WINAPI* fGetKnownFolderPath)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *path);
typedef std::map<int, std::wstring> SystemPathCache;
SystemPathCache gCachedSystemPaths;

std::wstring getSystemPathWithFallback(REFKNOWNFOLDERID aKnownFolderID, int aCLSID)
{
  static HINSTANCE hShell32DLLInst = LoadLibrary(L"Shell32.dll");

  SystemPathCache::iterator it = gCachedSystemPaths.find(aCLSID);
  if (it != gCachedSystemPaths.end()) {
    return it->second;
  }

  if (hShell32DLLInst)
  {
    // If we are on Vista or later we should use KNOWNFOLDERID to get a directory
    LPWSTR path;
    fGetKnownFolderPath pfnGetKnownFolderPath =
      (fGetKnownFolderPath) GetProcAddress(hShell32DLLInst, "SHGetKnownFolderPath");
    if (pfnGetKnownFolderPath) {
      IF_FAILED_THROW(pfnGetKnownFolderPath(aKnownFolderID, 0, NULL, &path));
      std::wstring tmpPath = path;
      ::CoTaskMemFree(path);
      gCachedSystemPaths[aCLSID] = tmpPath;
      return tmpPath;
    }
  }

  // This probably means we are on XP, so we fall back on the normal SHGetFolderPath call.
  WCHAR appDataPath[MAX_PATH];
  IF_FAILED_THROW(SHGetFolderPath(
                    0, //hwndOwner
                    aCLSID, //nFolder
                    NULL, //hToken
                    SHGFP_TYPE_CURRENT, //dwFlags
                    appDataPath //pszPath
                  ));
  gCachedSystemPaths[aCLSID] = appDataPath;
  return std::wstring(appDataPath);
}


namespace Ancho {
namespace Utils {

boost::filesystem::wpath getAnchoAppDataDirectory() {
  boost::filesystem::wpath path = getSystemPathWithFallback(FOLDERID_LocalAppDataLow, CSIDL_LOCAL_APPDATA);

  path /= L"Salsita";
  return path;
}

std::wstring getProductName(HMODULE hInstance)
{
  HRESULT hr = S_OK;
  WCHAR fileName[_MAX_PATH];
  DWORD size = GetModuleFileName(hInstance, fileName, _MAX_PATH);
  fileName[size] = 0;
  DWORD handle = 0;
  size = GetFileVersionInfoSize(fileName, &handle);
  boost::scoped_array<BYTE> versionInfo(new BYTE[size]);
  if (!GetFileVersionInfo(fileName, handle, size, versionInfo.get())) {
      ANCHO_THROW(EFail());
  }

  struct LANGANDCODEPAGE {
    WORD wLanguage;
    WORD wCodePage;
  } *lpTranslate = NULL;
  UINT cbTranslate = 0;

  IF_FAILED_THROW(VerQueryValue(versionInfo.get(), L"\\VarFileInfo\\Translation", (void**)&lpTranslate, &cbTranslate));
  if (cbTranslate/sizeof(struct LANGANDCODEPAGE) == 0) {
    ANCHO_THROW(EFail());
  }

  std::wstring productNameBlock = boost::str(boost::wformat(L"\\StringFileInfo\\%04x%04x\\ProductName")
                                                  % lpTranslate[0].wLanguage
                                                  % lpTranslate[0].wCodePage);
  wchar_t *lpBuffer = NULL;
  UINT len = 0;
  IF_FAILED_THROW(VerQueryValue(versionInfo.get(), productNameBlock.c_str(), (void**)&lpBuffer, &len));
  if (lpBuffer == NULL || len == 0) {
    ANCHO_THROW(EFail());
  }
  return lpBuffer;
}

} //namespace Utils
} //namespace Ancho
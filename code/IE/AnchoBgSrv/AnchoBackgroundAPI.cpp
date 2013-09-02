/****************************************************************************
 * AnchoBackground.cpp : Implementation of CAnchoBackgroundAPI
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoBackground.h"
#include "AnchoAddonService.h"
//#include "AnchoBackgroundConsole.h"

#include <algorithm>
#include <fstream>


/*============================================================================
 * class CAnchoBackgroundAPI
 */

#ifndef MAGPIE_REGISTERED
// need the function info for logging in case magpie is not registered
_ATL_FUNC_INFO CAnchoBackgroundAPI::info_Console_LogFunction = {CC_STDCALL,VT_EMPTY,2,{VT_BSTR,VT_ARRAY|VT_VARIANT}};
#endif

// string identifyer used in console logging for background
LPCTSTR CAnchoBackgroundAPI::s_BackgroundLogIdentifyer = _T("background");

//----------------------------------------------------------------------------
//  Init
HRESULT CAnchoBackgroundAPI::Init(LPCTSTR lpszThisPath, LPCTSTR lpszRootURL, BSTR bsID, LPCTSTR lpszGUID, LPCTSTR lpszPath, IAnchoServiceApi *pServiceApi)
{
  // set our ID
  m_bsID = bsID;
  m_GUID = lpszGUID;
  mPath = lpszPath;
  PathAddBackslash(mPath.GetBuffer(mPath.GetLength() + 2));
  mPath.ReleaseBuffer();

  m_sRootURL = lpszRootURL;

  // set service API inteface
  m_ServiceApi = pServiceApi;

#ifndef ANCHO_DISABLE_LOGWINDOW
  // create logger window
  IF_FAILED_RET(CLogWindow::CreateLogWindow(&m_LogWindow.p));
#endif //ANCHO_DISABLE_LOGWINDOW

  // create a magpie instance
#ifdef MAGPIE_REGISTERED
  IF_FAILED_RET(m_Magpie.CoCreateInstance(CLSID_MagpieApplication));
#else
  // Load magpie from the same path where this exe file is.
  CString s(lpszThisPath);
  s += _T("Magpie.dll");
  HMODULE hModMagpie = ::LoadLibrary(s);
  if (!hModMagpie)
  {
    return E_FAIL;
  }
  fnCreateMagpieInstance CreateMagpieInstance = (fnCreateMagpieInstance)::GetProcAddress(hModMagpie, "CreateMagpieInstance");
  if (!CreateMagpieInstance)
  {
    return E_FAIL;
  }
  IF_FAILED_RET(CreateMagpieInstance(&m_Magpie));
#endif
  CString appName;
  appName.Format(_T("Ancho background [%s]"), m_bsID);
  m_Magpie->Init((LPWSTR)(LPCWSTR)appName);

  // add a loader for scripts in the extension filesystem
  IF_FAILED_RET(m_Magpie->AddFilesystemScriptLoader((LPWSTR)(LPCWSTR)mPath));

  // add a loder for scripts in this exe file
  IF_FAILED_RET(m_Magpie->AddResourceScriptLoader((HANDLE_PTR)_AtlModule.GetResourceInstance()));

#ifndef ANCHO_DISABLE_LOGWINDOW
  // advise logger
  IF_FAILED_RET(AtlAdvise(m_Magpie, (IUnknown*)(CAnchoAddonBackgroundLogger*)(this), DIID__IMagpieLoggerEvents, &m_dwMagpieSinkCookie));
#endif //ANCHO_DISABLE_LOGWINDOW

  // load manifest
  CString sManifestFilename;
  sManifestFilename.Format(_T("%smanifest.json"), mPath);
  CString code;
  IF_FAILED_RET(appendJSONFileToVariableAssignment(sManifestFilename, L"exports.manifest", code));
  IF_FAILED_RET(m_Magpie->RunScript(L"manifest", (LPWSTR)(LPCWSTR)code));

  CString localesRootDir;
  localesRootDir.Format(L"%s_locales\\", mPath);
  loadAddonLocales(localesRootDir);

  // set ourselfs in magpie as a global accessible object
  IF_FAILED_RET(m_Magpie->AddExtension((LPWSTR)s_AnchoGlobalAPIObjectName, (IAnchoBackgroundAPI*)this));
  IF_FAILED_RET(m_Magpie->AddExtension((LPWSTR)s_AnchoServiceAPIName, pServiceApi));

  // initialize Ancho API
  // api.js will do now additional initialization, like looking at the manifest
  // and creating a background page.
  IF_FAILED_RET(m_Magpie->Run((LPWSTR)s_AnchoMainAPIModuleID));

  return S_OK;
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::FinalConstruct()
{
  return S_OK;
}

//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::FinalRelease()
{
  UnInit();
}

//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::UnInit()
{
  if (m_BackgroundWindow)
  {
    m_BackgroundWindow->DestroyWindow();
    m_BackgroundWindow.Release();
  }
  if (m_LogWindow)
  {
    m_LogWindow->DestroyWindow();
    m_LogWindow.Release();
  }
  if (m_Magpie)
  {
#ifndef ANCHO_DISABLE_LOGWINDOW
    if (m_dwMagpieSinkCookie)
    {
      AtlUnadvise(m_Magpie, DIID__IMagpieLoggerEvents, m_dwMagpieSinkCookie);
      m_dwMagpieSinkCookie = 0;
    }
#endif
    m_Magpie->Shutdown();
    m_Magpie.Release();
  }
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::GetLogWindow(CLogWindowComObject * & pLogWindow)
{
  if (!m_LogWindow)
  {
    return E_UNEXPECTED;
  }
  return m_LogWindow.CopyTo(&pLogWindow);
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::GetContentInfo(ULONG ulInstanceID, BSTR bstrUrl, LPDISPATCH* ppDisp)
{
  ENSURE_RETVAL(ppDisp);
  if (!m_Magpie)
  {
    return E_UNEXPECTED;
  }

  CIDispatchHelper script;
  IF_FAILED_RET(GetMainModuleExportsScript(script));

  CComVariant vtRet;
  CComVariant paramVariants[2] = {CComVariant(bstrUrl), CComVariant(ulInstanceID)};

  DISPPARAMS params = {paramVariants, NULL, 2, 0};
  IF_FAILED_RET(script.Call((LPOLESTR)s_AnchoFnGetContentAPI, &params, &vtRet));
  if (vtRet.vt != VT_DISPATCH)
  {
    return E_FAIL;
  }
  return vtRet.pdispVal->QueryInterface(IID_IDispatch, (void**)ppDisp);
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::ReleaseContentAPI(ULONG ulInstanceID)
{
  if (!m_Magpie)
  {
    return E_UNEXPECTED;
  }

  // get the main api module
  CIDispatchHelper script;
  IF_FAILED_RET(GetMainModuleExportsScript(script));

  CComVariant vtInstanceID(ulInstanceID);
  DISPPARAMS params = {&vtInstanceID, NULL, 1, 0};
  IF_FAILED_RET(script.Call((LPOLESTR)s_AnchoFnReleaseContentAPI, &params));
  return S_OK;
}

//----------------------------------------------------------------------------
//
BOOL CAnchoBackgroundAPI::GetURL(CStringW & sURL)
{
  CString s;
  DWORD dwLength = INTERNET_MAX_URL_LENGTH;
  BOOL b = InternetCombineUrlW(m_sRootURL, sURL, s.GetBuffer(INTERNET_MAX_URL_LENGTH), &dwLength, 0);
  s.ReleaseBuffer();
  if (b)
  {
    sURL = s;
  }
  return b;
}
//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::OnAddonServiceReleased()
{
  m_ServiceApi.Release();
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::GetMainModuleExportsScript(CIDispatchHelper & script)
{
  // get the main api module
  CComPtr<IMagpieModuleRestricted> mainModule;
  IF_FAILED_RET(m_Magpie->GetModuleObject((LPOLESTR)s_AnchoMainAPIModuleID, &mainModule));

  CComPtr<IDispatch> mainModuleExports;
  IF_FAILED_RET(mainModule->GetExportsObject(&mainModuleExports));

  script = mainModuleExports;
  return S_OK;
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::loadAddonLocales(CString aPath)
{
  std::vector<std::wstring> dirs;

  CString searchPath = aPath;
  searchPath.Append(L"*");
  WIN32_FIND_DATA searchInfo;
  HANDLE searchHandle = FindFirstFile(searchPath, &searchInfo);
  if (searchHandle!=INVALID_HANDLE_VALUE) {
    bool searching(true);
    while (searching) {
      if (searchInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        std::wstring dirName(searchInfo.cFileName);
        if (dirName != L"." && dirName != L"..") {
          dirs.push_back(dirName);
        }
      }
      searching = FALSE != FindNextFile(searchHandle, &searchInfo);
    }
  }
  CString code = L"exports.locales = {};";
  for (size_t i = 0; i < dirs.size(); ++i) {
    CString fileName = aPath + dirs[i].c_str() + L"\\messages.json";
    CString variableName;
    variableName.Format(L"exports.locales['%s']", dirs[i].c_str());
    appendJSONFileToVariableAssignment(fileName, variableName, code);
  }
  IF_FAILED_RET(m_Magpie->RunScript(L"locales", (LPWSTR)(LPCWSTR)code));
  return S_OK;
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoBackgroundAPI::appendJSONFileToVariableAssignment(CString aFileName, CString aVariableName, CString &aCode)
{
  std::ifstream f((char*)CW2A(aFileName), std::ios_base::in);
  if (!f.is_open()) {
    return E_FAIL;
  }
  std::string loadedCode;
  f.seekg(0, std::ios::end);
  if (f.tellg() > 0x00000000ffffffff) { //limit to 4gb
    return E_OUTOFMEMORY;
  }
  loadedCode.reserve(static_cast<size_t>(f.tellg()));
  f.seekg(0, std::ios::beg);
  loadedCode.assign((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

  CString code;
  code.Format(L"%s = %hs;", aVariableName, loadedCode.c_str());

  aCode += code;
  return S_OK;
}

//----------------------------------------------------------------------------
//  IAnchoBackgroundAPI methods
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::get_id(BSTR * pVal)
{
  ENSURE_RETVAL(pVal);
  return m_bsID.CopyTo(pVal);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::get_guid(BSTR * pVal)
{
  ENSURE_RETVAL(pVal);
  (*pVal) = m_GUID.AllocSysString();
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::get_path(BSTR * aPath)
{
  ENSURE_RETVAL(aPath);
  (*aPath) = mPath.AllocSysString();
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::startBackgroundWindow(BSTR bsPartialURL)
{
  // it's safe to call this method multiple times, anyhow the window
  // will be created only once
  if (m_BackgroundWindow) {
    return S_OK;
  }
  CStringW sURL(bsPartialURL);
  if (!GetURL(sURL)) {
    return E_FAIL;
  }

  // get the main api module and inject it into the background page
  CComPtr<IMagpieModuleRestricted> mainModule;
  IF_FAILED_RET(m_Magpie->GetModuleObject((LPOLESTR)s_AnchoMainAPIModuleID, &mainModule));

  CComPtr<IDispatch> mainModuleExports;
  IF_FAILED_RET(mainModule->GetExportsObject(&mainModuleExports));

  CComVariant chromeVT;
  IF_FAILED_RET(mainModuleExports.GetPropertyByName(s_AnchoBackgroundPageAPIName, &chromeVT));
  if (chromeVT.vt != VT_DISPATCH) {
    return E_FAIL;
  }

  CComVariant consoleVT;
  IF_FAILED_RET(mainModuleExports.GetPropertyByName(s_AnchoBackgroundConsoleObjectName, &consoleVT));
  if (consoleVT.vt != VT_DISPATCH) {
    return E_FAIL;
  }

  DispatchMap injectedObjects;
  injectedObjects[s_AnchoBackgroundPageAPIName] = chromeVT.pdispVal;
  injectedObjects[s_AnchoBackgroundConsoleObjectName] = consoleVT.pdispVal;

  IF_FAILED_RET(CBackgroundWindow::CreateBackgroundWindow(injectedObjects, sURL, &m_BackgroundWindow.p));
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::addEventObject(BSTR aEventName, INT aInstanceId, LPDISPATCH aListener)
{
  try {
    std::wstring eventName(aEventName, SysStringLen(aEventName));

    EventObjectList &managers = m_EventObjects[eventName];
    managers.push_back(EventObjectRecord(eventName, aInstanceId, aListener));
  } catch (std::bad_alloc &) {
    ATLTRACE(L"Adding event object %s::%d failed : %s", aEventName, aInstanceId);
    return E_FAIL;
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//
struct FindEventByInstanceIdFunctor
{
  FindEventByInstanceIdFunctor(int aInstanceId): instanceId(aInstanceId) {}
  int instanceId;
  bool operator()(const CAnchoBackgroundAPI::EventObjectRecord &aRec) {
    return aRec.instanceID == instanceId;
  }
};
STDMETHODIMP CAnchoBackgroundAPI::removeEventObject(BSTR aEventName, INT aInstanceId)
{
  std::wstring eventName(aEventName, SysStringLen(aEventName));
  EventObjectMap::iterator it = m_EventObjects.find(eventName);
  if (it == m_EventObjects.end()) {
    return S_OK;
  }
  it->second.remove_if(FindEventByInstanceIdFunctor(aInstanceId));
  return S_OK;
}
//----------------------------------------------------------------------------
//

STDMETHODIMP CAnchoBackgroundAPI::callFunction(LPDISPATCH aFunction, LPDISPATCH aArgs, VARIANT* aRet)
{
  ENSURE_RETVAL(aRet);
  CIDispatchHelper function(aFunction);
  VariantVector args;

  IF_FAILED_RET(addJSArrayToVariantVector(aArgs, args, true));
  return function.InvokeN((DISPID)0, args.size()>0? &(args[0]): NULL, (int)args.size(), aRet);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::invokeEventObject(BSTR aEventName, INT aSelectedInstance, BOOL aSkipInstance, LPDISPATCH aArgs, VARIANT* aRet)
{
  ENSURE_RETVAL(aRet);

  VariantVector args;
  VariantVector results;

  HRESULT hr = addJSArrayToVariantVector(aArgs, args, true);
  if (FAILED(hr)) {
    return hr;
  }

  IF_FAILED_RET(invokeEvent(aEventName, aSelectedInstance, aSkipInstance != FALSE, args, results));

  return appendVectorToSafeArray(results, *aRet);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::invokeEventWithIDispatchArgument(BSTR aEventName, LPDISPATCH aArg)
{
  if (!m_InvokeEventWithIDispatch) {
    return E_FAIL;
  }
  CComVariant eventName(aEventName);
  CComVariant dispatchObject(aArg);
  return m_InvokeEventWithIDispatch.Invoke2((DISPID)0, &eventName, &dispatchObject);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::setIDispatchEventInvocationHandler(LPDISPATCH aFunction)
{
  m_InvokeEventWithIDispatch = aFunction;
  return S_OK;
}
//----------------------------------------------------------------------------
//

struct InvokeSelectedEventFunctor
{
  InvokeSelectedEventFunctor(VariantVector &aArgs, VariantVector &aResults, int aSelectedInstance)
    : mArgs(aArgs), mResults(aResults), mSelectedInstance(aSelectedInstance) { }
  VariantVector &mArgs;
  VariantVector &mResults;
  int mSelectedInstance;

  void operator()(CAnchoBackgroundAPI::EventObjectRecord &aRec) {
    if (aRec.instanceID == mSelectedInstance) {
      CComVariant result;
      aRec.listener.InvokeN((DISPID)0, mArgs.size()>0? &(mArgs[0]): NULL, (int)mArgs.size(), &result);
      if (result.vt == VT_DISPATCH) {
        addJSArrayToVariantVector(result.pdispVal, mResults);
      }
    }
  }
};

struct InvokeUnselectedEventFunctor
{
  InvokeUnselectedEventFunctor(VariantVector &aArgs, VariantVector &aResults, int aSelectedInstance)
    : mArgs(aArgs), mResults(aResults), mSelectedInstance(aSelectedInstance) { }
  VariantVector &mArgs;
  VariantVector &mResults;
  int mSelectedInstance;

  void operator()(CAnchoBackgroundAPI::EventObjectRecord &aRec) {
    if (aRec.instanceID != mSelectedInstance) {
      CComVariant result;
      aRec.listener.InvokeN((DISPID)0, mArgs.size()>0? &(mArgs[0]): NULL, (int)mArgs.size(), &result);
      if (result.vt == VT_DISPATCH) {
        addJSArrayToVariantVector(result.pdispVal, mResults);
      }
    }
  }
};

STDMETHODIMP CAnchoBackgroundAPI::invokeEvent(BSTR aEventName, INT aSelectedInstance, bool aSkipInstance, VariantVector &aArgs, VariantVector &aResults)
{
  std::wstring eventName(aEventName, SysStringLen(aEventName));
  EventObjectMap::iterator it = m_EventObjects.find(eventName);
  if (it == m_EventObjects.end()) {
    return S_OK;
  }
  if(aSkipInstance) {
    std::for_each(it->second.begin(), it->second.end(), InvokeUnselectedEventFunctor(aArgs, aResults, aSelectedInstance));
  } else {
    std::for_each(it->second.begin(), it->second.end(), InvokeSelectedEventFunctor(aArgs, aResults, aSelectedInstance));
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
static void checkStorageType(const std::wstring &aStorageType)
{
  // Only two storage types are supported by google
  if (aStorageType != L"local" && aStorageType != L"sync") {
    ANCHO_THROW(std::runtime_error("Unsupported storage type"));
  }
}
//----------------------------------------------------------------------------
//
StorageDatabase & CAnchoBackgroundAPI::getStorageInstance(const std::wstring &aStorageType)
{
  checkStorageType(aStorageType);

  //We lock to be sure that we open the databse only once
  boost::unique_lock<boost::recursive_mutex> lock(mStorageMutex);

  if (!mStorageLocalDb.isOpened()) {
    boost::filesystem::wpath path = Ancho::Utils::getAnchoAppDataDirectory();

    if (!boost::filesystem::exists(path)) {
      ATLTRACE(L"Creating directory: %s\n", path.wstring().c_str());
      if (!boost::filesystem::create_directories(path)) {
        ATLTRACE(L"Failed to create directory: %s\n", path.wstring().c_str());
        ANCHO_THROW(std::runtime_error("Failed to create directory."));
      }
    }
    path /= s_AnchoStorageLocalDatabaseFileName;
    //TODO - escape table name
    std::wstring tableName = std::wstring(L"Ancho_") + m_bsID.m_str;
    mStorageLocalDb.open(path.wstring(), tableName);
  }

  //We currently map both storage types to storage.local
  return mStorageLocalDb;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::storageGet(BSTR aStorageType, LPDISPATCH aKeysArray, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aStorageType == NULL || aKeysArray == NULL || aCallback == NULL || aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  std::wstring storageType(aStorageType);
  checkStorageType(storageType);

  Ancho::Utils::JavaScriptCallback<StorageItemsObject, void> callback(aCallback);
  auto keysArray = Ancho::Utils::convertIDispatchToJSArray(aKeysArray);

  //All COM arguments wrapped or copied to normal C++ datastructures, so we can use them in other threads
  storageGet(storageType, keysArray, callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}
//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::storageGet(const std::wstring &aStorageType, const Ancho::Utils::JSArray &aKeysArray, StorageItemsObjectCallback aCallback, const std::wstring &aExtensionId, int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    auto &storage = this->getStorageInstance(aStorageType);
    // Create JS object by constructor from right context
    // This object will contain requested items
    CComPtr<IDispatch> data = CAnchoAddonService::instance().createObject(aExtensionId, aApiId);
    _variant_t vtData(data);
    // Wrap the object for easier manipulation
    Ancho::Utils::JSObjectWrapper dataWrapper = Ancho::Utils::JSValueWrapper(vtData.GetVARIANT()).toObject();
    for (size_t i = 0; i < aKeysArray.size(); ++i) {
      std::wstring key = boost::get<std::wstring>(aKeysArray[i]);
      std::wstring value;
      try {
        storage.getItem(key, value);
        dataWrapper[key] = value;
      } catch (StorageDatabase::EItemNotFound &) {
        // Item not in the database -> nothing stored in the result object
      }
    }
    aCallback(data);
  });
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::storageSet(BSTR aStorageType, LPDISPATCH aValuesObject, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aStorageType == NULL || aValuesObject == NULL || aCallback == NULL || aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  std::wstring storageType(aStorageType);
  checkStorageType(storageType);

  Ancho::Utils::JavaScriptCallback<void, void> callback(aCallback);
  auto valuesObject = Ancho::Utils::convertIDispatchToJSObject(aValuesObject);

  //All COM arguments wrapped or copied to normal C++ datastructures, so we can use them in other threads
  storageSet(storageType, valuesObject, callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}
//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::storageSet(const std::wstring &aStorageType, const Ancho::Utils::JSObject &aValuesObject, SimpleCallback aCallback, const std::wstring &aExtensionId, int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    auto &storage = this->getStorageInstance(aStorageType);

    // Store all atributes of passed obejct into the database
    for (auto it = aValuesObject.begin(); it != aValuesObject.end(); ++it) {
      std::wstring key = it->first;
      // Each attribute must be a string - JSON.stringify should have been called on JS side
      if (it->second.which() != Ancho::Utils::jsString) {
        ATLTRACE(L"Skipping store of key %s - value is not a string\n", key.c_str());
        continue;
      }
      std::wstring value = boost::get<std::wstring>(it->second);
      storage.setItem(key, value);
    }
    aCallback();
  });
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::storageRemove(BSTR aStorageType, LPDISPATCH aKeysArray, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aStorageType == NULL || aKeysArray == NULL || aCallback == NULL || aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  std::wstring storageType(aStorageType);
  checkStorageType(storageType);
  Ancho::Utils::JavaScriptCallback<void, void> callback(aCallback);

  auto keysArray = Ancho::Utils::convertIDispatchToJSArray(aKeysArray);

  //All COM arguments wrapped or copied to normal C++ datastructures, so we can use them in worker threads
  storageRemove(storageType, keysArray, callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}
//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::storageRemove(const std::wstring &aStorageType, const Ancho::Utils::JSArray &aKeysArray, SimpleCallback aCallback, const std::wstring &aExtensionId, int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    auto &storage = this->getStorageInstance(aStorageType);

    for (size_t i = 0; i < aKeysArray.size(); ++i) {
      std::wstring key = boost::get<std::wstring>(aKeysArray[i]);
      try {
        storage.removeItem(key);
      } catch (StorageDatabase::EItemNotFound &) {
        // No item - no problem
      }
    }
    aCallback();
  });
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoBackgroundAPI::storageClear(BSTR aStorageType, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aStorageType == NULL || aCallback == NULL || aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  std::wstring storageType(aStorageType);
  checkStorageType(storageType);
  Ancho::Utils::JavaScriptCallback<void, void> callback(aCallback);

  storageClear(storageType, callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}
//----------------------------------------------------------------------------
//
void CAnchoBackgroundAPI::storageClear(const std::wstring &aStorageType, SimpleCallback aCallback, const std::wstring &aExtensionId, int aApiId) {
  mAsyncTaskManager.addTask([=, this](){
    getStorageInstance(aStorageType).clear();
    aCallback();
  });
}

//----------------------------------------------------------------------------
//  _IMagpieLoggerEvents methods
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoBackgroundAPI::OnLog(BSTR bsModuleID, SAFEARRAY* pVals)
{
  if (m_LogWindow)
  {
    m_LogWindow->log(CComBSTR(s_BackgroundLogIdentifyer), bsModuleID, pVals);
  }
}

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoBackgroundAPI::OnDebug(BSTR bsModuleID, SAFEARRAY* pVals)
{
  if (m_LogWindow)
  {
    m_LogWindow->debug(CComBSTR(s_BackgroundLogIdentifyer), bsModuleID, pVals);
  }
}

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoBackgroundAPI::OnInfo(BSTR bsModuleID, SAFEARRAY* pVals)
{
  if (m_LogWindow)
  {
    m_LogWindow->info(CComBSTR(s_BackgroundLogIdentifyer), bsModuleID, pVals);
  }
}

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoBackgroundAPI::OnWarn(BSTR bsModuleID, SAFEARRAY* pVals)
{
  if (m_LogWindow)
  {
    m_LogWindow->warn(CComBSTR(s_BackgroundLogIdentifyer), bsModuleID, pVals);
  }
}

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoBackgroundAPI::OnError(BSTR bsModuleID, SAFEARRAY* pVals)
{
  if (m_LogWindow)
  {
    m_LogWindow->error(CComBSTR(s_BackgroundLogIdentifyer), bsModuleID, pVals);
  }
}

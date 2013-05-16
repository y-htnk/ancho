/****************************************************************************
 * AnchoAddonService.cpp : Implementation of CAnchoAddonService
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoAddonService.h"

#include <sstream>
#include <algorithm>

#include "PopupWindow.h"

struct CookieNotificationCallback: public ACookieCallbackFunctor
{
  CookieNotificationCallback(CAnchoAddonService &aService): service(aService)
  {}

  void operator()(CComVariant &aCookie)
  {
    ATLASSERT(aCookie.vt == VT_DISPATCH);
    CComBSTR eventName(L"cookies.onChanged");

    service.invokeEventObjectInAllExtensionsWithIDispatchArgument(eventName, aCookie.pdispVal);
    ATLTRACE("NOTIFICATION ");
  }

  CAnchoAddonService &service;
};




/*============================================================================
 * class CAnchoAddonService
 */

//----------------------------------------------------------------------------
//
void CAnchoAddonService::OnAddonFinalRelease(BSTR bsID)
{
  m_BackgroundObjects.erase(std::wstring(bsID, SysStringLen(bsID)));
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::get_cookieManager(LPDISPATCH* ppRet)
{
  ENSURE_RETVAL(ppRet);
  return m_Cookies.QueryInterface(ppRet);
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::get_tabManager(LPDISPATCH* ppRet)
{
  ENSURE_RETVAL(ppRet);
  return mITabManager.QueryInterface(ppRet);
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::invokeExternalEventObject(BSTR aExtensionId, BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet)
{
  ENSURE_RETVAL(aRet);
  CAnchoAddonBackgroundComObject* pObject = NULL;

  BackgroundObjectsMap::iterator it = m_BackgroundObjects.find(std::wstring(aExtensionId, SysStringLen(aExtensionId)));
  if (it != m_BackgroundObjects.end()) {
    ATLASSERT(it->second != NULL);
    return it->second->invokeExternalEventObject(aEventName, aArgs, aRet);
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::navigateBrowser(LPUNKNOWN aWebBrowserWin, const std::wstring &url, INT32 aNavigateOptions)
{
  CComQIPtr<IWebBrowser2> webBrowser = aWebBrowserWin;
  if (!webBrowser) {
    return E_NOINTERFACE;
  }

  CComVariant vtUrl(url.c_str());
  CComVariant vtFlags(aNavigateOptions);
  CComVariant vtEmpty;
  return webBrowser->Navigate2(&vtUrl, &vtFlags, &vtEmpty, &vtEmpty, &vtEmpty);
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::getActiveWebBrowser(LPUNKNOWN* pUnkWebBrowser)
{
  CComPtr<IWebBrowser2> pWebBrowser;
  HRESULT hr = FindActiveBrowser(&pWebBrowser);
  if (FAILED(hr)) {
    return hr;
  }
  return pWebBrowser->QueryInterface(IID_IUnknown, (void**) pUnkWebBrowser);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getBrowserActions(VARIANT* aBrowserActionsArray)
{
  ENSURE_RETVAL(aBrowserActionsArray);

  CComVariant tmp(m_BrowserActionInfos.p);
  return tmp.Detach(aBrowserActionsArray);
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::setBrowserActionUpdateCallback(INT aTabId, LPDISPATCH aBrowserActionUpdateCallback)
{
  m_BrowserActionCallbacks[aTabId] = CIDispatchHelper(aBrowserActionUpdateCallback);
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::browserActionNotification()
{
  for (BrowserActionCallbackMap::iterator it = m_BrowserActionCallbacks.begin(); it != m_BrowserActionCallbacks.end(); ++it) {
    it->second.Invoke0(DISPID(0));
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::addBrowserActionInfo(LPDISPATCH aBrowserActionInfo)
{
  if (!aBrowserActionInfo) {
    return E_POINTER;
  }

  m_BrowserActionInfos->push_back(CComVariant(aBrowserActionInfo));

  Ancho::Service::TabManager::instance().forEachTab(
              [](Ancho::Service::TabManager::TabRecord &aRec){ aRec.runtime()->showBrowserActionBar(TRUE); }
            );
  return S_OK;
}

//----------------------------------------------------------------------------
//
void CAnchoAddonService::fillWindowInfo(HWND aWndHandle, CIDispatchHelper &aInfo)
{
  //BOOL isVisible = IsWindowVisible(aWndHandle);
  WINDOWINFO winInfo;
  winInfo.cbSize = sizeof(WINDOWINFO);
  BOOL res = GetWindowInfo(aWndHandle, &winInfo);
  aInfo.SetProperty(L"top", CComVariant(winInfo.rcWindow.top));
  aInfo.SetProperty(L"left", CComVariant(winInfo.rcWindow.left));
  aInfo.SetProperty(L"width", CComVariant(winInfo.rcWindow.right - winInfo.rcWindow.left));
  aInfo.SetProperty(L"height", CComVariant(winInfo.rcWindow.bottom - winInfo.rcWindow.top));
  aInfo.SetProperty(L"focused", CComVariant(static_cast<bool>(winInfo.dwWindowStatus & WS_ACTIVECAPTION)));
  aInfo.SetProperty(L"alwaysOnTop", CComVariant(false));
  aInfo.SetProperty(L"id", CComVariant(winHWNDToId(aWndHandle)));
  if (IsIconic(aWndHandle)) {
    aInfo.SetProperty(L"state", CComVariant(L"minimized"));
  } else if (IsZoomed(aWndHandle)) {
    aInfo.SetProperty(L"state", CComVariant(L"maximized"));
  } else {
    aInfo.SetProperty(L"state", CComVariant(L"normal"));
  }
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getWindow(INT aWindowId, LPDISPATCH aCreator, BOOL aPopulate, VARIANT* aRet)
{
  if (!aCreator) {
    return E_POINTER;
  }
  ENSURE_RETVAL(aRet);

  HWND hwnd = winIdToHWND(aWindowId);

  if (!isIEWindow(hwnd)) {
    return E_INVALIDARG;
  }

  CComVariant info;
  if (S_OK != createIDispatchFromCreator(aCreator, &info)) {
    return E_FAIL;
  }
  CIDispatchHelper infoHelper(info.pdispVal);
  fillWindowInfo(hwnd, infoHelper);
  *aRet = info;
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getAllWindows(LPDISPATCH aCreator, BOOL aPopulate, VARIANT* aRet)
{
  if (!aCreator) {
    return E_POINTER;
  }
  ENSURE_RETVAL(aRet);

  VariantVector windowInfos;
  HWND hIEFrame = NULL;
  do {
    hIEFrame = ::FindWindowEx(NULL, hIEFrame, L"IEFrame", NULL);
    if (hIEFrame) {
      CComVariant info;
      if (S_OK != createIDispatchFromCreator(aCreator, &info)) {
        return E_FAIL;
      }
      CIDispatchHelper infoHelper(info.pdispVal);
      fillWindowInfo(hIEFrame, infoHelper);

      windowInfos.push_back(info);
    }
  }while(hIEFrame);
  return constructSafeArrayFromVector(windowInfos, *aRet);
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::updateWindow(INT aWindowId, LPDISPATCH aProperties)
{
  if (!aProperties) {
    return E_INVALIDARG;
  }
  HWND hwnd = winIdToHWND(aWindowId);
  if (!isIEWindow(hwnd)) {
    return E_INVALIDARG;
  }
  CIDispatchHelper properties = aProperties;

  WINDOWINFO winInfo;
  winInfo.cbSize = sizeof(WINDOWINFO);
  BOOL res = GetWindowInfo(hwnd, &winInfo);
  int top = winInfo.rcWindow.top;
  int left = winInfo.rcWindow.left;
  int width = winInfo.rcWindow.right - winInfo.rcWindow.left;
  int height = winInfo.rcWindow.bottom - winInfo.rcWindow.top;
  bool shouldMove = SUCCEEDED((properties.Get<int, VT_I4>(L"top", top)));
  shouldMove = SUCCEEDED((properties.Get<int, VT_I4>(L"left", left))) || shouldMove;
  shouldMove = SUCCEEDED((properties.Get<int, VT_I4>(L"width", width))) || shouldMove;
  shouldMove = SUCCEEDED((properties.Get<int, VT_I4>(L"height", height))) || shouldMove;
  if (shouldMove) {
    ::MoveWindow(hwnd, left, top, width, height, TRUE);
  }
  bool focused = false;
  if (SUCCEEDED((properties.Get<bool, VT_BOOL, VARIANT_BOOL>(L"focused", focused)))) {
    if(focused) {
      ::SetForegroundWindow(hwnd);
    } else {
      //Bring to foreground next IE window
      HWND nextWin = hwnd;
      while (nextWin = GetNextWindow(nextWin, GW_HWNDNEXT)) {
        if (isIEWindow(hwnd)) {
          ::SetForegroundWindow(nextWin);
          break;
        }
      }
    }
  }
  std::wstring state;
  if ( SUCCEEDED((properties.Get<std::wstring, VT_BSTR, BSTR>(L"state", state)))) {
    if (state == L"maximized") {
      ::ShowWindow(hwnd, SW_MAXIMIZE);
    } else if (state == L"minimized") {
      ::ShowWindow(hwnd, SW_MINIMIZE);
    } else if (state == L"normal") {
      ::ShowWindow(hwnd, SW_NORMAL);
    } else if (state == L"fullscreen") {
      //TODO - fullscreen
    }
  }
  //TODO - drawAttention
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::createWindow(LPDISPATCH aProperties, LPDISPATCH aCreator, LPDISPATCH aCallback)
{
 /* CIDispatchHelper properties(aProperties);

  try {
    m_WebBrowserPostInitTasks.addCommand(AQueuedCommand::Ptr(new CreateWindowCommand(*this, aProperties, aCreator, aCallback)));
  } catch (std::exception &e) {
    ATLTRACE("Error: %s\n", e.what());
    return E_FAIL;
  }*/
  return S_OK;
}

//----------------------------------------------------------------------------
//
/*HRESULT CAnchoAddonService::createWindowImpl(CIDispatchHelper &aProperties, ATabCreatedCallback::Ptr aCallback)
{
  //TODO - handle different types of windows
  return createTabImpl(aProperties, aCallback, true);
}*/
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::closeWindow(INT aWindowId)
{
  HWND win = winIdToHWND(aWindowId);
  if( ::DestroyWindow(win) ) {
    return S_OK;
  } else {
    return E_FAIL;
  }
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::createPopupWindow(BSTR aUrl, INT aX, INT aY, LPDISPATCH aInjectedData, LPDISPATCH aCloseCallback)
{
  if (!aInjectedData || !aCloseCallback) {
    return E_INVALIDARG;
  }

  CIDispatchHelper injectedData(aInjectedData);
  CIDispatchHelper closeCallback(aCloseCallback);
  DispatchMap injectedDataMap;

  IDispatch* api;
  IF_FAILED_RET((injectedData.Get<IDispatch*, VT_DISPATCH>((LPOLESTR)s_AnchoBackgroundPageAPIName, api)));
  injectedDataMap[s_AnchoBackgroundPageAPIName] = api;

  IDispatch* console;
  IF_FAILED_RET((injectedData.Get<IDispatch*, VT_DISPATCH>((LPOLESTR)s_AnchoBackgroundConsoleObjectName, console)));
  injectedDataMap[s_AnchoBackgroundConsoleObjectName] = console;

  HWND hwnd = getCurrentWindowHWND();
  IF_FAILED_RET(CPopupWindow::CreatePopupWindow(hwnd, this, injectedDataMap, aUrl, aX, aY, closeCallback));
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getCurrentWindowId(INT *aWinId)
{
  ENSURE_RETVAL(aWinId);
  HWND hwnd = getCurrentWindowHWND();
  if (hwnd) {
    *aWinId = winHWNDToId(hwnd);
    return S_OK;
  }
  return E_FAIL;
}


//----------------------------------------------------------------------------
//
HWND CAnchoAddonService::getCurrentWindowHWND()
{
  //FindWindowEx returns windows in z-order - the first one is also the top one
  return ::FindWindowEx(NULL, NULL, L"IEFrame", NULL);
}
//----------------------------------------------------------------------------
//
bool CAnchoAddonService::isIEWindow(HWND aHwnd)
{
  wchar_t className[256];
  return GetClassName(aHwnd, className, 256) && (std::wstring(L"IEFrame") == className);
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoAddonService::FinalConstruct()
{
  BEGIN_TRY_BLOCK;
    // Get and store the path, this will be used in some places (e.g. to load
    // additional DLLs).
    LPTSTR psc = m_sThisPath.GetBuffer(_MAX_PATH);
    GetModuleFileName(_AtlModule.m_hInstance, psc, _MAX_PATH);
    PathRemoveFileSpec(psc);
    PathAddBackslash(psc);
    m_sThisPath.ReleaseBuffer();

    CComObject<CIECookieManager> * pCookiesManager = NULL;
    IF_FAILED_RET(CComObject<CIECookieManager>::CreateInstance(&pCookiesManager));
    pCookiesManager->setNotificationCallback(ACookieCallbackFunctor::APtr(new CookieNotificationCallback(*this)));
    pCookiesManager->startWatching();

    m_Cookies = pCookiesManager;

    IF_FAILED_RET(SimpleJSArray::createInstance(m_BrowserActionInfos));

    Ancho::Service::TabManager::initSingleton();
    mITabManager = &Ancho::Service::TabManager::instance();
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}

//----------------------------------------------------------------------------
//
void CAnchoAddonService::FinalRelease()
{
  BackgroundObjectsMap::iterator it = m_BackgroundObjects.begin();
  while(it != m_BackgroundObjects.end())
  {
    ATLASSERT(it->second != NULL);
    it->second->OnAddonServiceReleased();
    ++it;
  }
  m_BackgroundObjects.clear();
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::GetAddonBackground(BSTR bsID, IAnchoAddonBackground ** ppRet)
{
  ENSURE_RETVAL(ppRet);

  CComPtr<IAnchoAddonBackground> ptr;
  std::wstring id = std::wstring(bsID, SysStringLen(bsID));
  BackgroundObjectsMap::iterator it = m_BackgroundObjects.find(id);
  if (it == m_BackgroundObjects.end()) {
    // not found, create new instance
    ATLTRACE(_T("ADD OBJECT %s\n"), bsID);
    CAnchoAddonBackgroundComObject* pObject = NULL;
    HRESULT hr = CAnchoAddonBackgroundComObject::CreateInstance(&pObject);
    if (FAILED(hr))
    {
      return hr;
    }
    // store to a safepointer
    ptr = pObject;

    ///////////////////////////////////////////////////////
    // Init the whole addon. This will load and init the
    // Ancho api.
    hr = pObject->Init(m_sThisPath, this, static_cast<IAnchoServiceApi*>(this), bsID);
    if (FAILED(hr))
    {
      return hr;
    }
    // store in map
    m_BackgroundObjects[id] = pObject;
  } else {
    ATLTRACE(_T("FOUND OBJECT %s\n"), bsID);
    // found, create a new intance ID
    // store to safepointer
    ptr = it->second;
  }

  // set return value
  (*ppRet) = ptr.Detach();
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::GetModulePath(BSTR * pbsPath)
{
  ENSURE_RETVAL(pbsPath);
  (*pbsPath) = m_sThisPath.AllocSysString();
  return S_OK;
}
//----------------------------------------------------------------------------
//

STDMETHODIMP CAnchoAddonService::invokeEventObjectInAllExtensions(BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet)
{
  for (BackgroundObjectsMap::iterator it = m_BackgroundObjects.begin(); it != m_BackgroundObjects.end(); ++it) {
    CComBSTR id(it->first.c_str());
    invokeExternalEventObject(id, aEventName, aArgs, aRet);
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::invokeEventObjectInAllExtensionsWithIDispatchArgument(BSTR aEventName, LPDISPATCH aArg)
{
  for (BackgroundObjectsMap::iterator it = m_BackgroundObjects.begin(); it != m_BackgroundObjects.end(); ++it) {
    it->second->invokeEventWithIDispatchArgument(aEventName, aArg);
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::webBrowserReady()
{
  m_WebBrowserPostInitTasks.autoExecuteAll();
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getInternalProtocolParameters(BSTR * aServiceHost, BSTR * aServicePath)
{
  ENSURE_RETVAL(aServiceHost);
  ENSURE_RETVAL(aServicePath);

  CString serviceHost = L"AnchoBackgroundService";

  WCHAR appPath[MAX_PATH] = {0};
  if (!GetModuleFileNameW(NULL, appPath, _countof(appPath))) {
    return E_FAIL;
  }
  CString servicePath(appPath);

  *aServiceHost = serviceHost.AllocSysString();
  *aServicePath = servicePath.AllocSysString();
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::registerBrowserActionToolbar(OLE_HANDLE aFrameTab, BSTR * aUrl, INT*aTabId)
{
  ENSURE_RETVAL(aUrl);
  ENSURE_RETVAL(aTabId);

  *aTabId = Ancho::Service::TabManager::instance().getFrameTabId((HWND)aFrameTab);

  /*WCHAR   appPath[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, appPath, _countof(appPath));*/

  CString url;
  url.Format(L"%s://AnchoBackgroundService/BROWSER_ACTION_TOOLBAR.HTML", s_AnchoInternalProtocolHandlerScheme);
  *aUrl = url.AllocSysString();
  ATLTRACE(L"ANCHO SERVICE: registered browser action toolbar; tab: %d\n", *aTabId);
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::unregisterBrowserActionToolbar(INT aTabId)
{
  ATLTRACE(L"ANCHO SERVICE: unregistering browser action toolbar; tab: %d\n", aTabId);
  m_BrowserActionCallbacks.erase(aTabId);
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddonService::getDispatchObject(IDispatch **aRet)
{
  ENSURE_RETVAL(aRet);
  *aRet = static_cast<IDispatch*>(this);
  AddRef();
  return S_OK;
}
//----------------------------------------------------------------------------
//

HRESULT CAnchoAddonService::FindActiveBrowser(IWebBrowser2** webBrowser)
{
  ATLASSERT(webBrowser != NULL);
  *webBrowser = NULL;
  // First find the IE frame windows.
  HWND hIEFrame = NULL;
  do {
    hIEFrame = ::FindWindowEx(NULL, hIEFrame, L"IEFrame", NULL);
    if (hIEFrame) {
      BOOL enable = ::IsWindowEnabled(hIEFrame);
      // Now we enumerate the child windows to find the "Internet Explorer_Server".
      ::EnumChildWindows(hIEFrame, EnumBrowserWindows, (LPARAM) webBrowser);
      if (*webBrowser) {
        return S_OK;
      }
    }
  }while(hIEFrame);

  // Oops, for some reason we didn't find it.
  return E_FAIL;
}

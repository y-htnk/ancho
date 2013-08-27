/****************************************************************************
 * AnchoRuntime.cpp : Implementation of CAnchoRuntime
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "stdafx.h"
#include <map>
#include "anchocommons.h"
#include "AnchoAddon.h"
#include "AnchoRuntime.h"
#include "AnchoBrowserEvents.h"
#include "AnchoPassthruAPP.h"
#include "dllmain.h"
#include <AnchoCommons/JSValueWrapper.hpp>
#include "ProtocolHandlerRegistrar.h"
#include <boost/scope_exit.hpp>

#include <string>
#include <ctime>

#include <Iepmapi.h>
#pragma comment(lib, "Iepmapi.lib")

using namespace Ancho::Utils;

//#define ARTTRACE
#define ARTTRACE(...)   ATLTRACE(__FUNCTION__);ATLTRACE(_T(": "));ATLTRACE(__VA_ARGS__)

/*============================================================================
 * class CAnchoRuntime
 */

//----------------------------------------------------------------------------
//  InitAddons
HRESULT CAnchoRuntime::InitAddons()
{
  // create all addons
  // open the registry key where all extensions are registered,
  // iterate subkeys and load each extension

  CRegKey regKey;
  LONG res = regKey.Open(HKEY_CURRENT_USER, s_AnchoExtensionsRegistryKey, KEY_READ);
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }
  DWORD iIndex = 0;
  CString sKeyName;
  DWORD dwLen = 4096;
  HRESULT hr = S_OK;

  while(ERROR_SUCCESS == regKey.EnumKey(iIndex++, sKeyName.GetBuffer(dwLen), &dwLen, NULL))
  {
    sKeyName.ReleaseBuffer();
    CAnchoAddonComObject * pNewObject = NULL;
    hr = CAnchoAddonComObject::CreateInstance(&pNewObject);
    if (SUCCEEDED(hr))
    {
      CComPtr<IAnchoAddon> addon(pNewObject);
      hr = addon->Init(sKeyName, mAnchoService, mWebBrowser);
      if (SUCCEEDED(hr))
      {
        mMapAddons[std::wstring(sKeyName)] = addon;
      }
    }
    dwLen = 4096;
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//  DestroyAddons
void CAnchoRuntime::DestroyAddons()
{
  AddonMap::iterator it = mMapAddons.begin();
  while(it != mMapAddons.end()) {
    it->second->Shutdown();
    ++it;
  }
  mMapAddons.clear();

  ARTTRACE(L"All addons destroyed for runtime %d\n", mTabId);
}

//----------------------------------------------------------------------------
//  Cleanup
HRESULT CAnchoRuntime::Cleanup()
{
  if (mToolbarWindow) {
    mToolbarWindow.DestroyWindow();
  }

  // release page actions first
  if (mAnchoService) {
    mAnchoService->releasePageActions(mTabId);
    mAnchoService->unregisterBrowserActionToolbar(mTabId);
  }

  // unadvise events
  if (mBrowserEventSource) {
    AtlUnadvise(mBrowserEventSource, IID_DAnchoBrowserEvents, mAnchoBrowserEventsCookie);
    mBrowserEventSource.Release();
    mAnchoBrowserEventsCookie = 0;
  }
  if (mWebBrowser) {
    AtlUnadvise(mWebBrowser, DIID_DWebBrowserEvents2, mWebBrowserEventsCookie);
    mWebBrowser.Release();
    mWebBrowserEventsCookie = 0;
  }

  // remove instance from tabmanager
  if(mTabManager) {
    mTabManager->unregisterRuntime(mTabId);
    mTabManager.Release();
  }

  // release service. must happen as the last step!
  if (mAnchoService) {
    mAnchoService.Release();
  }

  return S_OK;
}

//----------------------------------------------------------------------------
//  initCookieManager
HRESULT CAnchoRuntime::initCookieManager(IAnchoServiceApi * aServiceAPI)
{
  mCookieManager = Ancho::CookieManager::createInstance(aServiceAPI);
  return (mCookieManager) ? S_OK : E_FAIL;
}

//----------------------------------------------------------------------------
//  initTabManager
HRESULT CAnchoRuntime::initTabManager(IAnchoServiceApi * aServiceAPI, HWND aHwndFrameTab)
{
  CComQIPtr<IDispatch> dispatch;
  IF_FAILED_RET(aServiceAPI->get_tabManager(&dispatch));
  CComQIPtr<IAnchoTabManagerInternal> tabManager = dispatch;
  if (!tabManager) {
    return E_NOINTERFACE;
  }
  mTabManager = tabManager;

  // Registering tab in service - obtains tab id and assigns it to the tab as property
  return mTabManager->registerRuntime((OLE_HANDLE)aHwndFrameTab, this, mHeartbeatSlave.id());
}

//----------------------------------------------------------------------------
//  initWindowManager
HRESULT CAnchoRuntime::initWindowManager(IAnchoServiceApi * aServiceAPI, IAnchoWindowManagerInternal ** aWindowManager)
{
  // get WindowManager
  CComQIPtr<IDispatch> dispatch;
  IF_FAILED_RET(aServiceAPI->get_windowManager(&dispatch));
  return dispatch.QueryInterface(aWindowManager);
}

//----------------------------------------------------------------------------
//  Init
HRESULT CAnchoRuntime::Init()
{
  ATLASSERT(m_spUnkSite);

  // Expected window structure, currently IE10:
  //
  // + <page title>        IEFrame                 <-- IE main window  (hwndIEFrame)
  //   ...
  //   + Frame Tab                                 <-- IE current tab (hwndFrameTab)
  //     + "ITabBarHost"   InternetToolbarHost
  //       + "Menu Bar"    WorkerW
  //         + ""          ReBarWindow32           <-- parent of toolbar (hwndReBarWindow32)
  //         ...
  //         + ""          ATL:????                <-- our toolbar window
  //     + <page title>    TabWindowClass
  //       + ""            Shell DocObject View    <-- actual webbrowser control

  HWND hwndReBarWindow32 = NULL;
  HWND hwndFrameTab = NULL;
  HWND hwndIEFrame = NULL;

  //---------------------------------------------------------------------------
  // prepare our own webbrowser instance

  // get IServiceProvider to get IWebBrowser2 and IOleWindow
  CComQIPtr<IServiceProvider> pServiceProvider = m_spUnkSite;
  if (!pServiceProvider) {
    return E_FAIL;
  }

  // get IWebBrowser2
  pServiceProvider->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, (LPVOID*)&mWebBrowser.p);
  if (!mWebBrowser) {
    return E_FAIL;
  }

  //---------------------------------------------------------------------------
  // get required windows

  // get parent window for toolbar: ReBarWindow32
  CComQIPtr<IOleWindow> reBarWindow32(m_spUnkSite);
  if (!reBarWindow32) {
    return E_FAIL;
  }
  reBarWindow32->GetWindow(&hwndReBarWindow32);

  // get "Frame Tab" window
  hwndFrameTab = ::GetParent(getTabWindowClassWindow());
  if (!hwndFrameTab) {
    ATLASSERT(0 && "TOOLBAR: Failed to obtain 'Frame Tab' window handle.");
    return E_FAIL;
  }
  hwndIEFrame = ::GetParent(hwndFrameTab);

  //---------------------------------------------------------------------------
  // create addon service object
  ARTTRACE(L"Runtime initialization - CoCreateInstance(CLSID_AnchoAddonService)\n");
  IF_FAILED_RET(mAnchoService.CoCreateInstance(CLSID_AnchoAddonService));

  //---------------------------------------------------------------------------
  // toolbar window

  // register protocol handler for toolbar window
  CComBSTR serviceHost, servicePath;
  IF_FAILED_RET(mAnchoService->getInternalProtocolParameters(&serviceHost, &servicePath));
  IF_FAILED_RET(CProtocolHandlerRegistrar::
    RegisterTemporaryResourceHandler(s_AnchoInternalProtocolHandlerScheme, serviceHost, servicePath));

  // Register toolbar with ancho service.
  // NOTE: This is where we also receive our Tab-ID
  CComBSTR toolbarURL;
  mAnchoService->registerBrowserActionToolbar((INT)hwndFrameTab, &toolbarURL, &mTabId);
  mToolbarWindow.mTabId = mTabId;
  mAnchoService->getDispatchObject(&mToolbarWindow.mExternalDispatch);

  // create toolbar window
  mToolbarWindow.Create(hwndReBarWindow32, CWindow::rcDefault, NULL,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
  if (!mToolbarWindow.mWebBrowser) {
    return E_FAIL;
  }

  //---------------------------------------------------------------------------
  // webbrowser events, service API

  // subscribe to browser events
  AtlAdvise(mWebBrowser, (IUnknown *)(TWebBrowserEvents *) this, DIID_DWebBrowserEvents2, &mWebBrowserEventsCookie);

  // get IAnchoServiceApi interface
  CComQIPtr<IAnchoServiceApi> serviceApi = mAnchoService;
  if (!serviceApi) {
    return E_NOINTERFACE;
  }

  //---------------------------------------------------------------------------
  // init some required objects: TabManager, WindowManager, CookieManager

  IF_FAILED_RET(initCookieManager(serviceApi));
  IF_FAILED_RET(initTabManager(serviceApi, hwndFrameTab));

  CComPtr<IAnchoWindowManagerInternal> windowManager;
  IF_FAILED_RET(initWindowManager(serviceApi, &windowManager.p));
  // get our WindowId
  IF_FAILED_RET(windowManager->getWindowIdFromHWND(reinterpret_cast<OLE_HANDLE>(hwndIEFrame), &mWindowId));

  // set our tabID for the passthru app
  ::SetProp(hwndIEFrame, s_AnchoTabIDPropertyName, (HANDLE)mTabId);

  // subscribe to URL loading events
  CComObject<CAnchoBrowserEvents>* pBrowserEventSource;
  IF_FAILED_RET(CComObject<CAnchoBrowserEvents>::CreateInstance(&pBrowserEventSource));

  mBrowserEventSource = pBrowserEventSource;

  AtlAdvise(mBrowserEventSource, (IUnknown*)(TAnchoBrowserEvents*) this, IID_DAnchoBrowserEvents,
    &mAnchoBrowserEventsCookie);

  // Set the sink as property of the browser so it can be retrieved if someone wants to send
  // us events.
  IF_FAILED_RET(mWebBrowser->PutProperty(L"_anchoBrowserEvents", CComVariant(mBrowserEventSource)));
  ARTTRACE(L"Runtime %d initialized\n", mTabId);

  // initialize page actions for this process/window/tab
  mAnchoService->initPageActions((OLE_HANDLE)hwndIEFrame, mTabId);

  // load toolbar's html page
  IF_FAILED_RET(mToolbarWindow.mWebBrowser->Navigate(toolbarURL, NULL, NULL, NULL, NULL));

  return S_OK;
}
//----------------------------------------------------------------------------
//
HRESULT CAnchoRuntime::get_cookieManager(LPDISPATCH* ppRet)
{
  ENSURE_RETVAL(ppRet);
  return mCookieManager->QueryInterface(ppRet);
}
//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoRuntime::OnBrowserDownloadBegin()
{
  ARTTRACE(_T(""));
  mExtensionPageAPIPrepared = false;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP_(void) CAnchoRuntime::OnWindowStateChanged(LONG dwFlags, LONG dwValidFlagsMask)
{
  if (mAnchoService
      && (dwFlags & dwValidFlagsMask & OLECMDIDF_WINDOWSTATE_USERVISIBLE)
      && (dwFlags & dwValidFlagsMask & OLECMDIDF_WINDOWSTATE_ENABLED)) {
    mAnchoService->onTabActivate(mTabId);
  }
}

//----------------------------------------------------------------------------
//  OnNavigateComplete
STDMETHODIMP_(void) CAnchoRuntime::OnNavigateComplete(LPDISPATCH pDispatch, VARIANT *URL)
{
  CComBSTR url(URL->bstrVal);
  ARTTRACE(_T("To %s browser 0x%08x\n"), url, (ULONG_PTR)pDispatch);

  mIsExtensionPage = isExtensionPage(std::wstring(url));
  if (mIsExtensionPage) {
    // Too early for api injections
    if (S_OK == InitializeExtensionScripting(url)) {
      mExtensionPageAPIPrepared = true;
    }
  }
}

//----------------------------------------------------------------------------
//  OnBrowserBeforeNavigate2
STDMETHODIMP_(void) CAnchoRuntime::OnBrowserBeforeNavigate2(LPDISPATCH pDisp, VARIANT *pURL, VARIANT *Flags,
  VARIANT *TargetFrameName, VARIANT *PostData, VARIANT *Headers, BOOL *Cancel)
{
  static BOOL bFirstRun = TRUE;
  static BOOL bCancel = FALSE;

  // prepare URL, current browser
  ATLASSERT(pURL->vt == VT_BSTR && pURL->bstrVal != NULL);
  CComBSTR currentURL(pURL->bstrVal);
  BOOL isTopLevelBrowser = mWebBrowser.IsEqualObject(pDisp);
  CComQIPtr<IWebBrowser2> currentFrameBrowser(pDisp);
  ATLASSERT(currentFrameBrowser != NULL);

  ARTTRACE(_T("To %s browser 0x%08x\n"), currentURL, (ULONG_PTR)pDisp);

  // OnBrowserBeforeNavigate2 is called multiple times recursive because of
  // the call to pWebBrowser->Stop() (goes to res://ieframe.dll/navcancl.htm)
  // and the following Navigate2.
  if (bCancel) {
    // Cancel flag is set, the previous navigation was canceled,
    // so this is - hopefully - a call to res://ieframe.dll/navcancl.htm
    // Don't process.
    ARTTRACE(_T("\t\t ***canceled\n"));
    // But reset flag.
    bCancel = FALSE;
    return;
  }

  // Also we ignore certain pages, like about:*
  CComPtr<IUri> currentURI;
  IF_FAILED_RET2(::CreateUri(currentURL, Uri_CREATE_CANONICALIZE, 0, &currentURI), /*void*/);
  CComBSTR scheme;
  currentURI->GetSchemeName(&scheme);
  if (scheme == L"about") {
    ARTTRACE(_T("\t\t ABOUT URL, don't handle\n"));
    return; // don't handle
  }

  // Workaround to ensure that first request goes through PAPP
  if (bFirstRun) {
    ARTTRACE(_T("\t\tFIRST RUN, cancel\n"));
    bFirstRun = FALSE;
    *Cancel = bCancel = TRUE;
    currentFrameBrowser->Stop();
    ARTTRACE(_T("\t\tFIRST RUN, go\n"));
    bCancel = FALSE;
    currentFrameBrowser->Navigate2(pURL, Flags, TargetFrameName, PostData, Headers);
    return;
  }

  ARTTRACE(_T("\t\tPROCESS URL %s browser 0x%08x\n"), currentURL, currentFrameBrowser);

  // Check if this is a new tab we are creating programmatically.
  // If so redirect it to the correct URL.
  std::wstring url(pURL->bstrVal, SysStringLen(pURL->bstrVal));

  boost::wregex expression(L"(.*)://\\$\\$([0-9]+)\\$\\$(.*)");
  boost::wsmatch what;
  //TODO - find a better way
  if (boost::regex_match(url, what, expression)) {
    int requestId = boost::lexical_cast<int>(what[2].str());
    url = boost::str(boost::wformat(L"%1%://%2%") % what[1] % what[3]);

    _variant_t vtUrl(url.c_str());
    *Cancel = bCancel = TRUE;
    currentFrameBrowser->Stop();
    bCancel = FALSE;
    currentFrameBrowser->Navigate2(&vtUrl.GetVARIANT(), Flags, TargetFrameName, PostData, Headers);
    mTabManager->createTabNotification(mTabId, requestId);
    return;
  }

  if (isTopLevelBrowser) {
    // Loading the main frame so reset the frame list.
    mMapFrames.clear();
    mNextFrameId = 0;
  }
  // Add the new frame to our map.
  int frameID = mNextFrameId++;
  mMapFrames[COMOBJECTID(pDisp)].set(pDisp, isTopLevelBrowser, frameID);

  // For checking if a OnFrameStart or OnFrameEnd relates to the actual HTML document or
  // a resource we have to remember the URL this frame is navigating to.
  currentFrameBrowser->PutProperty(CComBSTR(L"anchoCurrentURL"), CComVariant(currentURL));

  // Add the current browser as a property to the top level browser. By this we pass
  // the current browser to the PAPP.
  // NOTE: If pDisp is same object as mWebBrowser we set the property to NULL. So we avoid
  // circular references on mWebBrowser and signalize that the request is for the top level frame.
  mWebBrowser->PutProperty( CComBSTR(L"anchoCurrentBrowserForFrame"),
    CComVariant(isTopLevelBrowser
                  ? NULL
                  : pDisp) );

  if (isTopLevelBrowser) {
    mAnchoService->onTabNavigate(mTabId);
  }
}

//----------------------------------------------------------------------------
//  OnFrameStart
STDMETHODIMP CAnchoRuntime::OnFrameStart(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelRefresh)
{
  // Only handle document URLs.
  ATLASSERT(aBrowser);
  if (!isBrowserDocumentURL(aBrowser, aUrl)) {
    return S_OK;
  }

  ARTTRACE(_T("%s browser 0x%08x %s\n"), aUrl, aBrowser, (aIsTopLevelRefresh) ? _T(": REFRESH") : _T(""));
  //For extension pages we don't execute content scripts.
  if (isExtensionPage(std::wstring(aUrl))) {
    return S_OK;
  }

  // Forward event to addons.
  AddonMap::iterator it = mMapAddons.begin();
  while(it != mMapAddons.end()) {
    VARIANT_BOOL isTopLevelFrame = (mWebBrowser.IsEqualObject(aBrowser)) ? VARIANT_TRUE : VARIANT_FALSE;
    it->second->OnFrameStart(aBrowser, aUrl, isTopLevelFrame, aIsTopLevelRefresh);
    ++it;
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//  OnFrameEnd
STDMETHODIMP CAnchoRuntime::OnFrameEnd(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelRefresh)
{
  // Only handle document URLs.
  ATLASSERT(aBrowser);
  if (!isBrowserDocumentURL(aBrowser, aUrl)) {
    return S_OK;
  }

  ARTTRACE(_T("%s browser 0x%08x %s\n"), aUrl, aBrowser, (aIsTopLevelRefresh) ? _T(": REFRESH") : _T(""));
  //For extension pages we don't execute content scripts.
  if (isExtensionPage(std::wstring(aUrl))) {
    return S_OK;
  }

  fireTabsOnUpdate();

  // Forward event to addons.
  AddonMap::iterator it = mMapAddons.begin();
  while(it != mMapAddons.end()) {
    VARIANT_BOOL isTopLevelFrame = (mWebBrowser.IsEqualObject(aBrowser)) ? VARIANT_TRUE : VARIANT_FALSE;
    it->second->OnFrameEnd(aBrowser, aUrl, isTopLevelFrame, aIsTopLevelRefresh);
    ++it;
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//  OnFrameRedirect
STDMETHODIMP CAnchoRuntime::OnFrameRedirect(IWebBrowser2 * aBrowser, BSTR bstrOldUrl, BSTR bstrNewUrl)
{
  // Update current URL so that OnFrameEnd propertly triggers the content scripts.
  if (aBrowser) {
    aBrowser->PutProperty(CComBSTR(L"anchoCurrentURL"), CComVariant(bstrNewUrl));
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::OnBeforeRequest(VARIANT aReporter)
{
  ATLASSERT(aReporter.vt == VT_UNKNOWN);
  CComBSTR str;
  CComQIPtr<IWebRequestReporter> reporter(aReporter.punkVal);
  if (!reporter) {
    return E_INVALIDARG;
  }
  BeforeRequestInfo outInfo;
  CComBSTR url;
  CComBSTR method;
  reporter->getUrl(&url);
  reporter->getHTTPMethod(&method);

  CComPtr<IWebBrowser2> currentFrameBrowser;
  reporter->getBrowser(&currentFrameBrowser.p);
  ARTTRACE(_T("%s browser 0x%08x\n"), url.m_str, currentFrameBrowser.p);

  FrameMap::const_iterator it = mMapFrames.find(COMOBJECTID(currentFrameBrowser.p));
  const FrameRecord *frameRecord = NULL;
  if (it != mMapFrames.end()) {
    frameRecord = &(it->second);
  } else {
    ARTTRACE(L"\t\tNo frame record for %s\n", url.m_str);
  }

  fireOnBeforeRequest(url.m_str, method.m_str, frameRecord, outInfo);
  if (outInfo.cancel) {
    reporter->cancelRequest();
  }
  if (outInfo.redirect) {
    reporter->redirectRequest(CComBSTR(outInfo.newUrl.c_str()));
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::OnBeforeSendHeaders(VARIANT aReporter)
{
  ATLASSERT(aReporter.vt == VT_UNKNOWN);
  CComBSTR str;
  CComQIPtr<IWebRequestReporter> reporter(aReporter.punkVal);
  if (!reporter) {
    return E_INVALIDARG;
  }
  BeforeSendHeadersInfo outInfo;
  CComBSTR url;
  CComBSTR method;
  reporter->getUrl(&url);
  reporter->getHTTPMethod(&method);

  CComPtr<IWebBrowser2> currentFrameBrowser;
  reporter->getBrowser(&currentFrameBrowser.p);
  ARTTRACE(_T("%s browser 0x%08x\n"), url.m_str, currentFrameBrowser.p);

  FrameMap::const_iterator it = mMapFrames.find(COMOBJECTID(currentFrameBrowser.p));
  const FrameRecord *frameRecord = NULL;
  if (it != mMapFrames.end()) {
    frameRecord = &(it->second);
  } else {
    ARTTRACE(L"\t\tNo frame record for %s\n", url.m_str);
  }

  fireOnBeforeSendHeaders(url.m_str, method.m_str, frameRecord, outInfo);
  if (outInfo.modifyHeaders) {
    reporter->setNewHeaders(CComBSTR(outInfo.headers.c_str()).Detach());
  }
  return S_OK;
}

void
CAnchoRuntime::fillRequestInfo(SimpleJSObject &aInfo, const std::wstring &aUrl, const std::wstring &aMethod, const CAnchoRuntime::FrameRecord *aFrameRecord)
{
  //TODO - get proper request ID
  aInfo.setProperty(L"requestId", CComVariant(L"TODO_RequestId"));
  aInfo.setProperty(L"url", CComVariant(aUrl.c_str()));
  aInfo.setProperty(L"method", CComVariant(aMethod.c_str()));
  aInfo.setProperty(L"tabId", CComVariant(mTabId));
  //TODO - find out parent frame id
  aInfo.setProperty(L"parentFrameId", CComVariant(-1));
  if (aFrameRecord) {
    aInfo.setProperty(L"frameId", CComVariant(aFrameRecord->frameId));
    aInfo.setProperty(L"type", CComVariant(aFrameRecord->isTopLevel ? L"main_frame" : L"sub_frame"));
  } else {
    aInfo.setProperty(L"frameId", CComVariant(-1));
    aInfo.setProperty(L"type", CComVariant(L"other"));
  }
  time_t timeSinceEpoch = time(NULL);
  aInfo.setProperty(L"timeStamp", CComVariant(double(timeSinceEpoch)*1000));
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoRuntime::fireOnBeforeRequest(const std::wstring &aUrl, const std::wstring &aMethod, const CAnchoRuntime::FrameRecord *aFrameRecord, /*out*/ BeforeRequestInfo &aOutInfo)
{
  CComPtr<ComSimpleJSObject> info;
  IF_FAILED_RET(SimpleJSObject::createInstance(info));
  fillRequestInfo(*info, aUrl, aMethod, aFrameRecord);

  CComPtr<ComSimpleJSArray> argArray;
  IF_FAILED_RET(SimpleJSArray::createInstance(argArray));
  argArray->push_back(CComVariant(info.p));

  CComVariant result;
  mAnchoService->invokeEventObjectInAllExtensions(CComBSTR(L"webRequest.onBeforeRequest"), argArray.p, &result);
  if (result.vt & VT_ARRAY) {
    CComSafeArray<VARIANT> arr;
    arr.Attach(result.parray);
    //contained data already managed by CComSafeArray
    VARIANT tmp = {0}; HRESULT hr = result.Detach(&tmp);
    BEGIN_TRY_BLOCK
      aOutInfo.cancel = false;
      for (ULONG i = 0; i < arr.GetCount(); ++i) {
        Ancho::Utils::JSObjectWrapperConst item = Ancho::Utils::JSValueWrapperConst(arr.GetAt(i)).toObject();

        Ancho::Utils::JSValueWrapperConst cancel = item[L"cancel"];
        if (!cancel.isNull()) {
          aOutInfo.cancel = aOutInfo.cancel || cancel.toBool();
        }

        Ancho::Utils::JSValueWrapperConst redirectUrl = item[L"redirectUrl"];
        if (!redirectUrl.isNull()) {
          aOutInfo.redirect = true;
          aOutInfo.newUrl = redirectUrl.toString();
        }
      }
    END_TRY_BLOCK_CATCH_TO_HRESULT

  }
  return S_OK;
}

//----------------------------------------------------------------------------
void CAnchoRuntime::fireTabsOnUpdate()
{
  try {
    if (!mCurrentTabInfo) {
      //We just created the tab - no updates
      IF_FAILED_THROW(SimpleJSObject::createInstance(mCurrentTabInfo));
      CComVariant vtTabInfo(mCurrentTabInfo.p);
      IF_FAILED_THROW(fillTabInfo(&vtTabInfo));
      return;
    }
    CComPtr<ComSimpleJSArray> argArray;
    IF_FAILED_THROW(SimpleJSArray::createInstance(argArray));
    argArray->push_back(CComVariant(mTabId));

    CComPtr<ComSimpleJSObject> changeInfo;
    IF_FAILED_THROW(SimpleJSObject::createInstance(changeInfo));
    argArray->push_back(CComVariant(changeInfo.p));


    CComPtr<ComSimpleJSObject> tabInfo;
    IF_FAILED_THROW(SimpleJSObject::createInstance(tabInfo));
    CComVariant vtTabInfo(tabInfo.p);
    IF_FAILED_THROW(fillTabInfo(&vtTabInfo));
    argArray->push_back(vtTabInfo);

    //TODO - list all changed attributes when supported
    CComVariant url;
    tabInfo->getProperty(L"url", url);
    changeInfo->setProperty(L"url", url);

    mCurrentTabInfo = tabInfo;
    CComVariant result;
    IF_FAILED_THROW(mAnchoService->invokeEventObjectInAllExtensions(CComBSTR(L"tabs.onUpdated"), argArray.p, &result));
  } catch (std::exception &e) {
    (e);
    ATLTRACE("FIRING tabs.onUpdatedEventFailed\n");
  }
}

//----------------------------------------------------------------------------
//
HRESULT CAnchoRuntime::fireOnBeforeSendHeaders(const std::wstring &aUrl, const std::wstring &aMethod, const CAnchoRuntime::FrameRecord *aFrameRecord, /*out*/ BeforeSendHeadersInfo &aOutInfo)
{
  aOutInfo.modifyHeaders = false;
  CComPtr<ComSimpleJSObject> info;
  IF_FAILED_RET(SimpleJSObject::createInstance(info));

  fillRequestInfo(*info, aUrl, aMethod, aFrameRecord);
  CComPtr<ComSimpleJSArray> requestHeaders;
  IF_FAILED_RET(SimpleJSArray::createInstance(requestHeaders));
  info->setProperty(L"requestHeaders", CComVariant(requestHeaders.p));

  CComPtr<ComSimpleJSArray> argArray;
  IF_FAILED_RET(SimpleJSArray::createInstance(argArray));
  argArray->push_back(CComVariant(info.p));

  CComVariant result;
  mAnchoService->invokeEventObjectInAllExtensions(CComBSTR(L"webRequest.onBeforeSendHeaders"), argArray.p, &result);
  if (result.vt & VT_ARRAY) {
    CComSafeArray<VARIANT> arr;
    arr.Attach(result.parray);
    //contained data already managed by CComSafeArray
    VARIANT tmp = {0}; HRESULT hr = result.Detach(&tmp);
    BEGIN_TRY_BLOCK
      for (ULONG i = 0; i < arr.GetCount(); ++i) {
        Ancho::Utils::JSObjectWrapperConst item = Ancho::Utils::JSValueWrapperConst(arr.GetAt(i)).toObject();
        Ancho::Utils::JSValueWrapperConst requestHeaders = item[L"requestHeaders"];
        if (!requestHeaders.isNull()) {
          Ancho::Utils::JSArrayWrapperConst headersArray = requestHeaders.toArray();
          std::wostringstream oss;
          size_t headerCount = headersArray.size();
          for (size_t i = 0; i < headerCount; ++i) {
            Ancho::Utils::JSValueWrapperConst headerRecord = headersArray[i];
            //TODO handle headerRecord[L"binaryValue"]
            if (headerRecord.isNull()) {
              continue;
            }
            std::wstring headerText = headerRecord.toObject()[L"name"].toString() + std::wstring(L": ") + headerRecord.toObject()[L"value"].toString();
            oss << headerText << L"\r\n";
          }
          aOutInfo.modifyHeaders = true;
          aOutInfo.headers = oss.str();
        }
      }
    END_TRY_BLOCK_CATCH_TO_HRESULT

  }

  return S_OK;
}

HRESULT CAnchoRuntime::InitializeExtensionScripting(BSTR aUrl)
{
  std::wstring domain = getDomainName(aUrl);
  AddonMap::iterator it = mMapAddons.find(domain);
  if (it != mMapAddons.end()) {
    return it->second->InitializeExtensionScripting(aUrl);
  }
  return S_FALSE;
}

//----------------------------------------------------------------------------
// GetBandInfo
STDMETHODIMP CAnchoRuntime::GetBandInfo(DWORD dwBandID, DWORD dwViewMode, DESKBANDINFO* pdbi)
{
  if (pdbi) {
    m_dwBandID = dwBandID;
    m_dwViewMode = dwViewMode;

    if (pdbi->dwMask & DBIM_MINSIZE) {
      pdbi->ptMinSize.x = 200;
      pdbi->ptMinSize.y = 28;
    }

    if (pdbi->dwMask & DBIM_MAXSIZE) {
      pdbi->ptMaxSize.x = -1;
      pdbi->ptMaxSize.y = 28;
    }

    if (pdbi->dwMask & DBIM_INTEGRAL) {
      pdbi->ptIntegral.x = 0;
      pdbi->ptIntegral.y = 0;
    }

    if (pdbi->dwMask & DBIM_ACTUAL) {
      pdbi->ptActual.x = 600;
      pdbi->ptActual.y = 28;
    }

    if (pdbi->dwMask & DBIM_TITLE) {
      pdbi->dwMask &= ~DBIM_TITLE;
    }

    if (pdbi->dwMask & DBIM_MODEFLAGS) {
      pdbi->dwModeFlags = DBIMF_VARIABLEHEIGHT;
    }

    if (pdbi->dwMask & DBIM_BKCOLOR) {
      pdbi->dwMask &= ~DBIM_BKCOLOR;
    }
    return S_OK;
  }
  return E_INVALIDARG;
}

//----------------------------------------------------------------------------
// GetWindow
STDMETHODIMP CAnchoRuntime::GetWindow(HWND* phwnd)
{
  if (!phwnd) {
    return E_POINTER;
  }
  (*phwnd) = mToolbarWindow;
  return S_OK;
}

//----------------------------------------------------------------------------
// ContextSensitiveHelp
STDMETHODIMP CAnchoRuntime::ContextSensitiveHelp(BOOL fEnterMode)
{
  return S_OK;
}

//----------------------------------------------------------------------------
// CloseDW
STDMETHODIMP CAnchoRuntime::CloseDW(unsigned long dwReserved)
{
  mToolbarWindow.DestroyWindow();
  return S_OK;
}

//----------------------------------------------------------------------------
// ResizeBorderDW
STDMETHODIMP CAnchoRuntime::ResizeBorderDW(const RECT* prcBorder, IUnknown* punkToolbarSite, BOOL fReserved)
{
  return E_NOTIMPL;
}

//----------------------------------------------------------------------------
// ShowDW
STDMETHODIMP CAnchoRuntime::ShowDW(BOOL fShow)
{
  mToolbarWindow.ShowWindow(fShow ? SW_SHOW : SW_HIDE);
  return S_OK;
}

//----------------------------------------------------------------------------
//  SetSite
STDMETHODIMP CAnchoRuntime::SetSite(IUnknown *pUnkSite)
{
  HRESULT hr = IObjectWithSiteImpl<CAnchoRuntime>::SetSite(pUnkSite);
  IF_FAILED_RET(hr);
  if (pUnkSite)
  {
    hr = Init();
    if (SUCCEEDED(hr)) {
      hr = InitAddons();
/*
      if (SUCCEEDED(hr)) {
        // in case IE has already a page loaded initialize scripting 
        READYSTATE readyState;
        mWebBrowser->get_ReadyState(&readyState);
        if (readyState >= READYSTATE_INTERACTIVE) {
          CComBSTR url;
          mWebBrowser->get_LocationURL(&url);
          if (!isExtensionPage(std::wstring(url))) {
            if (url != L"about:blank") {
              // give toolbar a chance to load
              //Sleep(200);
              //InitializeContentScripting(mWebBrowser, url, TRUE, documentLoadEnd);
            }
          }
        }
      }
      //showBrowserActionBar(TRUE);
*/
    }
  }
  else
  {
    DestroyAddons();
    Cleanup();
  }
  return hr;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::reloadTab()
{
  CComVariant var(REFRESH_COMPLETELY);
  mWebBrowser->Refresh2(&var);
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::closeTab()
{
  return mWebBrowser->Quit();
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::executeScript(BSTR aExtensionId, BSTR aCode, INT aFileSpecified)
{
  //TODO: check permissions from manifest
  ATLASSERT(0 && "not implemented");
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::showBrowserActionBar(INT aShow)
{
  wchar_t clsid[1024] = {0};
  IF_FAILED_RET(::StringFromGUID2( CLSID_AnchoRuntime, (OLECHAR*)clsid, sizeof(clsid)));
  CComVariant clsidVar(clsid);
  CComVariant show(aShow != FALSE);
  IF_FAILED_RET(mWebBrowser->ShowBrowserBar(&clsidVar, &show, NULL));
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::updateTab(LPDISPATCH aProperties)
{
  CIDispatchHelper properties(aProperties);
  CComBSTR url;
  HRESULT hr = properties.Get<CComBSTR, VT_BSTR, BSTR>(L"url", url);
  if (hr == S_OK) {
    CComVariant vtUrl(url);
    CComVariant vtEmpty;
    mWebBrowser->Navigate2(&vtUrl, &vtEmpty, &vtEmpty, &vtEmpty, &vtEmpty);
  }
  INT active = 0;
  hr = properties.Get<INT, VT_BOOL, INT>(L"active", active);
  if (hr == S_OK) {
    HWND hwnd = getTabWindowClassWindow();
    IAccessible *acc = NULL;
    //TODO - fix tab activation
    if (S_OK == AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible, (void**)&acc)) {
      CComVariant var(CHILDID_SELF, VT_I4);
      acc->accDoDefaultAction(var);
    }
  }
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoRuntime::fillTabInfo(VARIANT* aInfo)
{
  ENSURE_RETVAL(aInfo);
  if(aInfo->vt != VT_DISPATCH) {
    return E_NOINTERFACE;
  }
  CIDispatchHelper obj(aInfo->pdispVal);

  CComBSTR locationUrl;
  CComBSTR name;
  mWebBrowser->get_LocationURL(&locationUrl);
  obj.SetProperty(L"url", CComVariant(locationUrl));

  mWebBrowser->get_Name(&name);
  IF_FAILED_RET(obj.SetProperty(L"title", CComVariant(name)));

  IF_FAILED_RET(obj.SetProperty(L"id", CComVariant(mTabId)));

  IF_FAILED_RET(obj.SetProperty(L"active", CComVariant(isTabActive())));

  IF_FAILED_RET(obj.SetProperty(L"windowId", mWindowId));
  return S_OK;
}

//----------------------------------------------------------------------------
//
HWND CAnchoRuntime::getTabWindowClassWindow()
{
  ATLASSERT(mWebBrowser);
  HWND hwndBrowser = NULL;
  IServiceProvider* pServiceProvider = NULL;
  if (SUCCEEDED(mWebBrowser->QueryInterface(IID_IServiceProvider, (void**)&pServiceProvider))){
    IOleWindow* pWindow = NULL;
    if (SUCCEEDED(pServiceProvider->QueryService(SID_SShellBrowser, IID_IOleWindow,(void**)&pWindow))) {
      // hwndBrowser is the handle of TabWindowClass
      if (!SUCCEEDED(pWindow->GetWindow(&hwndBrowser))) {
        hwndBrowser = NULL;
      }
      pWindow->Release();
    }
    pServiceProvider->Release();
  }
  return hwndBrowser;
}

//----------------------------------------------------------------------------
//
bool CAnchoRuntime::isTabActive()
{
  HWND hwndBrowser = getTabWindowClassWindow();
  return hwndBrowser && ::IsWindowVisible(hwndBrowser);
}

//----------------------------------------------------------------------------
//
BOOL CAnchoRuntime::isBrowserDocumentURL(IWebBrowser2 * aBrowser, BSTR aURL)
{
  ATLASSERT(aBrowser);
  CComVariant currentURL;
  aBrowser->GetProperty(CComBSTR(L"anchoCurrentURL"), &currentURL);
  if (VT_BSTR == currentURL.vt && currentURL.bstrVal) {
    return 0 == _wcsicmp(aURL, currentURL.bstrVal);
  }
  return TRUE;
}

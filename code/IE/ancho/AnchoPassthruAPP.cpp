/****************************************************************************
 * AnchoPassthruAPP.cpp : Implementation of CAnchoPassthruAPP
 * Copyright 2012 Salsita (http://www.salsitasoft.com).
 * Author: Matthew Gertner <matthew@salsitasoft.com>
 * Author: Arne Seib <arne@salsitasoft.com>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoPassthruAPP.h"

#include "libbhohelper.h"

#include <atlbase.h>
#include <WinInet.h>
#include <htiframe.h>

//#define PAPPTRACE
#define PAPPTRACE(...)   ATLTRACE(__FUNCTION__);ATLTRACE(_T(": "));ATLTRACE(__VA_ARGS__)

//----------------------------------------------------------------------------
// Ancho related protocol states
enum {
  ANCHO_SWITCH_BASE = 50000,
  ANCHO_SWITCH_REPORT_DATA,
  ANCHO_SWITCH_REPORT_RESULT,
  ANCHO_SWITCH_REDIRECT,

  ANCHO_SWITCH_MAX
};

//----------------------------------------------------------------------------
// Translate a DWORD bindVerb value to a string
static CComBSTR
getMethodNameFromBindVerb(DWORD bindVerb)
{
  switch (bindVerb) {
    case BINDVERB_GET:
      return L"GET";
    case BINDVERB_POST:
      return L"POST";
    case BINDVERB_PUT:
      return L"PUT";
  };
  return L"";
}

//----------------------------------------------------------------------------
// Compare two URLs without the fragment part.
static BOOL isEqualURLNoFragment(LPCWSTR aUrl1, LPCWSTR aUrl2)
{
  CComPtr<IUri> uris[2];
  LPCWSTR args[2] = {aUrl1, aUrl2};
  for (int n = 0; n < 2; n++) {
    IF_FAILED_RET2(
        CreateUri(args[n], Uri_CREATE_CANONICALIZE, 0, &uris[n].p), FALSE);
    CComPtr<IUriBuilder> uriBuilder;
    IF_FAILED_RET2(
        CreateIUriBuilder(uris[n], 0, 0, &uriBuilder.p), FALSE);
    uriBuilder->SetFragment(NULL);
    uris[n].Release();
    IF_FAILED_RET2(
        uriBuilder->CreateUri(Uri_CREATE_CANONICALIZE, 0, 0, &uris[n].p), FALSE);
  }
  BOOL b;
  IF_FAILED_RET2(uris[0]->IsEqual(uris[1], &b), FALSE);
  return b;
}

/*============================================================================
 * class CAnchoStartPolicy
 */

//----------------------------------------------------------------------------
//  OnStart
HRESULT CAnchoStartPolicy::OnStart(
  LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
  IInternetBindInfo *pOIBindInfo,  DWORD grfPI, HANDLE_PTR dwReserved,
  IInternetProtocol* pTargetProtocol)
{
  GetSink()->m_Url = szUrl;
  return __super::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
}

//----------------------------------------------------------------------------
//  OnStartEx
HRESULT CAnchoStartPolicy::OnStartEx(
  IUri* pUri, IInternetProtocolSink *pOIProtSink,
  IInternetBindInfo *pOIBindInfo,  DWORD grfPI, HANDLE_PTR dwReserved,
  IInternetProtocolEx* pTargetProtocol)
{
  ATLASSERT(pUri != NULL);
  CComBSTR rawUri;
  HRESULT hr = pUri->GetRawUri(&rawUri);
  GetSink()->m_Url = std::wstring(rawUri, ::SysStringLen(rawUri));
  return __super::OnStartEx(pUri, pOIProtSink, pOIBindInfo, grfPI, dwReserved, pTargetProtocol);
}

/*============================================================================
 * class CAnchoProtocolSink
 */

CComPtr<IWebBrowser2>  CAnchoProtocolSink::sCurrentTopLevelBrowser;

//----------------------------------------------------------------------------
// SwitchParams::create
//  Creates SwitchParams and sets the params.
void CAnchoProtocolSink::SwitchParams::create(
    PROTOCOLDATA & aProtocolData,
    LPCWSTR aParam1,
    LPCWSTR aParam2)
{
  aProtocolData.cbData = sizeof(SwitchParams);
  aProtocolData.pData = new SwitchParams(aParam1, aParam2);
}

//----------------------------------------------------------------------------
// SwitchParams::extractAndDestroy
//  Extract SwitchParams and frees the memory allocated for it.
void CAnchoProtocolSink::SwitchParams::extractAndDestroy(
    PROTOCOLDATA & aProtocolData,
    CComBSTR & aParam1,
    CComBSTR & aParam2)
{
  ATLASSERT(aProtocolData.cbData == sizeof(SwitchParams));
  SwitchParams * switchParams = (SwitchParams*)aProtocolData.pData;
  ATLASSERT(switchParams);
  aParam1 = switchParams->param1;
  aParam2 = switchParams->param2;
  delete switchParams;
  aProtocolData.cbData = 0;
}

//----------------------------------------------------------------------------
//  BeginningTransaction
STDMETHODIMP CAnchoProtocolSink::BeginningTransaction(
  /* [in] */  LPCWSTR   szURL,
  /* [in] */  LPCWSTR   szHeaders,
  /* [in] */  DWORD     dwReserved,
  /* [out] */ LPWSTR  * pszAdditionalHeaders)
{
  if (pszAdditionalHeaders) {
    (*pszAdditionalHeaders) = 0;  // By default we will not add any headers.
  }

  // Get browsers for this request.
  initializeBrowsers();

  m_IsFrame = (mTopLevelBrowser && !mTopLevelBrowser.IsEqualObject(mCurrentFrameBrowser));
  mIsTopLevelRefresh = FALSE;

  // If the current URL is the same as the top level browser's current location
  // (without fragment) this should be a global refresh (on the top level browser).
  if (mTopLevelBrowser) {
    CComBSTR currentTopLevelUrl;
    mTopLevelBrowser->get_LocationURL(&currentTopLevelUrl);
    mIsTopLevelRefresh = isEqualURLNoFragment(currentTopLevelUrl, szURL);
  }

  PAPPTRACE(_T("%s browser 0x%08x, refresh: %i\n"), szURL, mCurrentFrameBrowser.p, mIsTopLevelRefresh);

  // Init frame info in CAnchoPassthruAPP for new request. The protocol will query the
  // browsers and other things from us, so we have to have that data at
  // this point - from initializeBrowsers()
  CAnchoPassthruAPP *protocol = CAnchoPassthruAPP::GetProtocol(this);
  protocol->initFromSink(this);

  // get additional headers for request
  CComBSTR method = getMethodNameFromBindVerb(m_bindVerb);
  CComPtr<IWebRequestReporter> reporter;
  IF_FAILED_RET(WebRequestReporter::createReporter(szURL, method, mCurrentFrameBrowser, &reporter.p));
  protocol->fireOnBeforeHeaders(reporter);

  CComPtr<IHttpNegotiate> spHttpNegotiate;
  QueryServiceFromClient(&spHttpNegotiate);

  IF_FAILED_RET((spHttpNegotiate)
      ? spHttpNegotiate->BeginningTransaction(szURL, szHeaders, dwReserved, pszAdditionalHeaders)
      : S_OK);

  //Adding headers
  if ( pszAdditionalHeaders && ((WebRequestReporter*)reporter.p)->mNewHeadersAdded) {
    CComBSTR headersFromReporter;
    reporter->getNewHeaders(&headersFromReporter);

    std::wstring tmp = std::wstring(*pszAdditionalHeaders) + (LPCWSTR)headersFromReporter;
    if (*pszAdditionalHeaders) {
      CoTaskMemFree(*pszAdditionalHeaders);
    }

    LPWSTR wszAdditionalHeaders = (LPWSTR) CoTaskMemAlloc((tmp.size()+1)*sizeof(WCHAR));
    if (!wszAdditionalHeaders) {
      return E_OUTOFMEMORY;
    }
    wcscpy_s(wszAdditionalHeaders, tmp.size()+1, tmp.c_str());
    (*pszAdditionalHeaders) = wszAdditionalHeaders;
  }

  return S_OK;
}

//----------------------------------------------------------------------------
//  OnResponse
STDMETHODIMP CAnchoProtocolSink::OnResponse(
  /* [in] */ DWORD dwResponseCode,
  /* [in] */ LPCWSTR szResponseHeaders,
  /* [in] */ LPCWSTR szRequestHeaders,
  /* [out] */ LPWSTR *pszAdditionalRequestHeaders)
{
  if (pszAdditionalRequestHeaders) {
    (*pszAdditionalRequestHeaders) = 0;
  }

  CComPtr<IHttpNegotiate> spHttpNegotiate;
  QueryServiceFromClient(&spHttpNegotiate);

  return (spHttpNegotiate)
      ? spHttpNegotiate->OnResponse(dwResponseCode, szResponseHeaders, szRequestHeaders, pszAdditionalRequestHeaders)
      : S_OK;
}

//----------------------------------------------------------------------------
//  ReportProgress
STDMETHODIMP CAnchoProtocolSink::ReportProgress(
  /* [in] */ ULONG ulStatusCode,
  /* [in] */ LPCWSTR szStatusText)
{
  if (ulStatusCode == BINDSTATUS_REDIRECTING) {
    PROTOCOLDATA data;
    data.grfFlags = PD_FORCE_SWITCH;
    data.dwState = ANCHO_SWITCH_REDIRECT;
    SwitchParams::create(data, m_Url.c_str(), szStatusText);
    AddRef();
    Switch(&data);

    m_Url = szStatusText;
  }

  ATLASSERT(m_spInternetProtocolSink != 0);
  return (m_spInternetProtocolSink)
      ? m_spInternetProtocolSink->ReportProgress(ulStatusCode, szStatusText)
      : S_OK;
}

//----------------------------------------------------------------------------
//  ReportData
STDMETHODIMP CAnchoProtocolSink::ReportData(
  /* [in] */ DWORD grfBSCF,
  /* [in] */ ULONG ulProgress,
  /* [in] */ ULONG ulProgressMax)
{
  PROTOCOLDATA data;
  data.grfFlags = PD_FORCE_SWITCH;
  data.dwState = ANCHO_SWITCH_REPORT_DATA;
  SwitchParams::create(data, m_Url.c_str(), NULL);
  AddRef();
  Switch(&data);

  ATLASSERT(m_spInternetProtocolSink != 0);
  return (m_spInternetProtocolSink)
      ? m_spInternetProtocolSink->ReportData(grfBSCF, ulProgress, ulProgressMax)
      : S_OK;
}

//----------------------------------------------------------------------------
//  ReportResult
STDMETHODIMP CAnchoProtocolSink::ReportResult(
  /* [in] */ HRESULT hrResult,
  /* [in] */ DWORD dwError,
  /* [in] */ LPCWSTR szResult)
{
  if (hrResult == 0) {
    PROTOCOLDATA data;
    data.grfFlags = PD_FORCE_SWITCH;
    data.dwState = ANCHO_SWITCH_REPORT_RESULT;
    SwitchParams::create(data, m_Url.c_str(), NULL);
    AddRef();
    Switch(&data);
  }

  ATLASSERT(m_spInternetProtocolSink != 0);
  return (m_spInternetProtocolSink)
      ? m_spInternetProtocolSink->ReportResult(hrResult, dwError, szResult)
      : S_OK;
}

//----------------------------------------------------------------------------
// GetBindInfo
STDMETHODIMP CAnchoProtocolSink::GetBindInfoEx(
  /* [out] */ DWORD *grfBINDF,
  /* [in, out] */ BINDINFO *pbindinfo,
  /* [out] */ DWORD *grfBINDF2,
  /* [out] */ DWORD* pdwReserved)
{
  CComQIPtr<IInternetBindInfoEx> pBindInfo(m_spInternetProtocolSink);
  HRESULT hr = pBindInfo->GetBindInfoEx(grfBINDF, pbindinfo, grfBINDF2, pdwReserved);
  if (SUCCEEDED(hr)) {
    m_bindVerb = pbindinfo->dwBindVerb;
  }
  return hr;
}

//----------------------------------------------------------------------------
// getCurrentBrowser
//  public version, returns stored browsers
//  Get browser for current frame if any, otherwise root browser.
HRESULT CAnchoProtocolSink::getCurrentBrowser(IWebBrowser2 ** aCurrentFrameBrowser, IWebBrowser2 ** aTopLevelBrowserPtr)
{
  ENSURE_RETVAL(aCurrentFrameBrowser)
  ENSURE_RETVAL(aTopLevelBrowserPtr)
  if (!mTopLevelBrowser || !mCurrentFrameBrowser) {
    return E_FAIL;
  }
  mTopLevelBrowser.CopyTo(aTopLevelBrowserPtr);
  mCurrentFrameBrowser.CopyTo(aCurrentFrameBrowser);
  return S_OK;
}

//----------------------------------------------------------------------------
// initializeBrowsers
//  private version, sets internal members
//  Get browser for current frame if any, otherwise root browser.
HRESULT CAnchoProtocolSink::initializeBrowsers()
{
  mTopLevelBrowser.Release();
  mCurrentFrameBrowser.Release();

  // Use different approaches to get the current webbrowser:

  // 1) Query directly from client
  HRESULT hr = QueryServiceFromClient(SID_SWebBrowserApp, &mTopLevelBrowser.p);
  if (FAILED(hr)) {
    // 2) If this failed, try getting the IWindowForBindingUI ...
    do {
      CComPtr<IWindowForBindingUI> windowForBindingUI;
      hr = QueryServiceFromClient(IID_IWindowForBindingUI, &windowForBindingUI);
      if (!windowForBindingUI) {
        break;
      }

      // ... and from there a HWND for "Internet Explorer_Server" ...
      HWND hwnd = NULL;
      hr = windowForBindingUI->GetWindow(IID_IAuthenticate, &hwnd);
      if (FAILED(hr)) {
        hr = windowForBindingUI->GetWindow(IID_IHttpSecurity, &hwnd);
      }
      if(FAILED(hr)) {
        break;
      }
      ATLASSERT(hwnd);

      // ... from there the IAccessible ...
      CComPtr<IAccessible> accessible;
      hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible, (void**)&accessible);
      ATLASSERT(SUCCEEDED(hr));
      if(FAILED(hr)) {
        break;
      }

      // ... that should be a IServiceProvider ...
      CComQIPtr<IServiceProvider> serviceProvider(accessible);
      ATLASSERT(serviceProvider);

      // ... that delivers us a IHTMLWindow2 ...
      CComPtr<IHTMLWindow2> window;
      hr = serviceProvider->QueryService(IID_IHTMLWindow2, IID_IHTMLWindow2, (void**)&window.p);
      if(FAILED(hr)) {
        break;
      }

      // ... which in turn is again a IServiceProvider ...
      serviceProvider = window;

      // ... giving us the IWebBrowser2 we are looking for.
      hr = serviceProvider->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, (void**)&mTopLevelBrowser.p);
    } while(false);
  }
  if(FAILED(hr)) {
    // 3) If 1 and 2 failed, get browser from CAnchoProtocolSink::sCurrentTopLevelBrowser
    mTopLevelBrowser = sCurrentTopLevelBrowser;
    sCurrentTopLevelBrowser.Release();
  }
  if (!mTopLevelBrowser) {
    return E_FAIL;
  }

  // Now get the browser for the current frame (if any).
  CComVariant currentFrameBrowser;
  hr = mTopLevelBrowser->GetProperty(CComBSTR(L"anchoCurrentBrowserForFrame"), &currentFrameBrowser);
  if (FAILED(hr)) {
    // failed means, we didn't get any value at all, even not NULL.
    return hr;
  }
  // Clear the property now that we have it so other requests don't get confused.
  mTopLevelBrowser->PutProperty(CComBSTR(L"anchoCurrentBrowserForFrame"), CComVariant(NULL));

  if (VT_DISPATCH == currentFrameBrowser.vt && currentFrameBrowser.pdispVal) {
    mCurrentFrameBrowser = currentFrameBrowser.pdispVal;
  }

  // if we don't have a framebrowser, set top level browser
  if (!mCurrentFrameBrowser) {
    mCurrentFrameBrowser = mTopLevelBrowser;
  }
  return S_OK;
}

/*============================================================================
 * class CAnchoPassthruAPP
 */

//----------------------------------------------------------------------------
//  StartEx
STDMETHODIMP CAnchoPassthruAPP::StartEx(
    IUri *pUri,
    IInternetProtocolSink *pOIProtSink,
    IInternetBindInfo *pOIBindInfo,
    DWORD grfPI,
    HANDLE_PTR dwReserved)
{
  CComBSTR rawUri;
  IF_FAILED_RET(pUri->GetRawUri(&rawUri));

  if (std::wstring(L"http:///") == std::wstring(rawUri.m_str)) {
    // IS THIS STILL REQUIRED?
    //Hack used for connecting browser navigate call and actual tab (request ID passed in url)
    //creation causes "http:///" being passed to PAPP - we can ignore it
    //TODO - remove after we also remove the hack (if even possible)
    return S_FALSE;
  }
  IF_FAILED_RET(__super::StartEx(pUri, pOIProtSink, pOIBindInfo, grfPI, dwReserved));
  CComPtr<CAnchoProtocolSink> pSink = GetSink();

  CComBSTR method;
  DWORD bindVerb = GetSink()->GetBindVerb();
  method = getMethodNameFromBindVerb(bindVerb);

  // sink set us up at this point already via initFromSink()
  if (mAnchoEvents) {
    // do we have a frame browser?
    PAPPTRACE(_T("%s browser 0x%08x\n"), rawUri, mCurrentFrameBrowser);

    CComPtr<IWebRequestReporter> reporter;
    if (SUCCEEDED(WebRequestReporter::createReporter(rawUri, method, mCurrentFrameBrowser, &reporter.p))) {
      IF_FAILED_RET(mAnchoEvents->OnBeforeRequest(CComVariant(reporter.p)));
      BOOL cancel = FALSE;
      if (SUCCEEDED(reporter->shouldCancel(&cancel)) && cancel) {
        Abort(INET_E_TERMINATED_BIND, 0);
      }
    }
  }

  return S_OK;
}

//----------------------------------------------------------------------------
//  Continue
STDMETHODIMP CAnchoPassthruAPP::Continue(PROTOCOLDATA* data)
{
  // If this is not an ancho related state...
  if (data->dwState < ANCHO_SWITCH_BASE || data->dwState >= ANCHO_SWITCH_MAX) {
    // ... just call super
    return __super::Continue(data);
  }

  CComPtr<CAnchoProtocolSink> pSink = GetSink();
  // Release the reference we added when calling Switch().
  pSink->InternalRelease();

  CComBSTR currentURL, bstrAdditional;
  ATLASSERT(data);
  CAnchoProtocolSink::SwitchParams::extractAndDestroy(*data, currentURL, bstrAdditional);

/***************************
TODO: Check if this is still reqired
  // Send the event for any redirects that occurred before we had access to the event sink.
  if (mAnchoEvents) {
    RedirectList::iterator it = mRedirects.begin();
    while(it != mRedirects.end()) {
      fireOnFrameRedirect(CComBSTR(it->first.c_str()), CComBSTR(it->second.c_str()));
      ++it;
    }
  }
  mRedirects.clear();
***************************/

  switch(data->dwState) {
    case ANCHO_SWITCH_REPORT_DATA:
      if (data->dwState > mLastRequestState && mCurrentFrameBrowser) {
        // S_OK and ONLY S_OK means it was handled and should not be called again
        if (S_OK == fireOnFrameStart(currentURL)) {
          mLastRequestState = data->dwState;
        }
      }
      break;
    case ANCHO_SWITCH_REDIRECT:
      fireOnFrameRedirect(currentURL, bstrAdditional);
      break;
    case ANCHO_SWITCH_REPORT_RESULT:
      if (data->dwState > mLastRequestState && mCurrentFrameBrowser) {
        // S_OK and ONLY S_OK means it was handled and should not be called again
        if (S_OK == fireOnFrameEnd(currentURL)) {
          mLastRequestState = data->dwState;
        }
      }
      break;
  }
  return S_OK;
}

//----------------------------------------------------------------------------
// initFromSink
// Called from sink after sink can provide the two browser objects.
HRESULT CAnchoPassthruAPP::initFromSink(CAnchoProtocolSink * aProtocolSink)
{
  ATLASSERT(aProtocolSink);
  reset();

  mIsTopLevelRefresh = aProtocolSink->isTopLevelRefresh();

  // get current frame browser and top level browser from sink
  aProtocolSink->getCurrentBrowser(&mCurrentFrameBrowser.p, &mTopLevelBrowser.p);

  if (mTopLevelBrowser) {
    // get ancho browser events callback
    CComVariant vt;
    mTopLevelBrowser->GetProperty(L"_anchoBrowserEvents", &vt);
    if (VT_DISPATCH == vt.vt) {
      mAnchoEvents = vt.pdispVal;
    }
  }
  // need all these three to be valid
  return (mCurrentFrameBrowser && mTopLevelBrowser && mAnchoEvents)
      ? S_OK
      : E_FAIL;
}

//----------------------------------------------------------------------------
//  reset
void CAnchoPassthruAPP::reset()
{
  mTopLevelBrowser.Release();
  mCurrentFrameBrowser.Release();
  mAnchoEvents.Release();
  mRedirects.clear();
  mIsTopLevelRefresh = FALSE;
  mLastRequestState = 0;
}

//----------------------------------------------------------------------------
//  fireOnFrameStart
HRESULT CAnchoPassthruAPP::fireOnFrameStart(CComBSTR & aCurrentURL)
{
  return (mAnchoEvents)
      ? mAnchoEvents->OnFrameStart(mCurrentFrameBrowser, aCurrentURL, mIsTopLevelRefresh ? VARIANT_TRUE : VARIANT_FALSE)
      : S_OK; // for resources we might not have events
}

//----------------------------------------------------------------------------
//  fireOnFrameRedirect
HRESULT CAnchoPassthruAPP::fireOnFrameRedirect(CComBSTR & aCurrentURL, CComBSTR & aAdditionalData)
{
  return (mAnchoEvents)
      ? mAnchoEvents->OnFrameRedirect(mCurrentFrameBrowser, aCurrentURL, aAdditionalData)
      : S_OK; // for resources we might not have events
}

//----------------------------------------------------------------------------
//  fireOnBeforeHeaders
HRESULT CAnchoPassthruAPP::fireOnBeforeHeaders(IWebRequestReporter * aReporter)
{
  return (mAnchoEvents)
    ? mAnchoEvents->OnBeforeSendHeaders(CComVariant(aReporter))
    : S_OK; // for resources we might not have events
}

//----------------------------------------------------------------------------
//  fireOnFrameEnd
HRESULT CAnchoPassthruAPP::fireOnFrameEnd(CComBSTR aUrl)
{
  // do we have a frame browser?
  CComPtr<IWebBrowser2> currentFrameBrowser((mCurrentFrameBrowser)
      ? mCurrentFrameBrowser
      : mTopLevelBrowser);
  PAPPTRACE(_T("%s browser 0x%08x %i\n"), aUrl, currentFrameBrowser, mIsTopLevelRefresh);

  if (!currentFrameBrowser) {
    return E_UNEXPECTED;
  }

  CComPtr<IDispatch> tmp;
  currentFrameBrowser->get_Document(&tmp);

  CComQIPtr<IHTMLDocument2> htmlDocument(tmp);
  if(htmlDocument && mAnchoEvents) {
    return DocumentSink::notifyEndFrame(currentFrameBrowser, htmlDocument, mAnchoEvents, aUrl, mIsTopLevelRefresh);
  }
  return S_FALSE;
}

/*============================================================================
 * class CAnchoPassthruAPP::DocumentSink
 */

//----------------------------------------------------------------------------
//  notifyEndFrame
HRESULT CAnchoPassthruAPP::DocumentSink::notifyEndFrame(
    IWebBrowser2 * aWebBrowser,
    IHTMLDocument2 *aHTMLDoc,
    DAnchoBrowserEvents* aBrowserEvents,
    BSTR aURL,
    BOOL aIsTopLevelRefresh)
{
  ATLASSERT(aHTMLDoc);
  ATLASSERT(aBrowserEvents);
  CComBSTR readyState;
  aHTMLDoc->get_readyState(&readyState);

  // complete state already reached, process immediately
  if (readyState == L"complete") {
    return aBrowserEvents->OnFrameEnd(aWebBrowser, aURL, aIsTopLevelRefresh ? VARIANT_TRUE : VARIANT_FALSE);
  }

  // Document not ready yet: Create instance and let it do the job when ready
  CComObject<DocumentSink> * sinkObject = NULL;
  HRESULT hr = CComObject<DocumentSink>::CreateInstance(&sinkObject);
  if (FAILED(hr)) {
    return hr;
  }
  CComPtr<IUnknown> tmp(sinkObject);
  // init will AddRef the object, it will stay alive until the job is done
  return sinkObject->init(aWebBrowser, aHTMLDoc, aBrowserEvents, aURL, aIsTopLevelRefresh);
}

//----------------------------------------------------------------------------
//  CTOR
CAnchoPassthruAPP::DocumentSink::DocumentSink() :
  mIsTopLevelRefresh(FALSE)
{
}

//----------------------------------------------------------------------------
//  DTOR
CAnchoPassthruAPP::DocumentSink::~DocumentSink()
{
  if (mHTMLDocument) {
    DispEventUnadvise(mHTMLDocument);
    mHTMLDocument.Release();
  }
}

//----------------------------------------------------------------------------
//  init
HRESULT CAnchoPassthruAPP::DocumentSink::init(IWebBrowser2 * aWebBrowser, IHTMLDocument2 *aHTMLDoc, DAnchoBrowserEvents* aBrowserEvents,
  BSTR aURL, BOOL aIsTopLevelRefresh)
{
  mBrowser = aWebBrowser;
  mHTMLDocument = aHTMLDoc;
  mEvents = aBrowserEvents;
  mURL = aURL;
  mIsTopLevelRefresh = aIsTopLevelRefresh;
  HRESULT hr = (mEvents && mHTMLDocument) ? S_OK : E_INVALIDARG;
  if (FAILED(hr)) {
    return hr;
  }
  // this will AddRef and keeps us alive
  return DispEventAdvise(mHTMLDocument);
}

//----------------------------------------------------------------------------
//  OnReadyStateChange
STDMETHODIMP_(void) CAnchoPassthruAPP::DocumentSink::OnReadyStateChange(IHTMLEventObj* ev)
{
  ATLASSERT(mHTMLDocument);
  ATLASSERT(mEvents);
  CComBSTR readyState;
  mHTMLDocument->get_readyState(&readyState);

  if (readyState == L"complete") {
    mEvents->OnFrameEnd(mBrowser, mURL, mIsTopLevelRefresh ? VARIANT_TRUE : VARIANT_FALSE);
    // tmp for DispEventUnadvise
    CComPtr<IHTMLDocument2> tmp(mHTMLDocument);
    mHTMLDocument.Release();

    // NOTE: this might destroy us, so no more member access beyond this point!
    DispEventUnadvise(tmp);
  }
}


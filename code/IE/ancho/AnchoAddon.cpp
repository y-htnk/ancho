/****************************************************************************
 * AnchoAddon.cpp : Implementation of CAnchoAddon
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoAddon.h"
#include "dllmain.h"
#include "ProtocolHandlerRegistrar.h"
#include "AnchoShared_i.h"
#include "AnchoShared/AnchoShared.h"
#include <crxProcessing/extract.hpp>
#include <fstream>
#include <AnchoCommons/COMConversions.hpp>

using namespace Ancho::Utils;

static boost::filesystem::wpath processCRXFile(std::wstring aExtensionName, boost::filesystem::wpath aPath, UpdateState &aUpdateState)
{
  aUpdateState = usNone;
  boost::filesystem::wpath extractedExtensionPath = Ancho::Utils::getAnchoAppDataDirectory()
                                                              / s_AnchoExtractedExtensionsDirName
                                                              / aExtensionName;
  //extractedExtensionPath /= s_AnchoExtractedExtensionsDirName;
  //extractedExtensionPath /= aExtensionName;

  boost::filesystem::wpath extensionSignaturePath = extractedExtensionPath / s_AnchoExtensionSignatureFileName;

  std::string signature = crx::getCRXSignature(aPath);
  bool shouldExtract = true;
  if (boost::filesystem::exists(extensionSignaturePath) && !signature.empty()) {
    //Check if we already extracted package with the same signature
    std::ifstream signatureFile(extensionSignaturePath.string().c_str());
    if (signatureFile.good()) {
      std::string oldSignature;
      signatureFile >> oldSignature;
      shouldExtract = oldSignature != signature;
      if (shouldExtract) {
        aUpdateState = usUpdated;
      }
    }
  } else {
    aUpdateState = usInstalled;
  }

  if (shouldExtract) {
    boost::filesystem::create_directories(extractedExtensionPath);
    crx::extract(aPath, extractedExtensionPath);
    if (!signature.empty()) {
      std::ofstream signatureFile(extensionSignaturePath.string().c_str());
      signatureFile << signature;
      signatureFile.close();
    }
  }
  return extractedExtensionPath;
}

/*============================================================================
 * class CAnchoAddon
 */

//----------------------------------------------------------------------------
// Init
STDMETHODIMP CAnchoAddon::Init(LPCOLESTR lpsExtensionID, IAnchoAddonService * pService,
  IWebBrowser2 * pWebBrowser)
{
  BEGIN_TRY_BLOCK;
  m_pWebBrowser = pWebBrowser;
  m_pAnchoService = pService;
  m_sExtensionName = lpsExtensionID;

  // lookup ID in registry
  CRegKey regKey;
  CString sKey;
  sKey.Format(_T("%s\\%s"), s_AnchoExtensionsRegistryKey, m_sExtensionName.c_str());
  LONG res = regKey.Open(HKEY_CURRENT_USER, sKey, KEY_READ);
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }

  // load flags to see if the addon is disabled
  DWORD flags = 0;
  res = regKey.QueryDWORDValue(s_AnchoExtensionsRegistryEntryFlags, flags);
  // to stay compatible with older versions we treat "no flags at all" as "enabled"
  if ( (ERROR_SUCCESS == res) && !(flags & ENABLED))
  {
    // ... means: only when flag is present AND ENABLED is not set
    // the addon is disabled
    return E_ABORT;
  }

  // get addon path
  {
    ULONG nChars = _MAX_PATH;
    CString tmp;
    LPTSTR pst = tmp.GetBuffer(nChars+1);
    res = regKey.QueryStringValue(s_AnchoExtensionsRegistryEntryPath, pst, &nChars);
    pst[nChars] = 0;
    //PathAddBackslash(pst);
    tmp.ReleaseBuffer();
    if (ERROR_SUCCESS != res) {
      return HRESULT_FROM_WIN32(res);
    }
    m_sExtensionPath = tmp;
  }

  boost::filesystem::wpath path(m_sExtensionPath);
  if (!boost::filesystem::is_directory(m_sExtensionPath)) {
    std::wstring extension = boost::to_lower_copy(path.extension().wstring());
    if (extension != L".crx") {
      return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    m_sExtensionPath = processCRXFile(m_sExtensionName, path, mUpdateState);
  }

  {
    // get addon GUID
    ULONG nChars = 37;  // length of a GUID + terminator
    CString tmp;
    res = regKey.QueryStringValue(s_AnchoExtensionsRegistryEntryGUID, tmp.GetBuffer(nChars), &nChars);
    tmp.ReleaseBuffer();
    if (ERROR_SUCCESS != res)
    {
      return HRESULT_FROM_WIN32(res);
    }
    m_sExtensionID = tmp;
  }

  // Register protocol handler
  IF_FAILED_RET(CProtocolHandlerRegistrar::
    RegisterTemporaryFolderHandler(s_AnchoProtocolHandlerScheme, m_sExtensionName.c_str(), m_sExtensionPath.wstring().c_str()));

  CRegKey hklmAncho;
  res = hklmAncho.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Salsita\\AnchoAddonService"), KEY_READ);
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }

  // Get install dir and load magpie from there (is different for 32 and 64bit!)
  ULONG nChars = MAX_PATH;
  LPTSTR pst = m_sInstallPath.GetBuffer(nChars);
  res = hklmAncho.QueryStringValue(_T("install"), pst, &nChars);
  pst[nChars] = 0;
  PathAddBackslash(pst);
  m_sInstallPath.ReleaseBuffer();

  // create content script engine. This engine is used for extension pages only.
  IF_FAILED_RET(createMagpieInstance(&m_Magpie.p));

  return S_OK;
  END_TRY_BLOCK_CATCH_TO_HRESULT;
}

//----------------------------------------------------------------------------
// Shutdown
STDMETHODIMP CAnchoAddon::Shutdown()
{
  // this method must be safe to be called multiple times
  cleanupScripting();
  m_pBackgroundConsole.Release();
  m_Magpie.Release();

  if (m_pAddonBackground) {
    m_pAddonBackground->UnadviseInstance(m_InstanceID);
  }
  m_pAddonBackground.Release();

  m_pAnchoService.Release();

  if (m_pWebBrowser) {
    m_pWebBrowser.Release();
  }

  cleanupFrames();
  cleanupScripting();

  return S_OK;
}

//----------------------------------------------------------------------------
// OnFrameStart
STDMETHODIMP CAnchoAddon::OnFrameStart(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelFrame, VARIANT_BOOL aIsTopLevelRefresh)
{
  if (VARIANT_TRUE == aIsTopLevelFrame) {
    // Now is the time to remove all frames and cleanup everything.
    // This method is also hit in case of a refresh, that's why we do it here.
    cleanupFrames();
    cleanupScripting();
  }

  // Get / setup frame record:
  FrameMap::iterator it = mMapFrames.find(COMOBJECTID(aBrowser));
  if (it != mMapFrames.end()) {
    // Found, just update.
    it->second->mIsTopLevel = (VARIANT_TRUE == aIsTopLevelFrame);
    it->second->OnFrameStart(aBrowser, aUrl, aIsTopLevelRefresh);
  }
  else {
    // Not found, create new one including Magpie.
    FrameRecordPtr newFrame(new FrameRecord(aBrowser, (VARIANT_TRUE == aIsTopLevelFrame)));
    IF_FAILED_RET(createMagpieInstance(&newFrame->mMagpie.p));

    newFrame->OnFrameStart(aBrowser, aUrl, aIsTopLevelRefresh);
    mMapFrames[COMOBJECTID(aBrowser)] = std::move(newFrame);
  }

  return S_OK;
}

//----------------------------------------------------------------------------
// OnFrameEnd
STDMETHODIMP CAnchoAddon::OnFrameEnd(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelFrame, VARIANT_BOOL aIsTopLevelRefresh)
{
  IF_FAILED_RET(initializeEnvironment());

  // get content our API
  if (!m_pContentInfo) {
    IF_FAILED_RET(m_pAddonBackground->GetContentInfo(m_InstanceID, aUrl, &m_pContentInfo));
    ATLTRACE(L"ANCHO - GetContentInfo() succeeded");
  }

  FrameMap::iterator it = mMapFrames.find(COMOBJECTID(aBrowser));
  if (it != mMapFrames.end()) {
    // name for that instance for debugger
    CString magpieDebugName;
    magpieDebugName.Format(_T("Ancho content [%s] [%i]"), m_sExtensionName, m_InstanceID);

    // get chrome API
    CIDispatchHelper contentInfo(m_pContentInfo);
    CComVariant chromeAPIVT;
    IF_FAILED_RET((contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"api", chromeAPIVT)));

    // get the name(s) of content scripts from manifest and run them in order
    CComVariant contentScriptsVT;
    IF_FAILED_RET((contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"scripts", contentScriptsVT)));
    if (VT_DISPATCH != contentScriptsVT.vt || !contentScriptsVT.pdispVal) {
      contentScriptsVT.Clear();
    }

    VariantVector contentScripts;
    IF_FAILED_RET(addJSArrayToVariantVector(contentScriptsVT.pdispVal, contentScripts));

    return it->second->InitializeContentScripting(this, magpieDebugName, m_sExtensionPath.c_str(), chromeAPIVT.pdispVal, aUrl, contentScripts);
  }
  return S_OK;
}

//----------------------------------------------------------------------------
// InitializeExtensionScripting
STDMETHODIMP CAnchoAddon::InitializeExtensionScripting(BSTR aUrl)
{
  IF_FAILED_RET(initializeEnvironment());

  cleanupScripting();

  // get content our API
  IF_FAILED_RET(m_pAddonBackground->GetContentInfo(m_InstanceID, aUrl, &m_pContentInfo));

  CIDispatchHelper contentInfo(m_pContentInfo);
  CComVariant api;
  IF_FAILED_RET((contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"api", api)));

  CIDispatchHelper script = CIDispatchHelper::GetScriptDispatch(m_pWebBrowser);
  IF_FAILED_RET(script.SetPropertyByRef(L"chrome", api));
  CComVariant console;
  IF_FAILED_RET((contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"console", console)));
  IF_FAILED_RET(script.SetPropertyByRef(L"console", console));

  return S_OK;
}

//----------------------------------------------------------------------------
// getWrappedDOMWindow
HRESULT CAnchoAddon::getWrappedDOMWindow(IWebBrowser2 * aFrameBrowser, IDispatch ** aRetHTMLWindow)
{
  ENSURE_RETVAL(aRetHTMLWindow);
  (*aRetHTMLWindow) = NULL;
  DOMWindowWrapperMap::iterator it = mDOMWindowWrapper.find(COMOBJECTID(aFrameBrowser));
  if (it == mDOMWindowWrapper.end()) {
    return E_FAIL;
  }

  return it->second.CopyTo(aRetHTMLWindow);
}

//----------------------------------------------------------------------------
// putWrappedDOMWindow
void CAnchoAddon::putWrappedDOMWindow(IWebBrowser2 * aFrameBrowser, IDispatch * aHTMLWindow)
{
  mDOMWindowWrapper[COMOBJECTID(aFrameBrowser)] = aHTMLWindow;
}

//----------------------------------------------------------------------------
// removeWrappedDOMWindow
void CAnchoAddon::removeWrappedDOMWindow(IWebBrowser2 * aFrameBrowser)
{
  mDOMWindowWrapper.erase(COMOBJECTID(aFrameBrowser));
}

//----------------------------------------------------------------------------
//  cleanupFrames
void CAnchoAddon::cleanupFrames()
{
  // Prevent us from being called from zombie frames.
  for (FrameMap::iterator it = mMapFrames.begin();
      it != mMapFrames.end(); ++it) {
    it->second->cleanup();
  }
  mMapFrames.clear();
  // Also remove all DOMWindowWrappers
  mDOMWindowWrapper.clear();
}

//----------------------------------------------------------------------------
//  cleanupScripting
void CAnchoAddon::cleanupScripting()
{
  if (m_Magpie) {
    m_Magpie->Shutdown();
  }
  if (m_pAddonBackground && m_InstanceID) {
    m_pAddonBackground->ReleaseContentInfo(m_InstanceID);
  }
  if (m_pContentInfo) {
    m_pContentInfo.Release();
  }
}

//----------------------------------------------------------------------------
//  notifyAboutUpdateStatus
void CAnchoAddon::notifyAboutUpdateStatus()
{
  CComQIPtr<IAnchoServiceApi> serviceApi = m_pAnchoService;
  if (!serviceApi) {
    return;
  }
  try {
    _variant_t ret;
    CComPtr<IDispatch> argPtr;
    CComPtr<IDispatch> arrayPtr;
    IF_FAILED_THROW(serviceApi->createJSObject(_bstr_t(m_sExtensionName.c_str()), 0 /*object is targeted for background*/, &argPtr));
    Ancho::Utils::JSObjectWrapper argument =  Ancho::Utils::JSValueWrapper(argPtr).toObject();
    IF_FAILED_THROW(serviceApi->createJSArray(_bstr_t(m_sExtensionName.c_str()), 0 /*object is targeted for background*/, &arrayPtr));
    Ancho::Utils::JSArrayWrapper argumentList =  Ancho::Utils::JSValueWrapper(arrayPtr).toArray();
    argumentList.push_back(argument);

    switch (mUpdateState) {
    case usInstalled:
      argument[L"reason"] = L"install";
      break;
    case usUpdated:
      argument[L"reason"] = L"update";
      //TODO - argument[L"previousVersion"] = ...
      break;
    default:
      ATLASSERT(false);
    }
    IF_FAILED_THROW(m_pAddonBackground->invokeExternalEventObject(CComBSTR(L"runtime.onInstalled"), arrayPtr, &ret.GetVARIANT()));
  } catch (std::exception &) {
    ATLTRACE(L"Firing chrome.runtime.onInstalled event failed\n");
  }
}

//----------------------------------------------------------------------------
// createMagpieInstance
//  Creates a new empty instance of Magpie.
HRESULT CAnchoAddon::createMagpieInstance(IMagpieApplication ** aMagpieRetVal)
{
  ENSURE_RETVAL(aMagpieRetVal);
  (*aMagpieRetVal) = NULL;

  CComPtr<IMagpieApplication> magpie;

#ifdef MAGPIE_REGISTERED
  IF_FAILED_RET(magpie.CoCreateInstance(CLSID_MagpieApplication));

#else
  CString magpieModule(m_sInstallPath);
  magpieModule += _T("Magpie.dll");
  HMODULE hModMagpie = ::LoadLibrary(magpieModule);
  if (!hModMagpie)
  {
    return E_FAIL;
  }
  fnCreateMagpieInstance CreateMagpieInstance = (fnCreateMagpieInstance)::GetProcAddress(hModMagpie, "CreateMagpieInstance");
  if (!CreateMagpieInstance)
  {
    return E_FAIL;
  }
  IF_FAILED_RET(CreateMagpieInstance(&magpie));
#endif

  if (!magpie) {
    return E_FAIL;
  }
  (*aMagpieRetVal) = magpie.Detach();
  return S_OK;
}

HRESULT CAnchoAddon::initializeEnvironment()
{
  //If create AddonBackground sooner - background script will be executed before initialization of tab windows
  if(!m_pAddonBackground || !m_pBackgroundConsole) {
    IF_FAILED_RET(m_pAnchoService->GetCreateAddonBackground(CComBSTR(m_sExtensionName.c_str()), &m_pAddonBackground));

    if (mUpdateState != usNone) {
      notifyAboutUpdateStatus();
    }

    // get console
    m_pBackgroundConsole = m_pAddonBackground;
    ATLASSERT(m_pBackgroundConsole);
  }
  if(!m_InstanceID) {
    // tell background we are there and get instance id
    m_pAddonBackground->AdviseInstance(&m_InstanceID);

     //TODO - should be executed as soon as possible
    m_pAnchoService->webBrowserReady();
  }

  return S_OK;
}

/*============================================================================
 * class CAnchoAddon::FrameRecord
 */

//----------------------------------------------------------------------------
//  CTOR
CAnchoAddon::FrameRecord::FrameRecord(
    IWebBrowser2  * aWebBrowser,
    BOOL            aIsTopLevel) :
      mBrowser(aWebBrowser),
      mIsTopLevel(aIsTopLevel)
{
}

//----------------------------------------------------------------------------
//  DTOR
CAnchoAddon::FrameRecord::~FrameRecord()
{
  cleanupScripting();
}

//----------------------------------------------------------------------------
//  cleanupScripting
void CAnchoAddon::FrameRecord::cleanupScripting()
{
  if (mMagpie) {
    // Don't destroy Magpie, just shut it down, it will be reused.
    mMagpie->Shutdown();
  }
  // The wrapped window will be a new one for the next page.
  if (mWrappedDOMWindow) {
    mWrappedDOMWindow->cleanup();
  }
  mWrappedDOMWindow.Release();
}

//----------------------------------------------------------------------------
//  cleanup
void CAnchoAddon::FrameRecord::cleanup()
{
  // The wrapped window has to release its IDOMWindowWrapperManager.
  if (mWrappedDOMWindow) {
    mWrappedDOMWindow->cleanup();
  }
}

//----------------------------------------------------------------------------
//  OnFrameStart
HRESULT CAnchoAddon::FrameRecord::OnFrameStart(
    IWebBrowser2  * aBrowser,
    BSTR            aUrl,
    VARIANT_BOOL    aIsTopLevelRefresh)
{
  cleanupScripting();
  return S_OK;
}

//----------------------------------------------------------------------------
//  InitializeContentScripting
HRESULT CAnchoAddon::FrameRecord::InitializeContentScripting(
    IDOMWindowWrapperManager  * aDOMWindowManager,
    LPCWSTR                     aMagpieDebugName,
    LPCWSTR                     aExtensionPath,
    LPDISPATCH                  aChromeAPI,
    BSTR                        aUrl,
    VariantVector             & aContentScripts)
{
  ATLASSERT(mBrowser);
  ATLASSERT(mMagpie);

  // Init magpie.
  IF_FAILED_RET(mMagpie->Init((LPWSTR)aMagpieDebugName));

  // Add a loader for scripts in the extension filesystem.
  IF_FAILED_RET(mMagpie->AddFilesystemScriptLoader((LPWSTR)aExtensionPath));

  // Inject items: chrome, and window wrapper (with global members).
  mMagpie->AddNamedItem(L"chrome", aChromeAPI, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_CODEONLY);
  IF_FAILED_RET(DOMWindowWrapper::createInstance(aDOMWindowManager, mBrowser, mWrappedDOMWindow));
  mMagpie->AddNamedItem(L"window", mWrappedDOMWindow, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_GLOBALMEMBERS);

  // DOM window wrapper.
  CIDispatchHelper window = mWrappedDOMWindow;

  // Replace XMLHttpRequest.
  CComPtr<IAnchoXmlHttpRequest> pRequest;
  IF_FAILED_RET(createAnchoXHRInstance(&pRequest));
  IF_FAILED_RET(window.SetProperty((LPOLESTR)L"XMLHttpRequest", CComVariant(pRequest.p)));
  mMagpie->AddNamedItem(L"XMLHttpRequest", pRequest, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_CODEONLY);

  // Execute content scripts.
  for(VariantVector::iterator it = aContentScripts.begin(); it != aContentScripts.end(); ++it) {
    if( it->vt == VT_BSTR ) {
      mMagpie->ExecuteGlobal(it->bstrVal);
    }
  }

  return S_OK;
}


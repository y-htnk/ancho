/****************************************************************************
 * AnchoAddon.cpp : Implementation of CAnchoAddon
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoAddon.h"
#include "dllmain.h"

extern class CanchoModule _AtlModule;

/*============================================================================
 * class CAnchoAddon
 */

//----------------------------------------------------------------------------
//  Init
STDMETHODIMP CAnchoAddon::Init(LPCOLESTR lpsExtensionID, IAnchoAddonService * pService,
  IWebBrowser2 * pWebBrowser)
{
  m_pWebBrowser = pWebBrowser;
  m_pAnchoService = pService;
  m_sExtensionName = lpsExtensionID;

  // lookup ID in registry
  CRegKey regKey;
  CString sKey;
  sKey.Format(_T("%s\\%s"), s_AnchoExtensionsRegistryKey, m_sExtensionName);
  LONG res = regKey.Open(HKEY_CURRENT_USER, sKey, KEY_READ);
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }

  // get addon GUID
  ULONG nChars = 37;  // length of a GUID + terminator
  res = regKey.QueryStringValue(s_AnchoExtensionsRegistryEntryGUID, m_sExtensionID.GetBuffer(nChars), &nChars);
  m_sExtensionID.ReleaseBuffer();
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }

  // get addon path
  nChars = _MAX_PATH;
  LPTSTR pst = m_sExtensionPath.GetBuffer(nChars+1);
  res = regKey.QueryStringValue(s_AnchoExtensionsRegistryEntryPath, pst, &nChars);
  pst[nChars] = 0;
  PathAddBackslash(pst);
  m_sExtensionPath.ReleaseBuffer();
  if (ERROR_SUCCESS != res)
  {
    return HRESULT_FROM_WIN32(res);
  }
  if (!PathIsDirectory(m_sExtensionPath))
  {
    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
  }

  // get addon instance
  //IF_FAILED_RET(m_pAnchoService->GetAddonBackground(CComBSTR(m_sExtensionName), &m_pAddonBackground));

  // The addon can be a resource DLL or simply a folder in the filesystem.
  // TODO: Load the DLL if there is any.

  // create content script engine
#ifdef MAGPIE_REGISTERED
  IF_FAILED_RET(m_Magpie.CoCreateInstance(CLSID_MagpieApplication));
#else
  CComBSTR bs;
  IF_FAILED_RET(m_pAnchoService->GetModulePath(&bs));

  // Load magpie from the same path where this exe file is.
  CString s(bs);
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

  return S_OK;
}

//----------------------------------------------------------------------------
//  Shutdown
STDMETHODIMP CAnchoAddon::Shutdown()
{
  // this method must be safe to be called multiple times
  m_pContentInfo.Release();
  m_pBackgroundConsole.Release();
  m_Magpie.Release();

  if (m_pAddonBackground)
  {
    m_pAddonBackground->UnadviseInstance(m_InstanceID);
  }
  m_pAddonBackground.Release();

  m_pAnchoService.Release();

  if (m_pWebBrowser)
  {
    m_pWebBrowser.Release();
  }
  return S_OK;
}
//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddon::executeScriptCode(BSTR aCode)
{
  //TODO: it needs method which is not implmented in magpie yet
  return S_OK;
}

//----------------------------------------------------------------------------
//
STDMETHODIMP CAnchoAddon::executeScriptFile(BSTR aFile)
{
  //TODO: improve, when manifest processing finished
  return m_Magpie->Run(aFile);
}

//----------------------------------------------------------------------------
//  ApplyContentScripts
STDMETHODIMP CAnchoAddon::ApplyContentScripts(IWebBrowser2* pBrowser, BSTR bstrUrl, documentLoadPhase aPhase)
{
  HRESULT hr = S_OK;
  //If create AddonBackground sooner - background script will be executed before initialization of tab windows
  if(!m_pAddonBackground || !m_pBackgroundConsole) {
    hr = m_pAnchoService->GetAddonBackground(CComBSTR(m_sExtensionName), &m_pAddonBackground);
    IF_FAILED_RET(hr);
    // get console
    m_pBackgroundConsole = m_pAddonBackground;
    ATLASSERT(m_pBackgroundConsole);
  }
  if(!m_pContentInfo) {
    // tell background we are there and get instance id
    m_pAddonBackground->AdviseInstance(&m_InstanceID);

     //TODO - should be executed as soon as possible
    m_pAnchoService->webBrowserReady();

    // get content our API
    m_pAddonBackground->GetContentAPI(m_InstanceID, &m_pContentInfo);
  }

  // content script handling happens here

  // no frame handling
  // TODO: decide how to handle frames
  if (!m_pWebBrowser.IsEqualObject(pBrowser)) {
    return S_OK;
  }

  if (aPhase != documentLoadEnd) {
    return S_OK;
  }

  if (!m_pContentInfo) {
    return S_OK;
  }

  // TODO: URL matching
  // (re)initialize magpie for this page
  m_Magpie->Shutdown();
  CString s;
  s.Format(_T("Ancho content [%s] [%i]"), m_sExtensionName, m_InstanceID);
  hr = m_Magpie->Init((LPWSTR)(LPCWSTR)s);
  if (FAILED(hr))
  {
    return hr;
  }

  // add a loader for scripts in the extension filesystem
  IF_FAILED_RET(m_Magpie->AddFilesystemScriptLoader((LPWSTR)(LPCWSTR)m_sExtensionPath));

  // inject items: chrome, console and window with global members
//  CComQIPtr<IDispatch> pDispConsole;
  CComQIPtr<IWebBrowser2> pWebBrowser(pBrowser);
  CIDispatchHelper script = CIDispatchHelper::GetScriptDispatch(pWebBrowser);
  if (!script) {
    return S_OK;
  }

  CIDispatchHelper contentInfo(m_pContentInfo);
  CComVariant jsObj;
  hr = contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"api", jsObj);
  if (FAILED(hr))
  {
    return hr;
  }

  m_Magpie->AddNamedItem(L"chrome", jsObj.pdispVal, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_CODEONLY);
  //m_Magpie->AddNamedItem(L"console", pDispConsole, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_CODEONLY);
  m_Magpie->AddNamedItem(L"window", script.p, SCRIPTITEM_ISVISIBLE|SCRIPTITEM_GLOBALMEMBERS);

  // get the name(s) of content scripts from manifest and run them in order
  hr = contentInfo.Get<CComVariant, VT_DISPATCH, IDispatch*>(L"scripts", jsObj);
  if (FAILED(hr))
  {
    return hr;
  }

  VariantVector scripts;
  IF_FAILED_RET(addJSArrayToVariantVector(jsObj.pdispVal, scripts));

  // scripts array is in reverse order here!
  for(VariantVector::reverse_iterator it = scripts.rbegin(); it != scripts.rend(); it++) {
    if( it->vt == VT_BSTR ) {
      m_Magpie->ExecuteGlobal(it->bstrVal);
    }
  }

  return S_OK;
}

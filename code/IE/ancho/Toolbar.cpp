#include "stdafx.h"
#include "Toolbar.h"
#include "ProtocolHandlerRegistrar.h"

HWND CAnchoToolbar::findParentWindowByClass(HWND window, std::wstring aClassName)
{
  wchar_t className[256];
  while (window) {
    if (!GetClassName(window, className, 256)) {
      return NULL;
    }
    if (aClassName == className) {
      return window;
    }
    window = GetParent(window);
  }
  return NULL;
}

HRESULT CAnchoToolbar::InternalSetSite()
{
  CComQIPtr<IOleWindow> parentWindow(m_spUnkSite);
  if (!parentWindow) {
    return E_FAIL;
  }
  HWND hwndParent = NULL;
  parentWindow->GetWindow(&hwndParent);

  HWND frameTab = findParentWindowByClass(hwndParent, L"Frame Tab");
  if (!frameTab) {
    ATLASSERT(0 && "TOOLBAR: Failed to obtain 'Frame Tab' window handle.");
    return E_FAIL;
  }
  ATLTRACE(L"ANCHO: toolbar InternalSetSite() - CoCreateInstace(CLSID_AnchoAddonService)\n");

  // create addon service object
  IF_FAILED_RET(mAnchoService.CoCreateInstance(CLSID_AnchoAddonService));

  CComBSTR serviceHost, servicePath;
  IF_FAILED_RET(mAnchoService->getInternalProtocolParameters(&serviceHost, &servicePath));
  IF_FAILED_RET(CProtocolHandlerRegistrar::
    RegisterTemporaryResourceHandler(s_AnchoInternalProtocolHandlerScheme, serviceHost, servicePath));

  CComBSTR url;
  mAnchoService->registerBrowserActionToolbar((INT)frameTab, &url, &mTabId);

  mAnchoService->getDispatchObject(&mToolbarWindow.mExternalDispatch);
  mToolbarWindow.mTabId = mTabId;

  mToolbarWindow.Create(hwndParent, CWindow::rcDefault, NULL,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);

  if (!mToolbarWindow.mWebBrowser) {
    return E_FAIL;
  }

  return mToolbarWindow.mWebBrowser->Navigate(url, NULL, NULL, NULL, NULL);
}

HRESULT CAnchoToolbar::InternalReleaseSite()
{
  mToolbarWindow.DestroyWindow();
  if (mAnchoService) {
    mAnchoService->unregisterBrowserActionToolbar(mTabId);
    mAnchoService.Release();
  }
  return S_OK;
}

STDMETHODIMP CAnchoToolbar::SetSite(IUnknown *pUnkSite)
{
  m_spUnkSite = pUnkSite;

  if (m_spUnkSite) {
    IF_FAILED_RET(InternalSetSite())
  }
  else {
    InternalReleaseSite();
  }
  return S_OK;
}

STDMETHODIMP CAnchoToolbar::GetBandInfo(DWORD dwBandID, DWORD dwViewMode, DESKBANDINFO* pdbi)
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
      //pdbi->dwModeFlags = DBIMF_VARIABLEHEIGHT;
    }

    if (pdbi->dwMask & DBIM_BKCOLOR) {
      pdbi->dwMask &= ~DBIM_BKCOLOR;
    }
    return S_OK;
  }
  return E_INVALIDARG;
}


STDMETHODIMP CAnchoToolbar::GetWindow(HWND* phwnd)
{
  if (!phwnd) {
    return E_POINTER;
  }
  (*phwnd) = mToolbarWindow;
  return S_OK;
}


STDMETHODIMP CAnchoToolbar::ContextSensitiveHelp(BOOL fEnterMode)
{
  return S_OK;
}


STDMETHODIMP CAnchoToolbar::CloseDW(unsigned long dwReserved)
{
  mToolbarWindow.DestroyWindow();
  return S_OK;
}


STDMETHODIMP CAnchoToolbar::ResizeBorderDW(const RECT* prcBorder, IUnknown* punkToolbarSite, BOOL fReserved)
{
  return E_NOTIMPL;
}


STDMETHODIMP CAnchoToolbar::ShowDW(BOOL fShow)
{
  mToolbarWindow.ShowWindow(fShow ? SW_SHOW : SW_HIDE);
  return S_OK;
}


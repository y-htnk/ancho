#pragma once

#include <Shlobj.h>
#include "HTMLToolbarWindow.h"
#include "ancho_i.h"
//#include <string>

class ATL_NO_VTABLE CAnchoToolbar :
  public CComObjectRootEx<CComSingleThreadModel>,
  public CComCoClass<CAnchoToolbar, &CLSID_IEToolbar>,
  public IObjectWithSiteImpl<CAnchoToolbar>,
  public IDeskBand
{
public:
  DECLARE_PROTECT_FINAL_CONSTRUCT()
  DECLARE_REGISTRY_RESOURCEID(IDR_ANCHOTOOLBAR)
  DECLARE_NOT_AGGREGATABLE(CAnchoToolbar)

  BEGIN_COM_MAP(CAnchoToolbar)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IDeskBand)
    COM_INTERFACE_ENTRY(IOleWindow)
    COM_INTERFACE_ENTRY(IDockingWindow)
  END_COM_MAP()

public:
  HRESULT FinalConstruct()
  {
    m_dwBandID = 0;
    m_dwViewMode = 0;
/*
    ComContentWindow *contentWindow = NULL;
    HRESULT hr = ComContentWindow::CreateInstance(&contentWindow);
    if (FAILED(hr)) {
      return hr;
    }
    contentWindow->AddRef();
    mContentWindow.Attach(contentWindow);
*/
    return S_OK;
  }

  void FinalRelease()
  {
  }

public:
// IObjectWithSite
  STDMETHOD(SetSite)(IUnknown *pUnkSite);

// IDeskBand
  STDMETHOD(GetBandInfo)(DWORD dwBandID, DWORD dwViewMode, DESKBANDINFO* pdbi);

// IOleWindow
  STDMETHOD(GetWindow)(HWND* phwnd);
  STDMETHOD(ContextSensitiveHelp)(BOOL fEnterMode);

// IDockingWindow
  STDMETHOD(CloseDW)(unsigned long dwReserved);
  STDMETHOD(ResizeBorderDW)(const RECT* prcBorder, IUnknown* punkToolbarSite, BOOL fReserved);
  STDMETHOD(ShowDW)(BOOL fShow);

public:
  HWND findParentWindowByClass(HWND window, std::wstring aClassName);

protected:
  HRESULT InternalSetSite();
  HRESULT InternalReleaseSite();

protected:
  //typedef CComObject<CHtmlToolbarWindow> ComContentWindow;
  //CComPtr<ComContentWindow> mContentWindow;

  //CComPtr<IInputObjectSite> mInputObjectSite;

  //HWND mHWNDParent;
  CComObjectStackEx<HtmlToolbarWindow>  mToolbarWindow;

  CComPtr<IAnchoAddonService> mAnchoService;
  int mTabId;

  DWORD m_dwBandID;
  DWORD m_dwViewMode;
};

//*******************************************************************************
/*
class CToolbar :
  public CBasicToolbar<CToolbar, CHtmlToolbarWindow, &CLSID_IEToolbar>,
  public ToolbarCallback
{
public:
  typedef CBasicToolbar<CToolbar, CHtmlToolbarWindow, &CLSID_IEToolbar> BaseClass;
  DECLARE_REGISTRY_RESOURCEID(IDR_ANCHOTOOLBAR)

  CToolbar(): mTabId(0)
  {
  }
  virtual void GetBandInfoValues(const wchar_t *& title, POINTL &minSize);

public:
  HRESULT FinalConstruct()
  {
    HRESULT hr = BaseClass::FinalConstruct();
    if (FAILED(hr)) {
      return hr;
    }
    mContentWindow->m_toolbarCallback = dynamic_cast<ToolbarCallback *>(this);
    return hr;
  }

  void FinalRelease()
  {
    if (mContentWindow) {
      mContentWindow->m_toolbarCallback = NULL;
    }
    BaseClass::FinalRelease();
  }

  // IInputObject
  STDMETHOD(HasFocusIO)(void);
  STDMETHOD(TranslateAcceleratorIO)(LPMSG lpMsg);
  STDMETHOD(UIActivateIO)(BOOL fActivate, LPMSG lpMsg);

  virtual void ToolbarWindowBeforeNavigate();
  virtual void ToolbarWindowReady(VARIANT *pURL);
  virtual void ToolbarWindowNavigateComplete(IDispatch *pDisp, VARIANT *URL);
  virtual void ToolbarWindowOnSetFocus();
  virtual void ToolbarWindowOnKillFocus();
protected:
  virtual HRESULT InternalSetSite();
  virtual HRESULT InternalReleaseSite();

private:

  HRESULT RunToolbarPage();
  std::wstring mUrl;
  CComPtr<IAnchoAddonService> mAnchoService;
  int mTabId;

};
*/

OBJECT_ENTRY_AUTO(CLSID_IEToolbar, CAnchoToolbar)

//#include "Toolbar.ipp"
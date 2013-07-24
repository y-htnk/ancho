#pragma once

#include <exdispid.h>

class CAnchoAddonService;

class CPopupWindow;
typedef CComObject<CPopupWindow>  CPopupWindowComObject;
typedef IDispEventImpl<1, CPopupWindow, &DIID_DWebBrowserEvents2, &LIBID_SHDocVw, 1, 0> PopupWebBrowserEvents;

class CPopupWindow :
  public CComObjectRootEx<CComSingleThreadModel>,
  public CWindowImpl<CPopupWindow, CAxWindow>,
  public IUnknown,
  public PopupWebBrowserEvents
{
public:
  friend struct OnClickFunctor;
  static const unsigned defaultWidth = 2;
  static const unsigned defaultHeight = 2;
  DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

  virtual void OnFinalMessage(HWND);
  void InjectJsObjects();

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  BEGIN_COM_MAP(CPopupWindow)
    COM_INTERFACE_ENTRY(IUnknown)
  END_COM_MAP()

  BEGIN_SINK_MAP(CPopupWindow)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_PROGRESSCHANGE, OnBrowserProgressChange)
    SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete)
  END_SINK_MAP()

  BEGIN_MSG_MAP(CPopupWindow)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
  END_MSG_MAP()

  HRESULT FinalConstruct();
  void FinalRelease();

  static HRESULT CreatePopupWindow(HWND aParent, CAnchoAddonService *aService, const DispatchMap &aInjectedObjects, LPCWSTR aURL, int aX, int aY, CIDispatchHelper aCloseCallback);

  LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
  LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
  LRESULT OnActivate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

  STDMETHOD_(void, OnBrowserProgressChange)(LONG Progress, LONG ProgressMax);
  STDMETHOD_(void, OnNavigateComplete)(IDispatch* pDispBrowser, VARIANT * vtURL);

  void checkResize();
private:
  CComPtr<IHTMLElement> getBodyElement();

  CComQIPtr<IWebBrowser2>   mWebBrowser;     // Embedded WebBrowserControl
  DispatchMap mInjectedObjects;
  CStringW    mURL;
  CIDispatchHelper mCloseCallback;
  CAnchoAddonService *mService;

  CComVariant mResizeEventHandler;
  CComVariant mOnClickEventHandler;
};


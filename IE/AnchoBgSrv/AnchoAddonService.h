/****************************************************************************
 * AnchoAddonService.h : Declaration of CAnchoAddonService
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#pragma once
#include "resource.h"       // main symbols

#include "AnchoBgSrv_i.h"
#include "AnchoBackground.h"
#include "AnchoBgSrvModule.h"
#include "IECookieManager.h"
#include "CommandQueue.h"


#include <Exceptions.h>
#include <SimpleWrappers.h>
#include <IPCHeartbeat.h>

#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"
#include "AnchoBackgroundServer/TabManager.hpp"
#include "AnchoBackgroundServer/COMConversions.hpp"
#include "AnchoBackgroundServer/JavaScriptCallback.hpp"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

class CAnchoAddonService;
/*============================================================================
 * class CAnchoAddonServiceCallback
 */
struct CAnchoAddonServiceCallback
{
  virtual void OnAddonFinalRelease(BSTR bsID) = 0;
};

/*============================================================================
 * class CAnchoAddonService
 */
class ATL_NO_VTABLE CAnchoAddonService :
  public CAnchoAddonServiceCallback,
  public CComObjectRootEx<CComSingleThreadModel>,
  public CComCoClass<CAnchoAddonService, &CLSID_AnchoAddonService>,
  public IAnchoAddonService,
  public IDispatchImpl<IAnchoServiceApi, &IID_IAnchoServiceApi, &LIBID_AnchoBgSrvLib, /*wMajor =*/ 0xffff, /*wMinor =*/ 0xffff>
{
public:
  // -------------------------------------------------------------------------
  // ctor
  CAnchoAddonService()
  {
  }

public:
  // -------------------------------------------------------------------------
  // COM standard stuff
  DECLARE_REGISTRY_RESOURCEID(IDR_SCRIPTSERVICE)
  DECLARE_CLASSFACTORY_SINGLETON(CAnchoAddonService)
  DECLARE_NOT_AGGREGATABLE(CAnchoAddonService)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

public:
  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(CAnchoAddonService)
    COM_INTERFACE_ENTRY(IAnchoAddonService)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IAnchoServiceApi)
  END_COM_MAP()

public:
  // -------------------------------------------------------------------------
  // COM standard methods
  HRESULT FinalConstruct();
  void FinalRelease();

public:
  // -------------------------------------------------------------------------
  // Methods
  inline LPCTSTR GetThisPath() {return m_sThisPath;}

  // CAnchoAddonServiceCallback implementation
  virtual void OnAddonFinalRelease(BSTR bsID);

  HRESULT navigateBrowser(LPUNKNOWN aWebBrowserWin, const std::wstring &url, INT32 aNavigateOptions);
  HRESULT getActiveWebBrowser(LPUNKNOWN* pUnkWebBrowser);
public:
  // -------------------------------------------------------------------------
  // IAnchoServiceApi methods. See .idl for description.
  STDMETHOD(get_cookieManager)(LPDISPATCH* ppRet);
  STDMETHOD(get_tabManager)(LPDISPATCH* ppRet);

  STDMETHOD(invokeExternalEventObject)(BSTR aExtensionId, BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet);

  STDMETHOD(getWindow)(INT aWindowId, LPDISPATCH aCreator, BOOL aPopulate, VARIANT* aRet);
  STDMETHOD(getAllWindows)(LPDISPATCH aCreator, BOOL aPopulate, VARIANT* aRet);
  STDMETHOD(updateWindow)(INT aWindowId, LPDISPATCH aProperties);
  STDMETHOD(createWindow)(LPDISPATCH aProperties, LPDISPATCH aCreator, LPDISPATCH aCallback);
  STDMETHOD(closeWindow)(INT aWindowId);
  STDMETHOD(createPopupWindow)(BSTR aUrl, INT aX, INT aY, LPDISPATCH aInjectedData, LPDISPATCH aCloseCallback);
  STDMETHOD(getCurrentWindowId)(INT *aWinId);


  //STDMETHOD(get_browserActionInfos)(VARIANT* aBrowserActionInfos);
  STDMETHOD(getBrowserActions)(VARIANT* aBrowserActionsArray);
  STDMETHOD(addBrowserActionInfo)(LPDISPATCH aBrowserActionInfo);
  STDMETHOD(setBrowserActionUpdateCallback)(INT aTabId, LPDISPATCH aBrowserActionUpdateCallback);
  STDMETHOD(browserActionNotification)();

  STDMETHOD(testFunction)(LPDISPATCH aObject, LPDISPATCH aCallback)
  {
    ATLTRACE(L"TEST FUNCTION -----------------\n");
    BEGIN_TRY_BLOCK;
    return S_OK;
    END_TRY_BLOCK_CATCH_TO_HRESULT;
  }
  // -------------------------------------------------------------------------
  // IAnchoAddonService methods. See .idl for description.
  STDMETHOD(GetAddonBackground)(BSTR bsID, IAnchoAddonBackground ** ppRet);
  STDMETHOD(GetModulePath)(BSTR * pbsPath);
  STDMETHOD(getInternalProtocolParameters)(BSTR * aServiceHost, BSTR * aServicePath);
  STDMETHOD(invokeEventObjectInAllExtensions)(BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet);
  STDMETHOD(invokeEventObjectInAllExtensionsWithIDispatchArgument)(BSTR aEventName, LPDISPATCH aArg);

  STDMETHOD(webBrowserReady)();

  STDMETHOD(registerBrowserActionToolbar)(OLE_HANDLE aFrameTab, BSTR * aUrl, INT*aTabId);
  STDMETHOD(unregisterBrowserActionToolbar)(INT aTabId);
  STDMETHOD(getDispatchObject)(IDispatch **aRet);
private:

  void fillWindowInfo(HWND aWndHandle, CIDispatchHelper &aInfo);
  HWND getCurrentWindowHWND();
  bool isIEWindow(HWND);

  INT winHWNDToId(HWND aHwnd)
  { return reinterpret_cast<INT>(aHwnd); }

  HWND winIdToHWND(INT aWinId)
  { return reinterpret_cast<HWND>(aWinId); }

public:
  /*class ATabCreatedCallback: public ACommand
  {
  public:
#if _HAS_CPP0X
  typedef std::shared_ptr<ATabCreatedCallback> Ptr;
#else
  typedef std::tr1::shared_ptr<ATabCreatedCallback> Ptr;
#endif

    void operator()(INT aTabID)
    { execute(aTabID); }
    virtual void execute(INT aTabID) = 0;
  };*/
  //class TabCreatedCallback;
  class WindowCreatedCallback;

  //typedef std::map<int, ATabCreatedCallback::Ptr> CreateTabCallbackMap;

  //HRESULT createTabImpl(CIDispatchHelper &aProperties, ATabCreatedCallback::Ptr aCallback, bool aInNewWindow);
//  HRESULT createWindowImpl(CIDispatchHelper &aProperties, ATabCreatedCallback::Ptr aCallback);

  HRESULT FindActiveBrowser(IWebBrowser2** webBrowser);
private:
    //Private type declarations

  // a map containing all addon background objects - one per addon
  typedef std::map<std::wstring, CAnchoAddonBackgroundComObject*> BackgroundObjectsMap;
  typedef std::map<int, CIDispatchHelper> BrowserActionCallbackMap;

  typedef CComCritSecLock<CComAutoCriticalSection> CSLock;
private:
  //private methods

private:
  // -------------------------------------------------------------------------
  // Private members.


  BackgroundObjectsMap          m_BackgroundObjects;

  CComPtr<ComSimpleJSArray>     m_BrowserActionInfos;
  BrowserActionCallbackMap      m_BrowserActionCallbacks;
  CommandQueue                  m_WebBrowserPostInitTasks;

  Ancho::Utils::AsynchronousTaskManager mAsyncTaskManager;

  // Path to this exe and also to magpie.
  CString                       m_sThisPath;

  CComPtr<IIECookieManager>     m_Cookies;

  CComPtr<ITabManager>          mITabManager;
};

OBJECT_ENTRY_AUTO(__uuidof(AnchoAddonService), CAnchoAddonService)

//--------------------------------------------------------------------

/*class CAnchoAddonService::TabCreatedCallback: public ATabCreatedCallback
{
public:
  TabCreatedCallback(CAnchoAddonService &aService, LPDISPATCH aCreator, LPDISPATCH aCallback)
    : mService(aService), mCreator(aCreator), mCallback(aCallback)
  {}
  void execute(INT aTabID)
  {
    CComVariant tabInfo;
    if (FAILED(mService.getTabInfo(aTabID, mCreator, &tabInfo))) {
      throw std::runtime_error("Failed to get tab info");
    }
    if (FAILED(mCallback.Invoke1((DISPID) 0, &tabInfo))) {
      throw std::runtime_error("Failed to invoke callback for on tab create");
    }
  }
protected:
  CAnchoAddonService &mService;
  CIDispatchHelper mCreator;
  CIDispatchHelper mCallback;
};

class CAnchoAddonService::WindowCreatedCallback: public ATabCreatedCallback
{
public:
  WindowCreatedCallback(CAnchoAddonService &aService, LPDISPATCH aCreator, LPDISPATCH aCallback)
    : mService(aService), mCreator(aCreator), mCallback(aCallback)
  {}
  void execute(INT aTabID)
  {
    CComVariant tabInfo;
    if (FAILED(mService.getTabInfo(aTabID, mCreator, &tabInfo)) || tabInfo.vt != VT_DISPATCH) {
      throw std::runtime_error("Failed to get tab info");
    }
    CIDispatchHelper tabInfoHelper = tabInfo.pdispVal;
    int winId;
    if (FAILED((tabInfoHelper.Get<int, VT_I4>(L"windowId", winId)))) {
      throw std::runtime_error("Failed to get windowID from tab info");
    }
    CComVariant windowInfo;
    if (FAILED(mService.getWindow(winId, mCreator, FALSE, &windowInfo))) {
      throw std::runtime_error("Failed to get window info");
    }
    if (FAILED(mCallback.Invoke1((DISPID) 0, &windowInfo))) {
      throw std::runtime_error("Failed to invoke callback for on tab create");
    }
  }
protected:
  CAnchoAddonService &mService;
  CIDispatchHelper mCreator;
  CIDispatchHelper mCallback;
};


class CAnchoAddonService::CreateWindowCommand: public AQueuedCommand
{
public:
  CreateWindowCommand(CAnchoAddonService &aService, LPDISPATCH aProperties, LPDISPATCH aCreator, LPDISPATCH aCallback)
    : mService(aService), mProperties(aProperties), mCreator(aCreator), mCallback(aCallback)
  {}
  void execute()
  {
    mService.createWindowImpl(mProperties, ATabCreatedCallback::Ptr(new WindowCreatedCallback(mService, mCreator, mCallback)));
  }
protected:
  CAnchoAddonService &mService;
  CIDispatchHelper mProperties;
  CIDispatchHelper mCreator;
  CIDispatchHelper mCallback;
};*/


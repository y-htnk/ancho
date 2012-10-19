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

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

/*============================================================================
 * class CAnchoAddonServiceCallback
 */
struct CAnchoAddonServiceCallback
{
  virtual void OnAddonFinalRelease(BSTR bsID) = 0;
  virtual HRESULT invokeExternalEventObject(BSTR aExtensionId, BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet) = 0;
  virtual HRESULT navigateBrowser(LPUNKNOWN aWebBrowserWin, const std::wstring &url) = 0;
  virtual HRESULT getActiveWebBrowser(LPUNKNOWN* pUnkWebBrowser) = 0;
  virtual HRESULT createTab(LPDISPATCH aProperties, LPDISPATCH aCreator, LPDISPATCH aCallback) = 0;
  virtual HRESULT reloadTab(INT aTabId) = 0;
  virtual HRESULT removeTabs(LPDISPATCH aTabs, LPDISPATCH aCallback) = 0;
  virtual HRESULT updateTab(INT aTabId, LPDISPATCH aProperties) = 0;
  virtual HRESULT getTabInfo(INT aTabId, LPDISPATCH aCreator, VARIANT* aRet) = 0;
  virtual HRESULT queryTabs(LPDISPATCH aQueryInfo, LPDISPATCH aCreator, VARIANT* aRet) = 0;
  virtual HRESULT executeScript(BSTR aExtensionID, INT aTabID, BSTR aCode, BOOL aFileSpecified, BOOL aInAllFrames) = 0;
};

/*============================================================================
 * class CAnchoAddonService
 */
class ATL_NO_VTABLE CAnchoAddonService :
  public CAnchoAddonServiceCallback,
  public CComObjectRootEx<CComSingleThreadModel>,
  public CComCoClass<CAnchoAddonService, &CLSID_AnchoAddonService>,
  public IAnchoAddonService
{
public:
  // -------------------------------------------------------------------------
  // ctor
  CAnchoAddonService(): m_NextTabID(1), m_NextRequestID(1)
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

  HRESULT invokeExternalEventObject(BSTR aExtensionId, BSTR aEventName, LPDISPATCH aArgs, VARIANT* aRet);

  HRESULT navigateBrowser(LPUNKNOWN aWebBrowserWin, const std::wstring &url);
  HRESULT getActiveWebBrowser(LPUNKNOWN* pUnkWebBrowser);
  HRESULT createTab(LPDISPATCH aProperties, LPDISPATCH aCreator, LPDISPATCH aCallback);
  HRESULT reloadTab(INT aTabId);
  HRESULT removeTabs(LPDISPATCH aTabs, LPDISPATCH aCallback);
  HRESULT updateTab(INT aTabId, LPDISPATCH aProperties);
  HRESULT getTabInfo(INT aTabId, LPDISPATCH aCreator, VARIANT* aRet);
  HRESULT queryTabs(LPDISPATCH aQueryInfo, LPDISPATCH aCreator, VARIANT* aRet);
  HRESULT executeScript(BSTR aExtensionID, INT aTabID, BSTR aCode, BOOL aFileSpecified, BOOL aInAllFrames);
public:
  // -------------------------------------------------------------------------
  // IAnchoAddonService methods. See .idl for description.
  STDMETHOD(GetExtension)(BSTR bsID, IAnchoAddonBackground ** ppRet);
  STDMETHOD(GetModulePath)(BSTR * pbsPath);
  STDMETHOD(registerRuntime)(IAnchoRuntime * aRuntime, INT *aTabID);
  STDMETHOD(unregisterRuntime)(INT aTabID);
  STDMETHOD(createTabNotification)(INT aTabID, INT aRequestID);
  STDMETHOD(invokeEventObjectInAllExtensions)(BSTR aEventName, LPDISPATCH aArgs);
private:
  HRESULT removeTab(INT aTabId, LPDISPATCH aCallback);
  HRESULT executeScriptInTab(BSTR aExtensionID, INT aTabID, BSTR aCode, BOOL aFileSpecified);

  HRESULT FindActiveBrowser(IWebBrowser2** webBrowser);

  struct RuntimeRecord {
    RuntimeRecord(IAnchoRuntime *aRuntime = NULL)
      : runtime(aRuntime) {}
    CComPtr<IAnchoRuntime> runtime;
    CIDispatchHelper callback;
  };
  typedef std::map<int, RuntimeRecord> RuntimeMap;

  //Used for storing requests for calling createTab callbacks
  struct CreateTabCallbackRecord {
    CreateTabCallbackRecord() {}
    CreateTabCallbackRecord(CIDispatchHelper aCreator, CIDispatchHelper aCallback)
      : creator(aCreator), callback(aCallback)
    { }
    CIDispatchHelper creator;
    CIDispatchHelper callback;
  };
  typedef std::map<int, CreateTabCallbackRecord> CreateTabCallbackMap;
  // -------------------------------------------------------------------------
  // Private members.

  // a map containing all addon background objects - one per addon
  typedef std::map<std::wstring, CAnchoAddonBackgroundComObject*> ObjectsMap;
  ObjectsMap  m_Objects;

  RuntimeMap  m_Runtimes;
  CreateTabCallbackMap m_CreateTabCallbacks;

  // Path to this exe and also to magpie.
  CString             m_sThisPath;

  int     m_NextTabID;
  int     m_NextRequestID;
};

OBJECT_ENTRY_AUTO(__uuidof(AnchoAddonService), CAnchoAddonService)

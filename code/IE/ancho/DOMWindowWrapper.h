/****************************************************************************
 * DOMWindowWrapper.h : Declaration of the DOMWindowWrapper
 * Copyright 2012 Salsita Software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#pragma once

class DOMWindowWrapper;

/*===========================================================================
 * class HTMLLocationWrapper
 *  Wraps window.location for the content scripts.
 * @TODO: The wrapped location object is in fact also a IDispatcEx, but for
 * now we don't sandbox it. So all expando properties are valid for the actual
 * page, for content scripts, and, if you pass it to the background, also for
 * background scripts.
 * This _might_ be a security risk, but extending location is not very common
 * and should anyway be avoided.
 * However it is possible to pass data across addon boundaries using location.
 * So this is subject to a later change.
 */
class ATL_NO_VTABLE HTMLLocationWrapper :
  public CComObjectRootEx<CComSingleThreadModel>,
  public IDispatchEx
{
public:
  typedef CComObject<HTMLLocationWrapper> ComObject;
  typedef CComPtr<ComObject> ComPtr;

  static HRESULT createInstance(IHTMLWindow2 * aHTMLWindow,
                                ComPtr & aRet);

  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(HTMLLocationWrapper)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IDispatchEx)
  END_COM_MAP()

  // -------------------------------------------------------------------------
  // COM standard methods
  HRESULT FinalConstruct() {
    return S_OK;
  }

  void FinalRelease() {
    mLocation.Release();
  }

public:
  // -------------------------------------------------------------------------
  // IDispatch methods: forward
#define FORWARD_CALL(_name, ...) \
    { return (mLocation) ? mLocation->_name(__VA_ARGS__) : E_UNEXPECTED; }

  STDMETHOD(GetTypeInfoCount)(UINT *pctinfo)
      FORWARD_CALL(GetTypeInfoCount, pctinfo)
  STDMETHOD(GetTypeInfo)(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
      FORWARD_CALL(GetTypeInfo, iTInfo, lcid, ppTInfo)
  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                         LCID lcid, DISPID *rgDispId)
      FORWARD_CALL(GetIDsOfNames, riid, rgszNames, cNames, lcid, rgDispId)
  STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                  DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                  EXCEPINFO *pExcepInfo, UINT *puArgErr)
      FORWARD_CALL(Invoke, dispIdMember, riid, lcid, wFlags, pDispParams,
                    pVarResult, pExcepInfo, puArgErr)

  // -------------------------------------------------------------------------
  // IDispatchEx methods: forward
  STDMETHOD(GetDispID)(BSTR bstrName, DWORD grfdex, DISPID *pid)
      FORWARD_CALL(GetDispID, bstrName, grfdex, pid)
  STDMETHOD(InvokeEx)(DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
                      VARIANT *pvarRes, EXCEPINFO *pei,
                      IServiceProvider *pspCaller)
      // NOTE: Here happens the knack: we provide NULL for pspCaller, so
      // the security check does not kick in.
      FORWARD_CALL(InvokeEx, id, lcid, wFlags, pdp, pvarRes, pei, NULL)
  STDMETHOD(DeleteMemberByName)(BSTR bstrName,DWORD grfdex)
      FORWARD_CALL(DeleteMemberByName, bstrName, grfdex)
  STDMETHOD(DeleteMemberByDispID)(DISPID id)
      FORWARD_CALL(DeleteMemberByDispID, id)
  STDMETHOD(GetMemberProperties)(DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
      FORWARD_CALL(GetMemberProperties, id, grfdexFetch, pgrfdex)
  STDMETHOD(GetMemberName)(DISPID id, BSTR *pbstrName)
      FORWARD_CALL(GetMemberName, id, pbstrName)
  STDMETHOD(GetNextDispID)(DWORD grfdex, DISPID id, DISPID *pid)
      FORWARD_CALL(GetNextDispID, grfdex, id, pid)
  STDMETHOD(GetNameSpaceParent)(IUnknown **ppunk)
      FORWARD_CALL(GetNameSpaceParent, ppunk)
#undef FORWARD_CALL

private:
  // the original IHTMLLocation object
  CComPtr<IDispatchEx> mLocation;
};

/*===========================================================================
 * class DOMWindowWrapper
 *  Wraps a DOM window for the content scripts.
 *  This is kind of a sandbox, originally created to prevent jquery from
 *  messing around with content windows.
 */
class ATL_NO_VTABLE DOMWindowWrapper :
  public CComObjectRootEx<CComSingleThreadModel>,
  public IDispatchEx
{
public:
  typedef CComObject<DOMWindowWrapper>  ComObject;

  // -------------------------------------------------------------------------
  // static methods
  static HRESULT createInstance(IWebBrowser2 * aWebBrowser,
                                CComPtr<ComObject> & aRet);

public:
  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(DOMWindowWrapper)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IDispatchEx)
  END_COM_MAP()

public:
  // -------------------------------------------------------------------------
  // COM standard methods
  void FinalRelease();

  // helper - see comment in implementation
  void cleanup();

public:
  // -------------------------------------------------------------------------
  // IDispatch methods: simply forward
#define FORWARD_CALL(_name, ...) \
    { return (mDOMWindowInterfaces.dispEx) ? mDOMWindowInterfaces.dispEx->_name(__VA_ARGS__) : E_UNEXPECTED; }
  STDMETHOD(GetTypeInfoCount)(UINT *pctinfo)
      FORWARD_CALL(GetTypeInfoCount, pctinfo)
  STDMETHOD(GetTypeInfo)(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
      FORWARD_CALL(GetTypeInfo, iTInfo, lcid, ppTInfo)
  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                         LCID lcid, DISPID *rgDispId)
      FORWARD_CALL(GetIDsOfNames, riid, rgszNames, cNames, lcid, rgDispId)
  STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                  DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                  EXCEPINFO *pExcepInfo, UINT *puArgErr)
      FORWARD_CALL(Invoke, dispIdMember, riid, lcid, wFlags, pDispParams,
                    pVarResult, pExcepInfo, puArgErr)
#undef FORWARD_CALL

  // -------------------------------------------------------------------------
  // IDispatchEx methods
  STDMETHOD(GetDispID)(BSTR bstrName, DWORD grfdex, DISPID *pid);
  STDMETHOD(InvokeEx)(DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
                      VARIANT *pvarRes, EXCEPINFO *pei,
                      IServiceProvider *pspCaller);
  STDMETHOD(DeleteMemberByName)(BSTR bstrName,DWORD grfdex);
  STDMETHOD(DeleteMemberByDispID)(DISPID id);
  STDMETHOD(GetMemberProperties)(DISPID id, DWORD grfdexFetch, DWORD *pgrfdex);
  STDMETHOD(GetMemberName)(DISPID id, BSTR *pbstrName);
  STDMETHOD(GetNextDispID)(DWORD grfdex, DISPID id, DISPID *pid);
  STDMETHOD(GetNameSpaceParent)(IUnknown **ppunk);

private:
  // -------------------------------------------------------------------------
  // ctor
  DOMWindowWrapper()
    {}

  HRESULT init(IWebBrowser2 * aWebBrowser);

  // Map DISPIDs of on... properties to their names. The on... properties need
  // a special handling (see dispatchPropertyPut).
  static const wchar_t *getEventPropertyName(DISPID id);

  // -------------------------------------------------------------------------
  // IDispatchEx::InvokeEx handlers.
  HRESULT dispatchMethod(DISPID dispIdMember, REFIID riid, LCID lcid,
                    DISPPARAMS *pDispParams, VARIANT *pVarResult,
                    EXCEPINFO *pExcepInfo, IServiceProvider *pspCaller,
                    BOOL & aHandled) const;
  HRESULT dispatchPropertyGet(WORD wFlags, DISPID dispIdMember, REFIID riid,
                    LCID lcid, DISPPARAMS *pDispParams, VARIANT *pVarResult,
                    EXCEPINFO *pExcepInfo, IServiceProvider *pspCaller,
                    BOOL & aHandled) const;
  HRESULT dispatchPropertyPut(WORD wFlags, DISPID dispIdMember, REFIID riid,
                    LCID lcid, DISPPARAMS *pDispParams, VARIANT *pVarResult,
                    EXCEPINFO *pExcepInfo, IServiceProvider *pspCaller,
                    BOOL & aHandled);
  HRESULT dispatchConstruct(DISPID dispIdMember, REFIID riid,
                    LCID lcid, DISPPARAMS *pDispParams, VARIANT *pVarResult,
                    EXCEPINFO *pExcepInfo, IServiceProvider *pspCaller,
                    BOOL & aHandled) const;

private:
  // -------------------------------------------------------------------------
  // Private types.
  typedef std::map<DISPID, CComVariant> MapDISPIDToCComVariant;
  typedef std::map<CComBSTR, DISPID> MapNameToDISPID;
  typedef std::map<DISPID, CComBSTR> MapDISPIDToName;

  /*==========================================================================
   * class HTMLWindowInterfaces
   *  Keeps track of all known IHTMLWindowN interfaces to get the original
   * DISPIDs of the wrapped window. We need this class, because the "window"
   * object we obtain from the webbrowser seems to be kind of a wrapper
   * itself - it has all the default properties wrapped up as expando
   * properties.
   * It's only purpose is to ask all known interfaces for a certain DISPID.
   */
  class HTMLWindowInterfaces {
  public:
    HRESULT getDispID(LPOLESTR name, DISPID & id);
    HTMLWindowInterfaces & operator = (LPDISPATCH pDisp);
    void Release();

    CComQIPtr<IDispatchEx>   dispEx;
    CComQIPtr<IHTMLWindow2>  w2;
    CComQIPtr<IHTMLWindow3>  w3;
    CComQIPtr<IHTMLWindow4>  w4;
    CComQIPtr<IHTMLWindow5>  w5;
  };

  // -------------------------------------------------------------------------
  // Private members.

  friend ComObject;   // needs to call constructor

  // The first DISPID for dynamic properties the IE dom window uses.
  // NOTE: This value comes from debugging, it is not documented.
  enum { DISPID_DOMWINDOW_EX_FIRST = 10000 };

  // These DISPIDs are related to on... properties. We have to handle them
  // in a special way, so we need the IDs.
  enum {
    DISPID_ONBEFOREUNLOAD = -2147412073,
    DISPID_ONMESSAGE      = -2147412002,
    DISPID_ONBLUR         = -2147412097,
    DISPID_ONUNLOAD       = -2147412079,
    DISPID_ONHASHCHANGE   = -2147412003,
    DISPID_ONLOAD         = -2147412080,
    DISPID_ONSCROLL       = -2147412081,
    DISPID_ONAFTERPRINT   = -2147412045,
    DISPID_ONRESIZE       = -2147412076,
    DISPID_ONERROR        = -2147412083,
    DISPID_ONHELP         = -2147412099,
    DISPID_ONBEFOREPRINT  = -2147412046,
    DISPID_ONFOCUS        = -2147412098,
    // also there is the DISPID for window.location
    DISPID_LOCATION       = 14,
    // and there are properties for frames:
    DISPID_PARENT         = 12,
    DISPID_FRAMES         = 3000700,
    DISPID_OPENER         = 3000701,
    DISPID_SELF           = 3000703,
    DISPID_TOP            = 3000704
  };

  // the original DOM window
  HTMLWindowInterfaces    mDOMWindowInterfaces;

  // map DISPID -> VARIANT (the actual property)
  MapDISPIDToCComVariant  mDOMWindowProperties;
  // map name -> DISPID
  MapNameToDISPID         mDOMWindowPropertyIDs;
  // map DISPID -> name (for reverse lookups in GetMemberName()
  MapDISPIDToName         mDOMWindowPropertyNames;

  // Event properties. We can't store them in the expando property map
  // because this will mess up GetNextDispID.
  MapDISPIDToCComVariant  mDOMEventProperties;

  // Wrapper for window.location
  HTMLLocationWrapper::ComPtr mLocation;
};

/****************************************************************************
 * DOMWindowWrapper.cpp : Implementation of DOMWindowWrapper
 * Copyright 2012 Salsita Software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#include "StdAfx.h"
#include "DOMWindowWrapper.h"

// ---------------------------------------------------------------------------
// HTMLFramesCollection::createInstance
HRESULT HTMLFramesCollection::createInstance(
    DOMWindowWrapper  * aWindowWrapper,
    IHTMLWindow2      * aHTMLWindow,
    ComPtr            & aRet)
{
  if (!aHTMLWindow) {
    return E_INVALIDARG;
  }

  CComPtr<IHTMLFramesCollection2> frameCollection;
  IF_FAILED_RET(aHTMLWindow->get_frames(&frameCollection.p));

  ComObject * frameCollectionObject = NULL;
  IF_FAILED_RET(ComObject::CreateInstance(&frameCollectionObject));
  ComPtr ptr = frameCollectionObject;

  frameCollectionObject->mWindowWrapper = aWindowWrapper;
  frameCollectionObject->mFrameCollection = frameCollection;
  if (!frameCollectionObject->mFrameCollection) {
    return E_NOINTERFACE;
  }
  aRet = ptr;
  return S_OK;
}

// ---------------------------------------------------------------------------
// HTMLFramesCollection::handsOffWrapper
void HTMLFramesCollection::handsOffWrapper()
{
  mWindowWrapper = NULL;
}

// ---------------------------------------------------------------------------
// HTMLFramesCollection::InvokeEx
STDMETHODIMP HTMLFramesCollection::InvokeEx(DISPID id, LCID lcid, WORD wFlags,
                                        DISPPARAMS *pdp, VARIANT *pvarRes,
                                        EXCEPINFO *pei,
                                        IServiceProvider *pspCaller)
{
  HRESULT hr = (mFrameCollection)
    ? mFrameCollection->InvokeEx(id, lcid, wFlags, pdp, pvarRes, pei, pspCaller)
    : E_INVALIDARG;
  if (FAILED(hr)) {
    return hr;
  }

  // IHTMLFramesCollection2 has only two methods. If it is not "length" then it is "item".
  // That in turn translates to a DISPATCH_PROPERTYGET for the property <framename-or-index>.
  // For that the return value is an object.
  // NOTE: Of course it is possible to extend window.frames, but this we ignore here.
  // So everything here that is an object should be a frame. Try to get it.
  if(    (wFlags & (DISPATCH_PROPERTYGET | DISPATCH_METHOD))
      && (VT_DISPATCH == pvarRes->vt)
      && pvarRes->pdispVal) {
    CComQIPtr<IHTMLWindow2> window(pvarRes->pdispVal);
    if (mWindowWrapper) {
      VariantClear(pvarRes);
      // Get wrapped window for original window.
      mWindowWrapper->getRelatedWrappedWindow(window, pspCaller, pvarRes);
      // In case getRelatedWrappedWindow failed we return an empty VARIANT, but no error.
      // We don't want js exceptions to be thrown.
    }
  }
  return hr;
}

// ---------------------------------------------------------------------------
// HTMLLocationWrapper::createInstance
HRESULT HTMLLocationWrapper::createInstance(IHTMLWindow2 * aHTMLWindow,
                              ComPtr & aRet)
{
  if (!aHTMLWindow) {
    return E_INVALIDARG;
  }
  CComPtr<IHTMLLocation> htmlLocation;
  IF_FAILED_RET(aHTMLWindow->get_location(&htmlLocation.p));

  ComObject * locationObject = NULL;
  IF_FAILED_RET(ComObject::CreateInstance(&locationObject));
  ComPtr ptr = locationObject;

  locationObject->mLocation = htmlLocation;
  if (!locationObject->mLocation) {
    return E_NOINTERFACE;
  }
  aRet = ptr;
  return S_OK;
}

// ---------------------------------------------------------------------------
// HTMLWindowInterfaces::operator =
DOMWindowWrapper::HTMLWindowInterfaces &
    DOMWindowWrapper::HTMLWindowInterfaces::operator = (LPDISPATCH pDisp) {
  dispEx = pDisp;
  w2 = pDisp;
  w3 = pDisp;
  w4 = pDisp;
  w5 = pDisp;
  return *this;
}

// ---------------------------------------------------------------------------
// HTMLWindowInterfaces::getDispID
HRESULT DOMWindowWrapper::HTMLWindowInterfaces::
    getDispID(LPOLESTR name, DISPID & id)
{
  LPOLESTR names[] = {name};
#define RETURN_IDS_OF_NAMES_FROM_IFACE(_if) \
  if (_if && SUCCEEDED(_if->GetIDsOfNames(IID_NULL, names, 1, LANG_NEUTRAL, &id))) { \
    return S_OK; \
  }
  RETURN_IDS_OF_NAMES_FROM_IFACE(w2);
  RETURN_IDS_OF_NAMES_FROM_IFACE(w3);
  RETURN_IDS_OF_NAMES_FROM_IFACE(w4);
  RETURN_IDS_OF_NAMES_FROM_IFACE(w5);
#undef RETURN_IDS_OF_NAMES_FROM_IFACE
  return DISP_E_UNKNOWNNAME;
}

// ---------------------------------------------------------------------------
// HTMLWindowInterfaces::Release
void DOMWindowWrapper::HTMLWindowInterfaces::Release() {
  dispEx.Release();
  w2.Release();
  w3.Release();
  w4.Release();
  w5.Release();
}

// ---------------------------------------------------------------------------
// createInstance
HRESULT DOMWindowWrapper::createInstance(
    IDOMWindowWrapperManager  * aDOMWindowManager,
    IWebBrowser2              * aWebBrowser,
    CComPtr<ComObject>        & aRet)
{
  ComObject * newObject = NULL;
  IF_FAILED_RET(ComObject::CreateInstance(&newObject));
  CComPtr<ComObject> owner(newObject);
  IF_FAILED_RET(newObject->init(aDOMWindowManager, aWebBrowser));
  aRet = owner;
  return S_OK;
}

// ---------------------------------------------------------------------------
// getEventPropertyName
const wchar_t *DOMWindowWrapper::getEventPropertyName(DISPID id) {
  // for simplicity and speed we use a hardcoded, static map here
  switch(id) {
    case DISPID_ONBEFOREUNLOAD: return L"onbeforeunload";
    case DISPID_ONMESSAGE: return L"onmessage";
    case DISPID_ONBLUR: return L"onblur";
    case DISPID_ONUNLOAD: return L"onunload";
    case DISPID_ONHASHCHANGE: return L"onhashchange";
    case DISPID_ONLOAD: return L"onload";
    case DISPID_ONSCROLL: return L"onscroll";
    case DISPID_ONAFTERPRINT: return L"onafterprint";
    case DISPID_ONRESIZE: return L"onresize";
    case DISPID_ONERROR: return L"onerror";
    case DISPID_ONHELP: return L"onhelp";
    case DISPID_ONBEFOREPRINT: return L"onbeforeprint";
    case DISPID_ONFOCUS: return L"onfocus";
  }
  return NULL;
}

// ---------------------------------------------------------------------------
// init
HRESULT DOMWindowWrapper::init(
    IDOMWindowWrapperManager  * aDOMWindowManager,
    IWebBrowser2              * aWebBrowser)
{
  if (!aWebBrowser) {
    return E_INVALIDARG;
  }
  mWebBrowser = aWebBrowser;
  mDOMWindowInterfaces = CIDispatchHelper::GetScriptDispatch(aWebBrowser);
  if (!mDOMWindowInterfaces.dispEx) {
    return E_INVALIDARG;
  }

  // add this frame to WrappedDOMWindowManager
  ATLASSERT(aDOMWindowManager);
  mDOMWindowManager = aDOMWindowManager;
  mDOMWindowManager->putWrappedDOMWindow(aWebBrowser, this);

  // location-wrapper
  IF_FAILED_RET(HTMLLocationWrapper::createInstance(mDOMWindowInterfaces.w2, mLocation));

  // frames-wrapper
  IF_FAILED_RET(HTMLFramesCollection::createInstance(this, mDOMWindowInterfaces.w2, mFrames));

  // also create NULL properties for all events in getEventPropertyName
  mDOMEventProperties[DISPID_ONBEFOREUNLOAD].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONMESSAGE].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONBLUR].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONUNLOAD].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONHASHCHANGE].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONLOAD].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONSCROLL].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONAFTERPRINT].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONRESIZE].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONERROR].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONHELP].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONBEFOREPRINT].vt = VT_NULL;
  mDOMEventProperties[DISPID_ONFOCUS].vt = VT_NULL;

  return S_OK;
}

// -------------------------------------------------------------------------
// forceDelete
void DOMWindowWrapper::cleanup()
{
  if (mDOMWindowManager) {
    mDOMWindowManager->removeWrappedDOMWindow(mWebBrowser);
  }
  // *sigh* - jquery (and probably other scripts) leaks like Deepwater
  // Horizon on IE, so we don't get released. Let's at least release
  // everything we use in this place.
  mDOMWindowInterfaces.Release();
  mDOMWindowProperties.clear();
  mDOMWindowPropertyIDs.clear();
  mDOMWindowPropertyNames.clear();
  mDOMEventProperties.clear();
  mLocation.Release();
  if (mFrames) {
    mFrames->handsOffWrapper();
  }
  mFrames.Release();
  mWebBrowser.Release();
  mDOMWindowManager = NULL;
}

// ---------------------------------------------------------------------------
// FinalRelease
void DOMWindowWrapper::FinalRelease()
{
  cleanup();
}

// ---------------------------------------------------------------------------
// dispatchMethod
HRESULT DOMWindowWrapper::dispatchMethod(DISPID dispIdMember, REFIID riid,
                                         LCID lcid, DISPPARAMS *pDispParams,
                                         VARIANT *pVarResult,
                                         EXCEPINFO *pExcepInfo,
                                         IServiceProvider *pspCaller,
                                         BOOL & aHandled) const
{
  if (dispIdMember < DISPID_DOMWINDOW_EX_FIRST) {
    return S_OK;  // original window will handle
  }

  aHandled = TRUE;
  // we handle it
  MapDISPIDToCComVariant::const_iterator it
    = mDOMWindowProperties.find(dispIdMember);
  if (it != mDOMWindowProperties.end()) {
    if (it->second.vt != VT_DISPATCH) {
      return DISP_E_TYPEMISMATCH;
    }
    return it->second.pdispVal->Invoke(0, riid, lcid, DISPATCH_METHOD,
        pDispParams, pVarResult, pExcepInfo, NULL);
  }


  return DISP_E_MEMBERNOTFOUND;
}

// ---------------------------------------------------------------------------
// dispatchPropertyGet
HRESULT DOMWindowWrapper::dispatchPropertyGet(WORD wFlags, DISPID dispIdMember,
                                         REFIID riid,
                                         LCID lcid, DISPPARAMS *pDispParams,
                                         VARIANT *pVarResult,
                                         EXCEPINFO *pExcepInfo,
                                         IServiceProvider *pspCaller,
                                         BOOL & aHandled)
{
  ENSURE_RETVAL(pVarResult);
  VariantInit(pVarResult);
  MapDISPIDToCComVariant::const_iterator it = mDOMWindowProperties.end();
  BOOL propertyFound = FALSE;

  // Specially handled properties
  switch(dispIdMember) {
    // -----------------------------
    // window.location
    case DISPID_LOCATION: {
      aHandled = TRUE;
      CComVariant vt((IDispatch*)mLocation);
      if (VT_DISPATCH != vt.vt) {
        return DISP_E_MEMBERNOTFOUND;
      }
      return vt.Detach(pVarResult);
    }
    // -----------------------------
    // window.frames
    case DISPID_FRAMES: {
      aHandled = TRUE;
      CComVariant vt((IDispatch*)mFrames);
      if (VT_DISPATCH != vt.vt) {
        return DISP_E_MEMBERNOTFOUND;
      }
      return vt.Detach(pVarResult);
    }
    // -----------------------------
    // window.parent
    case DISPID_PARENT: {
      aHandled = TRUE;
      ATLASSERT(mDOMWindowInterfaces.w2);
      CComPtr<IHTMLWindow2> window;
      mDOMWindowInterfaces.w2->get_parent(&window.p);
      getRelatedWrappedWindow(window, pspCaller, pVarResult);
      return S_OK;
    }
    // -----------------------------
    // window.opener
    case DISPID_OPENER: {
      aHandled = TRUE;
      ATLASSERT(mDOMWindowInterfaces.w2);
      CComVariant opener;
      mDOMWindowInterfaces.w2->get_opener(&opener);
      if (VT_DISPATCH == opener.vt && opener.pdispVal) {
        CComQIPtr<IHTMLWindow2> window(opener.pdispVal);
        getRelatedWrappedWindow(window, pspCaller, pVarResult);
      }
      return S_OK;
    }
    // -----------------------------
    // window.self
    case DISPID_SELF: {
      pVarResult->pdispVal = this;
      AddRef();
      pVarResult->vt = VT_DISPATCH;
      return S_OK;
    }
    // -----------------------------
    // window.top
    case DISPID_TOP: {
      aHandled = TRUE;
      ATLASSERT(mDOMWindowInterfaces.w2);
      CComPtr<IHTMLWindow2> window;
      mDOMWindowInterfaces.w2->get_top(&window.p);
      getRelatedWrappedWindow(window, pspCaller, pVarResult);
      return S_OK;
    }
  }

  const wchar_t * eventName = getEventPropertyName(dispIdMember);
  if (eventName) {
    // if an event property is requested...
    it = mDOMEventProperties.find(dispIdMember);
    propertyFound = (it != mDOMEventProperties.end());
  }
  else {
    it = mDOMWindowProperties.find(dispIdMember);
    propertyFound = (it != mDOMWindowProperties.end());

    if (!propertyFound && dispIdMember < DISPID_DOMWINDOW_EX_FIRST) {
      // no event, stored property or an expando property: let original window handle it
      return S_OK;
    }
  }

  aHandled = TRUE;
  // we handle it
  if (propertyFound) {
    ATLASSERT(pVarResult);
    ::VariantCopy(pVarResult, &it->second);
    return S_OK;
  }
  return DISP_E_MEMBERNOTFOUND;
}

// ---------------------------------------------------------------------------
// dispatchPropertyPut
HRESULT DOMWindowWrapper::dispatchPropertyPut(WORD wFlags, DISPID dispIdMember,
                                         REFIID riid,
                                         LCID lcid, DISPPARAMS *pDispParams,
                                         VARIANT *pVarResult,
                                         EXCEPINFO *pExcepInfo,
                                         IServiceProvider *pspCaller,
                                         BOOL & aHandled)
{
  // The on.... properties store some event handlers that will not get called
  // if set on the wrapper. To work around this problem we use attachEvent
  // instead.
  const wchar_t * eventName = getEventPropertyName(dispIdMember);
  if (eventName) {
    // Yes, it is an event. Prepare arguments for detachEvent / attachEvent.
    CComVariant dispparamsVariants[] = {NULL, eventName};
    DISPPARAMS params = {dispparamsVariants, NULL, 2, 0};

    CIDispatchHelper script(mDOMWindowInterfaces.dispEx);

    // if we have a handler stored for this event remove the old event handler
    // first
    MapDISPIDToCComVariant::const_iterator it =
        mDOMEventProperties.find(dispIdMember);
    if (it != mDOMEventProperties.end() && VT_NULL != it->second.vt) {
      dispparamsVariants[0] = it->second;
      script.Call(L"detachEvent", &params);
    }

    // now attach new handler
    if (VT_DISPATCH == pDispParams->rgvarg[0].vt) {
      dispparamsVariants[0] = pDispParams->rgvarg[0];
      script.Call(L"attachEvent", &params);
    }
    // store the property
    mDOMEventProperties[dispIdMember] = pDispParams->rgvarg[0];
  }
  else {
    // store the property
    mDOMWindowProperties[dispIdMember] = pDispParams->rgvarg[0];
  }
  aHandled = TRUE;
  return S_OK;
}

// ---------------------------------------------------------------------------
// dispatchConstruct
HRESULT DOMWindowWrapper::dispatchConstruct(DISPID dispIdMember,
                                         REFIID riid,
                                         LCID lcid, DISPPARAMS *pDispParams,
                                         VARIANT *pVarResult,
                                         EXCEPINFO *pExcepInfo,
                                         IServiceProvider *pspCaller,
                                         BOOL & aHandled) const
{
  // do nothing, window will handle this
  return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// getRelatedWrappedWindow
HRESULT DOMWindowWrapper::getRelatedWrappedWindow(
    IHTMLWindow2      * aHTMLWindow,
    IServiceProvider  * pspCaller,
    VARIANT           * pVarResult)
{
  ENSURE_RETVAL(pVarResult)

  VariantInit(pVarResult);
  pVarResult->vt = VT_NULL;

  if(!aHTMLWindow || !mDOMWindowManager) {
    return E_INVALIDARG;
  }

  // QI related window to IServiceProvider.
  CComQIPtr<IServiceProvider> serviceProvider(aHTMLWindow);
  ATLASSERT(serviceProvider);

  // Get the webbrowser from the related window.
  CComPtr<IWebBrowser2> webBrowser;
  serviceProvider->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, (void**)&webBrowser.p);
  if (!webBrowser) {
    return E_NOINTERFACE;
  }

  // Get the wrapped window for this browser.
  CComPtr<IDispatch> wrappedWindow;
  HRESULT hr = mDOMWindowManager->getWrappedDOMWindow(mWebBrowser, webBrowser, &wrappedWindow.p);
  if (SUCCEEDED(hr)) {
    pVarResult->vt = VT_DISPATCH;
    pVarResult->pdispVal = wrappedWindow.Detach();
    return S_OK;
  }

  if (E_ACCESSDENIED == hr && pspCaller) {
    // Chrome returns in this case an object, but all properties are null.
    // Here they will be "undefined" instead of null, but that's acceptable.
    CComPtr<IActiveScriptSite> scriptSite;
    pspCaller->QueryService(IID_IActiveScriptSite, IID_IActiveScriptSite, (void**)&scriptSite.p);
    CComQIPtr<IMagpieObjectCreator> magpie(scriptSite);
    if (magpie) {
      hr = magpie->createObject(CComBSTR(L"Object"), &pVarResult->pdispVal);
      if (SUCCEEDED(hr)) {
        pVarResult->vt = VT_DISPATCH;
        return S_OK;
      }
    }
  }

  return hr;
}

// ---------------------------------------------------------------------------
// IDispatchEx methods

// ---------------------------------------------------------------------------
// GetDispID
STDMETHODIMP DOMWindowWrapper::GetDispID(BSTR bstrName, DWORD grfdex,
                                         DISPID *pid)
{
  if (!pid) {
    return E_POINTER;
  }

  // check if we already have this DISPID in our local map
  MapNameToDISPID::const_iterator it = mDOMWindowPropertyIDs.find(bstrName);
  if (it != mDOMWindowPropertyIDs.end()) {
    // yep!
    (*pid) = it->second;
    return S_OK;
  }

  // Don't have it, see if window knows the property and if it is a
  // non-expando property. Unset fdexNameEnsure because the window should
  // not create a property.

  // Check if window has a IHTMLWindow2,3.. property of this name. If so,
  // return that ID, so that things like document, navigator etc are still
  // callable, even if they are mapped to expando properties.
  // Basically this means: Every property that exists in IHTMLWindowN,
  // we map back to its original.
  HRESULT hr = mDOMWindowInterfaces.getDispID(bstrName, *pid);
  if (S_OK == hr && (*pid) < DISPID_DOMWINDOW_EX_FIRST) {
    // yep!
    return hr;
  }
  if (!(grfdex & fdexNameEnsure)) {
    // should not create the property, so just return error
    return DISP_E_UNKNOWNNAME;
  }

  // the property should be created
  hr = mDOMWindowInterfaces.dispEx->GetDispID(bstrName, fdexNameCaseSensitive | fdexNameEnsure, pid);
  if (FAILED(hr)) {
    return hr;
  }
  mDOMWindowPropertyIDs[bstrName] = *pid;
  mDOMWindowPropertyNames[*pid] = bstrName;
  mDOMWindowProperties[*pid].vt = VT_EMPTY;
  return S_OK;
}

// ---------------------------------------------------------------------------
// InvokeEx
STDMETHODIMP DOMWindowWrapper::InvokeEx(DISPID id, LCID lcid, WORD wFlags,
                                        DISPPARAMS *pdp, VARIANT *pvarRes,
                                        EXCEPINFO *pei,
                                        IServiceProvider *pspCaller)
{
  BOOL handled = FALSE;
  HRESULT hrRet = E_INVALIDARG;
  switch(wFlags) {
    case DISPATCH_METHOD:
      hrRet = dispatchMethod(id, IID_NULL, lcid, pdp, pvarRes, pei, pspCaller,
          handled);
      break;
    case DISPATCH_PROPERTYGET:
      hrRet = dispatchPropertyGet(wFlags, id, IID_NULL, lcid, pdp, pvarRes,
          pei, pspCaller, handled);
      break;
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms221486%28v=vs.85%29.aspx :
      // "Some languages cannot distinguish between retrieving a property and
      // calling a method. In this case, you should set the flags
      // DISPATCH_PROPERTYGET and DISPATCH_METHOD".
      // This means, we have to handle both cases depending on the actual type.
      // If it is callable call it, otherwise return the property
      hrRet = dispatchMethod(id, IID_NULL, lcid, pdp, pvarRes, pei, pspCaller,
          handled);
      if (DISP_E_TYPEMISMATCH == hrRet) {
        hrRet = dispatchPropertyGet(wFlags, id, IID_NULL, lcid, pdp, pvarRes,
            pei, pspCaller, handled);
      }
      break;
    case DISPATCH_PROPERTYPUT:
    case DISPATCH_PROPERTYPUTREF:
    case DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF:
      hrRet = dispatchPropertyPut(wFlags, id, IID_NULL, lcid, pdp, pvarRes,
          pei, pspCaller, handled);
      break;
    case DISPATCH_CONSTRUCT:
      hrRet = dispatchConstruct(id, IID_NULL, lcid, pdp, pvarRes, pei,
          pspCaller, handled);
      break;
    default:
      ATLTRACE(_T("DOMWindowWrapper::InvokeEx ************* Unknown Flag 0x%08x\n"), wFlags);
      ATLASSERT(0 && "Ooops - did we forget to handle something?");
      return E_INVALIDARG;
      break;
  }
  if (handled) {
    return hrRet;
  }

  return mDOMWindowInterfaces.dispEx->InvokeEx(id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);
}

// ---------------------------------------------------------------------------
// DeleteMemberByName
STDMETHODIMP DOMWindowWrapper::DeleteMemberByName(BSTR bstrName,DWORD grfdex)
{
  // Wow, who would have guessed - IE does not support deleting properties on
  // window!
  // This means in turn: We don't have to do anything here, just let window
  // return an error.
  return mDOMWindowInterfaces.dispEx->DeleteMemberByName(bstrName, grfdex);
}

// ---------------------------------------------------------------------------
// DeleteMemberByDispID
STDMETHODIMP DOMWindowWrapper::DeleteMemberByDispID(DISPID id)
{
  // see DeleteMemberByName
  return mDOMWindowInterfaces.dispEx->DeleteMemberByDispID(id);
}

// ---------------------------------------------------------------------------
// GetMemberProperties
STDMETHODIMP DOMWindowWrapper::GetMemberProperties(DISPID id, DWORD
                                                   grfdexFetch, DWORD *pgrfdex)
{
  // for now it looks like we don't need this
  ATLASSERT(0 && "Seems we have to implement this");
  return mDOMWindowInterfaces.dispEx->GetMemberProperties(id, grfdexFetch, pgrfdex);
}

// ---------------------------------------------------------------------------
// GetMemberName
STDMETHODIMP DOMWindowWrapper::GetMemberName(DISPID id, BSTR *pbstrName)
{
  HRESULT hr = mDOMWindowInterfaces.dispEx->GetMemberName(id, pbstrName);
  if (S_OK == hr) {
    return hr;
  }
  // hm.. maybe we know the name?
  MapDISPIDToName::const_iterator it = mDOMWindowPropertyNames.find(id);
  if (it != mDOMWindowPropertyNames.end()) {
    it->second.CopyTo(pbstrName);
    return S_OK;
  }
  return hr;
}

// ---------------------------------------------------------------------------
// GetNextDispID
STDMETHODIMP DOMWindowWrapper::GetNextDispID(DWORD grfdex, DISPID id,
                                             DISPID *pid)
{
  HRESULT hr = E_FAIL;
  MapDISPIDToCComVariant::const_iterator it = mDOMWindowProperties.end();
  if (id < DISPID_DOMWINDOW_EX_FIRST) {
    // The previous property is still within the range of non-expando
    // properties, so let's ask the window for the next id
    hr = mDOMWindowInterfaces.dispEx->GetNextDispID(grfdex, id, pid);
    if (S_OK == hr && (*pid) < DISPID_DOMWINDOW_EX_FIRST) {
      // a property is found AND is also in the non-expando range, so we are done
      return hr;
    }
    // Window has no more properties OR the property found is an expando
    // property, so continue with our own properties.
    // The previous id is < DISPID_DOMWINDOW_EX_FIRST, so get our first id.
    if (mDOMWindowProperties.empty()) {
      // well, we don't have any
      return S_FALSE;
    }
    // set iterator to begin
    it = mDOMWindowProperties.begin();
  }
  else {
    // The previous id is >= DISPID_DOMWINDOW_EX_FIRST, so get the next id
    // from our own list of properties.
    it = mDOMWindowProperties.find(id);
    if (it == mDOMWindowProperties.end()) {
      // not found - this should not happen
      ATLASSERT(0);
      return S_FALSE;
    }
    // set iterator to next
    ++it;
  }
  if (it == mDOMWindowProperties.end()) {
    // nothing any more
    return S_FALSE;
  }
  // now we have the next id
  (*pid) = it->first;
  return S_OK;
}

// ---------------------------------------------------------------------------
// GetNameSpaceParent
STDMETHODIMP DOMWindowWrapper::GetNameSpaceParent(IUnknown **ppunk)
{
  return mDOMWindowInterfaces.dispEx->GetNameSpaceParent(ppunk);
}

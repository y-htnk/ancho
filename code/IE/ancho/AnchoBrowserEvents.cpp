/****************************************************************************
 * AnchoBrowserEvents.cpp : Implementation of CAnchoBrowserEvents
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Matthew Gertner <matthew@salsitasoft.com>
 ****************************************************************************/

#include "stdafx.h"
#include "AnchoBrowserEvents.h"

/*============================================================================
 * class CAnchoBrowserEvents
 */

//----------------------------------------------------------------------------
//  OnFrameStart
STDMETHODIMP CAnchoBrowserEvents::OnFrameStart(IWebBrowser2 * aBrowser, BSTR bstrUrl, VARIANT_BOOL bIsMainFrame)
{
  CComVariant vt[] = { (VARIANT_BOOL) (bIsMainFrame ? VARIANT_TRUE : VARIANT_FALSE), bstrUrl, aBrowser };
  DISPPARAMS disp = { vt, NULL, 3, 0 };
  return FireEvent(EID_ONFRAMESTART, &disp, 3);
}

//----------------------------------------------------------------------------
//  OnFrameEnd
STDMETHODIMP CAnchoBrowserEvents::OnFrameEnd(IWebBrowser2 * aBrowser, BSTR bstrUrl, VARIANT_BOOL bIsMainFrame)
{
  CComVariant vt[] = { (VARIANT_BOOL) (bIsMainFrame ? VARIANT_TRUE : VARIANT_FALSE), bstrUrl, aBrowser };
  DISPPARAMS disp = { vt, NULL, 3, 0 };
  return FireEvent(EID_ONFRAMEEND, &disp, 3);
}

//----------------------------------------------------------------------------
//  OnFrameRedirect
STDMETHODIMP CAnchoBrowserEvents::OnFrameRedirect(IWebBrowser2 * aBrowser, BSTR bstrOldUrl, BSTR bstrNewUrl)
{
  CComVariant vt[] = { bstrNewUrl, bstrOldUrl, aBrowser };
  DISPPARAMS disp = { vt, NULL, 3, 0 };
  return FireEvent(EID_ONFRAMEREDIRECT, &disp, 3);
}

//----------------------------------------------------------------------------
//  OnBeforeRequest
STDMETHODIMP CAnchoBrowserEvents::OnBeforeRequest(VARIANT aReporter)
{
  CComVariant vt[] = { CComVariant(aReporter) };
  DISPPARAMS disp = { vt, NULL, 1, 0 };
  return FireEvent(EID_ONBEFOREREQUEST, &disp, 1);
}

//----------------------------------------------------------------------------
//  OnBeforeSendHeaders
STDMETHODIMP CAnchoBrowserEvents::OnBeforeSendHeaders(VARIANT aReporter)
{
  CComVariant vt[] = { CComVariant(aReporter) };
  DISPPARAMS disp = { vt, NULL, 1, 0 };
  return FireEvent(EID_ONBEFORESENDHEADERS, &disp, 1);
}

//----------------------------------------------------------------------------
//  FireDocumentEvent
HRESULT CAnchoBrowserEvents::FireEvent(DISPID dispid, DISPPARAMS* disp, unsigned int count)
{
  int nConnectionIndex;
  int nConnections = m_vec.GetSize();

  HRESULT hr = S_OK;
  Lock();
  for (nConnectionIndex = 0; nConnectionIndex < nConnections; nConnectionIndex++)
  {
    CComQIPtr<IDispatch> pDispatch = m_vec.GetAt(nConnectionIndex);
    if (pDispatch != NULL) {
      pDispatch->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, disp, NULL, NULL, NULL);
    }
  }
  Unlock();
  return hr;
}

HRESULT WebRequestReporter::createReporter(LPCWSTR aUrl, LPCWSTR aMethod, LPDISPATCH aBrowser, IWebRequestReporter ** aReporterRetVal)
{
  ENSURE_RETVAL(aReporterRetVal);
  (*aReporterRetVal) = NULL;
  WebRequestReporterComObject * reporter = NULL;
  IF_FAILED_RET(WebRequestReporterComObject::CreateInstance(&reporter));

  CComQIPtr<IWebRequestReporter> owner(reporter);
  reporter->mUrl = aUrl;
  reporter->mHTTPMethod = aMethod;
  reporter->mCurrentBrowser = aBrowser;

  (*aReporterRetVal) = owner.Detach();
  return S_OK;
}

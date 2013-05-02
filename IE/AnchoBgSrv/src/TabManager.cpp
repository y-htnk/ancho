#include "stdafx.h"
#include "AnchoBackgroundServer/TabManager.hpp"
#include <Exceptions.h>
#include <SimpleWrappers.h>
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"
#include "AnchoBackgroundServer/COMConversions.hpp"
#include "AnchoBackgroundServer/JavaScriptCallback.hpp"

namespace AnchoBackgroundServer {

namespace utils {
std::wstring getLastError(HRESULT hr)
{
    LPWSTR lpMsgBuf;
    DWORD ret;
    std::wstring def(L"(UNNKOWN)");
    ret = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_HMODULE,
        GetModuleHandle(TEXT("imapi2.dll")),
        hr,
        0,
        (LPWSTR) &lpMsgBuf,
        0, NULL );

    if(ret)
    {
        std::wstring last(lpMsgBuf);
        LocalFree(lpMsgBuf);
        return last;
    }
    return def;
}


BOOL CALLBACK enumBrowserWindows(HWND hwnd, LPARAM lParam)
{
  wchar_t className[MAX_PATH];
  ::GetClassName(hwnd, className, MAX_PATH);
  try {
    if (wcscmp(className, L"Internet Explorer_Server") == 0) {
      // Now we need to get the IWebBrowser2 from the window.
      DWORD dwMsg = ::RegisterWindowMessage(L"WM_HTML_GETOBJECT");
      LRESULT lResult = 0;
      ::SendMessageTimeout(hwnd, dwMsg, 0, 0, SMTO_ABORTIFHUNG, 1000, (DWORD_PTR*) &lResult);
      if (!lResult) {
        return TRUE;
      }
      CComPtr<IHTMLDocument2> doc;
      IF_FAILED_THROW(::ObjectFromLresult(lResult, IID_IHTMLDocument2, 0, (void**) &doc))

      CComPtr<IHTMLWindow2> win;
      IF_FAILED_THROW(doc->get_parentWindow(&win));

      CComQIPtr<IServiceProvider> sp(win);
      if (!sp) { return TRUE; }

      CComPtr<IWebBrowser2> pWebBrowser;
      IF_FAILED_THROW(sp->QueryService(IID_IWebBrowserApp, IID_IWebBrowser2, (void**) &pWebBrowser));

      CComPtr<IDispatch> container;
      IF_FAILED_THROW(pWebBrowser->get_Container(&container));
      // IWebBrowser2 doesn't have a container if it is an IE tab, so if we have a container
      // then we must be an embedded web browser (e.g. in an HTML toolbar).
      if (container) { return TRUE; }

      // Now get the HWND associated with the tab so we can see if it is active.
      sp = pWebBrowser;
      if (!sp) { return TRUE; }

      CComPtr<IOleWindow> oleWindow;
      IF_FAILED_THROW(sp->QueryService(SID_SShellBrowser, IID_IOleWindow, (void**) &oleWindow));
      HWND hTab;
      IF_FAILED_THROW(oleWindow->GetWindow(&hTab));
      if (::IsWindowEnabled(hTab)) {
        // Success, we found the active browser!
        pWebBrowser.CopyTo((IWebBrowser2 **) lParam);
        return FALSE;
      }
    }
  } catch (std::exception &) {
    return TRUE;
  }
  return TRUE;
}


CComPtr<IWebBrowser2> findActiveBrowser()
{
  CComQIPtr<IWebBrowser2> browser;
  // First find the IE frame windows.
  HWND hIEFrame = NULL;
  do {
    hIEFrame = ::FindWindowEx(NULL, hIEFrame, L"IEFrame", NULL);
    if (hIEFrame) {
      BOOL enable = ::IsWindowEnabled(hIEFrame);
      // Now we enumerate the child windows to find the "Internet Explorer_Server".
      ::EnumChildWindows(hIEFrame, enumBrowserWindows, (LPARAM) (&browser));
      if (browser) {
        return browser;
      }
    }
  }while(hIEFrame);

  // Oops, for some reason we didn't find it.
  return CComPtr<IWebBrowser2>();
}


} //namespace utils


typedef AnchoBackgroundServer::JSVariant TabInfo;

struct CreateTabTask
{
  CreateTabTask(const JSObject &aProperties,
                const JavaScriptCallback<TabInfo, void>& aCallback,
                int aApiId)
                : mProperties(aProperties), mCallback(aCallback), mApiId(aApiId)
  { /*empty*/ }

  void operator()()
  {
    try {
      CComPtr<IWebBrowser2> browser = utils::findActiveBrowser();

      if (!browser) {
        //Problem - no browser available
        return;
      }

      JSVariant url = mProperties[L"url"];
      std::wstring tmpUrl = (url.which() == jsString) ? boost::get<std::wstring>(url) : L"about:blank";

      int requestID = 1;//mNextRequestID++;
      boost::wregex expression(L"(.*)://(.*)");
      boost::wsmatch what;
      if (boost::regex_match(tmpUrl, what, expression)) {
        tmpUrl = boost::str(boost::wformat(L"%1%://$$%2%$$%3%") % what[1] % requestID % what[2]);
      }

      _variant_t vtUrl = tmpUrl.c_str();
      _variant_t vtFlags((long)navOpenInNewTab, VT_I4);
      _variant_t vtTargetFrameName;
      _variant_t vtPostData;
      _variant_t vtHeaders;

      IF_FAILED_THROW(browser->Navigate2(
                                  &vtUrl.GetVARIANT(),
                                  &vtFlags.GetVARIANT(),
                                  &vtTargetFrameName.GetVARIANT(),
                                  &vtPostData.GetVARIANT(),
                                  &vtHeaders.GetVARIANT())
                                  );
    } catch (EHResult &e) {
      ATLTRACE(L"Error in create tab task: %s\n", utils::getLastError(e.mHResult).c_str());
    } catch (std::exception &e) {
      ATLTRACE(L"Error in create tab task\n");
    }

  }

  JSObject mProperties;
  JavaScriptCallback<TabInfo, void> mCallback;
  int mApiId;
};


STDMETHODIMP TabManager::createTab(LPDISPATCH aProperties, LPDISPATCH aCallback, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  CComQIPtr<IDispatchEx> tmp(aProperties);
  if (!tmp) {
    return E_FAIL;
  }

  AnchoBackgroundServer::JSVariant properties = AnchoBackgroundServer::convertToJSVariant(*tmp);
  AnchoBackgroundServer::JavaScriptCallback<TabInfo, void> callback(aCallback);

  mAsyncTaskManager.addTask(CreateTabTask(boost::get<JSObject>(properties), callback, aApiId));
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}

} // namespace AnchoBackgroundServer
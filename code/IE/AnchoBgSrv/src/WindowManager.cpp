#include "stdafx.h"
#include "AnchoBackgroundServer/WindowManager.hpp"
#include "AnchoBackgroundServer/TabManager.hpp"
#include <Exceptions.h>
#include <SimpleWrappers.h>
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"
#include "AnchoBackgroundServer/COMConversions.hpp"
#include "AnchoBackgroundServer/JavaScriptCallback.hpp"

namespace Ancho {
namespace Utils {

bool isIEWindow(HWND aHwnd)
{
  wchar_t className[256];
  return GetClassName(aHwnd, className, 256) && (std::wstring(L"IEFrame") == className);
}

} //namespace Utils

namespace Service {

CComObject<Ancho::Service::WindowManager> *gWindowManager = NULL;

//==========================================================================================
struct CreateWindowTask
{
  CreateWindowTask(const Utils::JSObject &aCreateData,
                const TabCallback& aCallback,
                const std::wstring &aExtensionId,
                int aApiId)
                : mCreateData(aCreateData), mCallback(aCallback), mExtensionId(aExtensionId), mApiId(aApiId)
  { /*empty*/ }

  void operator()()
  {
    /* TODO - handle these parameters:
    tabId ( optional integer )

    left ( optional integer )

    top ( optional integer )

    width ( optional integer )

    height ( optional integer )

    focused ( optional boolean )

    incognito ( optional boolean )

    type ( optional enumerated string ["normal", "popup", "panel", "detached_panel"] ) */

    Utils::JSObject properties;
    properties[L"windowId"] = WINDOW_ID_NON_EXISTENT;
    properties[L"url"] = mCreateData[L"url"];

    auto helperCallback = [=](TabInfo aInfo) {
      Utils::JSObjectWrapper info = Utils::JSValueWrapper(aInfo).toObject();

      WindowId windowId = info[L"windowId"].toInt();

      WindowManager::instance().getWindow(windowId, false, mCallback, mExtensionId, mApiId);
    };

    TabManager::instance().createTab(properties, helperCallback, mExtensionId, mApiId);
  }

  Utils::JSObject mCreateData;
  TabCallback mCallback;
  std::wstring mExtensionId;
  int mApiId;
};

//==========================================================================================
//              API methods
//==========================================================================================
STDMETHODIMP WindowManager::createWindow(LPDISPATCH aCreateData, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  CComQIPtr<IDispatchEx> tmp(aCreateData);
  if (!tmp) {
    return E_FAIL;
  }

  Utils::JSVariant createData = Utils::convertToJSVariant(*tmp);
  Utils::JavaScriptCallback<WindowInfo, void> callback(aCallback);

  createWindow(boost::get<Utils::JSObject>(createData), callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}

void WindowManager::createWindow(const Utils::JSObject &aCreateData,
                 const WindowCallback& aCallback,
                 const std::wstring &aExtensionId,
                 int aApiId)
{
  mAsyncTaskManager.addTask(CreateWindowTask(aCreateData, aCallback, aExtensionId, aApiId));
}
//==========================================================================================
STDMETHODIMP WindowManager::getWindows(LPDISPATCH aGetInfo, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  return S_OK;
}
//==========================================================================================
STDMETHODIMP WindowManager::updateWindow(LONG windowId, LPDISPATCH aUpdateInfo, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  CComQIPtr<IDispatchEx> tmp(aUpdateInfo);
  if (!tmp) {
    return E_FAIL;
  }

  Utils::JSVariant updateInfo = Utils::convertToJSVariant(*tmp);
  Utils::JavaScriptCallback<WindowInfo, void> callback(aCallback);

  updateWindow(windowId, boost::get<Utils::JSObject>(updateInfo), callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}

void WindowManager::updateWindow(
                 WindowId aWindowId,
                 const Utils::JSObject &aUpdateInfo,
                 const WindowCallback& aCallback,
                 const std::wstring &aExtensionId,
                 int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    HWND win = getHandleFromWindowId(aWindowId);
    updateWindowImpl(win, aUpdateInfo);

    getWindow(aWindowId,
              false,
              aCallback,
              aExtensionId,
              aApiId);
  });
}
//==========================================================================================
STDMETHODIMP WindowManager::removeWindow(LONG aWindowId, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId)
{
  BEGIN_TRY_BLOCK;
  if (aExtensionId == NULL) {
    return E_INVALIDARG;
  }
  Utils::JavaScriptCallback<void, void> callback(aCallback);

  removeWindow(aWindowId, callback, std::wstring(aExtensionId), aApiId);
  END_TRY_BLOCK_CATCH_TO_HRESULT;
  return S_OK;
}

void WindowManager::removeWindow(
                 WindowId aWindowId,
                 const SimpleCallback& aCallback,
                 const std::wstring &aExtensionId,
                 int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    HWND win = getHandleFromWindowId(aWindowId);
    if( !::DestroyWindow(win) ) {
      ANCHO_THROW(EFail());
    }
    if (aCallback) {
      aCallback();
    }
  });
}
//==========================================================================================
void WindowManager::getWindow(
                WindowId aWindowId,
                bool aPopulate,
                const WindowCallback& aCallback,
                const std::wstring &aExtensionId,
                int aApiId)
{
  mAsyncTaskManager.addTask([=, this](){
    HWND win = getHandleFromWindowId(aWindowId);
    CComPtr<IDispatch> info = TabManager::instance().createObject(aExtensionId, aApiId);
    //TODO - populate
    WindowManager::instance().fillWindowInfo(win, Utils::JSValueWrapper(info).toObject());
    aCallback(info);
  });
}
//==========================================================================================
HWND WindowManager::getHandleFromWindowId(WindowId aWindowId)
{
  boost::unique_lock<boost::mutex> lock(mWindowAccessMutex);
  auto it = mWindows.find(aWindowId);
  if (it == mWindows.end()) {
    ANCHO_THROW(EFail());
  }

  return it->second->getHWND();
}
//==========================================================================================
WindowId WindowManager::getWindowIdFromHWND(HWND aHWND)
{
  boost::unique_lock<boost::mutex> lock(mWindowAccessMutex);
  auto it = mWindowIds.find(aHWND);
  if (it == mWindowIds.end()) {
    return createNewWindowRecord(aHWND);
  }

  return it->second;
}

//==========================================================================================
WindowId WindowManager::createNewWindowRecord(HWND aHWND)
{
  boost::unique_lock<boost::mutex> lock(mWindowAccessMutex);
  WindowId id = mWindowIdGenerator.next();

  mWindowIds[aHWND] = id;
  mWindows[id] = boost::make_shared<WindowRecord>(aHWND);
  return id;
}
//==========================================================================================
void WindowManager::fillWindowInfo(HWND aWndHandle, Utils::JSObjectWrapper aInfo)
{
  //BOOL isVisible = IsWindowVisible(aWndHandle);
  WINDOWINFO winInfo;
  winInfo.cbSize = sizeof(WINDOWINFO);
  BOOL res = GetWindowInfo(aWndHandle, &winInfo);
  aInfo[L"top"] = winInfo.rcWindow.top;
  aInfo[L"left"] = winInfo.rcWindow.left;
  aInfo[L"width"] = winInfo.rcWindow.right - winInfo.rcWindow.left;
  aInfo[L"height"] = winInfo.rcWindow.bottom - winInfo.rcWindow.top;
  aInfo[L"focused"] = static_cast<bool>(winInfo.dwWindowStatus & WS_ACTIVECAPTION);
  aInfo[L"alwaysOnTop"] = false;
  aInfo[L"id"] = getWindowIdFromHWND(aWndHandle);
  std::wstring state = L"normal";
  if (IsIconic(aWndHandle)) {
    state = L"minimized";
  } else if (IsZoomed(aWndHandle)) {
    state = L"maximized";
  }
  aInfo[L"state"] = state;
}
//==========================================================================================
void WindowManager::updateWindowImpl(HWND aWndHandle, Utils::JSObject aInfo)
{
  if (!Utils::isIEWindow(aWndHandle)) {
    ANCHO_THROW(EFail());
  }
  WINDOWINFO winInfo;
  winInfo.cbSize = sizeof(WINDOWINFO);
  BOOL res = GetWindowInfo(aWndHandle, &winInfo);
  static const wchar_t* moveParamsNames[] = {L"left", L"top", L"width", L"height"};
  int moveParams[4];
  moveParams[0] = winInfo.rcWindow.left;
  moveParams[1] = winInfo.rcWindow.top;
  moveParams[2] = winInfo.rcWindow.right - winInfo.rcWindow.left;
  moveParams[3] = winInfo.rcWindow.bottom - winInfo.rcWindow.top;
  bool shouldMove = false;

  for (size_t i = 0; i < 4; ++i) {
    if (aInfo[moveParamsNames[i]].which() == Utils::jsInt) {
      moveParams[i] = boost::get<int>(aInfo[moveParamsNames[i]]);
      shouldMove = true;
    }
  }
  if (shouldMove) {
    ::MoveWindow(aWndHandle, moveParams[0], moveParams[1], moveParams[2], moveParams[3], TRUE);
  }
  bool focused = false;

  if (aInfo[L"focused"].which() == Utils::jsBool && (focused = boost::get<bool>(aInfo[L"focused"]))) {
    if(focused) {
      ::SetForegroundWindow(aWndHandle);
    } else {
      //Bring to foreground next IE window
      HWND nextWin = aWndHandle;
      while (nextWin = GetNextWindow(nextWin, GW_HWNDNEXT)) {
        if (Utils::isIEWindow(nextWin)) {
          ::SetForegroundWindow(nextWin);
          break;
        }
      }
    }
  }

  if (aInfo[L"state"].which() == Utils::jsString) {
    std::wstring state = boost::get<std::wstring>(aInfo[L"state"]);
    if (state == L"maximized") {
      ::ShowWindow(aWndHandle, SW_MAXIMIZE);
    } else if (state == L"minimized") {
      ::ShowWindow(aWndHandle, SW_MINIMIZE);
    } else if (state == L"normal") {
      ::ShowWindow(aWndHandle, SW_NORMAL);
    } else if (state == L"fullscreen") {
      //TODO - fullscreen
    }
  }
  //TODO - drawAttention
}

} //namespace Service
} //namespace Ancho

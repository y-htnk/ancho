#pragma once
#include "resource.h"
#include "AnchoBgSrv_i.h"
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"
#include "AnchoBackgroundServer/COMConversions.hpp"
#include "AnchoBackgroundServer/JavaScriptCallback.hpp"
#include "AnchoBackgroundServer/PeriodicTimer.hpp"
#include <IPCHeartbeat.h>
#include <Exceptions.h>

namespace Ancho {
namespace Utils {

/**
 * \tparam TId must be integral type
 **/
template<typename TId = int>
class IdGenerator
{
public:
  IdGenerator(TId aInitValue = 1): mNextValue(aInitValue)
  { /*empty*/ }

  TId next()
  {
    return mNextValue.fetch_add(1, boost::memory_order_relaxed);
  }
protected:
  boost::atomic<TId> mNextValue;
};
}
namespace Service {

struct ENotValidTabId : EAnchoException {};


typedef CComPtr<ComSimpleJSObject> TabInfo;
typedef CComPtr<ComSimpleJSArray> TabInfoList;
typedef int TabId;

typedef boost::function<void(TabInfo)> TabCallback;
typedef boost::function<void(TabInfoList)> TabListCallback;
typedef boost::function<void(void)> SimpleCallback;

class TabManager;
extern CComObject<Ancho::Service::TabManager> *gTabManager;

class TabManager:
  public CComObjectRootEx<CComMultiThreadModel>,
  public IAnchoTabManagerInternal,
  public IDispatchImpl<ITabManager, &IID_ITabManager, &LIBID_AnchoBgSrvLib, /*wMajor =*/ 0xffff, /*wMinor =*/ 0xffff>
{
public:
  friend struct CreateTabTask;
  friend struct ReloadTabTask;
  friend struct GetTabTask;
  friend struct UpdateTabTask;
  friend struct QueryTabsTask;
  friend struct RemoveTabsTask;

  class TabRecord;

  TabManager()
  {
  }

public:
  ///@{
  /** Interface methods available to JS.**/
  STDMETHOD(createTab)(LPDISPATCH aProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(reloadTab)(INT aTabId, LPDISPATCH aReloadProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(queryTabs)(LPDISPATCH aProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(updateTab)(INT aTabId, LPDISPATCH aUpdateProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(removeTabs)(LPDISPATCH aTabs, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  ///@}

  TabId getFrameTabId(HWND aFrameTab);

  template<typename TCallable>
  TCallable forEachTab(TCallable aCallable)
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    TabMap::iterator it = mTabs.begin();
    while (it != mTabs.end()) {
      ATLASSERT(it->second);
      aCallable(*it->second);
      ++it;
    }
    return aCallable;
  }

  template<typename TContainer, typename TCallable>
  TContainer forTabsInList(const TContainer &aTabIds, TCallable aCallable)
  {
    //Create list of tabs which don't exist
    TContainer missed;
    boost::unique_lock<boost::mutex> lock(mMutex);

    BOOST_FOREACH(auto tabId, aTabIds) {
      TabMap::iterator it = mTabs.find(tabId);
      if (it != mTabs.end()) {
        ATLASSERT(it->second);
        aCallable(*it->second);
        ++it;
      } else {
        missed.insert(missed.end(), tabId);
      }
    }
    return missed;
  }

  static void initSingleton()
  {
    ATLASSERT(gTabManager == NULL);
    CComObject<Ancho::Service::TabManager> *tabManager = NULL;
    IF_FAILED_THROW(CComObject<Ancho::Service::TabManager>::CreateInstance(&tabManager));
    gTabManager = tabManager;
  }

  static CComObject<Ancho::Service::TabManager> & instance()
  {
    ATLASSERT(gTabManager != NULL);
    return *gTabManager;
  }
public:
  void createTab(const Utils::JSObject &aProperties,
                 const TabCallback& aCallback = TabCallback(),
                 const std::wstring &aExtensionId = std::wstring(),
                 int aApiId = -1);

  void getTab(TabId aTabId,
              const TabCallback& aCallback = TabCallback(),
              const std::wstring &aExtensionId = std::wstring(),
              int aApiId = -1);

  void queryTabs(const Utils::JSObject &aProperties,
                 const TabListCallback& aCallback = TabCallback(),
                 const std::wstring &aExtensionId = std::wstring(),
                 int aApiId = -1);

  void removeTabs(const std::vector<TabId> &aTabs,
                 const SimpleCallback& aCallback = SimpleCallback(),
                 const std::wstring &aExtensionId = std::wstring(),
                 int aApiId = -1);
public:
  // -------------------------------------------------------------------------
  // COM standard stuff
  DECLARE_NO_REGISTRY()
  DECLARE_NOT_AGGREGATABLE(Ancho::Service::TabManager)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct()
  {
    BEGIN_TRY_BLOCK;
    mHeartbeatTimer.initialize(boost::bind(&TabManager::checkBHOConnections, this), 1000);
    mHeartbeatActive = false;
    END_TRY_BLOCK_CATCH_TO_HRESULT;
    return S_OK;
  }
  void FinalRelease()
  {

  }

public:
  STDMETHOD(registerRuntime)(OLE_HANDLE aFrameTab, IAnchoRuntime * aRuntime, ULONG aHeartBeat, INT *aTabID);
  STDMETHOD(unregisterRuntime)(INT aTabID);
  STDMETHOD(createTabNotification)(INT aTabId, ULONG aRequestID);
public:
  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(Ancho::Service::TabManager)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(ITabManager)
    COM_INTERFACE_ENTRY(IAnchoTabManagerInternal)
  END_COM_MAP()


protected:
  struct CreateTabCallbackRequestInfo
  {
    TabCallback callback;
    std::wstring extensionId;
    TabId apiId;
  };
  typedef std::map<ULONG, CreateTabCallbackRequestInfo> CreateTabCallbackMap;

  void checkBHOConnections();

  void addCreateTabCallbackInfo(ULONG aRequestId, CreateTabCallbackRequestInfo aInfo)
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    mCreateTabCallbacks[aRequestId] = aInfo;
  }

  boost::shared_ptr<TabRecord> getTabRecord(TabId aTabId)
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    TabMap::iterator it = mTabs.find(aTabId);
    if (it == mTabs.end()) {
      ANCHO_THROW(ENotValidTabId());
    }
    ATLASSERT(it->second);
    return it->second;
  }

  Ancho::Utils::AsynchronousTaskManager mAsyncTaskManager;

  Utils::IdGenerator<ULONG> mRequestIdGenerator;

  Utils::IdGenerator<TabId> mTabIdGenerator;
  typedef std::map<HWND, TabId> FrameTabToTabIDMap;
  FrameTabToTabIDMap mFrameTabIds;

  typedef std::map<TabId, boost::shared_ptr<TabRecord> > TabMap;
  TabMap mTabs;

  CreateTabCallbackMap mCreateTabCallbacks;
  Ancho::Utils::PeriodicTimer mHeartbeatTimer;
  boost::atomic<bool> mHeartbeatActive;
  boost::mutex mMutex;
};

//============================================================================================

class TabManager::TabRecord
{
public:
  typedef boost::shared_ptr<TabRecord> Ptr;

  TabRecord(CComPtr<IAnchoRuntime> aRuntime = CComPtr<IAnchoRuntime>(), TabId aTabId = 0, ULONG aHeartBeat = 0)
    : mRuntimeMarshaler(aRuntime), mTabId(aTabId), mHearbeatMaster(aHeartBeat)
  { /*empty*/ }

  CComPtr<IAnchoRuntime> runtime()
  { return mRuntimeMarshaler.get(); }

  bool isAlive()
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    CComPtr<IUnknown> runtimeInstance = runtime();
    return ::CoIsHandlerConnected(runtimeInstance.p) != FALSE;
  }

  void tabClosed()
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    std::for_each(mOnRemoveCallbacks.begin(), mOnRemoveCallbacks.end(), [](SimpleCallback aCallback){ aCallback(); });
    mOnRemoveCallbacks.clear();
  }

  void tabReloaded()
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    std::for_each(mOnReloadCallbacks.begin(), mOnReloadCallbacks.end(), [](SimpleCallback aCallback){ aCallback(); });
    mOnReloadCallbacks.clear();
  }

  void addOnReloadCallback(SimpleCallback aCallback)
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    mOnReloadCallbacks.push_back(aCallback);
  }

  void addOnCloseCallback(SimpleCallback aCallback)
  {
    boost::unique_lock<boost::mutex> lock(mMutex);
    mOnRemoveCallbacks.push_back(aCallback);
  }

  TabId tabId()const
  {
    return mTabId;
  }

  //safe access in different threads
  Ancho::Utils::ObjectMarshaller<IAnchoRuntime> mRuntimeMarshaler;

  TabId mTabId;

  mutable boost::mutex mMutex;
  HeartbeatMaster mHearbeatMaster;

  std::list<SimpleCallback> mOnReloadCallbacks;
  std::list<SimpleCallback> mOnRemoveCallbacks;

};


} //namespace Service
} //namespace Ancho
#pragma once
#include "resource.h"
#include "AnchoBgSrv_i.h"
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"
#include "AnchoBackgroundServer/COMConversions.hpp"
#include "AnchoBackgroundServer/JavaScriptCallback.hpp"
#include "AnchoBackgroundServer/PeriodicTimer.hpp"
#include <IPCHeartbeat.h>
#include <Exceptions.h>
#include <SimpleWrappers.h>

namespace Ancho {
namespace Utils {



} //namespace Utils

namespace Service {

struct ENotValidTabId : EAnchoException {};


typedef CComPtr<IDispatch> TabInfo;
typedef CComPtr<IDispatch/*ComSimpleJSArray*/> TabInfoList;
typedef int TabId;

typedef boost::function<void(TabInfo)> TabCallback;
typedef boost::function<void(TabInfoList)> TabListCallback;
typedef boost::function<void(void)> SimpleCallback;

class TabManager;
//Pointer to tabmanger singleton instance
extern CComObject<Ancho::Service::TabManager> *gTabManager;

/**
 * Stores basic JS entities (Object, Array) constructors for API instances.
 * Useful for creation of return values, so JS engines can process objects from same context.
 * NOTE: calling constructor from different thread doesn't work, so instead of storing
 * constructors directly we store wrapper functions which return proper object instances.
 **/
class JSConstructorManager
{
public:
  typedef boost::tuple<std::wstring, int> ApiInstanceId;
  typedef boost::tuple<Ancho::Utils::ObjectMarshaller<IDispatchEx>::Ptr, Ancho::Utils::ObjectMarshaller<IDispatchEx>::Ptr> Constructors;

  void registerConstructors(CComPtr<IDispatchEx> aObjectConstructor, CComPtr<IDispatchEx> aArrayConstructor, const std::wstring &aExtensionId, int aApiId)
  {
    boost::unique_lock<boost::mutex> lock(mConstructorMapMutex);

    mConstructorInstances[ApiInstanceId(aExtensionId, aApiId)] = Constructors(boost::make_shared<Ancho::Utils::ObjectMarshaller<IDispatchEx> >(aObjectConstructor),
                                                                              boost::make_shared<Ancho::Utils::ObjectMarshaller<IDispatchEx> >(aArrayConstructor));
  }

  void removeConstructors(const std::wstring &aExtensionId, int aApiId)
  {
    boost::unique_lock<boost::mutex> lock(mConstructorMapMutex);
    mConstructorInstances.erase(ApiInstanceId(aExtensionId, aApiId));
  }

  CComPtr<IDispatch> createObject(const std::wstring &aExtensionId, int aApiId)
  {
    return createInstanceByConstructor<0>(aExtensionId, aApiId);
  }

  CComPtr<IDispatch> createArray(const std::wstring &aExtensionId, int aApiId)
  {
    return createInstanceByConstructor<1>(aExtensionId, aApiId);
  }

protected:
  template<size_t tConstructorIdx>
  CComPtr<IDispatch> createInstanceByConstructor(const std::wstring &aExtensionId, int aApiId)
  {
    boost::unique_lock<boost::mutex> lock(mConstructorMapMutex);
    auto it = mConstructorInstances.find(ApiInstanceId(aExtensionId, aApiId));

    if (it == mConstructorInstances.end()) {
      ANCHO_THROW(EInvalidArgument());
    }
    CComPtr<IDispatchEx> creator = it->second.get<tConstructorIdx>()->get();
    DISPPARAMS params = {0};
    _variant_t result;
    //Workaround - DISPATCH_CONSTRUCT doesn't work in worker thread -
    //so we invoke wrapper function which calls constructors and returns newly created instance.
    IF_FAILED_THROW(creator->InvokeEx(DISPID_VALUE, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result.GetAddress(), NULL, NULL));
    try {
      return CComPtr<IDispatch>(static_cast<IDispatch*>(result));
    } catch (_com_error &e) {
      ANCHO_THROW(EHResult(e.Error()));
    }
  }

  std::map<ApiInstanceId, Constructors> mConstructorInstances;
  boost::mutex mConstructorMapMutex;
};


/**
 * Singleton class.
 * This manager stores information and references to all tabs currently available in all IE windows.
 * It provides dual interface - we can implement chrome.tabs API by calling methods of this class
 **/
class TabManager:
  public CComObjectRootEx<CComMultiThreadModel>,
  public IAnchoTabManagerInternal,
  public IDispatchImpl<ITabManager, &IID_ITabManager, &LIBID_AnchoBgSrvLib, /*wMajor =*/ 0xffff, /*wMinor =*/ 0xffff>,
  public JSConstructorManager
{
public:
  friend struct CreateTabTask;
  friend struct ReloadTabTask;
  friend struct GetTabTask;
  friend struct UpdateTabTask;
  friend struct QueryTabsTask;
  friend struct RemoveTabsTask;

  class TabRecord;
  typedef boost::recursive_mutex Mutex;

  TabManager()
  {
  }

public:
  ///@{
  /** Asynchronous methods available to JS.**/
  STDMETHOD(createTab)(LPDISPATCH aProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(reloadTab)(INT aTabId, LPDISPATCH aReloadProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(queryTabs)(LPDISPATCH aProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(updateTab)(INT aTabId, LPDISPATCH aUpdateProperties, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  STDMETHOD(removeTabs)(LPDISPATCH aTabs, LPDISPATCH aCallback, BSTR aExtensionId, INT aApiId);
  ///@}

  //TODO - move to service
  STDMETHOD(registerConstructors)(LPDISPATCH aObjectConstructor, LPDISPATCH aArrayConstructor, BSTR aExtensionId, INT aApiId)
  {
    BEGIN_TRY_BLOCK;
    JSConstructorManager::registerConstructors(CComQIPtr<IDispatchEx>(aObjectConstructor), CComQIPtr<IDispatchEx>(aArrayConstructor), std::wstring(aExtensionId), aApiId);
    CComPtr<IDispatch> tmp = JSConstructorManager::createArray(std::wstring(aExtensionId), aApiId);
    return S_OK;
    END_TRY_BLOCK_CATCH_TO_HRESULT;
  }

  STDMETHOD(getTabInfo)(INT aTabId, BSTR aExtensionId, INT aApiId, VARIANT* aRet);

  TabId getFrameTabId(HWND aFrameTab);

  /**
   * Apply callable object on each available tab.
   * \tparam TCallable Type of callable object with signature TCallable::operator()(Ancho::Service::TabManager::TabRecord &).
   * \param aCallable Callable object, which will be applied on each tab record instance obtained by ID from aTabIds list.
   * \result Copy of provided callable object - can be used for returning accumulated values, etc.
   **/
  template<typename TCallable>
  TCallable forEachTab(TCallable aCallable)
  {
    boost::unique_lock<Mutex> lock(mTabAccessMutex);
    TabMap::iterator it = mTabs.begin();
    while (it != mTabs.end()) {
      ATLASSERT(it->second);
      aCallable(*it->second);
      ++it;
    }
    return aCallable;
  }

  /**
   * Apply callable object on each tab from provided list.
   * \tparam TContainer Iterable container (vector, list) of tab IDs
   * \tparam TCallable Type of callable object with signature TCallable::operator()(Ancho::Service::TabManager::TabRecord &).
   * \param aTabIds List of tab IDs which should be processed.
   * \param aCallable Callable object, which will be applied on each tab record instance obtained by ID from aTabIds list.
   * \result List of IDs for tabs which couldn't be found (wrong ID, tab already closed, etc.)
   **/
  template<typename TContainer, typename TCallable>
  TContainer forTabsInList(const TContainer &aTabIds, TCallable aCallable)
  {
    //Create list of tabs which do not exist
    TContainer missed;
    boost::unique_lock<Mutex> lock(mTabAccessMutex);

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
    return std::move(missed);
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
    boost::unique_lock<Mutex> lock(mTabAccessMutex);
    mCreateTabCallbacks[aRequestId] = aInfo;
  }

  boost::shared_ptr<TabRecord> getTabRecord(TabId aTabId)
  {
    boost::unique_lock<Mutex> lock(mTabAccessMutex);
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

  /// Tab record for newly created tab doesn't yet exist - store its callbeack in separate datastructure.
  CreateTabCallbackMap mCreateTabCallbacks;
  Ancho::Utils::PeriodicTimer mHeartbeatTimer;
  boost::atomic<bool> mHeartbeatActive;

  /// Manipulations with tab record container is synchronized by this mutex
  Mutex mTabAccessMutex;
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
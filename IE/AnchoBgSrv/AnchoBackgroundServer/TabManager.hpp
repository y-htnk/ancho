#pragma once
#include "resource.h"
#include "AnchoBgSrv_i.h"
#include "AnchoBackgroundServer/AsynchronousTaskManager.hpp"

namespace AnchoBackgroundServer {


class TabManager: public CComObjectRootEx<CComSingleThreadModel>,
  public IDispatchImpl<ITabManager, &IID_ITabManager, &LIBID_AnchoBgSrvLib, /*wMajor =*/ 0xffff, /*wMinor =*/ 0xffff>
{
public:
  TabManager()
  {
  }

public:
  ///@{
  /** Interface methods available to JS.**/
  STDMETHOD(createTab)(LPDISPATCH aProperties, LPDISPATCH aCallback, INT aApiId);

  ///@}
public:
  // -------------------------------------------------------------------------
  // COM standard stuff
  DECLARE_NO_REGISTRY()
  DECLARE_NOT_AGGREGATABLE(AnchoBackgroundServer::TabManager)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

public:
  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(AnchoBackgroundServer::TabManager)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(ITabManager)
  END_COM_MAP()


protected:

  AsynchronousTaskManager mAsyncTaskManager;
};


} //namespace AnchoBackgroundServer
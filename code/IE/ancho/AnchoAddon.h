/****************************************************************************
 * AnchoAddon.h : Declaration of the CAnchoAddon
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 * Author: Arne Seib <kontakt@seiberspace.de>
 ****************************************************************************/

#pragma once
#include "resource.h"       // main symbols

#include "ancho_i.h"
#include "DOMWindowWrapper.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

enum UpdateState {
  usNone,
  usInstalled,
  usUpdated
};

namespace Ancho {
namespace Utils {
  // -------------------------------------------------------------------------
  // Object ID. IUnknown pointer cast to ULONG_PTR.
  // Key for the frame map.
  // This type is also used by CAnchoRuntime.
  // The struct here is just for a safe conversion.
  struct COMOBJECTID
  {
    ULONG_PTR id;
    COMOBJECTID(ULONG_PTR aId) : id(aId) {}
    COMOBJECTID(IUnknown * aUnk) : id((ULONG_PTR)aUnk) {}
    operator ULONG_PTR() {return id;}
  };

  // -------------------------------------------------------------------------
  // typedef for map COMOBJECTID -> TRecord.
  // Use as:
  //    typedef Ancho::Utils::FrameMap<MyFrameRecordClass>::Type MyFrameMapClass;
  template <typename TRecord>
  struct FrameMap
  {
      typedef std::map<ULONG_PTR, TRecord> Type;
  };

} //namespace Utils
} //namespace Ancho

/*============================================================================
 * class CAnchoAddon
 */
class CAnchoAddon;
typedef CComObject<CAnchoAddon> CAnchoAddonComObject;

class ATL_NO_VTABLE CAnchoAddon :
  public CComObjectRootEx<CComSingleThreadModel>,
  public IAnchoAddon,
  public IDOMWindowWrapperManager // NOTE: This is NOT a COM interface!
{
public:
  // enum for registry flags
  enum {
    NONE    = 0x00000000,
    ENABLED = 0x00000001,
    MASK    = 0x00000001  // masks all valid flags
  };

  // -------------------------------------------------------------------------
  // ctor
  CAnchoAddon() :
      m_InstanceID(0),
      mUpdateState(usNone),
      mContentScriptsForFrames(FALSE)
  {
  }

  // -------------------------------------------------------------------------
  // COM standard stuff
  DECLARE_NO_REGISTRY()
  DECLARE_NOT_AGGREGATABLE(CAnchoAddon)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(CAnchoAddon)
    COM_INTERFACE_ENTRY(IAnchoAddon)
  END_COM_MAP()

  // -------------------------------------------------------------------------
  // COM standard methods
  HRESULT FinalConstruct()
  {
    return S_OK;
  }

  void FinalRelease()
  {
    Shutdown();
  }

public:
  // -------------------------------------------------------------------------
  // IAnchoAddon methods. See .idl for description.
  STDMETHOD(Init)(LPCOLESTR lpsExtensionID, IAnchoAddonService * pService,
    IWebBrowser2 * pWebBrowser);
  STDMETHOD(Shutdown)();
  STDMETHOD(OnFrameStart)(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelFrame, VARIANT_BOOL aIsTopLevelRefresh);
  STDMETHOD(OnFrameEnd)(IWebBrowser2 * aBrowser, BSTR aUrl, VARIANT_BOOL aIsTopLevelFrame, VARIANT_BOOL aIsTopLevelRefresh);
  STDMETHOD(InitializeExtensionScripting)(BSTR aUrl);

  // IDOMWindowWrapperManager methods.
  virtual HRESULT getWrappedDOMWindow(
      IWebBrowser2  * aCallerBrowser,
      IWebBrowser2  * aFrameBrowser,
      IDispatch    ** aRetHTMLWindow);

  virtual void putWrappedDOMWindow(
      IWebBrowser2  * aFrameBrowser,
      IDispatch     * aHTMLWindow);

  virtual void removeWrappedDOMWindow(
      IWebBrowser2  * aFrameBrowser);

// ---------------------------------------------------------------------------
private:  // types
  /*==========================================================================
   * class FrameRecord
   * Holds frame related objects(Browser, Magpie, DOM Window) and infos.
   * Initializes / cleanups content scripting for that frame.
   */
  class FrameRecord
  {
  public:
    FrameRecord(
        IWebBrowser2  * aWebBrowser,
        BOOL            aIsTopLevel);
    ~FrameRecord();

    HRESULT OnFrameStart(
        IWebBrowser2  * aBrowser,
        BSTR            aUrl,
        VARIANT_BOOL    aIsTopLevelRefresh);

    HRESULT InitializeContentScripting(
        IDOMWindowWrapperManager  * aDOMWindowManager,
        LPCWSTR                     aMagpieDebugName,
        LPCWSTR                     aExtensionPath,
        LPDISPATCH                  aChromeAPI,
        BSTR                        aUrl,
        VariantVector             & aContentScripts);

    void cleanupScripting();
    void cleanup();

    CComPtr<IWebBrowser2>       mBrowser;
    CComPtr<IMagpieApplication> mMagpie;
    CComPtr<DOMWindowWrapper::ComObject>
                                mWrappedDOMWindow;
    BOOL  mIsTopLevel;  // true for the top level browser of a set of frames.
  };
  typedef std::unique_ptr<FrameRecord> FrameRecordPtr;

  // FrameRecord Map type
  // Note we store raw FrameRecord pointers here to avoid copying.
  typedef Ancho::Utils::FrameMap<FrameRecordPtr>::Type FrameMap;

  // Map with DOMWindowWrapper objects for the current frameset.
  // It might seem that storing a map IHTMLWindow(original) to IHTMLWindow(wrapped)
  // makes most sense, but when we receive an IHTMLWindow from a browser it might
  // be a proxy. Don't know though, but better safe than sorry...
  // So in the end we use the COMOBJECTID'd IWebBrowser2 as key and the IDispatch
  // of the wrapped window as value. IDispatch because the wrapped window does not
  // provide a IHTMLWindow interface.
  typedef std::map<ULONG_PTR, CComPtr<IDispatch> > DOMWindowWrapperMap;

// ---------------------------------------------------------------------------
private:  // methods
  HRESULT createMagpieInstance(IMagpieApplication ** aMagpieRetVal);
  HRESULT initializeEnvironment();
  void cleanupFrames();
  void cleanupScripting();
  void notifyAboutUpdateStatus();

// ---------------------------------------------------------------------------
private:  // members
  std::wstring                          m_sExtensionName;
  std::wstring                          m_sExtensionID;
  boost::filesystem::wpath              m_sExtensionPath;
  CString                               m_sInstallPath;

  ULONG                                 m_InstanceID;

  CComQIPtr<IWebBrowser2>               m_pWebBrowser;

  CComPtr<IAnchoAddonService>           m_pAnchoService;
  CComPtr<IAnchoAddonBackground>        m_pAddonBackground;
  CComQIPtr<IAnchoBackgroundConsole>    m_pBackgroundConsole;
  CComQIPtr<IDispatch>                  m_pContentInfo;

  // this instance is only for an extension page. TO BE REMOVED
  CComPtr<IMagpieApplication>           m_Magpie;

  UpdateState                           mUpdateState; //whether extension was freshly installed, updated or none of these

  // Map with all current frames.
  FrameMap  mMapFrames;
  BOOL      mContentScriptsForFrames; // value from the addon's manifest

  // Map with all DOMWindowWrapper's
  DOMWindowWrapperMap mDOMWindowWrapper;
};


//OBJECT_ENTRY_NON_CREATEABLE_EX_AUTO(__uuidof(AnchoAddon), CAnchoAddon)

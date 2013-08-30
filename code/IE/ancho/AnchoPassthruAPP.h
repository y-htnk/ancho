/****************************************************************************
 * AnchoPassthruAPP.h : Declaration of the CAnchoPassthruAPP
 * Copyright 2012 Salsita (http://www.salsitasoft.com).
 * Author: Matthew Gertner <matthew@salsitasoft.com>
 * Author: Arne Seib <arne@salsitasoft.com>
 ****************************************************************************/

#pragma once

#include "ProtocolImpl.h"
#include "AnchoBrowserEvents.h"

#include <string>
typedef std::basic_string<TCHAR> tstring;
#include <set>

class CAnchoPassthruAPP;
class CAnchoProtocolSink;
class CAnchoStartPolicy;

/*============================================================================
 * class CAnchoProtocolSink
 * TODO: what is the purpose of this class?
 */
class CAnchoProtocolSink :
  public PassthroughAPP::CInternetProtocolSinkWithSP<CAnchoProtocolSink>,
  public IHttpNegotiate
{
// ---------------------------------------------------------------------------
public: // attributes
  // This is the sheet anchor for getting the browser for the currently active
  // request. In case the user setup a startpage the papp is not able to get a
  // browser - and by this not able to get ancho browser events for calling
  // back. So we use this static variable which gets set in
  // CAnchoRuntime::OnBrowserBeforeNavigate2 and cleared in
  // CAnchoProtocolSink::initializeBrowsers.
  // This is a hack, but ok, because BeginningTransaction is called on the same
  // thread and ALWAYS after OnBrowserBeforeNavigate2.
  static CComPtr<IWebBrowser2>  sCurrentTopLevelBrowser;

public:  // types
  /*==========================================================================
   * class SwitchParams
   * Used to pass two BSTR values via Switch()
   */
  class SwitchParams
  {
  public:
    static void create(
        PROTOCOLDATA & aProtocolData,
        LPCWSTR aParam1,
        LPCWSTR aParam2);

    static void extractAndDestroy(
        PROTOCOLDATA & aProtocolData,
        CComBSTR & aParam1,
        CComBSTR & aParam2);

    CComBSTR param1;
    CComBSTR param2;

  private:
    SwitchParams(LPCWSTR aParam1, LPCWSTR aParam2)
      : param1(aParam1), param2(aParam2)
    {}
  };
  //==========================================================================

// ---------------------------------------------------------------------------
private:  // types
  friend class CAnchoStartPolicy;
  typedef PassthroughAPP::CInternetProtocolSinkWithSP<CAnchoProtocolSink> BaseClass;

// ---------------------------------------------------------------------------
public: // methods
  // -------------------------------------------------------------------------
  // Constructor
  CAnchoProtocolSink() :
      m_bindVerb(-1),
      m_IsFrame(FALSE),
      mIsTopLevelRefresh(FALSE)
  {}

  // -------------------------------------------------------------------------
  // COM interface map
  BEGIN_COM_MAP(CAnchoProtocolSink)
    COM_INTERFACE_ENTRY(IHttpNegotiate)
    COM_INTERFACE_ENTRY_CHAIN(BaseClass)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(CAnchoProtocolSink)
    SERVICE_ENTRY(IID_IHttpNegotiate)
  END_SERVICE_MAP()

  // -------------------------------------------------------------------------
  // IHttpNegotiate
  STDMETHOD(BeginningTransaction)(
    /* [in] */  LPCWSTR   szURL,
    /* [in] */  LPCWSTR   szHeaders,
    /* [in] */  DWORD     dwReserved,
    /* [out] */ LPWSTR  * pszAdditionalHeaders);

  STDMETHOD(OnResponse)(
    /* [in] */  DWORD     dwResponseCode,
    /* [in] */  LPCWSTR   szResponseHeaders,
    /* [in] */  LPCWSTR   szRequestHeaders,
    /* [out] */ LPWSTR  * pszAdditionalRequestHeaders);

  STDMETHOD(ReportProgress)(
    /* [in] */ ULONG    ulStatusCode,
    /* [in] */ LPCWSTR  szStatusText);

  STDMETHOD(ReportData)(
    /* [in] */ DWORD  grfBSCF,
    /* [in] */ ULONG  ulProgress,
    /* [in] */ ULONG  ulProgressMax);

  STDMETHOD(ReportResult)(
    /* [in] */ HRESULT  hrResult,
    /* [in] */ DWORD    dwError,
    /* [in] */ LPCWSTR  szResult);

  // IInternetBindInfo
  STDMETHODIMP GetBindInfoEx(
    /* [out] */     DWORD     * grfBINDF,
    /* [in, out] */ BINDINFO  * pbindinfo,
    /* [out] */     DWORD     * grfBINDF2,
    /* [in] */      DWORD     * pdwReserved);

  // -------------------------------------------------------------------------

  // Returns true if the current request is for a frame.
  BOOL IsFrame() { return m_IsFrame; }

  // Returns true if the current request is a refresh for the top level frame.
  BOOL isTopLevelRefresh() { return mIsTopLevelRefresh; }

  // Returns true if the current request is the (HTML) document, false for a resource.
  BOOL isDocumentRequest() { return (mCurrentFrameBrowser != NULL); }

  // Returns the current bind verb.
  DWORD GetBindVerb() { return m_bindVerb; }

  // Get the current frame browser and optional top level browser.
  HRESULT getCurrentBrowser(
      IWebBrowser2 ** aCurrentFrameBrowser,
      IWebBrowser2 ** aTopLevelBrowserPtr = NULL);

  // ???
  std::wstring getUrl() const { return m_Url; }

// ---------------------------------------------------------------------------
private:  // methods

  // Get top-level and frame browser and store them for later use.
  // Initializes mTopLevelBrowser and mCurrentFrameBrowser.
  HRESULT initializeBrowsers();

// ---------------------------------------------------------------------------
private:  // members
  CComQIPtr<IWebBrowser2> mTopLevelBrowser;
  CComQIPtr<IWebBrowser2> mCurrentFrameBrowser;
  std::wstring            m_Url;
  DWORD                   m_bindVerb;
  BOOL                    m_IsFrame;
  BOOL                    mIsTopLevelRefresh;
};

/*============================================================================
 * class CAnchoStartPolicy
 * TODO: what is the purpose of this class?
 */
class CAnchoStartPolicy :
  public PassthroughAPP::CustomSinkStartPolicy<CAnchoPassthruAPP, CAnchoProtocolSink>
{
// ---------------------------------------------------------------------------
public: // methods
  HRESULT OnStart(
      LPCWSTR                 szUrl,
      IInternetProtocolSink * pOIProtSink,
      IInternetBindInfo     * pOIBindInfo,
      DWORD                   grfPI,
      HANDLE_PTR              dwReserved,
      IInternetProtocol     * pTargetProtocol);

  HRESULT OnStartEx(
      IUri                  * pUri,
      IInternetProtocolSink * pOIProtSink,
      IInternetBindInfo     * pOIBindInfo,
      DWORD                   grfPI,
      HANDLE_PTR              dwReserved,
      IInternetProtocolEx   * pTargetProtocol);
};

/*============================================================================
 * class CAnchoPassthruAPP
 * TODO: what is the purpose of this class?
 */
class CAnchoPassthruAPP :
  public PassthroughAPP::CInternetProtocol<CAnchoStartPolicy>
{
// ---------------------------------------------------------------------------
public: // methods
  CAnchoPassthruAPP() :
      mIsTopLevelRefresh(FALSE), mLastRequestState(0)
  {}

  // -------------------------------------------------------------------------
  // IInternetProtocolRoot
  STDMETHOD(Continue)(PROTOCOLDATA *pProtocolData);

  STDMETHOD(StartEx)(
    IUri                  * pUri,
    IInternetProtocolSink * pOIProtSink,
    IInternetBindInfo     * pOIBindInfo,
    DWORD                   grfPI,
    HANDLE_PTR              dwReserved);

  // Initializes members from the protocol sink.
  HRESULT initFromSink(CAnchoProtocolSink * aProtocolSink);
  // Clear members, release objects.
  void reset();
  // Fire OnFrameStart event
  HRESULT fireOnFrameStart(CComBSTR & aCurrentURL);
  // Fire OnFrameRedirect event
  HRESULT fireOnFrameRedirect(CComBSTR & aCurrentURL, CComBSTR & aAdditionalData);
  // Fire OnBeforeHeaders event
  HRESULT fireOnBeforeHeaders(IWebRequestReporter * aReporter);
  // Fire OnFrameEnd event
  HRESULT fireOnFrameEnd(CComBSTR aUrl);

// ---------------------------------------------------------------------------
private:  // types
  typedef std::vector<std::pair<std::wstring, std::wstring> > RedirectList;

  /*==========================================================================
   * class DocumentSink
   * Listens to the readystatechange event of HTMLDocumentEvents2 and notifies
   * DAnchoBrowserEvents::OnEndFrame.
   */
  class DocumentSink :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IUnknown,
    public IDispEventImpl<1, DocumentSink, &DIID_HTMLDocumentEvents2, &LIBID_MSHTML, 4, 0>
  {
  // -------------------------------------------------------------------------
  public: // types
    friend CComObject<DocumentSink>;

  // -------------------------------------------------------------------------
  public: // methods and functions
    // Static function to notify aBrowserEvents that a frame is complete.
    // If the associated document is already in "complete" state, call
    // DAnchoBrowserEvents::OnEndFrame directly. Otherwise create an
    // instance and attach to the document's ready state change event.
    static HRESULT notifyEndFrame(
        IWebBrowser2        * aWebBrowser,
        IHTMLDocument2      * aHTMLDoc,
        DAnchoBrowserEvents * aBrowserEvents,
        BSTR                  aURL,
        BOOL                  aIsTopLevelRefresh);

    // DTOR
    ~DocumentSink();

    BEGIN_COM_MAP(DocumentSink)
      COM_INTERFACE_ENTRY(IUnknown)
    END_COM_MAP()

    // -------------------------------------------------------------------------
    // Event map
    BEGIN_SINK_MAP(DocumentSink)
      SINK_ENTRY_EX(
          1,
          DIID_HTMLDocumentEvents2,
          DISPID_READYSTATECHANGE,
          OnReadyStateChange)
    END_SINK_MAP()

    // HTMLDocumentEvents2
    STDMETHOD_(void, OnReadyStateChange)(IHTMLEventObj* ev);

  // -------------------------------------------------------------------------
  private:  // methods
    // CTOR
    DocumentSink();

    HRESULT init(
        IWebBrowser2        * aWebBrowser,
        IHTMLDocument2      * aHTMLDoc,
        DAnchoBrowserEvents * aBrowserEvents,
        BSTR                  aURL,
        BOOL                  aIsTopLevelRefresh);

  // -------------------------------------------------------------------------
  private:  // members
    CComPtr<IWebBrowser2>         mBrowser;
    CComPtr<IHTMLDocument2>       mHTMLDocument;
    CComPtr<DAnchoBrowserEvents>  mEvents;
    CComBSTR                      mURL;
    BOOL                          mIsTopLevelRefresh;
  };
  //==========================================================================

// ---------------------------------------------------------------------------
private:  // members
  CComPtr<IWebBrowser2> mTopLevelBrowser;
  CComPtr<IWebBrowser2> mCurrentFrameBrowser;
  CComQIPtr<DAnchoBrowserEvents>
                        mAnchoEvents;
  RedirectList          mRedirects;
  BOOL                  mIsTopLevelRefresh;

  // Guard to ensure each of the ANCHO_SWITCH_XXX states is handled only once.
  DWORD mLastRequestState;
};

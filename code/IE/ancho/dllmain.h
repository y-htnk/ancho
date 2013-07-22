// dllmain.h : Declaration of module class.

extern class CAnchoModule _AtlModule;

class CAnchoModule : public CAtlDllModuleT<CAnchoModule>
{
public :
  HINSTANCE m_hInstance;
  DECLARE_LIBID(LIBID_anchoLib)
  static HRESULT WINAPI UpdateRegistryAppId(BOOL bRegister) throw()
  {
	  CStringW modulePath;
    LPWSTR lps = modulePath.GetBuffer(MAX_PATH);
	  ::GetModuleFileNameW( _AtlModule.m_hInstance, lps, MAX_PATH );
    PathRemoveFileSpecW(lps);
    modulePath.ReleaseBuffer();

    ATL::_ATL_REGMAP_ENTRY aMapEntries [] =
    {
      { OLESTR("APPID"), L"{AD78048F-6651-4A8D-B10B-4E2F65F4FD67}" },
      { OLESTR("MODULEPATH"), lps },
      { NULL, NULL }
    };
    return ATL::_pAtlModule->UpdateRegistryFromResource(IDR_ANCHO, bRegister, aMapEntries);
  }


  BOOL WINAPI DllMain(DWORD dwReason, LPVOID lpReserved) throw();

protected:
  CComPtr<IClassFactory> m_CFHTTP;
  CComPtr<IClassFactory> m_CFHTTPS;
};


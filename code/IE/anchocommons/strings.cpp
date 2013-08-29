#include "anchocommons.h"

// module ID for the main API script
const wchar_t * s_AnchoMainAPIModuleID =
    L"api.js";

// the registry key where to find extensions
// If you change this you also have to change the according entry in
// AnchoAddonService.rgs
const wchar_t * s_AnchoExtensionsRegistryKey =
    L"Software\\Salsita\\AnchoAddonService";

// registry entry "guid"
const wchar_t * s_AnchoExtensionsRegistryEntryGUID =
    L"guid";

// registry entry "Flags"
const wchar_t * s_AnchoExtensionsRegistryEntryFlags =
    L"Flags";

// registry entry "Path"
const wchar_t * s_AnchoExtensionsRegistryEntryPath =
    L"Path";

// registry entry "UpdateUrl"
const wchar_t * s_AnchoUpdateUrlRegistryEntry =
    L"UpdateUrl";

// registry entry "Version"
const wchar_t * s_AnchoRegistryEntryVersion =
    L"Version";

// Name for directory containing extensions extracted from CRX packages
const wchar_t * s_AnchoExtractedExtensionsDirName =
    L"ExtractedExtensions";

// signature for extracted extension is stored under this name
const wchar_t * s_AnchoExtensionSignatureFileName =
    L"AnchoExtensionSignature.base64";

// signature for extracted extension is stored under this name
const wchar_t * s_AnchoStorageLocalDatabaseFileName =
    L"StorageLocal.sqlite";

// scheme name for protocol handler
const wchar_t * s_AnchoProtocolHandlerScheme =
    L"chrome-extension";

// URL prefix for extension protocol
const wchar_t * s_AnchoProtocolHandlerPrefix =
  L"chrome-extension://";

// scheme name for ancho internal protocol handler
const wchar_t * s_AnchoInternalProtocolHandlerScheme =
    L"ancho-internal";

// JS object name of the API in background scripts
const wchar_t * s_AnchoGlobalAPIObjectName =
    L"addonAPI";

// JS object name of the service API in background scripts
const wchar_t * s_AnchoServiceAPIName =
    L"serviceAPI";

// JS object name of the background API in main API script
const wchar_t * s_AnchoBackgroundAPIObjectName =
    L"anchoBackgroundAPI";

// JS object name of the API in background windows.
// The name is the same both in the background window and
// in the main module exports. Means: exports.chrome in api.js
// becomes chrome in background.html
const wchar_t * s_AnchoBackgroundPageAPIName =
    L"chrome";

const wchar_t * s_AnchoBackgroundConsoleObjectName =
    L"console";

// name of the exported function for getting a content API
const wchar_t * s_AnchoFnGetContentAPI =
    L"getContentInfo";


// name of the exported function for releasing a content API
const wchar_t * s_AnchoFnReleaseContentAPI =
    L"releaseContentInfo";

// name of the tab window property where tab id is stored
const wchar_t * s_AnchoTabIDPropertyName =
    L"AnchoTabID";

// scheme used to wrap request ID and relative url when creating new tab
const wchar_t * s_AnchoRelativeUrlHelperScheme =
    L"ancho-relative";

// scheme used to wrap request ID and absolute url when creating new tab
const wchar_t * s_AnchoAbsoluteUrlHelperScheme =
    L"ancho-absolute";
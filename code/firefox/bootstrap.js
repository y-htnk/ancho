const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/NetUtil.jsm');

const EXTENSION_ID = 'ancho@salsitasoft.com';
const CHROME_EXTENSION_ROOT = 'chrome-ext';

var require = null;

function createBackground(extensionRoot) {
  var backgroundWindow = Services.ww.openWindow(
    null, // parent
    'chrome://ancho/content/xul/background.xul',
    null, // window name
    null, // features
    extensionRoot  // extra arguments
  );

  // Make the window invisible
  var xulWindow = backgroundWindow.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIXULWindow);

  xulWindow.QueryInterface(Ci.nsIBaseWindow).visibility = false;
}

function setResourceSubstitution(resourceURI) {
  // Create a resource:// URL substitution so that we can access files
  // in the extension directly.
  var resourceProtocol = Services.io.getProtocolHandler('resource').
    QueryInterface(Ci.nsIResProtocolHandler);
  resourceProtocol.setSubstitution('ancho', resourceURI);
}

function registerComponents() {
  require('./js/protocolHandler').register();
  require('./js/contentPolicy').register();
  require('./js/httpRequestObserver').register();
}

function unregisterComponents(callback) {
  require('./js/protocolHandler').unregister();
  require('./js/contentPolicy').unregister(callback);
  require('./js/httpRequestObserver').unregister();
}

// Required functions for bootstrapped extensions.
function install(data, reason) {
}

function uninstall(data, reason) {
}

// When the extension is activated:
//
function startup(data, reason) {
  var requireURI = NetUtil.newURI('modules/Require.jsm', '', data.resourceURI);
  Cu.import(requireURI.spec);
  require = Require.createRequireForWindow(this, data.resourceURI);
  var Config = require('./js/config');
  Config.firstRun = ((reason === ADDON_INSTALL || reason === ADDON_ENABLE));

  registerComponents();
  setResourceSubstitution(data.resourceURI);

  var protocolHandler = require('./js/protocolHandler');
  var extensionRoot = data.installPath;
  extensionRoot.append(CHROME_EXTENSION_ROOT);
  var directoryEntries = extensionRoot.directoryEntries;
  while (directoryEntries.hasMoreElements()) {
    var directory = directoryEntries.getNext().QueryInterface(Ci.nsIFile);
    var rootURI = Services.io.newFileURI(directory);
    protocolHandler.registerExtensionURI(directory.leafName, rootURI);
    createBackground(directory);
  }
}

// When the extension is deactivated:
//
function shutdown(data, reason) {
  var Global = require('./js/state').Global;
  Global.shutdown();
  unregisterComponents(function() {
    // Unload the modules so that we will load new versions if the add-on is installed again.
    Cu.unload('resource://ancho/modules/Require.jsm');
    Cu.unload('resource://ancho/modules/External.jsm');
    // Get rid of the resource package substitution.
    setResourceSubstitution(null);
  });
}

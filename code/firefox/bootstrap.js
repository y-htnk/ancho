const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/FileUtils.jsm');

const EXTENSION_ID = 'ancho@salsitasoft.com';
const CHROME_EXTENSION_ROOT = 'chrome-extensions';

var require = null;

function createBackground(extensionRoot, reason) {
  var params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  params.appendElement(extensionRoot, false);
  var param = Cc["@mozilla.org/supports-PRUint32;1"].createInstance(Ci.nsISupportsPRUint32);
  param.data = reason;
  params.appendElement(param, false);
  var backgroundWindow = Services.ww.openWindow(
    null, // parent
    'chrome://ancho/content/xul/background.xul',
    null, // window name
    null, // features
    params  // extra arguments
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

function loadExtensions(extensionRoot, reason) {
  if (!extensionRoot.exists()) {
    extensionRoot.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  var protocolHandler = require('./js/protocolHandler');
  var directoryEntries = extensionRoot.directoryEntries;
  while (directoryEntries.hasMoreElements()) {
    var directory = directoryEntries.getNext().QueryInterface(Ci.nsIFile);
    if (!directory.isDirectory()) {
      continue;
    }
    var rootURI = Services.io.newFileURI(directory);
    protocolHandler.registerExtensionURI(directory.leafName, rootURI);
    createBackground(directory, reason);
  }
}

// Required functions for bootstrapped extensions.
function install(data, reason) {
}

function uninstall(data, reason) {
  // Clean up temporary data
  var tempDir = FileUtils.getFile('ProfD', ['ancho_data']);
  if (tempDir.exists()) {
    tempDir.remove(true);
  }
}

function startup(data, reason) {
  setResourceSubstitution(data.resourceURI);
  Cu.import('resource://ancho/modules/Require.jsm');
  var baseURI = Services.io.newURI('resource://ancho/', '', null);
  require = Require.createRequireForWindow(this, baseURI);
  registerComponents();

  var extensionRoot = data.installPath.clone();
  extensionRoot.append(CHROME_EXTENSION_ROOT);
  loadExtensions(extensionRoot, reason);
  loadExtensions(FileUtils.getFile('ProfD', [CHROME_EXTENSION_ROOT]), reason);
}

function shutdown(data, reason) {
  var Global = require('./js/state').Global;
  Global.shutdown(reason);
  unregisterComponents(function() {
    // Unload the modules so that we will load new versions if the add-on is installed again.
    Cu.unload('resource://ancho/modules/Require.jsm');
    Cu.unload('resource://ancho/modules/External.jsm');
    // Get rid of the resource package substitution.
    setResourceSubstitution(null);
  });
}

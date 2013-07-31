const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/FileUtils.jsm');
Cu.import('resource://gre/modules/AddonManager.jsm');

const EXTENSION_ID = 'ancho@salsitasoft.com';
const CHROME_EXTENSION_ROOT = 'chrome-extensions';
const CHROME_EXTENSION_ENABLED_TOPIC = 'ancho-chrome-extension-enabled';
const CHROME_EXTENSION_DISABLED_TOPIC = 'ancho-chrome-extension-disabled';
const CHROME_EXTENSION_INSTALLED_TOPIC = 'ancho-chrome-extension-installed';
const CHROME_EXTENSION_UNINSTALLED_TOPIC = 'ancho-chrome-extension-uninstalled';

var require = null;

var loadedExtensions = [];

var ExtensionLifecycleObserver = {
  registerObservers: function() {
    Services.obs.addObserver(this, CHROME_EXTENSION_INSTALLED_TOPIC, false);
    Services.obs.addObserver(this, CHROME_EXTENSION_UNINSTALLED_TOPIC, false);
    Services.obs.addObserver(this, CHROME_EXTENSION_ENABLED_TOPIC, false);
    Services.obs.addObserver(this, CHROME_EXTENSION_DISABLED_TOPIC, false);
  },

  unregisterObservers: function() {
    Services.obs.removeObserver(this, CHROME_EXTENSION_INSTALLED_TOPIC, false);
    Services.obs.removeObserver(this, CHROME_EXTENSION_UNINSTALLED_TOPIC, false);
    Services.obs.removeObserver(this, CHROME_EXTENSION_ENABLED_TOPIC, false);
    Services.obs.removeObserver(this, CHROME_EXTENSION_DISABLED_TOPIC, false);
  },

  observe: function(subject, topic, data) {
    var rootDirectory = FileUtils.getFile('ProfD', [CHROME_EXTENSION_ROOT]);
    rootDirectory.append(data);
    if (CHROME_EXTENSION_INSTALLED_TOPIC === topic) {
      loadExtension(rootDirectory, require('./js/state').ADDON_INSTALL);
    }
    else if (CHROME_EXTENSION_UNINSTALLED_TOPIC === topic) {
      uninstallExtension(rootDirectory);
    }
    else if (CHROME_EXTENSION_ENABLED_TOPIC === topic) {
      loadExtension(rootDirectory, require('./js/state').ADDON_ENABLE);
    }
    else if (CHROME_EXTENSION_DISABLED_TOPIC === topic) {
      unloadExtension(this._getExtension(data), require('./js/state').ADDON_DISABLE);
    }
  },

  _getExtension: function(id) {
    return require('./js/state').Global.getExtension(id);
  }
};

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

function uninstallExtension(rootDirectory) {
  var file = rootDirectory.clone();
  file.append('ancho_data');
  if (file.exists()) {
    file.remove(true);
  }
}

function uninstallExtensions(extensionRoot) {
  var directoryEntries = extensionRoot.directoryEntries;
  while (directoryEntries.hasMoreElements()) {
    var directory = directoryEntries.getNext().QueryInterface(Ci.nsIFile);
    if (!directory.isDirectory()) {
      continue;
    }
    uninstallExtension(directory);
  }
}

function unloadExtension(extension, reason) {
  var protocolHandler = require('./js/protocolHandler');
  protocolHandler.unregisterExtensionURI(extension.id);
  extension.unload(reason);
}

function loadExtension(rootDirectory, reason) {
  var protocolHandler = require('./js/protocolHandler');
  var rootURI = Services.io.newFileURI(rootDirectory);
  protocolHandler.registerExtensionURI(rootDirectory.leafName, rootURI);
  createBackground(rootDirectory, reason);
}

function loadExtensions(extensionRoot, reason) {
  function makeLoader(directory, reason) {
    return function(addon) {
      if (addon) {
        loadExtension(directory, reason);
      }
    };
  }
  if (!extensionRoot.exists()) {
    extensionRoot.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  var directoryEntries = extensionRoot.directoryEntries;
  while (directoryEntries.hasMoreElements()) {
    var directory = directoryEntries.getNext().QueryInterface(Ci.nsIFile);
    if (!directory.isDirectory()) {
      continue;
    }
    // Only load extensions that have a wrapper XPI installed.
    AddonManager.getAddonByID(directory.leafName, makeLoader(directory, reason));
  }
}

function install(data, reason) {
}

function uninstall(data, reason) {
  var extensionRoot = data.installPath.clone();
  extensionRoot.append(CHROME_EXTENSION_ROOT);
  uninstallExtensions(extensionRoot);
  uninstallExtensions(FileUtils.getFile('ProfD', [CHROME_EXTENSION_ROOT]));
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

  ExtensionLifecycleObserver.registerObservers();
}

function shutdown(data, reason) {
  ExtensionLifecycleObserver.unregisterObservers();

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

(function() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cu = Components.utils;

  Cu.import('resource://gre/modules/Services.jsm');
  Cu.import('resource://gre/modules/FileUtils.jsm');

  const APP_STARTUP = 1;
  const APP_SHUTDOWN = 2;
  const ADDON_ENABLE = 3;
  const ADDON_DISABLE = 4;
  const ADDON_INSTALL = 5;
  const ADDON_UNINSTALL = 6;
  const ADDON_UPGRADE = 7;
  const ADDON_DOWNGRADE = 8;

  let inherits = require('inherits');
  let EventEmitter2 = require('eventemitter2').EventEmitter2;
  let Utils = require('./utils');
  let WindowWatcher = require('./windowWatcher');
  let Binder = require('./binder');

  let gGlobal = null;

  function WindowEventEmitter(win) {
    this._window = win;
  }
  inherits(WindowEventEmitter, EventEmitter2);

  WindowEventEmitter.prototype.init = function() {
    this._window.addEventListener('unload', Binder.bind(this, 'shutdown'), false);
  };

  WindowEventEmitter.prototype.shutdown = function(reason) {
    if (this._window) {
      this.emit('unload', reason);
      this._window.removeEventListener('unload', Binder.unbind(this, 'shutdown'), false);
      this._window = null;
    }
  };

  function Extension(id) {
    EventEmitter2.call(this, { wildcard: true });
    this._id = id;
    this._rootDirectory = null;
    this._manifest = null;
    this._windowEventEmitters = {};
    this._windowWatcher = null;
    this._firstRun = null;
  }
  inherits(Extension, EventEmitter2);

  Object.defineProperty(Extension.prototype, 'id', {
    get: function id() {
      return this._id;
    }
  });

  Object.defineProperty(Extension.prototype, 'rootDirectory', {
    get: function rootDirectory() {
      return this._rootDirectory.clone();
    }
  });

  Object.defineProperty(Extension.prototype, 'firstRun', {
    get: function firstRun() {
      return this._firstRun;
    }
  });

  Object.defineProperty(Extension.prototype, 'manifest', {
    get: function manifest() {
      return this._manifest;
    }
  });

  Object.defineProperty(Extension.prototype, 'windowWatcher', {
    get: function windowWatcher() {
      if (!this._windowWatcher) {
        this._windowWatcher = new WindowWatcher(this);
      }
      return this._windowWatcher;
    }
  });

  Extension.prototype.getURL = function(path) {
    var URI = NetUtil.newURI('chrome-extension://' + this._id + '/' + path, '', null);
    return URI.spec;
  };

  Extension.prototype.getStorageTableName = function(storageSpace) {
    return this._id.replace(/[^A-Za-z]/g, '_') + '_' + storageSpace;
  };

  Extension.prototype.load = function(rootDirectory, reason) {
    this._rootDirectory = rootDirectory;
    this._firstRun = (reason > APP_STARTUP);
    if (ADDON_ENABLE === reason) {
      this._onEnabled();
    }
    this._loadManifest();
  };

  Extension.prototype.unload = function(reason) {
    if (this._windowWatcher) {
      this._windowWatcher.unload();
    }

    for (var windowId in this._windowEventEmitters) {
      this._windowEventEmitters[windowId].shutdown(reason);
    }
    this._windowEventEmitters = {};

    this.emit('unload', reason);

    if (ADDON_DISABLE === reason) {
      this._onDisabled();
    }
  };

  Extension.prototype.forWindow = function(win) {
    var windowId = Utils.getWindowId(win);
    var windowEventEmitter;
    if (!(windowId in this._windowEventEmitters)) {
      windowEventEmitter = new WindowEventEmitter(win);
      windowEventEmitter.init();
      this._windowEventEmitters[windowId] = windowEventEmitter;
    }
    else {
      windowEventEmitter = this._windowEventEmitters[windowId];
    }
    return windowEventEmitter;
  };

  Extension.prototype._loadManifest = function() {
    var manifestFile = this.rootDirectory;
    manifestFile.append('manifest.json');
    var manifestURI = Services.io.newFileURI(manifestFile);
    var manifestString = Utils.readStringFromUrl(manifestURI);
    this._manifest = JSON.parse(manifestString);
    var i, j;
    if ('content_scripts' in this._manifest) {
      for (i=0; i<this._manifest.content_scripts.length; i++) {
        var scriptInfo = this._manifest.content_scripts[i];
        for (j=0; j<scriptInfo.matches.length; j++) {
          // Convert from Google's simple wildcard syntax to a regular expression
          // TODO: Implement proper match pattern matcher.
          scriptInfo.matches[j] = Utils.matchPatternToRegexp(scriptInfo.matches[j]);
        }
      }
    }
    if ('web_accessible_resources' in this._manifest) {
      for (i=0; i<this._manifest.web_accessible_resources.length; i++) {
        this._manifest.web_accessible_resources[i] =
          Utils.matchPatternToRegexp(this._manifest.web_accessible_resources[i]);
      }
    }
  };

  Extension.prototype._onEnabled = function() {
    this._restoreStorage('local');
    this._restoreStorage('sync');
  };

  Extension.prototype._onDisabled = function() {
    this._backupStorage('local');
    this._backupStorage('sync');
  };

  Extension.prototype._restoreStorage = function(storageSpace) {
    var tableName = this.getStorageTableName(storageSpace);
    var file = this._getStorageBackupFile(tableName);
    if (!file.exists()) {
      return;
    }

    var sql = '';
    var stream = Cc['@mozilla.org/network/file-input-stream;1'].createInstance(Ci.nsIFileInputStream);
    var is = Cc['@mozilla.org/intl/converter-input-stream;1'].createInstance(Ci.nsIConverterInputStream);
    stream.init(file, -1, 0, 0);
    is.init(stream, "UTF-8", 0, 0);

    var str;
    var read = 0;
    do {
      str = {};
      read = is.readString(0xffffffff, str);
      sql += str.value;
    } while (read !== 0);
    is.close();

    sqlLines = sql.split('\n');
    var storageConnection = gGlobal.storageConnection;
    for (var i=0; i<sqlLines.length; i++) {
      if (sqlLines[i]) {
        var statement = storageConnection.createStatement(sqlLines[i]);
        statement.execute();
      }
    }

    file.remove(false);
  };

  Extension.prototype._backupStorage = function(storageSpace) {
    var tableName = this.getStorageTableName(storageSpace);
    var sqlDump = 'CREATE TABLE IF NOT EXISTS ' + tableName + ' (key TEXT PRIMARY KEY, value TEXT);\n';
    var storageConnection = gGlobal.storageConnection;
    var statement = storageConnection.createStatement('SELECT key, value FROM ' + tableName);
    while (statement.executeStep()) {
      sqlDump += 'INSERT INTO ' + tableName + ' (key, value) VALUES (\'';
      sqlDump += statement.row.key;
      sqlDump += '\', \'';
      sqlDump += statement.row.value;
      sqlDump += '\');\n';
    }

    var file = this._getStorageBackupFile(tableName);
    var stream = FileUtils.openSafeFileOutputStream(file);
    var os = Cc["@mozilla.org/intl/converter-output-stream;1"].createInstance(Ci.nsIConverterOutputStream);
    os.init(stream, 'UTF-8', 0, 0x0000);
    os.writeString(sqlDump);
    FileUtils.closeSafeFileOutputStream(stream);

    statement = storageConnection.createStatement('DROP TABLE ' + tableName);
    statement.execute();
  };

  Extension.prototype._getStorageBackupFile = function(tableName) {
    var file = this.rootDirectory;
    file.append('ancho_data');
    if (!file.exists()) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    file.append(tableName + '.sql');
    return file;
  };

  function GlobalId() {
    this._id = 1;
  }

  GlobalId.prototype.getNext = function() {
    return this._id++;
  };

  function Global() {
    EventEmitter2.call(this, { wildcard: true });
    this._extensions = {};
    this._globalIds = {};

    var dbFile = FileUtils.getFile('ProfD', ['ancho_storage.sqlite3']);
    this._storageConnection = Services.storage.openDatabase(dbFile);
  }
  inherits(Global, EventEmitter2);

  Object.defineProperty(Global.prototype, 'storageConnection', {
    get: function storageConnection() {
      return this._storageConnection;
    }
  });

  Global.prototype.getGlobalId = function(name) {
    if (!this._globalIds[name]) {
      this._globalIds[name] = new GlobalId();
    }
    return this._globalIds[name].getNext();
  };

  Global.prototype.getExtension = function(id) {
    return this._extensions[id];
  };

  Global.prototype.loadExtension = function(id, rootDirectory, reason) {
    this._extensions[id] = new Extension(id);
    this._extensions[id].load(rootDirectory, reason);
    return this._extensions[id];
  };

  Global.prototype.unloadExtension = function(id, reason) {
    this._extensions[id].unload(reason);
    delete this._extensions[id];
    return (this._extensions.length > 0);
  };

  Global.prototype.shutdown = function(reason) {
    this.emit('unload', reason);
    this.removeAllListeners();

    this._storageConnection.asyncClose();
  };

  exports.Global = gGlobal = new Global();

  // Bootstrapped extension lifecycle constants.
  exports.APP_STARTUP = APP_STARTUP;
  exports.APP_SHUTDOWN = APP_SHUTDOWN;
  exports.ADDON_ENABLE = ADDON_ENABLE;
  exports.ADDON_DISABLE = ADDON_DISABLE;
  exports.ADDON_INSTALL = ADDON_INSTALL;
  exports.ADDON_UNINSTALL = ADDON_UNINSTALL;
  exports.ADDON_UPGRADE = ADDON_UPGRADE;
  exports.ADDON_DOWNGRADE = ADDON_DOWNGRADE;

}).call(this);

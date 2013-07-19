(function() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cu = Components.utils;

  Cu.import('resource://gre/modules/Services.jsm');
  Cu.import('resource://gre/modules/FileUtils.jsm');

  let inherits = require('inherits');
  let EventEmitter2 = require('eventemitter2').EventEmitter2;
  let Utils = require('./utils');
  let WindowWatcher = require('./windowWatcher');

  function Extension(id) {
    EventEmitter2.call(this, { wildcard: true });
    this._id = id;
    this._rootDirectory = null;
    this._firstRun = false;
    this._manifest = null;
    this._windowWatcher = null;
  }
  inherits(Extension, EventEmitter2);

  Object.defineProperty(Extension.prototype, 'id', {
    get: function id() {
      return this._id;
    }
  });

  Object.defineProperty(Extension.prototype, 'rootDirectory', {
    get: function rootDirectory() {
      return this._rootDirectory;
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

  Extension.prototype.load = function(rootDirectory) {
    this._rootDirectory = rootDirectory;
    this._loadManifest();
  };

  Extension.prototype.unload = function() {
    if (this._windowWatcher) {
      this._windowWatcher.unload();
    }
    this.emit('unload');
  };

  Extension.prototype._loadManifest = function() {
    var initFile = this._rootDirectory.clone();
    initFile.append('__init__');
    if (!initFile.exists()) {
      this._firstRun = true;
      // Create the file so we know in subsequent runs that
      // the extension was already installed.
      var stream = FileUtils.openFileOutputStream(initFile);
      stream.flush();
      stream.close();
    }

    var manifestFile = this._rootDirectory.clone();
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
  }
  inherits(Global, EventEmitter2);

  Global.prototype.getGlobalId = function(name) {
    if (!this._globalIds[name]) {
      this._globalIds[name] = new GlobalId();
    }
    return this._globalIds[name].getNext();
  };

  Global.prototype.getExtension = function(id) {
    return this._extensions[id];
  };

  Global.prototype.loadExtension = function(id, rootDirectory) {
    this._extensions[id] = new Extension(id);
    this._extensions[id].load(rootDirectory);
    return this._extensions[id];
  };

  Global.prototype.watchExtensions = function(callback) {
    for (var id in this._extensions) {
      callback(this._extensions[id]);
    }

    this.addListener('load', callback);
  };

  Global.prototype.unloadExtension = function(id) {
    this._extensions[id].unload();
    delete this._extensions[id];
  };

  Global.prototype.unloadAllExtensions = function() {
    let id;
    for (id in this._extensions) {
      this.unloadExtension(id);
    }
    this.emit('unload');
  };

  Global.prototype.shutdown = function() {
    this.unloadAllExtensions();
    this.removeAllListeners();
  };

  exports.Global = new Global();

}).call(this);

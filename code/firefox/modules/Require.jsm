EXPORTED_SYMBOLS = ['Require'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');

var moduleCache = {};

var Require = {
  _findModuleInPath: function(id, scriptUrl) {
    var url = null;
    do {
      if (url) {
        url = Services.io.newURI('..', '', url);
      }
      else {
        url = scriptUrl;
      }
      var nodeModules = Services.io.newURI('node_modules/', '', url);
      var moduleUrl = Services.io.newURI(id + '.js', '', nodeModules);
      var channel = Services.io.newChannelFromURI(moduleUrl);
      try {
        var inputStream = channel.open();
        return moduleUrl;
      }
      catch(e) {
        // No stream so the module doesn't exist
      }
    }while(!url.equals(this._baseUrl));
    return null;
  },

  createRequire: function(baseUrl) {
    var self = this;
    this._baseUrl = baseUrl;
    return function require(id, scriptUrl) {
      if (baseUrl && !scriptUrl) {
        scriptUrl = baseUrl;
      }
      if (!scriptUrl) {
        // No base URL available so we need to get it from the stacktrace.
        try {
          // To get a stacktrace we have to thrown an exception.
          throw new Error();
        }
        catch (e) {
          var frames = e.stack.split('\n');
          var baseSpec = frames[1].match(/@(.*):\d+$/)[1];
          scriptUrl = Services.io.newURI(baseSpec, '', null);
        }
      }

      var url;
      if (id[0] != '.' && id[0] != '/') {
        // Try to find the module in the search path
        url = self._findModuleInPath(id, scriptUrl);
      }
      else {
        url = Services.io.newURI(id + '.js', '', scriptUrl);
      }
      if (!url) {
        // TODO: Logging.
        return;
      }

      var spec = url.spec;
      if (spec in moduleCache) {
        return moduleCache[spec];
      }

      var scriptLoader = Cc['@mozilla.org/moz/jssubscript-loader;1'].
        getService(Ci.mozIJSSubScriptLoader);

      var context = {};
      var directoryUrl = Services.io.newURI('.', '', url);
      context.require = function(id) { return require(id, directoryUrl); };
      context.process = { title: 'Ancho' };

      // Need to add to the cache here to avoid stack overflow in case of require() cycles
      // (e.g. A requires B which requires A).
      moduleCache[spec] = context.exports = {};
      // Support for 'module.exports' (overrides 'exports' if 'module.exports' is used).
      context.module = {};

      try {
        scriptLoader.loadSubScript(spec, context);
      } catch (reason) {
        dump('\nERROR: Loading of subscript "' + spec + '" failed:\n');
        dump(reason + '\n\n');
      }
      if (context.module.exports) {
        context.exports = context.module.exports;
      }
      moduleCache[spec] = context.exports;
      return context.exports;
    };
  }
};

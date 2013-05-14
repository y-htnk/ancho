(function() {

  /*
      Handler for Network domain of debugger protocol.
  */

  var debuggerData = require('./debuggerData');

  function processNetworkCommand(target, method, commandParams, callback) {
    var obj = debuggerData.data[target.tabId];
    if (!obj) {
      return;  // not attached
    }
    switch (method) {
      case 'enable':
        obj.networkMonitor = true;
        if (commandParams && commandParams.exclude) {
          obj.exclude = commandParams.exclude;
        }
        break;
      case 'disable':
        obj.networkMonitor = false;
        delete obj.exclude;
        break;
      case 'setExtraHTTPHeaders':
        obj.httpHeaders = commandParams;
        break;
      default:
        dump('ERROR: unsupported debugger method "Network.' + method + '".\n');
        break;
    }
    if ('function' === typeof(callback)) {
      callback();
    }
  }

  module.exports = {
    register: function() {
      debuggerData.handlers['Network'] = processNetworkCommand;
    },

    unregister: function() {
      delete debuggerData.handlers['Network'];
    }
  };

}).call(this);

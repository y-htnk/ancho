(function() {

/*
   Client portion of Debugger API that provides access to events and methods.
   The actual implementation runs in the background window of the extension and
   fires the events. For the implementation, please see httpRequestObserver.js.
*/


  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var Cu = Components.utils;

  Cu.import('resource://gre/modules/Services.jsm');

  var Event = require('./event');
  var Utils = require('./utils');
  var observer = require('./httpRequestObserver');

  var DebuggerAPI = function(state, window) {
    this._state = state;
    this._tab = Utils.getWindowId(window);

    this.onEvent  = new Event(window, this._tab, this._state, 'debugger.event');
    this.onDetach = new Event(window, this._tab, this._state, 'debugger.detach');
  };

  DebuggerAPI.prototype = {

    attach: function(target, requiredVersion, callback) {
      // notify observer
      observer.debuggerAttach(target, requiredVersion);
      if (callback) {
        callback();
      }
    },

    detach: function(target, callback) {
      // notify observer
      observer.debuggerDetach(target);
      if (callback) {
        callback();
      }
    },

    sendCommand: function(target, method, commandParams, callback) {
      // shift args if commandParams skipped
      if (typeof(commandParams) === 'function') {
        callback = commandParams;
        commandParams = null;
      }
      // notify observer
      observer.debuggerSendCommand(target, method, commandParams, callback);
    }

  };

  module.exports = DebuggerAPI;

}).call(this);

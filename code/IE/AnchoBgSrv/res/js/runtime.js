/******************************************************************************
 * runtime.js
 * Part of Ancho browser extension framework
 * Implements chrome.runtime
 * Copyright 2013 Salsita software (http://www.salsitasoft.com).
 ******************************************************************************/

//******************************************************************************
//* requires
var Event = require("events.js").Event;
var EventFactory = require("utils.js").EventFactory;

require("runtime_spec.js");
var preprocessArguments = require("typeChecking.js").preprocessArguments;
var notImplemented = require("typeChecking.js").notImplemented;
var strippedCopy = require("utils.js").strippedCopy;

var EVENT_LIST = [
    //'onStartup',
    'onInstalled',
    //'onSuspend',
    //'onSuspendCanceled',
    //'onUpdateAvailable',
    'onConnect',
    'onConnectExternal',
    'onMessage',
    'onMessageExternal'
    //'onRestartRequired'
];
var API_NAME = 'runtime';

//******************************************************************************
var NOP = function() { /*dummy*/ }
var portPairs = {}
var addPortPair = function (pair, instanceID) {
  if (!portPairs[instanceID]) {
    portPairs[instanceID] = [];
  }
  portPairs[instanceID].push(pair);
}
exports.addPortPair = addPortPair;

function releasePorts(instanceID) {
  if (portPairs[instanceID]) {
    for (var i = 0; i < portPairs[instanceID].length; ++i) {
      portPairs[instanceID][i].release();
    }
    delete portPairs[instanceID];
  }
}

var Port = function(aName, aSender) {
  var self = this;
  this.postMessage = function(msg) {
    if (self.otherPort) {
      self.otherPort.onMessage.fire(msg);
    }
  };
  this.otherPort = null;
  this.sender = aSender;
  this.onDisconnect = new Event('port.onDisconnect');
  this.onMessage = new Event('port.onMessage');
  this.name = aName;

  this.disconnect = function() {
    self.otherPort.onDisconnect.fire();
    self.otherPort = null;
  }
  this.release = function() {
    delete onDisconnect;
    delete onMessage;
  }
};

/*
Contains both ends (ports) of communication channel used by connect()
*/
var PortPair = function(aName, aMessageSender) {
  var self = this;
  this.near = new Port(aName, aMessageSender);
  this.far = new Port(aName, aMessageSender);

  this.near.otherPort = this.far;
  this.far.otherPort = this.near;

  this.release = function() {
    self.near.disconnect();
    self.far.disconnect();
    self.near.release();
    self.far.release();
  }
}
exports.PortPair = PortPair;

/*
To sendMessage is passed responseCallback, which can be invoked only once
for all listeners and if requested also asynchronously. This object wrapps the original
callback and prohibits multiple invocations.
*/
var CallbackWrapper = function(responseCallback) {
  var self = this;

  this.shouldInvokeCallback = true;
  this.callback = function() {
      if (!self.shouldInvokeCallback) {
        return;
      }
      self.shouldInvokeCallback = false;

      var processedArguments = [];
      for (var i = 0; i < arguments.length; ++i) {
        //passed objects are copied and stripped of their methods
        processedArguments.push(strippedCopy(arguments[i]));
      }

      //Solves the 'Different array constructors' problem:
      //apply cannot be called because if array was created in different script engine
      //it fails comparison with array constructor
      addonAPI.callFunction(responseCallback, processedArguments);
    };
}
exports.CallbackWrapper = CallbackWrapper;

var MessageSender = function(aInstanceId) {
  this.id = aInstanceId;

  if (aInstanceId > 0) { //we are in tab context
    try {
      this.tab = serviceAPI.tabManager.getTabInfo(aInstanceId, addonAPI.id, aInstanceId);
      this.url = this.tab.url;
    } catch (e) {
      console.error("Internal error: getTabInfo() failed! Tab ID = " + aInstanceId + "; " + e.description + "; error code = " + e.number);
      this.tab = { "id": aInstanceId };
    }
  } else {
    this.tab = { //Dummy tab - reproducing chrome behavior
      "active": false,
      "highlighted": false,
      "id": -1,
      "incognito": false,
      "index": -1,
      "pinned": false,
      "selected": false,
      "status": "complete",
      "title": "", //TODO - get proper value for popup windows
      "url": "", //TODO - get proper value for popup windows
      "windowId": -1
    };
  }
};
exports.MessageSender = MessageSender;

var connectImplementation = function(extensionId, connectInfo, instanceID, apiName) {
  var name = (connectInfo != undefined) ? connectInfo.name : undefined;
  var pair = new PortPair(name, new MessageSender(instanceID));
  addPortPair(pair, instanceID);
  if (extensionId != undefined && extensionId != addonAPI.id) {
    serviceAPI.invokeExternalEventObject(
            extensionId,
            apiName + '.onConnectExternal',
            [pair.far]
            );
  } else {
    addonAPI.invokeEventObject(
            apiName + '.onConnect',
            -1, //TODO: after tabs API finished prevent content scripts from notifications
            true,
            [pair.far]
            );
  }
  return pair.near;
};
exports.connectImplementation = connectImplementation;

var sendMessageImplementation = function(extensionId, message, responseCallback, instanceID, apiName) {
  var sender = new MessageSender(instanceID);
  var callback = undefined;
  var ret = undefined;
  if (responseCallback) {
    var callbackWrapper = new CallbackWrapper(responseCallback);
    callback = callbackWrapper.callback;
  } else {
    callback = NOP;
  }
  if (extensionId && extensionId != addonAPI.id) {
    ret = serviceAPI.invokeExternalEventObject(
          extensionId,
          apiName + '.onMessageExternal',
          [message, sender, callback]
          );
  } else {
    ret = addonAPI.invokeEventObject(
          apiName + '.onMessage',
          instanceID,
          true, //we are skipping instanceID
          [message, sender, callback]
          );
  }
  if (callbackWrapper === undefined) {
    return;
  }

  //if responseCallaback not yet called, check if some of the listeners
  //requests asynchronous responseCallback, otherwise disable responseCallback
  if (callbackWrapper.shouldInvokeCallback && ret != undefined) {
    var arr = new VBArray(ret).toArray();
    for (var i = 0; i < arr.length; ++i) {
      if (arr[i] === true) {
        console.debug("Asynchronous call to responseCallback requested!");
        return;
      }
    }
  }
  callbackWrapper.shouldInvokeCallback = false;
};
exports.sendMessageImplementation = sendMessageImplementation;

var addonRootURL = 'chrome-extension://' + addonAPI.id + '/';
exports.addonRootURL = addonRootURL;

//******************************************************************************
//* main closure
var Runtime = function(instanceID) {
  //============================================================================
  // private variables
  var _instanceID = instanceID;

  //============================================================================
  // public properties

  //----------------------------------------------------------------------------
  // chrome.runtime.getBackgroundPage
  this.getBackgroundPage = function(callback) {
    var args = notImplemented('chrome.runtime.getBackgroundPage', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.getManifest
  //   returns JS object created from manifest file contents
  this.getManifest = function () {
    return manifest;
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.getURL
  //   returns   string
  this.getURL = function(path) {
    return addonRootURL + path;
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.setUninstallUrl
  this.setUninstallUrl = function(url) {
    var args = preprocessArguments('chrome.runtime.setUninstallUrl', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.reload
  this.reload = function() {
    notImplemented('chrome.runtime.reload', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.requestUpdateCheck
  this.requestUpdateCheck = function(callback) {
    var args = notImplemented('chrome.runtime.requestUpdateCheck', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.connect
  //   returns   Port
  this.connect = function(extensionId, connectInfo) {
    var args = preprocessArguments('chrome.runtime.connect', arguments);
    console.debug("runtime.connect(..) called");
    return connectImplementation(args.extensionId, args.connectInfo, _instanceID, API_NAME);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.connectNative
  this.connectNative = function(application) {
    var args = notImplemented('chrome.runtime.connectNative', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.sendMessage
  this.sendMessage = function(extensionId, message, responseCallback) {
    var args = preprocessArguments('chrome.runtime.sendMessage', arguments);
    console.debug("runtime.sendMessage(..) called: " + args.message);
    sendMessageImplementation(args.extensionId, args.message, args.responseCallback, _instanceID, API_NAME);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.sendNativeMessage
  this.sendNativeMessage = function(application, message, responseCallback) {
    var args = notImplemented('chrome.runtime.sendNativeMessage', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.getPlatformInfo
  this.getPlatformInfo = function(callback) {
    var args = notImplemented('chrome.runtime.getPlatformInfo', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.runtime.getPackageDirectoryEntry
  this.getPackageDirectoryEntry = function(callback) {
    var args = notImplemented('chrome.runtime.getPackageDirectoryEntry', arguments);
  };

  //============================================================================
  // events

  EventFactory.createEvents(this, instanceID, API_NAME, EVENT_LIST);

  //============================================================================
  //============================================================================
  // main initialization


}

exports.createAPI = function(instanceID) {
  return new Runtime(instanceID);
}

exports.releaseAPI = function(instanceID) {
  EventFactory.releaseEvents(instanceID, API_NAME, EVENT_LIST);

  releasePorts(instanceID);
}

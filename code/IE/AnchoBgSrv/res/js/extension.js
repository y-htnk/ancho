/******************************************************************************
 * extension.js
 * Part of Ancho browser extension framework
 * Implements chrome.extension
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 ******************************************************************************/

//******************************************************************************
//* requires
var Event = require("events.js").Event;

var EventFactory = require("utils.js").EventFactory;

require("extension_spec.js");
var preprocessArguments = require("typeChecking.js").preprocessArguments;
var notImplemented = require("typeChecking.js").notImplemented;
var strippedCopy = require("utils.js").strippedCopy;
var runtime = require("runtime.js");

var EVENT_LIST = ['onConnect',
                  'onConnectExternal',
                  'onMessage',
                  'onMessageExternal',
                  'onRequest' //deprecated
                  ];
var API_NAME = 'extension';

var PortPair = runtime.PortPair;
var addPortPair = runtime.addPortPair;

var MessageSender = runtime.MessageSender;
var CallbackWrapper = runtime.CallbackWrapper;
var connectImplementation = runtime.connectImplementation;
var sendMessageImplementation = runtime.sendMessageImplementation;
var addonRootURL = runtime.addonRootURL;



//******************************************************************************
//* main closure
var Extension = function(instanceID) {
  //============================================================================
  // private variables
  _instanceID = instanceID;
  //============================================================================
  // public properties

  this.lastError = null;
  this.inIncognitoContext = null;

  //============================================================================
  // public methods

  //----------------------------------------------------------------------------
  // chrome.extension.connect
  //   returns   Port
  this.connect = function(extensionId, connectInfo) {
    var args = preprocessArguments('chrome.extension.connect', arguments);
    console.debug("extension.connect(..) called");
    return connectImplementation(args.extensionId, args.connectInfo, _instanceID, API_NAME);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.getBackgroundPage
  //   returns   global
  this.getBackgroundPage = function() {
    var args = notImplemented('chrome.extension.getBackgroundPage', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.getURL
  //   returns   string
  this.getURL = function(path) {
    return addonRootURL + path;
  };

  //----------------------------------------------------------------------------
  // chrome.extension.getViews
  //   returns   array of global
  this.getViews = function(fetchProperties) {
    var args = notImplemented('chrome.extension.getViews', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.isAllowedFileSchemeAccess
  this.isAllowedFileSchemeAccess = function(callback) {
    var args = notImplemented('chrome.extension.isAllowedFileSchemeAccess', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.isAllowedIncognitoAccess
  this.isAllowedIncognitoAccess = function(callback) {
    var args = notImplemented('chrome.extension.isAllowedIncognitoAccess', arguments);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.sendMessage
  this.sendMessage = function(extensionId, message, responseCallback) {
    var args = preprocessArguments('chrome.extension.sendMessage', arguments);
    console.debug("extension.sendMessage(..) called: " + args.message);
    sendMessageImplementation(args.extensionId, args.message, args.responseCallback, _instanceID, API_NAME);
  };

  //----------------------------------------------------------------------------
  // chrome.extension.setUpdateUrlData
  this.setUpdateUrlData = function(data) {
    var args = notImplemented('chrome.extension.setUpdateUrlData', arguments);
  };

  //============================================================================
  // events

  EventFactory.createEvents(this, instanceID, API_NAME, EVENT_LIST);
  //============================================================================
  //============================================================================
  // main initialization


  this.testFunction = function() {
    return serviceAPI.testFunction();
  }
}

exports.createAPI = function(instanceID) {
  return new Extension(instanceID);
}

exports.releaseAPI = function(instanceID) {
  EventFactory.releaseEvents(instanceID, API_NAME, EVENT_LIST);
}

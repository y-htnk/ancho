/******************************************************************************
 * extension.js
 * Part of Ancho browser extension framework
 * Implements aji.extension
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 ******************************************************************************/

//******************************************************************************
//* requires
var Event = require("Event.js").Event;

//******************************************************************************
//* main closure
exports.createAPI = function(instanceID) {
  return new (function() {
    //============================================================================
    // private variables


    //============================================================================
    // public properties

    this.lastError = null;
    this.inIncognitoContext = null;

    //============================================================================
    // public methods

    //----------------------------------------------------------------------------
    // aji.extension.connect
    //   returns   Port
    this.connect = function(extensionId, connectInfo) {
      console.debug("extension.connect(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.getBackgroundPage
    //   returns   global
    this.getBackgroundPage = function() {
      console.debug("extension.getBackgroundPage(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.getURL
    //   returns   string
    this.getURL = function(path) {
      console.debug("extension.getURL(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.getViews
    //   returns   array of global
    this.getViews = function(fetchProperties) {
      console.debug("extension.getViews(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.isAllowedFileSchemeAccess
    this.isAllowedFileSchemeAccess = function(callback) {
      console.debug("extension.isAllowedFileSchemeAccess(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.isAllowedIncognitoAccess
    this.isAllowedIncognitoAccess = function(callback) {
      console.debug("extension.isAllowedIncognitoAccess(..) called");
    };

    //----------------------------------------------------------------------------
    // aji.extension.sendMessage
    this.sendMessage = function(extensionId, message, responseCallback) {
      console.debug("extension.sendMessage(..) called: " + message);
    };

    //----------------------------------------------------------------------------
    // aji.extension.setUpdateUrlData
    this.setUpdateUrlData = function(data) {
      console.debug("extension.setUpdateUrlData(..) called");
    };

    //============================================================================
    // events

    this.onConnect = new Event('extension.onConnect', instanceID);
    this.onConnectExternal = new Event('extension.onConnectExternal', instanceID);
    this.onMessage = new Event('extension.onMessage', instanceID);
    this.onMessageExternal = new Event('extension.onMessageExternal', instanceID);

    //============================================================================
    //============================================================================
    // main initialization

  })();
}
/*
(function(me){
  //============================================================================
  // private variables


  //============================================================================
  // public properties

  me.lastError = null;
  me.inIncognitoContext = null;

  //============================================================================
  // public methods

  //----------------------------------------------------------------------------
  // aji.extension.connect
  //   returns   Port
  me.connect = function(extensionId, connectInfo) {
    console.debug("extension.connect(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.getBackgroundPage
  //   returns   global
  me.getBackgroundPage = function() {
    console.debug("extension.getBackgroundPage(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.getURL
  //   returns   string
  me.getURL = function(path) {
    console.debug("extension.getURL(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.getViews
  //   returns   array of global
  me.getViews = function(fetchProperties) {
    console.debug("extension.getViews(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.isAllowedFileSchemeAccess
  me.isAllowedFileSchemeAccess = function(callback) {
    console.debug("extension.isAllowedFileSchemeAccess(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.isAllowedIncognitoAccess
  me.isAllowedIncognitoAccess = function(callback) {
    console.debug("extension.isAllowedIncognitoAccess(..) called");
  };

  //----------------------------------------------------------------------------
  // aji.extension.sendMessage
  me.sendMessage = function(extensionId, message, responseCallback) {
    console.debug("extension.sendMessage(..) called: " + message);
  };

  //----------------------------------------------------------------------------
  // aji.extension.setUpdateUrlData
  me.setUpdateUrlData = function(data) {
    console.debug("extension.setUpdateUrlData(..) called");
  };

  //============================================================================
  // events

  me.onConnect = new Event();
  me.onConnectExternal = new Event();
  me.onMessage = new Event();
  me.onMessageExternal = new Event();

  //============================================================================
  //============================================================================
  // main initialization


}).call(this, exports);
*/
/******************************************************************************
 * app.js
 * Part of Ancho browser extension framework
 * Implements chrome.app
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 ******************************************************************************/

// chrome.app is not properly documented - but it should provide access to information from manifest

// get manifest
var manifest = require("manifest").manifest;

//******************************************************************************
//* main closure
(function(me){
  //============================================================================
  // private variables


  //============================================================================
  // public properties

  me.getDetails = function() {
    return manifest;
  }

  //============================================================================
  //============================================================================
  // main initialization


}).call(this, exports);

exports.createAPI = function(instanceID) {
  //We don't need special instances
  return exports;
}

exports.releaseAPI = function(instanceID) {
  //Nothing needs to be released
}
/******************************************************************************
 * storage.js
 * Part of Ancho browser extension framework
 * Implements chrome.storage
 * Copyright 2012 Salsita software (http://www.salsitasoft.com).
 ******************************************************************************/

//******************************************************************************
//* requires
var Event = require("events.js").Event;
var EventFactory = require("utils.js").EventFactory;

var EVENT_LIST = ['onChanged'];
var API_NAME = 'storage';

var JSON = require("JSON.js");
var utils = require("utils.js");

require("storage_spec.js");
var preprocessArguments = require("typeChecking.js").preprocessArguments;
var notImplemented = require("typeChecking.js").notImplemented;

var StorageArea = function(aStorageType, instanceID) {
  var mStorageType = aStorageType;
  var mInstanceID = instanceID;

  this.get = function(keys, callback) {
    var args = preprocessArguments('chrome.storage.StorageArea.get', arguments);
    var items = {};
    var keyList = args.keys;
    var defaults = {}
    if (utils.isString(args.keys)) {
      keyList = [args.keys];
    }
    // If keys aren't string nor array it must be object providing
    // default values for requested keys.
    // We get array of attribute names and store orignal object as defaults
    if (!utils.isArray(args.keys)) {
      defaults = args.keys;
      keyList = [];
      for (var key in args.keys) {
        if (args.keys.hasOwnProperty(key)) {
          keyList.push(key);
        }
      }
    }

    // We cannot directly use data provided by C++ implementation
    // So we wrap the provided callback and preprocess the data before we call it
    var internalCallback = function(data) {
      try {
        // We obtained object with stringified attributes
        items = {};
        // Check all requsted keys - missing ones are set to default values (if provided)
        for (var key in data) {
          if (data.hasOwnProperty(key)) {
            var item = data[key];
            item = (item && JSON.parse(item)) || item;
            if (item != null && item != undefined) {
              items[key] = item;
            } else {
              items[key] = defaults[key];
            }
          }
        }
        args.callback(items);
      } catch (e) {
        console.error("Exception in chrome.storage." + mStorageType + ".get() callback: " + e.message);
      }
    }

    addonAPI.storageGet(mStorageType, keyList, internalCallback, addonAPI.id, mInstanceID);
  }

  this.getBytesInUse = function(keys, callback) {
    var args = notImplemented('chrome.storage.StorageArea.getBytesInUse', arguments);
    args.callback(-1);
  }

  this.set = function(items, callback) {
    var args = preprocessArguments('chrome.storage.StorageArea.set', arguments);
    var serializedValues = {};
    // All data passed to database must be stringified
    for (var key in args.items) {
      if (args.items.hasOwnProperty(key)) {
        var value = args.items[key];
        var serializedValue = JSON.stringify(value);
        serializedValues[key] = serializedValue;
      }
    }
    var internalCallback = function() {
      //TODO - fire event
      try {
        // Callback is not mandatory - check if available
        args.callback && args.callback();
      } catch (e) {
        console.error("Exception in chrome.storage." + mStorageType + ".set() callback: " + e.message);
      }
    }

    addonAPI.storageSet(mStorageType, serializedValues, internalCallback, addonAPI.id, mInstanceID);
  }

  this.remove = function(keys, callback) {
    var args = preprocessArguments('chrome.storage.StorageArea.remove', arguments);
    var keyList = args.keys;
    // addonAPI.storageRemove expects array
    if (utils.isString(keyList)) {
      keyList = [args.keys];
    }
    var internalCallback = function() {
      //TODO - fire event
      try {
        // Callback is not mandatory - check if available
        args.callback && args.callback();
      } catch (e) {
        console.error("Exception in chrome.storage." + mStorageType + ".remove() callback: " + e.message);
      }
    }
    addonAPI.storageRemove(mStorageType, keyList, internalCallback, addonAPI.id, mInstanceID);
  }

  this.clear = function(callback) {
    var args = preprocessArguments('chrome.storage.StorageArea.clear', arguments);

    var internalCallback = function() {
      //TODO - fire event
      try {
        // Callback is not mandatory - check if available
        args.callback && args.callback();
      } catch (e) {
        console.error("Exception in chrome.storage." + mStorageType + ".clear() callback: " + e.message);
      }
    }
    addonAPI.storageClear(mStorageType, internalCallback, addonAPI.id, mInstanceID);
  }

  this.QUOTA_BYTES = 880;
};

//******************************************************************************
//* main closure
exports.createAPI = function(instanceID) {
  return new (function() {
  //============================================================================
  // private variables


  //============================================================================
  // public properties

  this.sync = new StorageArea('sync', instanceID);
  this.local = new StorageArea('local', instanceID);
  //============================================================================
  // events

  EventFactory.createEvents(this, instanceID, API_NAME, EVENT_LIST);

  //============================================================================
  //============================================================================
  // main initialization


})();
}

exports.releaseAPI = function(instanceID) {
  EventFactory.releaseEvents(instanceID, API_NAME, EVENT_LIST);
}

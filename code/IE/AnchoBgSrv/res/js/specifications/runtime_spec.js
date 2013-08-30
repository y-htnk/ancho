var apiName = 'chrome.runtime';


//------------------------------------------------------------
//          Types extracted for chrome.runtime
//------------------------------------------------------------
var types = [
  {
    "id": "MessageSender",
    "properties": {
      "id": {
        "id": "id",
        "required": true,
        "type": "string"
      },
      "tab": {
        "id": "tab",
        "required": false,
        "type": "tabs.Tab"
      }
    },
    "type": "object"
  },
  {
    "id": "Port",
    "properties": {
      "name": {
        "id": "name",
        "required": true,
        "type": "string"
      },
      "onDisconnect": {
        "id": "onDisconnect",
        "required": true,
        "type": "events.Event"
      },
      "onMessage": {
        "id": "onMessage",
        "required": true,
        "type": "events.Event"
      },
      "postMessage": {
        "id": "postMessage",
        "required": true,
        "type": "function"
      },
      "sender": {
        "id": "sender",
        "required": false,
        "type": "MessageSender"
      }
    },
    "type": "object"
  }
];

//------------------------------------------------------------
//          Methods extracted for chrome.runtime
//------------------------------------------------------------
var methods = [
  {
    "id": "getBackgroundPage", 
    "items": [
      {
        "id": "callback", 
        "required": true, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "getManifest", 
    "items": [], 
    "type": "functionArguments"
  }, 
  {
    "id": "getURL", 
    "items": [
      {
        "id": "path", 
        "required": true, 
        "type": "string"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "setUninstallUrl", 
    "items": [
      {
        "id": "url", 
        "required": true, 
        "type": "string"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "reload", 
    "items": [], 
    "type": "functionArguments"
  }, 
  {
    "id": "requestUpdateCheck", 
    "items": [
      {
        "id": "callback", 
        "required": true, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "connect", 
    "items": [
      {
        "id": "extensionId", 
        "required": false, 
        "type": "string"
      }, 
      {
        "id": "connectInfo", 
        "properties": {
          "name": {
            "id": "name", 
            "required": false, 
            "type": "string"
          }
        }, 
        "required": false, 
        "type": "object"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "connectNative", 
    "items": [
      {
        "id": "application", 
        "required": true, 
        "type": "string"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "sendMessage", 
    "items": [
      {
        "id": "extensionId", 
        "required": false, 
        "type": "string"
      }, 
      {
        "id": "message", 
        "required": true, 
        "type": "any"
      }, 
      {
        "id": "responseCallback", 
        "required": false, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "sendNativeMessage", 
    "items": [
      {
        "id": "application", 
        "required": true, 
        "type": "string"
      }, 
      {
        "id": "message", 
        "required": true, 
        "type": "object"
      }, 
      {
        "id": "responseCallback", 
        "required": false, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "getPlatformInfo", 
    "items": [
      {
        "id": "callback", 
        "required": true, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }, 
  {
    "id": "getPackageDirectoryEntry", 
    "items": [
      {
        "id": "callback", 
        "required": true, 
        "type": "function"
      }
    ], 
    "type": "functionArguments"
  }
];


var typeChecking = require("typeChecking.js");
var validatorManager = typeChecking.validatorManager;

for (var i = 0; i < types.length; ++i) {
  validatorManager.addSpecValidatorWrapper(apiName + '.' + types[i].id, types[i]);
}

for (var i = 0; i < methods.length; ++i) {
  validatorManager.addSpecValidatorWrapper(apiName + '.' + methods[i].id, methods[i]);
}

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://ancho/modules/Require.jsm');
Cu.import('resource://ancho/modules/External.jsm');

var sandbox = Cu.Sandbox(window);
var baseURI = Services.io.newURI('resource://ancho/js/', '', null);
var require = Require.createRequireForWindow(sandbox, baseURI);

var ExtensionState = require('./state');
var applyContentScripts = require('./scripting').applyContentScripts;
var prepareWindow = require('./scripting').prepareWindow;
var loadHtml = require('./scripting').loadHtml;
var BrowserEvents = require('./browserEvents');
var Toolbar = require('./toolbarSingleton');
var Config = require('./config');
var httpObserver = require('./httpRequestObserver');
var WindowWatcher = require('./windowWatcher').WindowWatcher;

window.addEventListener('load', function(event) {
  window.removeEventListener('load', arguments.callee, false);
  ExtensionState.backgroundWindow = window;
  httpObserver.register(ExtensionState, window);

  WindowWatcher.register(function(win) {
    var browser = win.document.getElementById('content');
    if (browser) {
      var browserEvents = new BrowserEvents(browser, ExtensionState);
      ExtensionState.registerUnloader(win, browserEvents.init(applyContentScripts));
    }
  },
  function(win) {
    ExtensionState.unloadWindow(win);
  });

  var spec = Config.backgroundPage
        ? 'chrome-extension://ancho/' + Config.backgroundPage
        // Cannot use 'about:blank' here, because DOM for 'about:blank'
        // is inappropriate for script inserting: neither 'document.head'
        // nor 'document.body' are defined.
        : 'chrome://ancho/content/html/blank.html';

  var browser = document.getElementById('content');

  function runBackground() {
    loadHtml(document, browser, spec, function(targetWindow) {
      // load background scripts, if any
      for (var i = 0; i < Config.backgroundScripts.length; i++) {
        var script = targetWindow.document.createElement('script');
        script.src = Config.hostExtensionRoot + Config.backgroundScripts[i];
        targetWindow.document.head.appendChild(script);
      }
      AnchoExternal.__set(targetWindow.ancho.external);
    });
  }

  // We don't want the background scripts to run until the main browser window
  // has loaded since some of them may depend on it being ready.
  var browserWindow = Services.wm.getMostRecentWindow('navigator:browser');
  if ('complete' === browserWindow.document.readyState) {
    runBackground();
  }
  else {
    browserWindow.document.addEventListener('readystatechange', function(e) {
      if ('complete' === browserWindow.document.readyState) {
        browserWindow.document.removeEventListener('readystatechange', arguments.callee, false);
        // TODO: Figure out why loading the background window directly from the event listener
        // causes its compartment to leak when the extension is disabled.
        // Using setTimeout() is a workaround and appears to fix the problem.
        setTimeout(runBackground, 500);
      }
    }, false);
  }
}, false);

window.addEventListener('unload', function(event) {
  window.removeEventListener('unload', arguments.callee, false);
  AnchoExternal.__set(null);
  WindowWatcher.unload();
  ExtensionState.unloadAll();
}, false);

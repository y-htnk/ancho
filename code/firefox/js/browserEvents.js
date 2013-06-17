(function() {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var Cu = Components.utils;

  var Utils = require('./utils');

  var prepareWindow = require('./scripting').prepareWindow;

  function isContentBrowser(document) {
    if (!document.head || !document.body) {
      // Not an HTML document.
      return false;
    }
    if ('about:' === document.location.protocol) {
      return false;
    }
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=863303
    // Currently there is a bug in Firefox and tabs opened by the session
    // saver are erroneously given the readyState 'complete', so we
    // need this hack to check whether they have really been loaded.
    if (!document.head.hasChildNodes() && !document.body.hasChildNodes()) {
      return false;
    }
    return true;
  }

  function BrowserEvents(tabbrowser, extensionState) {
    this.init = function(contentLoadedCallback) {
      function onContentLoaded(document, isFrame) {
        var win = document.defaultView;
        var browser = tabbrowser.mCurrentBrowser;

        // We don't want to trigger the content scripts for about:blank.
        if (isContentBrowser(document)) {
          if (!isFrame) {
            if (browser._anchoCurrentLocation != document.location.href) {
              browser._anchoCurrentLocation = document.location.href;
              var tabId = Utils.getWindowId(browser.contentWindow);
              extensionState.eventDispatcher.notifyListeners('tab.updated', null,
                [ tabId, { url: document.location.href }, { id: tabId } ]);
            }
          }
          if (contentLoadedCallback) {
            contentLoadedCallback(document.defaultView, document.location.href, isFrame);
          }
        }
      }

      function unload() {
        tabbrowser.removeProgressListener(progressListener);
        tabbrowser.removeEventListener('DOMWindowCreated', onWindowCreated, false);
        container.removeEventListener('TabOpen', onTabOpen, false);
        container.removeEventListener('TabClose', onTabClose, false);
        container.removeEventListener('TabSelect', onTabSelect, false);
      }

      function onTabOpen(event) {
        let browser = tabbrowser.getBrowserForTab(event.target);
        browser._anchoCurrentLocation = browser.contentDocument.location.href;
        extensionState.eventDispatcher.notifyListeners('tab.created', null,
          [ { id: Utils.getWindowId(browser.contentWindow) } ]);
      }

      function onTabClose(event) {
        let browser = tabbrowser.getBrowserForTab(event.target);
        extensionState.eventDispatcher.notifyListeners('tab.removed', null,
          [ Utils.getWindowId(browser.contentWindow), {} ]);
      }

      function onTabSelect(event) {
        extensionState.eventDispatcher.notifyListeners('tab.activated', null,
          [ { tabId: Utils.getWindowId(tabbrowser.selectedBrowser.contentWindow) } ]);
      }

      function onWindowCreated(event) {
        var document = event.target;
        var win = document.defaultView;
        var isFrame = !!((document instanceof Ci.nsIDOMHTMLDocument) && win.frameElement);
        if (isFrame) {
          win.frameElement.addEventListener('load', function(event) {
            win.frameElement.removeEventListener('load', arguments.callee, false);
            onContentLoaded(win.frameElement.contentDocument, true);
          }, false);
        }
        else {
          document.addEventListener('readystatechange', function(event) {
            if ('interactive' === document.readyState) {
              document.removeEventListener('readystatechange', arguments.callee, false);
              onContentLoaded(document, false);
            }
          }, false);
        }

        if ('chrome-extension:' === document.location.protocol) {
          prepareWindow(win.wrappedJSObject);
        }
      }

      tabbrowser.addEventListener('DOMWindowCreated', onWindowCreated, false);

      var container = tabbrowser.tabContainer;
      container.addEventListener('TabOpen', onTabOpen, false);
      container.addEventListener('TabClose', onTabClose, false);
      container.addEventListener('TabSelect', onTabSelect, false);

      // Trigger the content loaded callback on any tabs that are already open.
      if (contentLoadedCallback) {
        for (var i=0; i<tabbrowser.browsers.length; i++) {
          let browser = tabbrowser.browsers[i];
          if (isContentBrowser(browser.contentDocument)) {
            let location = browser.contentDocument.location.href;
            browser._anchoCurrentLocation = location;
            if ('complete' === browser.contentDocument.readyState) {
              contentLoadedCallback(browser.contentWindow, location);
            }
          }
        }
      }

      return unload;
    };
  }

  module.exports = BrowserEvents;
}).call(this);

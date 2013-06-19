(function() {

  var Cu = Components.utils;

  Cu.import('resource://gre/modules/Services.jsm');

  //var Event = require('./event');
  var Utils = require('./utils');
  var WindowWatcher = require('./windowWatcher').WindowWatcher;
  var Config = require('./config');
  var Manifest = Config.manifest;

  const BUTTON_ID = '__ANCHO_BROWSER_ACTION_BUTTON__';
  const CANVAS_ID = '__ANCHO_BROWSER_ACTION_CANVAS__';
  const HBOX_ID = '__ANCHO_BROWSER_ACTION_HBOX__';
  const IMAGE_ID = '__ANCHO_BROWSER_ACTION_IMAGE__';
  const PANEL_ID = '__ANCHO_BROWSER_ACTION_PANEL__';
  const NAVIGATOR_TOOLBOX = 'navigator-toolbox';
  const TOOLBAR_ID = 'nav-bar';
  const BROWSER_ACTION_ICON_WIDTH = 19;
  const BROWSER_ACTION_ICON_HEIGHT = 19;

  var BrowserActionService = {
    iconType: null,
    badgeText: null,
    badgeBackgroundColor: '#f00',
    tabBadgeText: {},
    tabBadgeBackgroundColor: {},

    init: function() {
      // TODO: this.onClicked = new Event();
      if (Manifest.browser_action && Manifest.browser_action.default_icon) {
        this.iconType = 'browser_action';
      }
      else if (Manifest.page_action && Manifest.page_action.default_icon) {
        this.iconType = 'page_action';
      }

      if (this.iconType) {
        WindowWatcher.register(function(win, context) {
          this.startup(win);
          var tabbrowser = win.document.getElementById('content');
          var container = tabbrowser.tabContainer;
          context.listener = function() {
            this.setIcon(win, {});
          };
          container.addEventListener('TabSelect', context.listener, false);
        }.bind(this), function(win, context) {
          var tabbrowser = win.document.getElementById('content');
          var container = tabbrowser.tabContainer;
          container.removeEventListener('TabSelect', context.listener, false);
          this.shutdown(win);
        }.bind(this));
      }
    },

    _installAction: function(window, button, iconPath) {
      var document = window.document;
      var panel = document.createElement('panel');
      panel.id = PANEL_ID;
      var iframe = document.createElement('iframe');
      iframe.setAttribute('type', 'chrome');

      document.getElementById('mainPopupSet').appendChild(panel);
      panel.appendChild(iframe);

      var hbox = document.createElement('hbox');
      hbox.id = HBOX_ID;
      hbox.setAttribute('hidden', 'true');
      panel.appendChild(hbox);

      // Catch keypresses that propagate up to the panel so that they don't get processed
      // by the toolbar button.
      panel.addEventListener('keypress', function(event) {
        event.stopPropagation();
      }, false);

      button.addEventListener('click', function(event) {
        this.clickHandler(event);
      }.bind(this), false);
      this.setIcon(window, { path: iconPath });
    },

    installBrowserAction: function(window) {
      var id = BUTTON_ID;
      var document = window.document;
      if (document.getElementById(id)) {
        // We already have the toolbar button.
        return;
      }
      var toolbar = document.getElementById(TOOLBAR_ID);
      if (!toolbar) {
        // No toolbar in this window so we're done.
        return;
      }
      var toolbarButton = document.createElement('toolbarbutton');
      toolbarButton.setAttribute('id', id);
      toolbarButton.setAttribute('type', 'button');
      toolbarButton.setAttribute('removable', 'true');
      toolbarButton.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional');
      toolbarButton.setAttribute('label', Manifest.name);

      var iconPath = Config.hostExtensionRoot + Manifest.browser_action.default_icon;
      toolbarButton.style.listStyleImage = 'url(' + iconPath + ')';

      var palette = document.getElementById(NAVIGATOR_TOOLBOX).palette;
      palette.appendChild(toolbarButton);

      var currentset = toolbar.getAttribute('currentset').split(',');
      var index = currentset.indexOf(id);
      if (index === -1) {
        if (Config.firstRun) {
          // No button yet so add it to the toolbar.
          toolbar.appendChild(toolbarButton);
          toolbar.setAttribute('currentset', toolbar.currentSet);
          document.persist(toolbar.id, 'currentset');
        }
      }
      else {
        var before = null;
        for (var i=index+1; i<currentset.length; i++) {
          before = document.getElementById(currentset[i]);
          if (before) {
            toolbar.insertItem(id, before);
            break;
          }
        }
        if (!before) {
          toolbar.insertItem(id);
        }
      }

      this._installAction(window, toolbarButton, iconPath);
    },

    installPageAction: function(window) {
      var id = BUTTON_ID;
      var document = window.document;
      if (document.getElementById(id)) {
        // We already have the toolbar button.
        return;
      }
      var image = document.createElement('image');
      image.setAttribute('id', id);
      image.setAttribute('class', 'urlbar-icon');

      var iconPath = Config.hostExtensionRoot + Manifest.page_action.default_icon;
      image.style.listStyleImage = 'url(' + iconPath + ')';

      var icons = document.getElementById('urlbar-icons');
      icons.insertBefore(image, icons.firstChild);

      this._installAction(window, image, iconPath);
    },

    showPopup: function(panel, iframe, document) {
      panel.style.visibility = 'hidden';
      // Deferred loading of scripting.js since we have a circular reference that causes
      // problems if we load it earlier.
      var loadHtml = require('./scripting').loadHtml;
      loadHtml(document, iframe, 'chrome-extension://ancho/' + Manifest[this.iconType].default_popup, function() {
        iframe.contentDocument.addEventListener('readystatechange', function(event) {
          iframe.contentDocument.removeEventListener('readystatechange', arguments.callee, false);
          panel.style.removeProperty('visibility');
        }, false);
        var body = iframe.contentDocument.body;
        // Need to float the body so that it will resize to the contents of its children.
        if (!body.style.cssFloat) {
          body.style.cssFloat = 'left';
        }
        // We need to intercept link clicks and open them in the current browser window.
        body.addEventListener('click', function(event) {
          var link = event.target;
          if (link.href) {
            event.preventDefault();
            var browser = document.getElementById('content');
            if ('_newtab' === link.target) {
              browser.selectedTab = browser.addTab(link.href);
            }
            else {
              browser.contentWindow.open(link.href, link.target);
            }
            panel.hidePopup();
            return false;
          }
        }, false);

        // Remember the height and width of the popup.
        // Check periodically and resize it if necessary.
        var oldHeight = 0,
          oldWidth = 0;
        function getPanelBorderWidth(which) {
          return parseFloat(document.defaultView.getComputedStyle(panel)['border' + which + 'Width']);
        }

        function resizePopup() {
          if (body.scrollHeight !== oldHeight && body.scrollWidth !== oldWidth) {
            oldHeight = iframe.height = body.scrollHeight + 1;
            oldWidth = iframe.width = body.scrollWidth + 1;
            panel.sizeTo(oldWidth + getPanelBorderWidth('Left') + getPanelBorderWidth('Right'),
              oldHeight + getPanelBorderWidth('Top') + getPanelBorderWidth('Bottom'));
          }
        }
        iframe.contentDocument.addEventListener('MozScrolledAreaChanged', function(event) {
          resizePopup();
        }, false);
        iframe.contentWindow.close = function() {
          panel.hidePopup();
        };
      });
    },

    clickHandler: function(event) {
      var document = event.target.ownerDocument;
      if (event.target !== document.getElementById(BUTTON_ID)) {
        // Only react when button itself is clicked (i.e. not the panel).
        return;
      }
      var self = this;
      var button = event.target;
      var panel = document.getElementById(PANEL_ID);
      var iframe = panel.firstChild;
      iframe.setAttribute('src', 'about:blank');
      panel.addEventListener('popupshowing', function(event) {
        panel.removeEventListener('popupshowing', arguments.callee, false);
        self.showPopup(panel, iframe, document);
      }, false);
      panel.openPopup(button, 'after_start', 0, 0, false, false);
    },

    _drawButton: function(tabId, button, canvas) {
      var ctx = canvas.getContext('2d');
      ctx.textBaseline = 'top';
      ctx.font = 'bold 9px sans-serif';

      var text = this.getBadgeText(tabId);
      if (text)
      {
        var w = ctx.measureText(text).width;
        var h = 7;

        var rp = ((canvas.width - 4) > w) ? 2 : 1; // right padding = 2, or 1 if text is wider
        var x = canvas.width - w - rp;
        var y = canvas.height - h - 1; // 1 = bottom padding

        var color = this.getBadgeBackgroundColor(tabId);
        ctx.fillStyle = color;
        ctx.fillRect(x-rp, y-1, w+rp+rp, h+2);
        ctx.fillStyle = '#fff'; // text color
        ctx.fillText(text, x, y);
      }

      button.image = canvas.toDataURL('image/png', '');  // set new toolbar image
    },

    setIcon: function(window, details) {
      var document = window.document;
      var hbox = document.getElementById(HBOX_ID);
      var canvas = document.getElementById(CANVAS_ID);
      if (!canvas) {
        canvas = document.createElementNS('http://www.w3.org/1999/xhtml', 'canvas');
        canvas.id = CANVAS_ID;
        hbox.appendChild(canvas);
      }
      var img = document.getElementById(IMAGE_ID);
      if (!img) {
        img = document.createElementNS('http://www.w3.org/1999/xhtml', 'img');
        img.id = IMAGE_ID;
        hbox.appendChild(img);
      }

      canvas.setAttribute('width', BROWSER_ACTION_ICON_WIDTH);
      canvas.setAttribute('height', BROWSER_ACTION_ICON_HEIGHT);
      var ctx = canvas.getContext('2d');

      var button = document.getElementById(BUTTON_ID);
      var browser = document.getElementById('content');
      var tabId = Utils.getWindowId(browser.contentWindow);

      var self = this;
      if (details.path) {
        img.onload = function() {
          ctx.clearRect(0, 0, canvas.width, canvas.height);
          ctx.drawImage(img, 0, 0);
          self._drawButton(tabId, button, canvas, ctx);
        };
        img.src = details.path;
      }
      else if (details.imageData) {
        ctx.putImageData(details.imageData, 0, 0);
        self._drawButton(tabId, button, canvas, ctx);
      }
      else {
        // No image provided so just update the badge using the existing image.
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.drawImage(img, 0, 0);
        self._drawButton(tabId, button, canvas, ctx);
      }
    },

    updateIcon: function(details) {
      var self = this;
      WindowWatcher.forAllWindows(function(window) {
        self.setIcon(window, details);
      });
    },

    shutdown: function(window) {
      var document = window.document;
      var button = document.getElementById(BUTTON_ID);
      var parent = button.parentNode;
      if (parent.contains(button)) {
        button.parentNode.removeChild(button);
      }
      var panel = document.getElementById(PANEL_ID);
      parent = panel.parentNode;
      if (parent.contains(panel)) {
        parent.removeChild(panel);
      }
    },

    startup: function(window) {
      switch(this.iconType) {
        case 'browser_action':
          this.installBrowserAction(window);
          break;
        case 'page_action':
          this.installPageAction(window);
          break;
      }
    },

    getBadgeBackgroundColor: function(tabId) {
      if (this.tabBadgeBackgroundColor[tabId]) {
        return this.tabBadgeBackgroundColor[tabId];
      }
      else {
        return this.badgeBackgroundColor;
      }
    },

    getBadgeText: function(tabId) {
      if (this.tabBadgeText[tabId]) {
        return this.tabBadgeText[tabId];
      }
      else {
        return this.badgeText;
      }
    },

    setBadgeBackgroundColor: function(tabId, color) {
      if ('undefined' !== typeof(tabId)) {
        this.tabBadgeBackgroundColor[tabId] = color;
      }
      else {
        this.badgeBackgroundColor = color;
      }
      this.updateIcon({});
    },

    setBadgeText: function(tabId, text) {
      if ('undefined' !== typeof(tabId)) {
        this.tabBadgeText[tabId] = text;
      }
      else {
        this.badgeText = text;
      }
      this.updateIcon({});
    }
  };

  // Start the service once
  BrowserActionService.init();

  var BrowserActionAPI = function() {
  };

  BrowserActionAPI.prototype.getBadgeBackgroundColor = function(details, callback) {
    callback(BrowserActionService.getBadgeBackgroundColor(details.tabId));
  };

  BrowserActionAPI.prototype.getBadgeText = function(details, callback) {
    callback(BrowserActionService.getBadgeText(details.tabId));
  };

  BrowserActionAPI.prototype.getPopup = function() {
    throw new Error('Unsupported method');
  };

  BrowserActionAPI.prototype.getTitle = function() {
    throw new Error('Unsupported method');
  };

  BrowserActionAPI.prototype.setBadgeBackgroundColor = function(details) {
    function colorToString(color) {
      if ('string' === typeof(color)) {
        // TODO: Support three digit RGB codes.
        if (/#[0-9A-F]{6}/i.exec(color)) {
          return color;
        }
      } else if (Array.isArray(color)){
        if (color.length === 3) {
          // TODO: Support alpha.
          var str = '#';
          for (var i = 0; i < 3; ++i) {
            var tmp = Math.max(0, Math.min(color[i], 255));
            str += tmp.toString(16);
          }
          return str;
        }
      }
      throw new Error('Unsupported color format');
    }

    BrowserActionService.setBadgeBackgroundColor(details.tabId, colorToString(details.color));
  };

  BrowserActionAPI.prototype.setBadgeText = function(details) {
    BrowserActionService.setBadgeText(details.tabId, details.text);
  };

  BrowserActionAPI.prototype.setPopup = function() {
    throw new Error('Unsupported method');
  };

  BrowserActionAPI.prototype.setTitle = function() {
    throw new Error('Unsupported method');
  };

  BrowserActionAPI.prototype.setIcon = function(details) {
    BrowserActionService.updateIcon(details);
  };


  module.exports = BrowserActionAPI;

}).call(this);

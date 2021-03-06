(function() {

  var Cu = Components.utils;
  var Cc = Components.classes;
  var Ci = Components.interfaces;

  Cu.import('resource://gre/modules/Services.jsm');

  var Event = require('./events').Event;
  var Utils = require('./utils');

  var COOKIE_CHANGED = 'cookie-changed';
  var COOKIE_CHANGED_DATA_ADDED = 'added';
  var COOKIE_CHANGED_DATA_CHANGED = 'changed';
  var COOKIE_CHANGED_DATA_DELETED = 'deleted';

  var CookiesAPI = function(extension) {
    this.onChanged = new Event(extension, 'cookie.changed');
    Services.obs.addObserver(this, COOKIE_CHANGED, false);
    var self = this;
    extension.once('unload', function() {
      Services.obs.removeObserver(self, COOKIE_CHANGED, false);
    });
  };

  CookiesAPI.prototype.observe = function(subject, topic, data) {
    try {
      if (topic === COOKIE_CHANGED) {
        var changed = data === COOKIE_CHANGED_DATA_CHANGED
            || data === COOKIE_CHANGED_DATA_ADDED
            || data === COOKIE_CHANGED_DATA_DELETED;
        // TODO: handle batch-deleted and cleared events
        // https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsICookieService
        if (changed) {
          var cookie = this.toCookie(subject);
          this.onChanged.fire({
            cookie : cookie,
            removed : data === COOKIE_CHANGED_DATA_DELETED
          });
        }

      }
    } catch (e) {
      dump('error:  ' + e + '\n');
    }
  };

  CookiesAPI.prototype.parseUrl = function(url) {
    return Services.io.newURI(url, null, null);
  };

  CookiesAPI.prototype.toCookie = function(cookieItem) {
    var nsCookie = cookieItem.QueryInterface(Ci.nsICookie);
    return {
      name : nsCookie.name,
      value : nsCookie.value,
      domain : nsCookie.host,
      hostOnly : nsCookie.isDomain,
      path : nsCookie.path,
      secure : nsCookie.isSecure,
      // httpOnly is not supported in FireFox
      session : nsCookie.expires == 0 ? true : false,
      expirationDate : nsCookie.expires
    };
  };

  /**
   * @param details
   *          {url, name, domain, path, secure, session}
   * @param callback
   *          function([cookies])
   */
  CookiesAPI.prototype.getAll = function(details, callback) {
    var urlRegexp = 'url' in details ? new RegExp('^' + details.url) : null;
    var domainRegexp = 'domain' in details ? new RegExp('([^.]*\\.)?'
        + details.domain) : null;
    var pathRegexp = 'path' in details ? new RegExp('^' + details.path + '$')
        : null;
    var enumerator = Services.cookies.enumerator;
    var result = [];
    while (enumerator.hasMoreElements()) {
      var next = enumerator.getNext();
      var cookie = this.toCookie(next);
      var condition = true;
      if (urlRegexp) {
        condition &= urlRegexp.test(cookie.domain + cookie.path);
      }
      if (domainRegexp) {
        condition &= domainRegexp.test(cookie.domain);
      }
      if (pathRegexp) {
        condition &= pathRegexp.test(cookie.path);
      }
      if ('name' in details) {
        condition &= details.name == cookie.name;
      }
      if ('secure' in details) {
        condition &= details.secure == cookie.secure;
      }
      if ('session' in details) {
        condition &= details.session == cookie.secure;
      }
      if (condition) {
        result.push(cookie);
      }
    }
    callback(result);
  };

  CookiesAPI.prototype.get = function(details, callback) {
    var url = this.parseUrl(details.url);
    var enumerator = Services.cookies.getCookiesFromHost(url.host);
    while (enumerator.hasMoreElements()) {
      var next = enumerator.getNext();
      var cookie = this.toCookie(next);
      var condition = true;
      condition &= url.host == cookie.domain;
      condition &= url.path.indexOf(cookie.path) == 0;
      condition &= details.name == cookie.name;
      if (condition) {
        callback(cookie);
        return;
      }
    }
    // not found
    callback(null);
  };

  /**
   * @param details
   *          {url, name}
   * @param callback
   *          function({url, name})
   */
  CookiesAPI.prototype.remove = function(details, callback) {
    var url = this.parseUrl(details.url);
    Services.cookies.remove(url.host, details.name, url.path, false);
    if (callback) {
      callback({
        url : url.protocol + '://' + url.host + url.path,
        name : name
      });
    }
  };

  module.exports = CookiesAPI;

}).call(this);

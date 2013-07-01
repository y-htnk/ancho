(function() {

  Cu.import('resource://gre/modules/Services.jsm');

  const Cc = Components.classes;

  let utils = require('./utils');

  let messages = {};
  let loaded = false;

  function loadMessages(extension) {
    loaded = true;
    let localeDir = extension.rootDirectory;
    localeDir.append('_locales');
    if (localeDir.exists()) {
      let entries = localeDir.directoryEntries;
      while (entries.hasMoreElements()) {
        let entry = entries.getNext().QueryInterface(Ci.nsIFile);
        let locale = entry.leafName;
        entry.append('messages.json');
        if (entry.exists()) {
          let entryURI = Services.io.newFileURI(entry);
          let json = utils.readStringFromUrl(entryURI);
          messages[locale] = JSON.parse(json);
        }
      }
    }
  }

  function I18nAPI(extension) {
    if (!loaded) {
      loadMessages(extension);
    }
    this._defaultLocale = extension.manifest.default_locale;
  }

  I18nAPI.prototype = {
    getMessage: function(messageName, substitutions) {
      if (messages[this._defaultLocale]) {
        var info = messages[this._defaultLocale][messageName];
        if (info && info.message) {
          // TODO - substitutions
          return info.message.replace(/\$\$/g, '$');
        }
      }
      return '';
    }
  };

  module.exports = I18nAPI;

}).call(this);

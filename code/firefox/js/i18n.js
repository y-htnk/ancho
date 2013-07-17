(function() {

  Cu.import('resource://gre/modules/Services.jsm');

  const Cc = Components.classes;

  let utils = require('./utils');

  let messages = {};
  
  function loadMessages(extension) {
    messages[extension.id] = {};
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
          messages[extension.id][locale] = JSON.parse(json);
        }
      }
    }
  }

  function I18nAPI(extension) {
    if (!(extension.id in messages)) {
      loadMessages(extension);
    }
    this._messages = messages[extension.id];
    this._defaultLocale = extension.manifest.default_locale;
  }

  I18nAPI.prototype = {
    getMessage: function(messageName, substitutions) {
      if (this._messages[this._defaultLocale]) {
        var info = this._messages[this._defaultLocale][messageName];
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

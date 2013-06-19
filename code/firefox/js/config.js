(function() {

  module.exports = {
    manifest: null,
    // Are we running it for the first time?
    firstRun: false,
    // Path to hosted browser extension code.
    hostExtensionPath: '/content/chrome-ext/',
    // Init function for calculated values.
    _init: function() {
      // A URL prefix to hosted browser extesnion code.
      this.hostExtensionRoot = 'chrome://ancho' + this.hostExtensionPath;
      return this;
    }
  }._init();

}).call(this);

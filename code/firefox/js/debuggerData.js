(function() {

  // Singleton for debugger protocol data.
  // Mapping tab id --> object representing debugging state
  var debuggerData = {};

  // Singleton for sendCommand handlers.
  // Mapping protocol domain ---> function hadling the domain
  var debuggerHandlers = {};


  module.exports = {
    data: debuggerData,
    handlers: debuggerHandlers
  };

}).call(this);

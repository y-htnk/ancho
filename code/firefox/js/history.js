(function() {

  var Cc = Components.classes;
  var Ci = Components.interfaces;

  var HistoryAPI = function(extension) {
    this._historyService = Cc['@mozilla.org/browser/nav-history-service;1']
        .getService(Ci.nsINavHistoryService);
  };

  HistoryAPI.prototype.search = function(details, callback) {
    var query = this._historyService.getNewQuery();
    query.beginTime = details.startTime;
    query.endTime = details.endTime === undefined ? 0 : details.endTime;
    if (details.text && (details.text != '')) {
      query.searchTerms = details.text;
    }
    var options = this._historyService.getNewQueryOptions();
    options.maxResults = details.maxResults;
    var result = this._historyService.executeQuery(query, options);
    var cont = result.root;
    cont.containerOpen = true;
    var items = [];
    for ( var i = 0; i < cont.childCount; i++) {
      var node = cont.getChild(i);
      // see : https://developer.mozilla.org/en/nsINavHistoryResultNode
      items.push({
        url : node.uri,
        lastVisitTime : node.time,
        visitCount : node.accessCount
      });
    }
    cont.containerOpen = false;
    callback(items);
  };

  module.exports = HistoryAPI;

}).call(this);

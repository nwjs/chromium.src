var manifest = chrome.runtime.getManifest();
var url = manifest.cmdlineUrl || 'nw_blank.html';
chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create(
      url,
      {'id': 'nwjs_default_app', 'height': 550, 'width': 750});
});

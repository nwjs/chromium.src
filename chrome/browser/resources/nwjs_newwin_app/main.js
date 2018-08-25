var manifest = chrome.runtime.getManifest();
var url = manifest.cmdlineUrl || 'nw_blank.html';
chrome.app.runtime.onLaunched.addListener(function() {
  console.log("NEWWIN MODE!");
  chrome.windows.create(
      {'url': url, 'height': 550, 'width': 750});
});

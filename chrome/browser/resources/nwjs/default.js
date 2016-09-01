(function() {
let manifest = chrome.runtime.getManifest();
nw.Window.open(manifest.main, manifest.window || {});
})()

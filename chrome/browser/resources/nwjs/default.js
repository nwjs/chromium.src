//console.log("NWJS/DEFAULT.JS");
var manifest = chrome.runtime.getManifest();
var options = Object.assign({id: '.main'}, manifest.window);
nw.Window.open(manifest.main, options);
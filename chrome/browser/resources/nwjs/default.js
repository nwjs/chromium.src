//console.log("NWJS/DEFAULT.JS");
var manifest = chrome.runtime.getManifest();
var options = {};
var title = null;
if (manifest.window) {
  if (manifest.window.id)
    options.id = manifest.window.id;
  options.innerBounds = {};
  if (manifest.window.frame === false)
    options.frame = 'none';
  if (manifest.window.resizable === false)
    options.resizable = false;
  if (manifest.window.height)
    options.innerBounds.height = manifest.window.height;
  if (manifest.window.width)
    options.innerBounds.width = manifest.window.width;
  if (manifest.window.min_width)
    options.innerBounds.minWidth = manifest.window.min_width;
  if (manifest.window.max_width)
    options.innerBounds.maxWidth = manifest.window.max_width;
  if (manifest.window.min_height)
    options.innerBounds.minHeight = manifest.window.min_height;
  if (manifest.window.max_height)
    options.innerBounds.maxHeight = manifest.window.max_height;
  if (manifest.window.fullscreen === true)
    options.state = 'fullscreen';
  if (manifest.window.show === false)
    options.hidden = true;
  if (manifest.window.show_in_taskbar === false)
    options.show_in_taskbar = false;
  if (manifest.window['always_on_top'] === true)
    options.alwaysOnTop = true;
  if (manifest.window['visible_on_all_workspaces'] === true)
    options.visibleOnAllWorkspaces = true;
  if (manifest.window.transparent)
    options.alphaEnabled = true;
  if (manifest.window.kiosk === true)
    options.kiosk = true;
  if (manifest.window.position)
    options.position = manifest.window.position;
  if (manifest.window.title)
    options.title = manifest.window.title;
}
chrome.app.window.create(manifest.main, options, function(win) {
});

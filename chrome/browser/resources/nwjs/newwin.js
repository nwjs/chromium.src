//console.log("NWJS/DEFAULT.JS");
var manifest = chrome.runtime.getManifest();
var options = { 'url' : manifest.main, 'type': 'popup' };
var title = null;
var splash = null;
if (manifest.window) {
  //options.innerBounds = {};
  if (manifest.window.frame === false)
    options.frameless = true;
  if (manifest.window.resizable === false)
    options.resizable = false;
  if (manifest.window.height)
    options.height = manifest.window.height;
  if (manifest.window.width)
    options.width = manifest.window.width;
  if (manifest.window.min_width)
    options.minWidth = manifest.window.min_width;
  if (manifest.window.max_width)
    options.maxWidth = manifest.window.max_width;
  if (manifest.window.min_height)
    options.minHeight = manifest.window.min_height;
  if (manifest.window.max_height)
    options.maxHeight = manifest.window.max_height;
  if (manifest.window.fullscreen === true)
    options.state = 'fullscreen';
  if (manifest.window.show === false)
    options.hidden = true;
  if (manifest.window.show_in_taskbar === false)
    options.showInTaskbar = false;
  if (manifest.window['always_on_top'] === true)
    options.alwaysOnTop = true;
  if (manifest.window['visible_on_all_workspaces'] === true)
    options.allVisible = true;
  if (manifest.window.transparent)
    options.alphaEnabled = true;
  if (manifest.window.kiosk === true)
    options.kiosk = true;
  if (manifest.window.position)
    options.position = manifest.window.position;
  if (manifest.window.icon)
    options.icon = manifest.window.icon;
  if (manifest.window.title)
    options.title = manifest.window.title;
  if (manifest.window.id)
    options.id = manifest.window.id;
  splash = nwSplashConfig(manifest.window.splash);
  // The main window stays hidden behind the splash until it has loaded.
  if (splash)
    options.hidden = true;
}

// Normalizes the `window.splash` manifest field: either the path/URL of the
// page or image to show, or an object {url, width, height, min_duration,
// transparent}. Returns null when no usable splash is configured.
function nwSplashConfig(value) {
  if (typeof value === 'string')
    value = { 'url': value };
  if (!value || typeof value !== 'object' ||
      typeof value.url !== 'string' || !value.url)
    return null;
  var positive = function(n) {
    return typeof n === 'number' && isFinite(n) && n > 0 ? n : 0;
  };
  return {
    'url': value.url,
    'width': Math.round(positive(value.width)),
    'height': Math.round(positive(value.height)),
    'min_duration': positive(value.min_duration),
    'transparent': value.transparent === true
  };
}

// Calls back with the splash window size: the configured size, else the
// natural size of the splash image, else 400x300.
function nwSplashSize(cfg, callback) {
  var fallback = { 'width': cfg.width || 400, 'height': cfg.height || 300 };
  if ((cfg.width && cfg.height) ||
      !/\.(png|jpe?g|gif|webp|bmp|svg|ico)([?#]|$)/i.test(cfg.url)) {
    callback(fallback);
    return;
  }
  var img = new Image();
  img.onload = function() {
    callback({ 'width': cfg.width || img.naturalWidth || fallback.width,
               'height': cfg.height || img.naturalHeight || fallback.height });
  };
  img.onerror = function() { callback(fallback); };
  img.src = cfg.url;
}

// Shows the splash window, creates the main window hidden, and once the main
// window has loaded (and `min_duration` ms have passed since the splash
// appeared) shows the main window and closes the splash.
function nwShowSplash(cfg) {
  var state = { splashId: null, splashStart: Date.now(), splashReady: false,
                mainId: null, mainLoaded: false, done: false,
                loadedWindows: [] };

  var cleanup = function() {
    chrome.tabs.onUpdated.removeListener(onUpdated);
    chrome.windows.onRemoved.removeListener(onRemoved);
  };

  var finish = function() {
    if (state.done || !state.mainLoaded || !state.splashReady)
      return;
    state.done = true;
    var wait = state.splashId === null ? 0 :
        Math.max(0, cfg.min_duration - (Date.now() - state.splashStart));
    setTimeout(function() {
      if (manifest.window.show !== false)
        chrome.windows.update(state.mainId, {'show': true});
      if (state.splashId !== null)
        chrome.windows.remove(state.splashId);
      cleanup();
    }, wait);
  };

  // Same signal as the `loaded` event of nw.Window.
  var onUpdated = function(tabId, changeInfo, tab) {
    if (!('nwstatus' in changeInfo) || changeInfo.nwstatus != 'complete')
      return;
    if (state.mainId === null) {
      state.loadedWindows.push(tab.windowId);
      return;
    }
    if (tab.windowId !== state.mainId)
      return;
    state.mainLoaded = true;
    finish();
  };

  // Do not leave the splash behind if the main window goes away first.
  var onRemoved = function(windowId) {
    if (state.done || state.mainId === null || windowId !== state.mainId)
      return;
    state.done = true;
    if (state.splashId !== null)
      chrome.windows.remove(state.splashId);
    cleanup();
  };

  chrome.tabs.onUpdated.addListener(onUpdated);
  chrome.windows.onRemoved.addListener(onRemoved);

  chrome.windows.create(options, function(win) {
    if (!win) {
      // No main window: close the splash now or as soon as it exists.
      state.done = true;
      if (state.splashId !== null)
        chrome.windows.remove(state.splashId);
      cleanup();
      return;
    }
    state.mainId = win.id;
    if (state.loadedWindows.indexOf(win.id) !== -1)
      state.mainLoaded = true;
    finish();
  });

  nwSplashSize(cfg, function(size) {
    state.splashStart = Date.now();
    var splashOptions = {
      'url': cfg.url,
      'type': 'popup',
      'frameless': true,
      'resizable': false,
      'width': size.width,
      'height': size.height,
      'position': 'center',
      'alwaysOnTop': true,
      'showInTaskbar': false
    };
    if (cfg.transparent)
      splashOptions.alphaEnabled = true;
    chrome.windows.create(splashOptions, function(win) {
      if (win) {
        if (state.done)
          chrome.windows.remove(win.id);  // main window already gone
        else
          state.splashId = win.id;
      }
      state.splashReady = true;
      finish();
    });
  });
}

if (splash) {
  nwShowSplash(splash);
} else {
  chrome.windows.create(options, function(win) {
  });
}

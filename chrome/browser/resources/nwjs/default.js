//console.log("NWJS/DEFAULT.JS");
var manifest = chrome.runtime.getManifest();
var options = {};
var splash = null;
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
  var state = { splashWin: null, splashStart: Date.now(), splashReady: false,
                mainWin: null, mainLoaded: false, done: false };

  var finish = function() {
    if (state.done || !state.mainLoaded || !state.splashReady)
      return;
    state.done = true;
    var wait = state.splashWin === null ? 0 :
        Math.max(0, cfg.min_duration - (Date.now() - state.splashStart));
    setTimeout(function() {
      if (manifest.window.show !== false)
        state.mainWin.show();
      if (state.splashWin !== null)
        state.splashWin.close();
    }, wait);
  };

  var onMainLoaded = function() {
    state.mainLoaded = true;
    finish();
  };

  chrome.app.window.create(manifest.main, options, function(win) {
    if (!win) {
      // No main window: close the splash now or as soon as it exists.
      state.done = true;
      if (state.splashWin !== null)
        state.splashWin.close();
      return;
    }
    state.mainWin = win;
    // Do not leave the splash behind if the main window goes away first.
    win.onClosed.addListener(function() {
      if (state.done)
        return;
      state.done = true;
      if (state.splashWin !== null)
        state.splashWin.close();
    });
    var contentWindow = win.contentWindow;
    if (contentWindow.document.readyState === 'complete')
      onMainLoaded();
    else
      contentWindow.addEventListener('load', onMainLoaded);
  });

  nwSplashSize(cfg, function(size) {
    state.splashStart = Date.now();
    var splashOptions = {
      'frame': 'none',
      'resizable': false,
      'alwaysOnTop': true,
      'show_in_taskbar': false,
      'innerBounds': { 'width': size.width, 'height': size.height }
    };
    if (cfg.transparent)
      splashOptions.alphaEnabled = true;
    chrome.app.window.create(cfg.url, splashOptions, function(win) {
      if (win) {
        if (state.done)
          win.close();  // main window already gone
        else
          state.splashWin = win;
      }
      state.splashReady = true;
      finish();
    });
  });
}

if (splash)
  nwShowSplash(splash);
else
  chrome.app.window.create(manifest.main, options);


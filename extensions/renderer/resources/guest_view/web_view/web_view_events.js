// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for WebView.

var CreateEvent = require('guestViewEvents').CreateEvent;
var DeclarativeWebRequestSchema =
    requireNative('schema_registry').GetSchema('declarativeWebRequest');
var EventBindings = require('event_bindings');
var GuestViewEvents = require('guestViewEvents').GuestViewEvents;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var WebRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebViewActionRequests =
    require('webViewActionRequests').WebViewActionRequests;

var WebRequestMessageEvent = CreateEvent('webViewInternal.onMessage');

function WebViewEvents(webViewImpl) {
  GuestViewEvents.call(this, webViewImpl);

  this.setupWebRequestEvents();
  this.view.maybeSetupContextMenus();
}

WebViewEvents.prototype.__proto__ = GuestViewEvents.prototype;

// A dictionary of <webview> extension events to be listened for. This
// dictionary augments |GuestViewEvents.EVENTS| in guest_view_events.js. See the
// documentation there for details.
WebViewEvents.EVENTS = {
  'close': {
    evt: CreateEvent('webViewInternal.onClose')
  },
  'consolemessage': {
    evt: CreateEvent('webViewInternal.onConsoleMessage'),
    fields: ['level', 'message', 'line', 'sourceId']
  },
  'contentload': {
    evt: CreateEvent('webViewInternal.onContentLoad')
  },
  'dialog': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url'],
    handler: 'handleDialogEvent'
  },
  'droplink': {
    evt: CreateEvent('webViewInternal.onDropLink'),
    fields: ['url']
  },
  'exit': {
    evt: CreateEvent('webViewInternal.onExit'),
    fields: ['processId', 'reason']
  },
  'exitfullscreen': {
    evt: CreateEvent('webViewInternal.onExitFullscreen'),
    fields: ['url'],
    handler: 'handleFullscreenExitEvent',
    internal: true
  },
  'findupdate': {
    evt: CreateEvent('webViewInternal.onFindReply'),
    fields: [
      'searchText',
      'numberOfMatches',
      'activeMatchOrdinal',
      'selectionRect',
      'canceled',
      'finalUpdate'
    ]
  },
  'framenamechanged': {
    evt: CreateEvent('webViewInternal.onFrameNameChanged'),
    handler: 'handleFrameNameChangedEvent',
    internal: true
  },
  'loadabort': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onLoadAbort'),
    fields: ['url', 'isTopLevel', 'code', 'reason'],
    handler: 'handleLoadAbortEvent'
  },
  'loadcommit': {
    evt: CreateEvent('webViewInternal.onLoadCommit'),
    fields: ['url', 'isTopLevel'],
    handler: 'handleLoadCommitEvent'
  },
  'loadprogress': {
    evt: CreateEvent('webViewInternal.onLoadProgress'),
    fields: ['url', 'progress']
  },
  'loadredirect': {
    evt: CreateEvent('webViewInternal.onLoadRedirect'),
    fields: ['isTopLevel', 'oldUrl', 'newUrl']
  },
  'loadstart': {
    evt: CreateEvent('webViewInternal.onLoadStart'),
    fields: ['url', 'isTopLevel']
  },
  'loadstop': {
    evt: CreateEvent('webViewInternal.onLoadStop')
  },
  'newwindow': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onNewWindow'),
    fields: [
      'initialHeight',
      'initialWidth',
      'targetUrl',
      'windowOpenDisposition',
      'name'
    ],
    handler: 'handleNewWindowEvent'
  },
  'permissionrequest': {
    cancelable: true,
    evt: CreateEvent('webViewInternal.onPermissionRequest'),
    fields: [
      'identifier',
      'lastUnlockedBySelf',
      'name',
      'permission',
      'requestMethod',
      'url',
      'userGesture'
    ],
    handler: 'handlePermissionEvent'
  },
  'responsive': {
    evt: CreateEvent('webViewInternal.onResponsive'),
    fields: ['processId']
  },
  'sizechanged': {
    evt: CreateEvent('webViewInternal.onSizeChanged'),
    fields: ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
    handler: 'handleSizeChangedEvent'
  },
  'sslchange': {
      evt: CreateEvent('webViewInternal.onSSLChange'),
      fields: ['certificate']
  },
  'unresponsive': {
    evt: CreateEvent('webViewInternal.onUnresponsive'),
    fields: ['processId']
  },
  'zoomchange': {
    evt: CreateEvent('webViewInternal.onZoomChange'),
    fields: ['oldZoomFactor', 'newZoomFactor']
  }
};

WebViewEvents.prototype.setupWebRequestEvents = function() {
  var request = {};
  var createWebRequestEvent = function(webRequestEvent) {
    return this.weakWrapper(function() {
      if (!this[webRequestEvent.name]) {
        this[webRequestEvent.name] =
            new WebRequestEvent(
                'webViewInternal.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.extraParameters, webRequestEvent.options,
                this.view.viewInstanceId);
      }
      return this[webRequestEvent.name];
    });
  }.bind(this);

  var createDeclarativeWebRequestEvent = function(webRequestEvent) {
    return this.weakWrapper(function() {
      if (!this[webRequestEvent.name]) {
        // The onMessage event gets a special event type because we want
        // the listener to fire only for messages targeted for this particular
        // <webview>.
        var EventClass = webRequestEvent.name === 'onMessage' ?
            DeclarativeWebRequestEvent : EventBindings.Event;
        this[webRequestEvent.name] =
            new EventClass(
                'webViewInternal.declarativeWebRequest.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.options,
                this.view.viewInstanceId);
      }
      return this[webRequestEvent.name];
    });
  }.bind(this);

  for (var i = 0; i < DeclarativeWebRequestSchema.events.length; ++i) {
    var eventSchema = DeclarativeWebRequestSchema.events[i];
    var webRequestEvent = createDeclarativeWebRequestEvent(eventSchema);
    Object.defineProperty(
        request,
        eventSchema.name,
        {
          get: webRequestEvent,
          enumerable: true
        }
        );
  }

  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < WebRequestSchema.events.length; ++i) {
    var webRequestEvent = createWebRequestEvent(WebRequestSchema.events[i]);
    Object.defineProperty(
        request,
        WebRequestSchema.events[i].name,
        {
          get: webRequestEvent,
          enumerable: true
        }
        );
  }

  this.view.setRequestPropertyOnWebViewElement(request);
};

WebViewEvents.prototype.getEvents = function() {
  return WebViewEvents.EVENTS;
};

WebViewEvents.prototype.handleDialogEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  new WebViewActionRequests.Dialog(this.view, event, webViewEvent);
};

WebViewEvents.prototype.handleFrameNameChangedEvent = function(event) {
  this.view.onFrameNameChanged(event.name);
};

WebViewEvents.prototype.handleFullscreenExitEvent = function(event, eventName) {
  document.webkitCancelFullScreen();
};

WebViewEvents.prototype.handleLoadAbortEvent = function(event, eventName) {
  var showWarningMessage = function(code, reason) {
    var WARNING_MSG_LOAD_ABORTED = '<webview>: ' +
        'The load has aborted with error %1: %2.';
    window.console.warn(
        WARNING_MSG_LOAD_ABORTED.replace('%1', code).replace('%2', reason));
  };
  var webViewEvent = this.makeDomEvent(event, eventName);
  if (this.view.dispatchEvent(webViewEvent)) {
    showWarningMessage(event.code, event.reason);
  }
};

WebViewEvents.prototype.handleLoadCommitEvent = function(event, eventName) {
  this.view.onLoadCommit(event.baseUrlForDataUrl,
                         event.currentEntryIndex,
                         event.entryCount,
                         event.processId,
                         event.url,
                         event.isTopLevel);
  var webViewEvent = this.makeDomEvent(event, eventName);
  this.view.dispatchEvent(webViewEvent);
};

WebViewEvents.prototype.handleNewWindowEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  new WebViewActionRequests.NewWindow(this.view, event, webViewEvent);
};

WebViewEvents.prototype.handlePermissionEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  if (event.permission === 'fullscreen') {
    new WebViewActionRequests.FullscreenPermissionRequest(
        this.view, event, webViewEvent);
  } else {
    new WebViewActionRequests.PermissionRequest(this.view, event, webViewEvent);
  }
};

WebViewEvents.prototype.handleSizeChangedEvent = function(event, eventName) {
  var webViewEvent = this.makeDomEvent(event, eventName);
  this.view.onSizeChanged(webViewEvent);
};

function DeclarativeWebRequestEvent(opt_eventName,
                                    opt_argSchemas,
                                    opt_eventOptions,
                                    opt_webViewInstanceId) {
  var subEventName = opt_eventName + '/' + IdGenerator.GetNextId();
  EventBindings.Event.call(this,
                           subEventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);

  var view = GuestViewInternalNatives.GetViewFromID(opt_webViewInstanceId || 0);
  if (!view) {
    return;
  }
  view.events.addScopedListener(WebRequestMessageEvent, function() {
    // Re-dispatch to subEvent's listeners.
    $Function.apply(this.dispatch, this, $Array.slice(arguments));
  }.bind(this), {instanceId: opt_webViewInstanceId || 0});
}

DeclarativeWebRequestEvent.prototype.__proto__ = EventBindings.Event.prototype;

// Exports.
exports.$set('WebViewEvents', WebViewEvents);

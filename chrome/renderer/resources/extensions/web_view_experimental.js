// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim extension to provide permission request API (and possibly other future
// experimental APIs) for <webview> tag.
// See web_view.js for details.
//
// We want to control the permission API feature in <webview> separately from
// the <webview> feature itself. <webview> is available in stable channel, but
// permission API would only be available for channels CHANNEL_DEV and
// CHANNEL_CANARY.

var WebView = require('webView').WebView;

var forEach = require('utils').forEach;

/** @type {Array.<string>} */
var PERMISSION_TYPES = ['download', 'media', 'geolocation', 'pointerLock'];

/** @type {string} */
var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
    'Permission has already been decided for this "permissionrequest" event.';

var EXPOSED_PERMISSION_EVENT_ATTRIBS = [
    'lastUnlockedBySelf',
    'permission',
    'requestMethod',
    'url',
    'userGesture'
];

/**
 * @private
 */
WebView.prototype.maybeSetupExperimentalAPI_ = function() {
  this.setupPermissionEvent_();
  this.setupExecuteCodeAPI_();
}

/**
 * @param {!Object} detail The event details, originated from <object>.
 * @private
 */
WebView.prototype.setupPermissionEvent_ = function() {
  var node = this.webviewNode_;
  var browserPluginNode = this.browserPluginNode_;
  var internalevent = '-internal-permissionrequest';
  browserPluginNode.addEventListener(internalevent, function(e) {
    var evt = new Event('permissionrequest', {bubbles: true, cancelable: true});
    var detail = e.detail ? JSON.parse(e.detail) : {};
    forEach(EXPOSED_PERMISSION_EVENT_ATTRIBS, function(i, attribName) {
      if (detail[attribName] !== undefined)
        evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;

    if (detail.requestId !== undefined &&
        PERMISSION_TYPES.indexOf(detail.permission) >= 0) {
      // TODO(lazyboy): Also fill in evt.details (see webview specs).
      // http://crbug.com/141197.
      var decisionMade = false;
      // Construct the event.request object.
      var request = {
        allow: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            browserPluginNode['-internal-setPermission'](requestId, true);
            decisionMade = true;
          }
        },
        deny: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            browserPluginNode['-internal-setPermission'](requestId, false);
            decisionMade = true;
          }
        }
      };
      evt.request = request;

      // Make browser plugin track lifetime of |request|.
      browserPluginNode['-internal-persistObject'](
          request, detail.permission, requestId);

      var defaultPrevented = !node.dispatchEvent(evt);
      if (!decisionMade && !defaultPrevented) {
        decisionMade = true;
        browserPluginNode['-internal-setPermission'](requestId, false);
      }
    }
  });
};

/**
 * @private
 */
WebView.prototype.setupExecuteCodeAPI_ = function() {
  var self = this;
  this.webviewNode_['executeScript'] = function(var_args) {
    var args = [self.browserPluginNode_.getProcessId(),
                self.browserPluginNode_.getRouteId()].concat(
                    Array.prototype.slice.call(arguments));
    chrome.webview.executeScript.apply(null, args);
  }
};

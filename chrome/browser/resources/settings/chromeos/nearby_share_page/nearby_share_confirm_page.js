// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share-confirm-page' component show the user the
 * details of an incoming share request and allows the user to confirm or
 * reject the request
 */

import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import '../../shared/nearby_page_template.js';
import '../../shared/nearby_device.js';
import '../../shared/nearby_preview.js';
import '../../shared/nearby_progress.js';

import {ShareTarget, TransferStatus} from '/mojo/nearby_share.mojom-webui.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_share_confirm_page.html.js';

/**
 * The progress bar asset URL for light mode.
 * @type {string}
 */
const PROGRESS_BAR_URL_LIGHT = 'nearby_share_progress_bar_light.json';

/**
 * The progress bar asset URL for dark mode.
 * @type {string}
 */
const PROGRESS_BAR_URL_DARK = 'nearby_share_progress_bar_dark.json';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyShareConfirmPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class NearbyShareConfirmPageElement extends NearbyShareConfirmPageElementBase {
  static get is() {
    return 'nearby-share-confirm-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {?ShareTarget} */
      shareTarget: {
        type: Object,
        value: null,
      },

      /** @type {?string} */
      connectionToken: {
        type: String,
        value: null,
      },

      /**
       * @type {?TransferStatus}
       */
      transferStatus: {
        type: TransferStatus,
        value: null,
        observer: 'onTransferStatusChanged_',
      },

      /**
       * Header text for error. Controls error display on the confirm page.
       * The error section is not displayed if this is falsey.
       * @private {?string}
       */
      errorTitle_: {
        type: String,
        value: null,
      },

      /**
       * Description text for error display on confirm page, displayed under the
       * error title.
       * @private {?string}
       */
      errorDescription_: {
        type: String,
        value: null,
      },

      /**
       * Whether the confirm page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Update the |errorTitle_| and the |errorDescription_| when the transfer
   * status changes.
   * @param {?TransferStatus} newStatus
   */
  onTransferStatusChanged_(newStatus) {
    switch (newStatus) {
      case TransferStatus.kTimedOut:
        this.errorTitle_ = this.i18n('nearbyShareErrorTimeOut');
        this.errorDescription_ = this.i18n('nearbyShareErrorTryAgain');
        break;
      case TransferStatus.kUnsupportedAttachmentType:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ =
            this.i18n('nearbyShareErrorUnsupportedFileType');
        break;
      case TransferStatus.kNotEnoughSpace:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorNotEnoughSpace');
        break;
      case TransferStatus.kCancelled:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorCancelled');
        break;
      case TransferStatus.kFailed:
      case TransferStatus.kMediaUnavailable:
      case TransferStatus.kAwaitingRemoteAcceptanceFailed:
      case TransferStatus.kDecodeAdvertisementFailed:
      case TransferStatus.kMissingTransferUpdateCallback:
      case TransferStatus.kMissingShareTarget:
      case TransferStatus.kMissingEndpointId:
      case TransferStatus.kMissingPayloads:
      case TransferStatus.kPairedKeyVerificationFailed:
      case TransferStatus.kInvalidIntroductionFrame:
      case TransferStatus.kIncompletePayloads:
      case TransferStatus.kFailedToCreateShareTarget:
      case TransferStatus.kFailedToInitiateOutgoingConnection:
      case TransferStatus.kFailedToReadOutgoingConnectionResponse:
      case TransferStatus.kUnexpectedDisconnection:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        break;
      default:
        this.errorTitle_ = null;
        this.errorDescription_ = null;
    }
  }

  /**
   * @return {string}
   * @protected
   */
  getConnectionTokenString_() {
    return this.connectionToken ?
        this.i18n(
            'nearbyShareReceiveConfirmPageConnectionId', this.connectionToken) :
        '';
  }

  /**
   * Returns the URL for the asset that defines a file transfer's animated
   * progress bar.
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? PROGRESS_BAR_URL_DARK :
                                    PROGRESS_BAR_URL_LIGHT;
  }
}

customElements.define(
    NearbyShareConfirmPageElement.is, NearbyShareConfirmPageElement);

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {TransferStatus} from 'chrome://os-settings/mojo/nearby_share.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NearbyShare', function() {
  let nearbyShareConfirmPage;

  setup(function() {
    PolymerTest.clearBody();

    nearbyShareConfirmPage =
        document.createElement('nearby-share-confirm-page');

    document.body.appendChild(nearbyShareConfirmPage);
    flush();
  });

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('renders progress bar', async function() {
    nearbyShareConfirmPage.set('transferStatus', TransferStatus['kConnecting']);
    await flushAsync();

    const isAnimationHidden =
        !!nearbyShareConfirmPage.shadowRoot.querySelector('cr-lottie[style]');

    if (nearbyShareConfirmPage.shadowRoot.querySelector('#errorTitle')) {
      assertTrue(isAnimationHidden);
    } else {
      assertFalse(isAnimationHidden);
    }
  });

  test('hide progress bar when error', async function() {
    nearbyShareConfirmPage.set('transferStatus', TransferStatus['kRejected']);
    await flushAsync();

    const isAnimationHidden =
        !!nearbyShareConfirmPage.shadowRoot.querySelector('cr-lottie[style]');

    if (nearbyShareConfirmPage.shadowRoot.querySelector('#errorTitle')) {
      assertTrue(isAnimationHidden);
    } else {
      assertFalse(isAnimationHidden);
    }
  });

  test('Ensure all final transfer states explicitly handled', async () => {
    // Transfer states that do not result in an error message.
    const nonErrorStates = {
      kUnknown: true,
      kConnecting: true,
      kAwaitingLocalConfirmation: true,
      kAwaitingRemoteAcceptance: true,
      kInProgress: true,
      kMediaDownloading: true,
      kComplete: true,
      kRejected: true,
      MIN_VALUE: true,
      MAX_VALUE: true,
    };

    let key;
    for (key of Object.keys(TransferStatus)) {
      const isErrorState = !(key in nonErrorStates);
      if (isErrorState) {
        nearbyShareConfirmPage.set('transferStatus', TransferStatus[key]);
        await flushAsync();
        assertTrue(
            !!nearbyShareConfirmPage.shadowRoot.querySelector('#errorTitle')
                  .textContent);

        // Set back to a good state
        nearbyShareConfirmPage.set(
            'transferStatus', TransferStatus['kConnecting']);
        await flushAsync();
        assertFalse(
            !!nearbyShareConfirmPage.shadowRoot.querySelector('#errorTitle'));
      }
    }
  });
});

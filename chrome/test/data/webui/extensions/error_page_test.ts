// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */
import 'chrome://extensions/extensions.js';

import type {ErrorPageDelegate, ExtensionsErrorPageElement} from 'chrome://extensions/extensions.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {createExtensionInfo, MockItemDelegate} from './test_util.js';

// The delegate in the error page is an intersection type of
// ItemDelegate&ErrorPageDelegate and MockItemDelegate extends ClickMock.
class MockErrorPageDelegate extends MockItemDelegate implements
    ErrorPageDelegate {
  requestFileSourceArgs: chrome.developerPrivate.RequestFileSourceProperties|
      undefined;
  requestFileSourceResolver:
      PromiseResolver<chrome.developerPrivate.RequestFileSourceResponse> =
          new PromiseResolver();

  deleteErrors(
      _extensionId: string, _errorIds?: number[],
      _type?: chrome.developerPrivate.ErrorType) {}

  requestFileSource(args: chrome.developerPrivate.RequestFileSourceProperties) {
    this.requestFileSourceArgs = args;
    this.requestFileSourceResolver = new PromiseResolver();
    return this.requestFileSourceResolver.promise;
  }
}

suite('ExtensionErrorPageTest', function() {
  let extensionData: chrome.developerPrivate.ExtensionInfo;

  let errorPage: ExtensionsErrorPageElement;

  let mockDelegate: MockErrorPageDelegate;

  const extensionId: string = 'a'.repeat(32);

  // Common data for runtime errors.
  const runtimeErrorBase = {
    occurrences: 1,
    type: chrome.developerPrivate.ErrorType.RUNTIME,
    extensionId: extensionId,
    fromIncognito: false,
  };

  // Common data for manifest errors.
  const manifestErrorBase = {
    type: chrome.developerPrivate.ErrorType.MANIFEST,
    extensionId: extensionId,
    fromIncognito: false,
  };

  // Initialize an extension item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const runtimeError = Object.assign(
        {
          contextUrl: 'Unknown',
          source: 'chrome-extension://' + extensionId + '/source.html',
          message: 'message',
          renderProcessId: 0,
          renderViewId: 0,
          canInspect: false,
          id: 1,
          stackTrace: [],
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
        },
        runtimeErrorBase);
    extensionData = createExtensionInfo({
      runtimeErrors: [runtimeError],
      manifestErrors: [],
    });
    errorPage = document.createElement('extensions-error-page');
    mockDelegate = new MockErrorPageDelegate();
    errorPage.delegate = mockDelegate;
    errorPage.data = extensionData;
    document.body.appendChild(errorPage);

    // The toast manager is needed for reloading, see item_mixin.ts.
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('Layout', function() {
    flush();

    const testIsVisible = isChildVisible.bind(null, errorPage);
    assertTrue(testIsVisible('#closeButton'));
    assertTrue(testIsVisible('#heading'));
    assertTrue(testIsVisible('#errorsList'));

    let errorElements = errorPage.shadowRoot!.querySelectorAll('.error-item');
    assertEquals(1, errorElements.length);
    let error = errorElements[0]!;
    assertEquals(
        'message',
        error.querySelector<HTMLElement>(
                 '.error-message')!.textContent!.trim());
    assertTrue(error.querySelector('cr-icon')!.icon === 'cr:error');

    const manifestError = Object.assign(
        {
          source: 'manifest.json',
          message: 'invalid key',
          id: 2,
          manifestKey: 'permissions',
        },
        manifestErrorBase);
    errorPage.set('data.manifestErrors', [manifestError]);
    flush();
    errorElements = errorPage.shadowRoot!.querySelectorAll('.error-item');
    assertEquals(2, errorElements.length);
    error = errorElements[0]!;
    assertEquals(
        'invalid key',
        error.querySelector<HTMLElement>(
                 '.error-message')!.textContent!.trim());
    assertTrue(error.querySelector('cr-icon')!.icon === 'cr:warning');

    mockDelegate.testClickingCalls(
        error.querySelector<HTMLElement>('.icon-delete-gray')!, 'deleteErrors',
        [extensionId, [manifestError.id]]);
  });

  test(
      'CodeSection', function(done) {
        flush();

        assertTrue(!!mockDelegate.requestFileSourceArgs);
        const args = mockDelegate.requestFileSourceArgs;
        assertEquals(extensionId, args.extensionId);
        assertEquals('source.html', args.pathSuffix);
        assertEquals('message', args.message);

        assertTrue(!!mockDelegate.requestFileSourceResolver);
        const code = {
          beforeHighlight: 'foo',
          highlight: 'bar',
          afterHighlight: 'baz',
          message: 'quu',
          title: '',
        };
        mockDelegate.requestFileSourceResolver.resolve(code);
        mockDelegate.requestFileSourceResolver.promise.then(function() {
          flush();
          assertEquals(
              code,
              errorPage.shadowRoot!.querySelector(
                                       'extensions-code-section')!.code);
          done();
        });
      });

  test('ErrorSelection', function() {
    const nextRuntimeError = Object.assign(
        {
          source: 'chrome-extension://' + extensionId + '/other_source.html',
          message: 'Other error',
          id: 2,
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
          renderProcessId: 111,
          renderViewId: 222,
          canInspect: true,
          contextUrl: 'http://test.com',
          stackTrace: [{url: 'url', lineNumber: 123, columnNumber: 321}],
        },
        runtimeErrorBase);
    // Add a new runtime error to the end.
    errorPage.push('data.runtimeErrors', nextRuntimeError);
    flush();

    const errorElements = errorPage.shadowRoot!.querySelectorAll<HTMLElement>(
        '.error-item .start');
    const crCollapses = errorPage.shadowRoot!.querySelectorAll('cr-collapse');
    assertEquals(2, errorElements.length);
    assertEquals(2, crCollapses.length);

    // The first error should be focused by default, and we should have
    // requested the source for it.
    assertEquals(extensionData.runtimeErrors[0], errorPage.getSelectedError());
    assertTrue(!!mockDelegate.requestFileSourceArgs);
    let args = mockDelegate.requestFileSourceArgs;
    assertEquals('source.html', args.pathSuffix);
    assertTrue(crCollapses[0]!.opened);
    assertFalse(crCollapses[1]!.opened);

    mockDelegate.requestFileSourceResolver = new PromiseResolver();
    mockDelegate.requestFileSourceArgs = undefined;

    // Tap the second error. It should now be selected and we should request
    // the source for it.
    errorElements[1]!.click();
    assertEquals(nextRuntimeError, errorPage.getSelectedError());
    assertTrue(!!mockDelegate.requestFileSourceArgs);
    args = mockDelegate.requestFileSourceArgs;
    assertEquals('other_source.html', args.pathSuffix);
    assertTrue(crCollapses[1]!.opened);
    assertFalse(crCollapses[0]!.opened);

    assertEquals(
        'Unknown',
        crCollapses[0]!.querySelector<HTMLElement>(
                           '.context-url')!.textContent!.trim());
    assertEquals(
        nextRuntimeError.contextUrl,
        crCollapses[1]!.querySelector<HTMLElement>(
                           '.context-url')!.textContent!.trim());
  });

  // Tests that the element can still be shown with an invalid URL. Regression
  // test for crbug.com/1257170, as without the fix, this test would simply
  // crash when the page tries and fails to create a URL object.
  test('InvalidUrl', function() {
    const newRuntimeError = Object.assign(
        {
          severity: chrome.developerPrivate.ErrorLevel.ERROR,
          source: 'invalid_url',
        },
        runtimeErrorBase);
    // Replace the runtime error URL with something malformed, and check that
    // the error is still displayed and opened.
    errorPage.set('data.runtimeErrors', [newRuntimeError]);
    flush();

    assertEquals(extensionData.runtimeErrors[0], errorPage.getSelectedError());
  });

  // Test that the reload button is only shown for unpacked extensions in dev
  // mode, and that it can be clicked.
  test('ReloadItem', async function() {
    flush();

    const isVisible = isChildVisible.bind(null, errorPage);
    assertFalse(isVisible('#dev-reload-button'));

    errorPage.inDevMode = true;
    errorPage.set('data.location', chrome.developerPrivate.Location.UNPACKED);

    flush();
    assertTrue(isVisible('#dev-reload-button'));

    await mockDelegate.testClickingCalls(
        errorPage.shadowRoot!.querySelector('#dev-reload-button')!,
        'reloadItem', [errorPage.data.id], Promise.resolve());

    // Disable the extension. The button should now be hidden.
    errorPage.set(
        'data.state', chrome.developerPrivate.ExtensionState.DISABLED);

    flush();
    assertFalse(isVisible('#dev-reload-button'));
  });
});

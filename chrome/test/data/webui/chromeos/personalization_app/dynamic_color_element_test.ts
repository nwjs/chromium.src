// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for dynamic-color-element component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ColorScheme, DynamicColorElement, emptyState, SetColorSchemeAction, SetSampleColorSchemesAction, SetStaticColorAction, ThemeActionName, ThemeObserver} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, dispatchKeydown, getActiveElement, initElement, teardownElement, waitForActiveElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

suite('DynamicColorElementTest', function() {
  let dynamicColorElement: DynamicColorElement|null;
  let personalizationStore: TestPersonalizationStore;
  let themeProvider: TestThemeProvider;

  function getToggleButton(): CrToggleElement {
    return dynamicColorElement!.shadowRoot!.querySelector('cr-toggle')!;
  }

  function getColorSchemeSelector(): HTMLElement {
    return dynamicColorElement!.shadowRoot!.getElementById(
        'colorSchemeSelector')!;
  }

  function getStaticColorSelector(): HTMLElement {
    return dynamicColorElement!.shadowRoot!.getElementById(
        'staticColorSelector')!;
  }

  function getColorSchemeButtons(): NodeListOf<CrButtonElement> {
    return getColorSchemeSelector().querySelectorAll('cr-button')!;
  }

  function getStaticColorButtons(): NodeListOf<CrButtonElement> {
    return getStaticColorSelector().querySelectorAll('cr-button')!;
  }

  async function showStaticColorButtons() {
    const toggleButton = getToggleButton();
    if (toggleButton.checked) {
      await clickToggleButton();
    }
    assertFalse(getStaticColorSelector().hidden);
  }

  async function showColorSchemeButtons() {
    const toggleButton = getToggleButton();
    if (!toggleButton.checked) {
      await clickToggleButton();
    }
    assertFalse(getColorSchemeSelector().hidden);
  }

  async function clickToggleButton() {
    // Any time the toggle button is clicked, the color scheme is set (if
    // dynamic colors are disabled, then it is set to ColorScheme.kStatic).
    personalizationStore.expectAction(ThemeActionName.SET_COLOR_SCHEME);
    getToggleButton().click();
    await personalizationStore.waitForAction(ThemeActionName.SET_COLOR_SCHEME);
  }

  setup(() => {
    const mocks = baseSetup();
    themeProvider = mocks.themeProvider;
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    ThemeObserver.initThemeObserverIfNeeded();
  });

  teardown(async () => {
    teardownElement(dynamicColorElement);
    ThemeObserver.shutdown();
  });

  async function initDynamicColorElement() {
    dynamicColorElement = initElement(DynamicColorElement)!;
    await waitAfterNextRender(dynamicColorElement);
  }

  test('displays content', async () => {
    await initDynamicColorElement();

    assertEquals(
        '[temp]Theme color[temp]Auto',
        dynamicColorElement!.shadowRoot!.getElementById(
                                            'themeHeader')!.textContent);
    assertTrue(getToggleButton().checked, 'default toggle should be on');
    assertFalse(
        getColorSchemeSelector().hidden,
        'when the toggle is on, the color scheme buttons should be visible.');
    assertTrue(
        getStaticColorSelector().hidden,
        'when the toggle is on, the static color buttons should be hidden.');
  });

  test('sets color scheme in store on first load', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_COLOR_SCHEME);

    await initDynamicColorElement();

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_COLOR_SCHEME) as SetColorSchemeAction;
    assertEquals(ColorScheme.kTonalSpot, action.colorScheme);
  });

  test('sets sample color schemes in store on first load', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_SAMPLE_COLOR_SCHEMES);

    await initDynamicColorElement();

    const action = await personalizationStore.waitForAction(
                       ThemeActionName.SET_SAMPLE_COLOR_SCHEMES) as
        SetSampleColorSchemesAction;
    assertEquals(4, action.sampleColorSchemes.length);
  });

  test('sets color scheme data in store on changed', async () => {
    const colorScheme = ColorScheme.kExpressive;
    assertDeepEquals(emptyState(), personalizationStore.data);
    await themeProvider.whenCalled('setThemeObserver');
    personalizationStore.expectAction(ThemeActionName.SET_COLOR_SCHEME);

    themeProvider.themeObserverRemote!.onColorSchemeChanged(colorScheme);

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_COLOR_SCHEME) as SetColorSchemeAction;
    assertEquals(colorScheme, action.colorScheme);
  });

  test('sets static color data in store on changed', async () => {
    const staticColor = hexColorToSkColor('#123456');
    assertDeepEquals(emptyState(), personalizationStore.data);
    await themeProvider.whenCalled('setThemeObserver');
    personalizationStore.expectAction(ThemeActionName.SET_STATIC_COLOR);

    themeProvider.themeObserverRemote!.onStaticColorChanged(staticColor);

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_STATIC_COLOR) as SetStaticColorAction;
    assertDeepEquals(staticColor, action.staticColor);
  });

  test('displays color scheme on load', async () => {
    const colorScheme = ColorScheme.kExpressive;
    themeProvider.setColorScheme(colorScheme);

    await initDynamicColorElement();

    assertTrue(getToggleButton().checked, 'default toggle should be on');
    assertFalse(
        getColorSchemeSelector().hidden,
        'when the toggle is on, the color scheme buttons should be visible.');
    assertTrue(
        getStaticColorSelector().hidden,
        'when the toggle is on, the static color buttons should be hidden.');
    const checkedButton = getColorSchemeSelector().querySelector(
                              'cr-button[aria-checked="true"]') as HTMLElement;
    assertEquals(String(colorScheme), checkedButton.dataset['colorSchemeId']);
  });

  test('displays static color on load', async () => {
    const staticColorHex = '#edd0e4';
    themeProvider.setStaticColor(hexColorToSkColor(staticColorHex));

    await initDynamicColorElement();

    assertFalse(getToggleButton().checked, 'default toggle should be off');
    assertTrue(
        getColorSchemeSelector().hidden,
        'when the toggle is off, the color scheme buttons should be hidden.');
    assertFalse(
        getStaticColorSelector().hidden,
        'when the toggle is off, the static color buttons should be visible.');
    const checkedButton = getStaticColorSelector().querySelector(
                              'cr-button[aria-checked="true"]') as HTMLElement;
    assertEquals(staticColorHex, checkedButton.dataset['staticColor']);
  });

  test('flips toggle', async () => {
    await initDynamicColorElement();
    const toggleButton = getToggleButton();
    const colorSchemeSelector = getColorSchemeSelector();
    const staticColorSelector = getStaticColorSelector();
    await showColorSchemeButtons();

    await clickToggleButton();

    assertFalse(
        toggleButton.checked, 'after clicking toggle, toggle should be off');
    assertTrue(
        colorSchemeSelector.hidden,
        'when the toggle is off, the color scheme buttons should be hidden');
    assertFalse(
        staticColorSelector.hidden,
        'when the toggle is off, the static color buttons should be visible.');

    await clickToggleButton();

    assertFalse(
        colorSchemeSelector.hidden,
        'when the toggle is on, the color scheme buttons should be visible.');
    assertTrue(
        staticColorSelector.hidden,
        'when the toggle is on, the static color buttons should be hidden.');
  });

  test('keypress navigates color scheme buttons', async () => {
    await initDynamicColorElement();
    assertTrue(!!dynamicColorElement);
    await showColorSchemeButtons();
    const colorSchemeButtons = getColorSchemeButtons();
    (colorSchemeButtons[0] as HTMLElement)!.focus();

    for (let i = 1; i <= 3; ++i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowRight');
      await waitForActiveElement(colorSchemeButtons[i]!, dynamicColorElement!);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }

    for (let i = 2; i >= 0; --i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowLeft');
      await waitForActiveElement(colorSchemeButtons[i]!, dynamicColorElement!);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }
  });

  test('keypress navigates static color buttons', async () => {
    await initDynamicColorElement();
    assertTrue(!!dynamicColorElement);
    await showStaticColorButtons();
    const staticColorButtons = getStaticColorButtons();
    (staticColorButtons![0] as HTMLElement)!.focus();

    for (let i = 1; i <= 3; ++i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowRight');
      await waitForActiveElement(staticColorButtons[i]!, dynamicColorElement);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }

    for (let i = 2; i >= 0; --i) {
      dispatchKeydown(
          (getActiveElement(dynamicColorElement) as HTMLElement), 'ArrowLeft');
      await waitForActiveElement(staticColorButtons[i]!, dynamicColorElement);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }
  });

  test('set color scheme', async () => {
    await initDynamicColorElement();
    personalizationStore.expectAction(ThemeActionName.SET_COLOR_SCHEME);
    await showColorSchemeButtons();
    const button = getColorSchemeButtons()[1]!;
    assertEquals(button.getAttribute('aria-checked'), 'false');

    button.click();
    await themeProvider.whenCalled('setColorScheme');

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_COLOR_SCHEME) as SetColorSchemeAction;
    assertTrue(!!action.colorScheme);
    assertEquals(
        Number(button.dataset['colorSchemeId']!),
        personalizationStore.data.theme.colorSchemeSelected);
    assertEquals(button.getAttribute('aria-checked'), 'true');
  });

  test('set static color', async () => {
    await initDynamicColorElement();
    personalizationStore.expectAction(ThemeActionName.SET_STATIC_COLOR);
    await showStaticColorButtons();
    const button = getStaticColorButtons()[1]!;
    assertEquals(button.getAttribute('aria-checked'), 'false');

    button.click();
    await themeProvider.whenCalled('setStaticColor');

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_STATIC_COLOR) as SetStaticColorAction;
    assertTrue(!!action.staticColor);
    assertDeepEquals(
        hexColorToSkColor(button.dataset['staticColor']!),
        personalizationStore.data.theme.staticColorSelected);
    assertEquals(button.getAttribute('aria-checked'), 'true');
  });

  test('store previous color scheme selection locally', async () => {
    const colorScheme = ColorScheme.kExpressive;
    themeProvider.setColorScheme(colorScheme);
    await initDynamicColorElement();

    // Toggle to show static color buttons.
    await clickToggleButton();
    // Toggle to show color scheme buttons again.
    await clickToggleButton();

    // The same color scheme button should be selected.
    assertDeepEquals(
        colorScheme, personalizationStore.data.theme.colorSchemeSelected);
  });

  test('store previous static color selection locally', async () => {
    const staticColorHex = '#edd0e4';
    themeProvider.setStaticColor(hexColorToSkColor(staticColorHex));
    await initDynamicColorElement();

    // Toggle to show color scheme buttons.
    await clickToggleButton();
    // Toggle to show static color buttons again.
    await clickToggleButton();

    // The same static color button should be selected.
    assertDeepEquals(
        hexColorToSkColor(staticColorHex),
        personalizationStore.data.theme.staticColorSelected);
  });
});

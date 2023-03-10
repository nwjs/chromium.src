// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {AmbientObserver, AmbientPreviewLarge, Paths, PersonalizationRouter, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


suite('AmbientPreviewLargeTest', function() {
  let ambientPreviewLargeElement: AmbientPreviewLarge|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouter.instance;
  const routerMock = TestBrowserProxy.fromClass(PersonalizationRouter);

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouter.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientPreviewLargeElement);
    ambientPreviewLargeElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouter.instance = routerOriginal;
  });

  test(
      'displays zero state message when ambient mode is disabled', async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.googlePhotosAlbumsPreviews =
            ambientProvider.googlePhotosAlbumsPreviews;
        ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewLargeElement);

        const messageContainer =
            ambientPreviewLargeElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const textSpan = messageContainer.querySelector<HTMLSpanElement>(
            '#turnOnDescription');
        assertTrue(!!textSpan);
        assertEquals(
            ambientPreviewLargeElement.i18n(
                'ambientModeMainPageZeroStateMessageV2'),
            textSpan.innerText.trim());
      });

  test(
      'clicks turn on button enables ambient mode and navigates to ambient subpage',
      async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.googlePhotosAlbumsPreviews =
            ambientProvider.googlePhotosAlbumsPreviews;
        ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewLargeElement);

        const messageContainer =
            ambientPreviewLargeElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const button = messageContainer.querySelector('cr-button');
        assertTrue(!!button);

        personalizationStore.setReducersEnabled(true);
        button.click();
        assertTrue(personalizationStore.data.ambient.ambientModeEnabled);

        const original = PersonalizationRouter.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouter.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouter.instance = original;
              },
            } as PersonalizationRouter;
          };
        });
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.AMBIENT, path);
        assertDeepEquals({}, queryParams);
      });

  test('click big image preview goes to ambient subpage', async () => {
    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      googlePhotosAlbumsPreviews: ambientProvider.googlePhotosAlbumsPreviews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    ambientPreviewLargeElement.shadowRoot!
        .querySelector<HTMLImageElement>('.preview-image.clickable')!.click();

    const [path, queryParams] = await goToRoutePromise;

    assertEquals(Paths.AMBIENT, path, 'navigates to ambient subpage');
    assertDeepEquals({}, queryParams, 'no query params set');
  });

  test('click ambient collage goes to ambient albums subpage', async () => {
    // Disables `isAmbientSubpageUiChangeEnabled` to show the previous UI.
    loadTimeData.overrideValues({isAmbientSubpageUiChangeEnabled: false});

    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      googlePhotosAlbumsPreviews: ambientProvider.googlePhotosAlbumsPreviews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    function setFakeRouter() {
      const original = PersonalizationRouter.instance;
      return new Promise<TopicSource>(resolve => {
        PersonalizationRouter.instance = () => {
          return {
            selectAmbientAlbums(topicSource: TopicSource) {
              resolve(topicSource);
              PersonalizationRouter.instance = original;
            },
          } as PersonalizationRouter;
        };
      });
    }

    const artGalleryPromise = setFakeRouter();

    ambientPreviewLargeElement.shadowRoot!.getElementById(
                                              'collageContainer')!.click();

    let topicSource = await artGalleryPromise;
    assertEquals(
        topicSource, TopicSource.kArtGallery,
        'navigates to art gallery topic source');

    // Set the topic source to kGooglePhotos and check that clicking the photo
    // collage goes to kGooglePhotos subpage.
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.notifyObservers();
    const googlePhotosPromise = setFakeRouter();

    ambientPreviewLargeElement.shadowRoot!.getElementById(
                                              'collageContainer')!.click();

    topicSource = await googlePhotosPromise;
    assertEquals(
        topicSource, TopicSource.kGooglePhotos,
        'navigates to google photos topic source');
  });

  test('click ambient thumbnail goes to ambient albums subpage', async () => {
    loadTimeData.overrideValues({isAmbientSubpageUiChangeEnabled: true});

    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      googlePhotosAlbumsPreviews: ambientProvider.googlePhotosAlbumsPreviews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    function setFakeRouter() {
      const original = PersonalizationRouter.instance;
      return new Promise<TopicSource>(resolve => {
        PersonalizationRouter.instance = () => {
          return {
            selectAmbientAlbums(topicSource: TopicSource) {
              resolve(topicSource);
              PersonalizationRouter.instance = original;
            },
          } as PersonalizationRouter;
        };
      });
    }

    const artGalleryPromise = setFakeRouter();

    ambientPreviewLargeElement.shadowRoot!.getElementById(
                                              'thumbnailContainer')!.click();

    let topicSource = await artGalleryPromise;
    assertEquals(
        topicSource, TopicSource.kArtGallery,
        'navigates to art gallery topic source');

    // Set the topic source to kGooglePhotos and check that clicking the photo
    // collage goes to kGooglePhotos subpage.
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.notifyObservers();
    const googlePhotosPromise = setFakeRouter();

    ambientPreviewLargeElement.shadowRoot!.getElementById(
                                              'thumbnailContainer')!.click();

    topicSource = await googlePhotosPromise;
    assertEquals(
        topicSource, TopicSource.kGooglePhotos,
        'navigates to google photos topic source');
  });

  test('displays zero state message before UI change', async () => {
    // Disables `isAmbientSubpageUiChangeEnabled` to show the previous UI.
    loadTimeData.overrideValues({isAmbientSubpageUiChangeEnabled: false});

    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
    personalizationStore.data.ambient.ambientModeEnabled = false;
    personalizationStore.data.ambient.googlePhotosAlbumsPreviews =
        ambientProvider.googlePhotosAlbumsPreviews;
    ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    const messageContainer =
        ambientPreviewLargeElement.shadowRoot!.getElementById(
            'messageContainer');
    assertTrue(!!messageContainer);
    const textSpan =
        messageContainer.querySelector<HTMLSpanElement>('#turnOnDescription');
    assertTrue(!!textSpan);
    assertEquals(
        ambientPreviewLargeElement.i18n('ambientModeMainPageZeroStateMessage'),
        textSpan.innerText.trim());
  });

  test(
      'displays not available message for enterprise controlled user',
      async () => {
        // Enable `isAmbientModeManaged` to mock an enterprise controlled user.
        loadTimeData.overrideValues({
          isAmbientSubpageUiChangeEnabled: true,
          isAmbientModeManaged: true,
        });

        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.googlePhotosAlbumsPreviews =
            ambientProvider.googlePhotosAlbumsPreviews;
        ambientPreviewLargeElement = initElement(AmbientPreviewLarge);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewLargeElement);

        const messageContainer =
            ambientPreviewLargeElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const textSpan = messageContainer.querySelector<HTMLSpanElement>(
            '#turnOnDescription');
        assertTrue(!!textSpan);
        assertEquals(
            ambientPreviewLargeElement.i18n(
                'ambientModeMainPageEnterpriseUserMessage'),
            textSpan.innerText.trim());
      });
});

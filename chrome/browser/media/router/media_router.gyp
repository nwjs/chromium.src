# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'media_router.gypi',
  ],
  'targets': [
    {
      # GN version: //chrome/browser/media/router:router
      'target_name': 'media_router',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/chrome/common_constants.gyp:common_constants',
        '<(DEPTH)/components/components.gyp:keyed_service_content',
        '<(DEPTH)/components/components.gyp:keyed_service_core',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
      'sources': [
        '<@(media_router_sources)',
      ],
      'conditions': [
        [ 'OS!="android" and OS!="ios"', {
          'dependencies': [
            # media_router_type_converters.h needs the generated file.
            'media_router_mojo_gen',
            'media_router_mojo',
            '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
          ],
          'sources': [
            '<@(media_router_non_android_sources)',
          ]
        }],
        [ 'OS!="win"', {
           'sources/': [ ['exclude', '_win(_browsertest|_unittest|_test)?\\.(h|cc)$'],
                    ['exclude', '(^|/)win/'],
                    ['exclude', '(^|/)win_[^/]*\\.(h|cc)$'] ],
        }],
      ]
    },
    {
      # Mojo compiler for the Media Router internal API.
      'target_name': 'media_router_mojo_gen',
      'type': 'none',
      'sources': [
        'media_router.mojom',
      ],
      'includes': [
        '../../../../mojo/mojom_bindings_generator.gypi',
      ],
    },
    {
      'target_name': 'media_router_mojo',
      'type': 'static_library',
      'dependencies': [
        'media_router_mojo_gen',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/media/router/media_router.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/media/router/media_router.mojom.h',
      ],
    },
    {
      # GN version: //chrome/browser/media/router:test_support
      'target_name': 'media_router_test_support',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        'media_router',
        'media_router_mojo',
        'media_router_mojo_gen',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
      ],
      'sources': [
        '<@(media_router_test_support_sources)',
      ],
    },
  ],
}

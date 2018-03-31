vars = {
  'android_git':
    'https://android.googlesource.com',
  'angle_revision':
    'e858cb1db3c266f67adf998dc14bb17eccec084b',
  'aomedia_git':
    'https://aomedia.googlesource.com',
  'boringssl_git':
    'https://boringssl.googlesource.com',
  'boringssl_revision':
    'a6bfc45b6286e358ba83f7daa769e1e9012cc7bb',
  'buildspec_platforms':
    'all',
  'buildtools_revision':
    '3748a2a90871fc25b0455790fa5a6699553f5197',
  'catapult_revision':
    '0428350035df0644149605dc1e0dc83938a8ee09',
  'checkout_configuration':
    'default',
  'checkout_instrumented_libraries':
    'checkout_linux and checkout_configuration == "default"',
  'checkout_libaom': True,
  'checkout_nacl': True,
  'checkout_oculus_sdk':
    'checkout_src_internal and checkout_win',
  'checkout_src_internal': False,
  'checkout_telemetry_dependencies': False,
  'checkout_traffic_annotation_tools':
    'checkout_configuration == "default"',
  'chromium_git':
    'https://chromium.googlesource.com',
  'devtools_node_modules_revision':
    '5f7cd2497d7a643125c3b6eb910d99ba28be6899',
  'feed_revision':
    'c9c8ed449e6a4a15715c4bb2e9be3a12a1e01250',
  'freetype_revision':
    '713d68ee9f47cc8df56e47fa2f54b191bb8c3186',
  'nwjs_git':
    'https://github.com/nwjs',
  'google_toolbox_for_mac_revision':
    '3c3111d3aefe907c8c0f0e933029608d96ceefeb',
  'harfbuzz_revision':
    '957e7756634a4fdf1654041e20e883cf964ecac9',
  'libfuzzer_revision':
    'ba2c1cd6f87accb32b5dbce297387c56a2e53a2f',
  'libprotobuf-mutator':
    '3fc43a01d721ef1bacfefed170bc22abf1b8b051',
  'lighttpd_revision':
    '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  'lss_revision':
    'e6527b0cd469e3ff5764785dadcb39bf7d787154',
  'nacl_revision':
    '303fc9961cb4231aa9828218362914ee4e51d16a',
  'openmax_dl_revision':
    '63d8cf4708c94c9c8931c389ce333954541a96f2',
  'pdfium_git':
    'https://pdfium.googlesource.com',
  'pdfium_revision':
    'e6ce3428fce89f17e2e416adc567a401901f340b',
  'sfntly_revision':
    '2804148152d27fa2e6ec97a32bc2d56318e51142',
  'skia_git':
    'https://skia.googlesource.com',
  'skia_revision':
    '33f02edb14bb2ad802324daa99886709990893a1',
  'swarming_revision':
    '88229872dd17e71658fe96763feaa77915d8cbd6',
  'swiftshader_git':
    'https://swiftshader.googlesource.com',
  'swiftshader_revision':
    'b04881b856b90984a54c9d370f2d5e04d6383cc1',
  'v8_revision':
    'd8f4d70144792c65d45e560f999d17407e8bed65',
  'webrtc_git':
    'https://webrtc.googlesource.com'
}

allowed_hosts = [
  'android.googlesource.com',
  'aomedia.googlesource.com',
  'boringssl.googlesource.com',
  'chrome-infra-packages.appspot.com',
  'chrome-internal.googlesource.com',
  'chromium.googlesource.com',
  'pdfium.googlesource.com',
  'skia.googlesource.com',
  'swiftshader.googlesource.com',
  'webrtc.googlesource.com'
]

deps = {
  'src-internal': {
    'condition':
      'checkout_src_internal',
    'url':
      'https://chrome-internal.googlesource.com/chrome/src-internal.git@48c82919be0ec44223941341bf2f8c199649df89'
  },
  'src/buildtools':
    (Var("chromium_git")) + '/chromium/buildtools.git@3748a2a90871fc25b0455790fa5a6699553f5197',
  'src/chrome/android/profiles': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/afdo/profiles/android',
        'version':
          'version:3309'
      }
    ]
  },
  'src/chrome/browser/resources/media_router/extension/src':
    (Var("chromium_git")) + '/media_router.git@50d927e720309777a39fdf1ca89fd096bb51e103',
  'src/chrome/installer/mac/third_party/xz/xz': {
    'condition':
      'checkout_mac',
    'url':
      (Var("chromium_git")) + '/chromium/deps/xz.git@eecaf55632ca72e90eb2641376bce7cdbc7284f7'
  },
  'src/chrome/test/data/perf/canvas_bench':
    (Var("chromium_git")) + '/chromium/canvas_bench.git@a7b40ea5ae0239517d78845a5fc9b12976bfc732',
  'src/chrome/test/data/perf/frame_rate/content':
    (Var("chromium_git")) + '/chromium/frame_rate/content.git@c10272c88463efeef6bb19c9ec07c42bc8fe22b9',
  'src/chrome/test/data/vr/webvr_info':
    (Var("chromium_git")) + '/external/github.com/toji/webvr.info.git@c58ae99b9ff9e2aa4c524633519570bf33536248',
  'src/ios/third_party/earl_grey/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/EarlGrey.git@2fd8a7d4b76f820fb95bce495c0ceb324dbe3edb'
  },
  'src/ios/third_party/fishhook/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/facebook/fishhook.git@d172d5247aa590c25d0b1885448bae76036ea22c'
  },
  'src/ios/third_party/gcdwebserver/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/swisspol/GCDWebServer.git@43555c66627f6ed44817855a0f6d465f559d30e0'
  },
  'src/ios/third_party/material_components_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-components/material-components-ios.git@3ac6d0ff2ac59613b12f3278a2ca36f43692ad8c'
  },
  'src/ios/third_party/material_font_disk_loader_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-font-disk-loader-ios.git@8e30188777b016182658fbaa0a4a020a48183224'
  },
  'src/ios/third_party/material_internationalization_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-internationalization-ios.git@8f28a55c7f35b95a587bba01a8467ea470647873'
  },
  'src/ios/third_party/material_roboto_font_loader_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-roboto-font-loader-ios.git@4aa51e906e5671c71d24e991f1f10d782a58409f'
  },
  'src/ios/third_party/material_sprited_animation_view_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-sprited-animation-view-ios.git@c6e16d06bdafd95540c62b3402d9414692fbca81'
  },
  'src/ios/third_party/material_text_accessibility_ios/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-text-accessibility-ios.git@92c9e56f4e07622084b3d931247db974fec55dde'
  },
  'src/ios/third_party/motion_animator_objc/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-motion/motion-animator-objc.git@5df831026445004b2fc0f6a42f8b8f33af46512b'
  },
  'src/ios/third_party/motion_interchange_objc/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-motion/motion-interchange-objc.git@9be1e8572f8debb8dd9033ce9bd6ae56dc7ae1ab'
  },
  'src/ios/third_party/motion_transitioning_objc/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/material-motion/motion-transitioning-objc.git@994fd02d1de3d80ed284f0c1a4b5f459b8b051a6'
  },
  'src/ios/third_party/ochamcrest/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/hamcrest/OCHamcrest.git@92d9c14d13bb864255e65c09383564653896916b'
  },
  'src/media/cdm/api':
    (Var("chromium_git")) + '/chromium/cdm.git@cc347b850c11ea791837d5c70b12421fd77a3731',
  'src/native_client': {
    'condition':
      'checkout_nacl',
    'url':
      (Var("chromium_git")) + '/native_client/src/native_client.git@303fc9961cb4231aa9828218362914ee4e51d16a'
  },
  'src/third_party/SPIRV-Tools/src':
    (Var("chromium_git")) + '/external/github.com/KhronosGroup/SPIRV-Tools.git@9166854ac93ef81b026e943ccd230fed6c8b8d3c',
  'src/third_party/accessibility_test_framework': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/accessibility-test-framework',
        'version':
          'version:2.1-cr0'
      }
    ]
  },
  'src/third_party/android_ndk': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/android_ndk.git@635bc380968a76f6948fee65f80a0b28db53ae81'
  },
  'src/third_party/android_protobuf/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("android_git")) + '/platform/external/protobuf.git@7fca48d8ce97f7ba3ab8eea5c472f1ad3711762f'
  },
  'src/third_party/android_support_test_runner': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/android_support_test_runner',
        'version':
          'version:0.5-cr0'
      }
    ]
  },
  'src/third_party/android_system_sdk': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/android_system_sdk',
        'version':
          'version:27-cr0'
      }
    ]
  },
  'src/third_party/android_tools': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/android_tools.git@c22a664c39af72dd8f89200220713dcad811300a'
  },
  'src/third_party/angle':
    (Var("chromium_git")) + '/angle/angle.git@e858cb1db3c266f67adf998dc14bb17eccec084b',
  'src/third_party/apache-portable-runtime/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/apache-portable-runtime.git@c3f11fcd86b42922834cae91103cf068246c6bb6'
  },
  'src/third_party/apk-patch-size-estimator': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/apk-patch-size-estimator',
        'version':
          'version:0.2-cr0'
      }
    ]
  },
  'src/third_party/auto/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/auto.git@8a81a858ae7b78a1aef71ac3905fade0bbd64e82'
  },
  'src/third_party/bazel': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/bazel',
        'version':
          'version:0.10.0'
      }
    ]
  },
  'src/third_party/bidichecker':
    (Var("chromium_git")) + '/external/bidichecker/lib.git@97f2aa645b74c28c57eca56992235c79850fa9e0',
  'src/third_party/bison': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/bison.git@083c9a45e4affdd5464ee2b224c2df649c6e26c3'
  },
  'src/third_party/boringssl/src':
    (Var("boringssl_git")) + '/boringssl.git@a6bfc45b6286e358ba83f7daa769e1e9012cc7bb',
  'src/third_party/bouncycastle': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/bouncycastle',
        'version':
          'version:1.46-cr0'
      }
    ]
  },
  'src/third_party/breakpad/breakpad':
    (Var("chromium_git")) + '/breakpad/breakpad.git@ac4a549e29fbc8fe58212380e41fa24f36e6c3ec',
  'src/third_party/byte_buddy': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/byte_buddy',
        'version':
          'version:1.4.17-cr0'
      }
    ]
  },
  'src/third_party/catapult':
    (Var("chromium_git")) + '/catapult.git@0428350035df0644149605dc1e0dc83938a8ee09',
  'src/third_party/ced/src':
    (Var("chromium_git")) + '/external/github.com/google/compact_enc_det.git@94c367a1fe3a13207f4b22604fcfd1d9f9ddf6d9',
  'src/third_party/chromite': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/chromite.git@44e40f3f36712c6ba20055407e05b5f6d7f807bc'
  },
  'src/third_party/cld_3/src':
    (Var("chromium_git")) + '/external/github.com/google/cld_3.git@484afe9ba7438d078e60b3a26e7fb590213c0e17',
  'src/third_party/colorama/src':
    (Var("chromium_git")) + '/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/crc32c/src':
    (Var("chromium_git")) + '/external/github.com/google/crc32c.git@0f771ed5ef83556451e1736f22b1a11054dc81c3',
  'src/third_party/cros_system_api': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/system_api.git@c873de4b9021e002a2a742aee91fbc612c2445d6'
  },
  'src/third_party/custom_tabs_client/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/custom-tabs-client.git@5f4df5ce54f4956212eef396e67be376fc4c043c'
  },
  'src/third_party/depot_tools':
    (Var("chromium_git")) + '/chromium/tools/depot_tools.git@fe68c91e470adcb5b2e8b20f35e1ac0b1999b27f',
  'src/third_party/devtools-node-modules':
    (Var("chromium_git")) + '/external/github.com/ChromeDevTools/devtools-node-modules@5f7cd2497d7a643125c3b6eb910d99ba28be6899',
  'src/third_party/dom_distiller_js/dist':
    (Var("chromium_git")) + '/chromium/dom-distiller/dist.git@60b46718e28f553ab57e3d2bbda5b3b41456f417',
  'src/third_party/elfutils/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/elfutils.git@249673729a7e5dbd5de4f3760bdcaa3d23d154d7'
  },
  'src/third_party/errorprone/lib': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/chromium/third_party/errorprone.git@ecc57c2b00627667874744b9ad8efe10734d97a8'
  },
  'src/third_party/espresso': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/espresso',
        'version':
          'version:2.2.1-cr0'
      }
    ]
  },
  'src/third_party/feed/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/feed@c9c8ed449e6a4a15715c4bb2e9be3a12a1e01250'
  },
  'src/third_party/ffmpeg':
    (Var("chromium_git")) + '/chromium/third_party/ffmpeg.git@5baad932589a94969782df630db02ec0293d920f',
  'src/third_party/flac':
    (Var("chromium_git")) + '/chromium/deps/flac.git@7d0f5b3a173ffe98db08057d1f52b7787569e0a6',
  'src/third_party/flatbuffers/src':
    (Var("chromium_git")) + '/external/github.com/google/flatbuffers.git@01c50d57a67a52ee3cddd81b54d4647e9123a290',
  'src/third_party/fontconfig/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/fontconfig.git@b546940435ebfb0df575bc7a2350d1e913919c34'
  },
  'src/third_party/freetype/src':
    (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@713d68ee9f47cc8df56e47fa2f54b191bb8c3186',
  'src/third_party/gestures/gestures': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/gestures.git@74f55100df966280d305d5d5ada824605f875839'
  },
  'src/third_party/glslang/src':
    (Var("chromium_git")) + '/external/github.com/google/glslang.git@ec1476b7060306fd9109faf7a4c70a20ea3b538c',
  'src/third_party/gnu_binutils': {
    'condition':
      'checkout_nacl and checkout_win',
    'url':
      (Var("chromium_git")) + '/native_client/deps/third_party/gnu_binutils.git@f4003433b61b25666565690caf3d7a7a1a4ec436'
  },
  'src/third_party/google_toolbox_for_mac/src': {
    'condition':
      'checkout_ios or checkout_mac',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/google-toolbox-for-mac.git@3c3111d3aefe907c8c0f0e933029608d96ceefeb'
  },
  'src/third_party/googletest/src':
    (Var("chromium_git")) + '/external/github.com/google/googletest.git@a325ad2db5deb623eab740527e559b81c0f39d65',
  'src/third_party/gperf': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/gperf.git@d892d79f64f9449770443fb06da49b5a1e5d33c1'
  },
  'src/third_party/gson': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/gson',
        'version':
          'version:2.8.0-cr0'
      }
    ]
  },
  'src/third_party/guava': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/guava',
        'version':
          'version:23.0-cr0'
      }
    ]
  },
  'src/third_party/gvr-android-sdk/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/googlevr/gvr-android-sdk.git@233e7fe922a543e0bc55382d64cacd047307d0e7'
  },
  'src/third_party/hamcrest': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/hamcrest',
        'version':
          'version:1.3-cr0'
      }
    ]
  },
  'src/third_party/harfbuzz-ng/src':
    (Var("chromium_git")) + '/external/github.com/harfbuzz/harfbuzz.git@957e7756634a4fdf1654041e20e883cf964ecac9',
  'src/third_party/hunspell_dictionaries':
    (Var("chromium_git")) + '/chromium/deps/hunspell_dictionaries.git@a9bac57ce6c9d390a52ebaad3259f5fdb871210e',
  'src/third_party/icu':
    (Var("chromium_git")) + '/chromium/deps/icu.git@d888fd2a1be890f4d35e43f68d6d79f42519a357',
  'src/third_party/icu4j': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/icu4j',
        'version':
          'version:53.1-cr0'
      }
    ]
  },
  'src/third_party/intellij': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/intellij',
        'version':
          'version:12.0-cr0'
      }
    ]
  },
  'src/third_party/javax_inject': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/javax_inject',
        'version':
          'version:1-cr0'
      }
    ]
  },
  'src/third_party/jsoncpp/source':
    (Var("chromium_git")) + '/external/github.com/open-source-parsers/jsoncpp.git@f572e8e42e22cfcf5ab0aea26574f408943edfa4',
  'src/third_party/jsr-305/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/jsr-305.git@642c508235471f7220af6d5df2d3210e3bfc0919'
  },
  'src/third_party/junit/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/junit.git@64155f8a9babcfcf4263cf4d08253a1556e75481'
  },
  'src/third_party/leakcanary/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/square/leakcanary.git@608ded739e036a3aa69db47ac43777dcee506f8e'
  },
  'src/third_party/leveldatabase/src':
    (Var("chromium_git")) + '/external/leveldb.git@41172a24016bc29fc795ed504737392587f54e3d',
  'src/third_party/libFuzzer/src':
    (Var("chromium_git")) + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git@ba2c1cd6f87accb32b5dbce297387c56a2e53a2f',
  'src/third_party/libaddressinput/src':
    (Var("chromium_git")) + '/external/libaddressinput.git@d955c63ec7048d59dffd20af25eeec23da878d27',
  'src/third_party/libaom/source/libaom': {
    'condition':
      'checkout_libaom',
    'url':
      (Var("aomedia_git")) + '/aom.git@cc92258a08d98f469dff1be288acbc322632377b'
  },
  'src/third_party/libdrm/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/third_party/libdrm.git@16ffb1e6fce0fbd57f7a1e76021c575a40f6dc7a'
  },
  'src/third_party/libevdev/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/libevdev.git@9f7a1961eb4726211e18abd147d5a11a4ea86744'
  },
  'src/third_party/libjpeg_turbo':
    (Var("chromium_git")) + '/chromium/deps/libjpeg_turbo.git@a1750dbc79a8792dde3d3f7d7d8ac28ba01ac9dd',
  'src/third_party/liblouis/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/liblouis-github.git@5f9c03f2a3478561deb6ae4798175094be8a26c2'
  },
  'src/third_party/libphonenumber/dist':
    (Var("chromium_git")) + '/external/libphonenumber.git@a4da30df63a097d67e3c429ead6790ad91d36cf4',
  'src/third_party/libprotobuf-mutator/src':
    (Var("chromium_git")) + '/external/github.com/google/libprotobuf-mutator.git@3fc43a01d721ef1bacfefed170bc22abf1b8b051',
  'src/third_party/libsrtp':
    (Var("chromium_git")) + '/chromium/deps/libsrtp.git@1d45b8e599dc2db6ea3ae22dbc94a8c504652423',
  'src/third_party/libsync/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/aosp/platform/system/core/libsync.git@f4f4387b6bf2387efbcfd1453af4892e8982faf6'
  },
  'src/third_party/libvpx/source/libvpx':
    (Var("chromium_git")) + '/webm/libvpx.git@1f82e061229352b6e86564da7e3132b97ed924be',
  'src/third_party/libwebm/source':
    (Var("chromium_git")) + '/webm/libwebm.git@b03c65468b06d097f27235d93d76bfc45f490ede',
  'src/third_party/libyuv':
    (Var("chromium_git")) + '/libyuv/libyuv.git@98a0a157dcf5dee0882b2dfcc9578ab1f44afb12',
  'src/third_party/lighttpd': {
    'condition':
      'checkout_mac or checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/lighttpd.git@9dfa55d15937a688a92cbf2b7a8621b0927d06eb'
  },
  'src/third_party/lss': {
    'condition':
      'checkout_android or checkout_linux',
    'url':
      (Var("chromium_git")) + '/linux-syscall-support.git@e6527b0cd469e3ff5764785dadcb39bf7d787154'
  },
  'src/third_party/material_design_icons/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/material-design-icons.git@5ab428852e35dc177a8c37a2df9dc9ccf768c65a'
  },
  'src/third_party/mesa/src':
    (Var("chromium_git")) + '/chromium/deps/mesa.git@803b1132096707417736df8d167176a33813aa9f',
  'src/third_party/mingw-w64/mingw/bin': {
    'condition':
      'checkout_nacl and checkout_win',
    'url':
      (Var("chromium_git")) + '/native_client/deps/third_party/mingw-w64/mingw/bin.git@3cc8b140b883a9fe4986d12cfd46c16a093d3527'
  },
  'src/third_party/minigbm/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/minigbm.git@6eca36809e185337bfcca95310a1765c34c360e1'
  },
  'src/third_party/minizip/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/github.com/nmoinvaz/minizip@53a657318af1fccc4bac7ed230729302b2391d1d'
  },
  'src/third_party/mockito/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/mockito/mockito.git@de83ad4598ad4cf5ea53c69a8a8053780b04b850'
  },
  'src/third_party/nacl_sdk_binaries': {
    'condition':
      'checkout_nacl and checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/nacl_sdk_binaries.git@759dfca03bdc774da7ecbf974f6e2b84f43699a5'
  },
  'src/third_party/netty-tcnative/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/netty-tcnative.git@5b46a8ef4a39c39c576fcdaaf718b585d75df463'
  },
  'src/third_party/netty4/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/netty4.git@cc4420b13bb4eeea5b1cf4f93b2755644cd3b120'
  },
  'src/third_party/objenesis': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/objenesis',
        'version':
          'version:2.4-cr0'
      }
    ]
  },
  'src/third_party/openh264/src':
    (Var("chromium_git")) + '/external/github.com/cisco/openh264@2e96d62426547ac4fb5cbcd122e5f6eb68d66ee6',
  'src/third_party/openmax_dl':
    (Var("webrtc_git")) + '/deps/third_party/openmax.git@63d8cf4708c94c9c8931c389ce333954541a96f2',
  'src/third_party/ow2_asm': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/ow2_asm',
        'version':
          'version:5.0.1-cr0'
      }
    ]
  },
  'src/third_party/pdfium':
    (Var("pdfium_git")) + '/pdfium.git@e6ce3428fce89f17e2e416adc567a401901f340b',
  'src/third_party/pefile': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/external/pefile.git@72c6ae42396cb913bcab63c15585dc3b5c3f92f1'
  },
  'src/third_party/perfetto':
    (Var("android_git")) + '/platform/external/perfetto.git@135841c8077f13f14c6b80e32d391da84d2ee131',
  'src/third_party/perl': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/perl.git@ac0d98b5cee6c024b0cffeb4f8f45b6fc5ccdb78'
  },
  'src/third_party/pyelftools': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/third_party/pyelftools.git@19b3e610c86fcadb837d252c794cb5e8008826ae'
  },
  'src/third_party/pyftpdlib/src':
    (Var("chromium_git")) + '/external/pyftpdlib.git@2be6d65e31c7ee6320d059f581f05ae8d89d7e45',
  'src/third_party/pywebsocket/src':
    (Var("chromium_git")) + '/external/github.com/google/pywebsocket.git@2d7b73c3acbd0f41dcab487ae5c97c6feae06ce2',
  'src/third_party/re2/src':
    (Var("chromium_git")) + '/external/github.com/google/re2.git@5185d85264d23cfae4b38e2703703e9a4c8e974c',
  'src/third_party/requests/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/kennethreitz/requests.git@f172b30356d821d180fa4ecfa3e71c7274a32de4'
  },
  'src/third_party/robolectric': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/robolectric',
        'version':
          'version:3.5.1'
      }
    ]
  },
  'src/third_party/robolectric/robolectric': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/robolectric.git@7e067f1112e1502caa742f7be72d37b5678d3403'
  },
  'src/third_party/sfntly/src':
    (Var("chromium_git")) + '/external/github.com/googlei18n/sfntly.git@2804148152d27fa2e6ec97a32bc2d56318e51142',
  'src/third_party/shaderc/src':
    (Var("chromium_git")) + '/external/github.com/google/shaderc.git@cd8793c34907073025af2622c28bcee64e9879a4',
  'src/third_party/skia':
    (Var("skia_git")) + '/skia.git@33f02edb14bb2ad802324daa99886709990893a1',
  'src/third_party/smhasher/src':
    (Var("chromium_git")) + '/external/smhasher.git@e87738e57558e0ec472b2fc3a643b838e5b6e88f',
  'src/third_party/snappy/src':
    (Var("chromium_git")) + '/external/github.com/google/snappy.git@ca37ab7fb9b718e056009babb4fea591626e5882',
  'src/third_party/sqlite4java': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/sqlite4java',
        'version':
          'version:0.282-cr0'
      }
    ]
  },
  'src/third_party/swiftshader':
    (Var("swiftshader_git")) + '/SwiftShader.git@b04881b856b90984a54c9d370f2d5e04d6383cc1',
  'src/third_party/ub-uiautomator/lib': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/chromium/third_party/ub-uiautomator.git@00270549ce3161ae72ceb24712618ea28b4f9434'
  },
  'src/third_party/usrsctp/usrsctplib':
    (Var("chromium_git")) + '/external/github.com/sctplab/usrsctp@159d060dceec41a64a57356cbba8c455105f3f72',
  'src/third_party/visualmetrics/src':
    (Var("chromium_git")) + '/external/github.com/WPO-Foundation/visualmetrics.git@1edde9d2fe203229c895b648fdec355917200ad6',
  'src/third_party/wayland-protocols/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git@4f789286e4ab7f6fecc2ccb895d79362a9b2382a'
  },
  'src/third_party/wayland/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/anongit.freedesktop.org/git/wayland/wayland.git@1361da9cd5a719b32d978485a29920429a31ed25'
  },
  'src/third_party/wds/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/github.com/01org/wds@ac3d8210d95f3000bf5c8e16a79dbbbf22d554a5'
  },
  'src/third_party/webdriver/pylib':
    (Var("chromium_git")) + '/external/selenium/py.git@5fd78261a75fe08d27ca4835fb6c5ce4b42275bd',
  'src/third_party/webgl/src':
    (Var("chromium_git")) + '/external/khronosgroup/webgl.git@3c1cb0203b6cfc10389e85a350b2ea6ca29d01ce',
  'src/third_party/webrtc':
    (Var("webrtc_git")) + '/src.git@bb50ce5bb6d580b243b0fde532c596d6f4248083',
  'src/third_party/xdg-utils': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromium/deps/xdg-utils.git@d80274d5869b17b8c9067a1022e4416ee7ed5e0d'
  },
  'src/third_party/xstream': {
    'condition':
      'checkout_android',
    'dep_type':
      'cipd',
    'packages': [
      {
        'package':
          'chromium/third_party/xstream',
        'version':
          'version:1.4.8-cr0'
      }
    ]
  },
  'src/third_party/yasm/source/patched-yasm':
    (Var("chromium_git")) + '/chromium/deps/yasm/patched-yasm.git@b98114e18d8b9b84586b10d24353ab8616d4c5fc',
  'src/tools/gyp':
    (Var("chromium_git")) + '/external/gyp.git@d61a9397e668fa9843c4aa7da9e79460fe590bfb',
  'src/tools/page_cycler/acid3':
    (Var("chromium_git")) + '/chromium/deps/acid3.git@6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',
  'src/tools/swarming_client':
    (Var("chromium_git")) + '/infra/luci/client-py.git@88229872dd17e71658fe96763feaa77915d8cbd6',
  #'src/v8':
  #  (Var("chromium_git")) + '/v8/v8.git@d8f4d70144792c65d45e560f999d17407e8bed65'
  #  (Var("nwjs_git")) + '/v8.git@origin/nw16',
  #'src/content/nw':
  #  (Var("nwjs_git")) + '/nw.js.git@origin/nw16',
  #'src/third_party/node':
  #  (Var("nwjs_git")) + '/node.git@origin/nw16',
}

gclient_gn_args = [
  'checkout_libaom',
  'checkout_nacl',
  'checkout_oculus_sdk'
]

gclient_gn_args_file =  'src/build/config/gclient_args.gni'

hooks = [
  {
    'action': [
      'python',
      'src/build/landmines.py'
    ],
    'pattern':
      '.',
    'name':
      'landmines'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/update_depot_tools_toggle.py',
      '--disable'
    ],
    'pattern':
      '.',
    'name':
      'disable_depot_tools_selfupdate'
  },
  {
    'action': [
      'python',
      'src/tools/remove_stale_pyc_files.py',
      'src/android_webview/tools',
      'src/build/android',
      'src/gpu/gles2_conform_support',
      'src/infra',
      'src/ppapi',
      'src/printing',
      'src/third_party/catapult',
      'src/third_party/closure_compiler/build',
      'src/third_party/WebKit/Tools/Scripts',
      'src/tools'
    ],
    'pattern':
      '.',
    'name':
      'remove_stale_pyc_files'
  },
  {
    'action': [
      'python',
      'src/build/download_nacl_toolchains.py',
      '--mode',
      'nacl_core_sdk',
      'sync',
      '--extract'
    ],
    'pattern':
      '.',
    'name':
      'nacltools',
    'condition':
      'checkout_nacl'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--arch=arm'
    ],
    'pattern':
      '.',
    'name':
      'sysroot_arm',
    'condition':
      'checkout_linux and checkout_arm'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--arch=arm64'
    ],
    'pattern':
      '.',
    'name':
      'sysroot_arm64',
    'condition':
      'checkout_linux and checkout_arm64'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--arch=x86'
    ],
    'pattern':
      '.',
    'name':
      'sysroot_x86',
    'condition':
      'checkout_linux and (checkout_x86 or checkout_x64)'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--arch=mips'
    ],
    'pattern':
      '.',
    'name':
      'sysroot_mips',
    'condition':
      'checkout_linux and checkout_mips'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--arch=x64'
    ],
    'pattern':
      '.',
    'name':
      'sysroot_x64',
    'condition':
      'checkout_linux and checkout_x64'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-browser-clang/ciopfs',
      '-s',
      'src/build/ciopfs.sha1'
    ],
    'pattern':
      '.',
    'name':
      'ciopfs_linux',
    'condition':
      'checkout_win and host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/build/vs_toolchain.py',
      'update',
      '--force'
    ],
    'pattern':
      '.',
    'name':
      'win_toolchain',
    'condition':
      'checkout_win'
  },
  {
    'action': [
      'python',
      'src/build/mac_toolchain.py'
    ],
    'pattern':
      '.',
    'name':
      'mac_toolchain',
    'condition':
      'checkout_ios or checkout_mac'
  },
  {
    'action': [
      'python',
      'src/third_party/binutils/download.py'
    ],
    'pattern':
      'src/third_party/binutils',
    'name':
      'binutils',
    'condition':
      'host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/tools/clang/scripts/update.py'
    ],
    'pattern':
      '.',
    'name':
      'clang'
  },
  {
    'action': [
      'python',
      'src/tools/clang/scripts/download_lld_mac.py'
    ],
    'pattern':
      '.',
    'name':
      'lld/mac',
    'condition':
      'host_os == "mac" and checkout_win'
  },
  {
    'action': [
      'python',
      'src/build/util/lastchange.py',
      '-o',
      'src/build/util/LASTCHANGE'
    ],
    'pattern':
      '.',
    'name':
      'lastchange'
  },
  {
    'action': [
      'python',
      'src/build/util/lastchange.py',
      '-m',
      'GPU_LISTS_VERSION',
      '--revision-id-only',
      '--header',
      'src/gpu/config/gpu_lists_version.h'
    ],
    'pattern':
      '.',
    'name':
      'gpu_lists_version'
  },
  {
    'action': [
      'python',
      'src/build/util/lastchange.py',
      '-m',
      'SKIA_COMMIT_HASH',
      '-s',
      'src/third_party/skia',
      '--header',
      'src/skia/ext/skia_commit_hash.h'
    ],
    'pattern':
      '.',
    'name':
      'lastchange_skia'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/win/gn.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_win',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/mac/gn.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_mac',
    'condition':
      'host_os == "mac"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/linux64/gn.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_linux64',
    'condition':
      'host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/win/clang-format.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_win',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/mac/clang-format.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_mac',
    'condition':
      'host_os == "mac"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/linux64/clang-format.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_linux',
    'condition':
      'host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-browser-clang/rc',
      '-s',
      'src/build/toolchain/win/rc/win/rc.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'rc_win',
    'condition':
      'checkout_win and host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-browser-clang/rc',
      '-s',
      'src/build/toolchain/win/rc/mac/rc.sha1'
    ],
    'pattern':
      '.',
    'name':
      'rc_mac',
    'condition':
      'checkout_win and host_os == "mac"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-browser-clang/rc',
      '-s',
      'src/build/toolchain/win/rc/linux64/rc.sha1'
    ],
    'pattern':
      '.',
    'name':
      'rc_linux',
    'condition':
      'checkout_win and host_os == "linux"'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-fonts',
      '-s',
      'src/third_party/test_fonts/test_fonts.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'test_fonts',
    'condition':
      'checkout_linux or (checkout_android or checkout_fuchsia)'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-browser-clang/orderfiles',
      '-d',
      'src/chrome/build'
    ],
    'pattern':
      '.',
    'name':
      'orderfiles_win',
    'condition':
      'checkout_win'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/win64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_win',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/mac64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_mac',
    'condition':
      'host_os == "mac"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/linux64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_linux',
    'condition':
      'host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/build/get_syzygy_binaries.py',
      '--output-dir=src/third_party/syzygy/binaries',
      '--revision=8164b24ebde9c5649c9a09e88a7fc0b0fcbd1bc5',
      '--overwrite',
      '--copy-dia-binaries'
    ],
    'pattern':
      '.',
    'name':
      'syzygy-binaries',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--directory',
      '--recursive',
      '--no_auth',
      '--num_threads=16',
      '--bucket',
      'chromium-apache-win32',
      'src/third_party/apache-win32'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'apache_win32',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-instrumented-libraries',
      '-s',
      'src/third_party/instrumented_libraries/binaries/msan-chained-origins-trusty.tgz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'msan_chained_origins',
    'condition':
      'checkout_instrumented_libraries'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-instrumented-libraries',
      '-s',
      'src/third_party/instrumented_libraries/binaries/msan-no-origins-trusty.tgz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'msan_no_origins',
    'condition':
      'checkout_instrumented_libraries'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '-u',
      '--bucket',
      'v8-wasm-fuzzer',
      '-s',
      'src/v8/test/fuzzer/wasm_corpus.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'wasm_fuzzer'
  },
  {
    'action': [
      'python',
      'src/content/nw/tools/patcher.py'
    ],
    'pattern':
      '.',
    'name':
      'nw_patch'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/8.9.1',
      '-s',
      'src/third_party/node/linux/node-linux-x64.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'node_linux64',
    'condition':
      'host_os == "linux"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/8.9.1',
      '-s',
      'src/third_party/node/mac/node-darwin-x64.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'node_mac',
    'condition':
      'host_os == "mac"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/8.9.1',
      '-s',
      'src/third_party/node/win/node.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'node_win',
    'condition':
      'host_os == "win"'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-nodejs',
      '-s',
      'src/third_party/node/node_modules.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'webui_node_modules'
  },
  {
    'action': [
      'vpython',
      'src/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies'
    ],
    'pattern':
      '.',
    'name':
      'checkout_telemetry_binary_dependencies',
    'condition':
      'checkout_telemetry_dependencies'
  },
  {
    'action': [
      'vpython',
      'src/tools/perf/fetch_benchmark_deps.py',
      '-f'
    ],
    'pattern':
      '.',
    'name':
      'checkout_telemetry_benchmark_deps',
    'condition':
      'checkout_telemetry_dependencies'
  },
  {
    'action': [
      'vpython',
      'src/tools/perf/conditionally_execute',
      '--gyp-condition',
      'fetch_telemetry_dependencies=1',
      'src/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies'
    ],
    'pattern':
      '.',
    'name':
      'fetch_telemetry_binary_dependencies'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--num_threads=4',
      '--bucket',
      'chromium-tools-traffic_annotation',
      '-d',
      'src/tools/traffic_annotation/bin/linux64'
    ],
    'pattern':
      '.',
    'name':
      'tools_traffic_annotation_linux',
    'condition':
      'host_os == "linux" and checkout_traffic_annotation_tools'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--num_threads=4',
      '--bucket',
      'chromium-tools-traffic_annotation',
      '-d',
      'src/tools/traffic_annotation/bin/win32'
    ],
    'pattern':
      '.',
    'name':
      'tools_traffic_annotation_windows',
    'condition':
      'host_os == "win" and checkout_traffic_annotation_tools'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--num_threads=4',
      '--bucket',
      'chromium-binary-patching',
      '-d',
      'src/components/zucchini/testdata'
    ],
    'pattern':
      '.',
    'name':
      'zucchini_testdata'
  },
  {
    'action': [
      'src/build/cipd/clobber_cipd_root.py',
      '--root',
      'src'
    ],
    'pattern':
      '.',
    'name':
      'Android Clobber Deprecated CIPD Root',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'vpython',
      'src/chrome/android/profiles/update_afdo_profile.py'
    ],
    'pattern':
      '.',
    'name':
      'Fetch Android AFDO profile',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/play_services/update.py',
      'download'
    ],
    'pattern':
      '.',
    'name':
      'sdkextras',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-android-tools/checkstyle',
      '-s',
      'src/third_party/checkstyle/checkstyle-8.0-all.jar.sha1'
    ],
    'pattern':
      '.',
    'name':
      'checkstyle',
    'condition':
      'checkout_android or checkout_linux'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gvr-static-shim',
      '-s',
      'src/third_party/gvr-android-sdk/libgvr_shim_static_arm.a.sha1'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'gvr_static_shim_android_arm',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gvr-static-shim',
      '-s',
      'src/third_party/gvr-android-sdk/libgvr_shim_static_arm64.a.sha1'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'gvr_static_shim_android_arm64',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--bucket',
      'chrome-vr-assets',
      '--recursive',
      '--directory',
      'src/chrome/browser/resources/vr/assets/google_chrome'
    ],
    'pattern':
      '.',
    'name':
      'vr_assets',
    'condition':
      'checkout_src_internal and checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--no_auth',
      '--bucket',
      'chromium-gvr-static-shim/controller_test_api',
      '-s',
      'src/third_party/gvr-android-sdk/test-libraries/controller_test_api.aar.sha1'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'vr_controller_test_api',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/gvr-android-sdk/test-apks/update.py'
    ],
    'pattern':
      '.',
    'name':
      'vr_test_apks',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--bucket',
      'chrome-oculus-sdk',
      '--recursive',
      '--num_threads=10',
      '--directory',
      'src/third_party/libovr/src'
    ],
    'pattern':
      '.',
    'name':
      'libovr',
    'condition':
      'checkout_oculus_sdk'
  },
  {
    'action': [
      'python',
      'src/build/android/download_doclava.py'
    ],
    'pattern':
      '.',
    'name':
      'doclava',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/fuchsia/update_sdk.py'
    ],
    'pattern':
      '.',
    'name':
      'fuchsia_sdk',
    'condition':
      'checkout_fuchsia'
  },
  {
    'action': [
      'vpython',
      '-vpython-spec',
      'src/.vpython',
      '-vpython-tool',
      'install'
    ],
    'pattern':
      '.',
    'name':
      'vpython_common'
  }
]

include_rules = [
  '+base',
  '+build',
  '+ipc',
  '+library_loaders',
  '+testing',
  '+third_party/icu/source/common/unicode',
  '+third_party/icu/source/i18n/unicode',
  '+url'
]

recursedeps = [
  'src/buildtools',
  [
    'src/third_party/angle',
    'DEPS.chromium'
  ],
  'src-internal'
]

skip_child_includes = [
  'native_client_sdk',
  'out',
  'skia',
  'testing',
  'third_party/breakpad/breakpad',
  'v8',
  'win8'
]

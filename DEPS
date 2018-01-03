vars = {
  'angle_revision':
    'c3bc984156967837f0b17b866aca0b12a2edf0e6',
  'boringssl_git':
    'https://boringssl.googlesource.com',
  'boringssl_revision':
    '664e99a6486c293728097c661332f92bf2d847c6',
  'buildspec_platforms':
    'all',
  'buildtools_revision':
    'f6d165d9d842ddd29056c127a5f3a3c5d8e0d2e3',
  'catapult_revision':
    '2c4422f10a7858aa6107fd76d69e8329f80b5ee4',
  'checkout_configuration':
    'default',
  'checkout_instrumented_libraries':
    'checkout_linux and checkout_configuration == "default"',
  'checkout_src_internal': False,
  'checkout_traffic_annotation_tools':
    'checkout_configuration == "default"',
  'chromium_git':
    'https://chromium.googlesource.com',
  'devtools_node_modules_revision':
    '6226d6cd80aaf2e5295ed460cf73ef6a582e4d78',
  'freetype_revision':
    'c3a51e430ca5c7a9cc047548b446b044196da995',
  'google_toolbox_for_mac_revision':
    '3c3111d3aefe907c8c0f0e933029608d96ceefeb',
  'libfuzzer_revision':
    '06fb50cc1f0197398c8a70658928a3b91912e68a',
  'libprotobuf-mutator':
    '52af4b0f6f55f9d8836acb605e902db49eb8e6b4',
  'lighttpd_revision':
    '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  'lss_revision':
    '63f24c8221a229f677d26ebe8f3d1528a9d787ac',
  'nacl_revision':
    '7f07816e463da403255f0ab4f6d88004450dd09d',
  'openmax_dl_revision':
    '7acede9c039ea5d14cf326f44aad1245b9e674a7',
  'pdfium_git':
    'https://pdfium.googlesource.com',
  'pdfium_revision':
    '5d98f3044ed5fd3e41d1e764351a8829e1a47e4b',
  'sfntly_revision':
    '2439bd08ff93d4dce761dd6b825917938bd35a4f',
  'skia_git':
    'https://skia.googlesource.com',
  'skia_revision':
    'e60ca7eae432bbff7d2213920cbfb8f3aa9a9c28',
  'swarming_revision':
    '5e8001d9a710121ce7a68efd0804430a34b4f9e4',
  'swiftshader_git':
    'https://swiftshader.googlesource.com',
  'swiftshader_revision':
    'ec5da193b1c29dc8bee19dcc8fe297901ff74911',
  'v8_revision':
    '094a7c93dcdcd921de3883ba4674b7e1a0feffbe',
  'webrtc_git':
    'https://webrtc.googlesource.com'
}

allowed_hosts = [
  'android.googlesource.com',
  'boringssl.googlesource.com',
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
      'https://chrome-internal.googlesource.com/chrome/src-internal.git@ab0fe35aefe18d6eaa5d111bf47cf001dfce3565'
  },
  'src/buildtools':
    (Var("chromium_git")) + '/chromium/buildtools.git@f6d165d9d842ddd29056c127a5f3a3c5d8e0d2e3',
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
    (Var("chromium_git")) + '/external/github.com/toji/webvr.info.git@d73e6182287e3c6c0daa4d3d2ea578a31a2e5a6b',
  'src/ios/third_party/earl_grey/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/EarlGrey.git@b8f1ed850b3b8c059d2912ac4c25a2480ddba826'
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
      (Var("chromium_git")) + '/external/github.com/material-components/material-components-ios.git@e4ef28e6796430e550023be60ff58d6418b40a49'
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
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-internationalization-ios.git@f34691a78fc9a0607b565062f6d5cf9f8ea268a4'
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
      (Var("chromium_git")) + '/external/github.com/material-foundation/material-text-accessibility-ios.git@7340b22cc589101ba0b11516afe4f3a733041951'
  },
  'src/ios/third_party/ochamcrest/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/hamcrest/OCHamcrest.git@d7ee4ecfb6bd13c3c8d364682b6228ccd86e1e1a'
  },
  'src/media/cdm/api':
    (Var("chromium_git")) + '/chromium/cdm.git@1f49c55d3151a4e1eec088befee5f578fea81f4b',
  'src/native_client':
    (Var("chromium_git")) + '/native_client/src/native_client.git@7f07816e463da403255f0ab4f6d88004450dd09d',
  'src/third_party/SPIRV-Tools/src':
    (Var("chromium_git")) + '/external/github.com/KhronosGroup/SPIRV-Tools.git@9166854ac93ef81b026e943ccd230fed6c8b8d3c',
  'src/third_party/android_protobuf/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/android_protobuf.git@7fca48d8ce97f7ba3ab8eea5c472f1ad3711762f'
  },
  'src/third_party/android_tools': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/android_tools.git@ca9dc7245b888c75307f0619e4a39fb46a82de66'
  },
  'src/third_party/angle':
    (Var("chromium_git")) + '/angle/angle.git@2ff870db3a3ba33c6dbf4b88c9ec2e52127b7206',
  'src/third_party/apache-portable-runtime/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/apache-portable-runtime.git@c76a8c4277e09a82eaa229e35246edea1ee0a6a1'
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
    (Var("boringssl_git")) + '/boringssl.git@9f69f139ed1088daabb6525f0c9c34d1e89688f7',
  'src/third_party/breakpad/breakpad':
    (Var("chromium_git")) + '/breakpad/breakpad.git@1c6d1613966eab5d77531e85f3b60c40124b43f0',
  'src/third_party/catapult':
    (Var("chromium_git")) + '/catapult.git@67c01572b02248ec553a2064be7e5e065fb494b8',
  'src/third_party/ced/src':
    (Var("chromium_git")) + '/external/github.com/google/compact_enc_det.git@94c367a1fe3a13207f4b22604fcfd1d9f9ddf6d9',
  'src/third_party/chromite': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/chromite.git@66814c3bbbd2068dd34af9df2f677316bf081752'
  },
  'src/third_party/cld_2/src':
    (Var("chromium_git")) + '/external/github.com/CLD2Owners/cld2.git@84b58a5d7690ebf05a91406f371ce00c3daf31c0',
  'src/third_party/cld_3/src':
    (Var("chromium_git")) + '/external/github.com/google/cld_3.git@ae02d6b8a2af41e87c956c7c7d3f651a8b7b9e79',
  'src/third_party/colorama/src':
    (Var("chromium_git")) + '/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/crc32c/src':
    (Var("chromium_git")) + '/external/github.com/google/crc32c.git@0f771ed5ef83556451e1736f22b1a11054dc81c3',
  'src/third_party/cros_system_api': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/system_api.git@72120182b44dcaf936241c8a9b3695c13bc6b589'
  },
  'src/third_party/custom_tabs_client/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/GoogleChrome/custom-tabs-client.git@cff061038b852d647f7044d828a9df78aa135f38'
  },
  'src/third_party/depot_tools':
    (Var("chromium_git")) + '/chromium/tools/depot_tools.git@9f20d020855c8ee87ba64d53ef94051c06aab860',
  'src/third_party/devtools-node-modules': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/github.com/ChromeDevTools/devtools-node-modules@6226d6cd80aaf2e5295ed460cf73ef6a582e4d78'
  },
  'src/third_party/dom_distiller_js/dist':
    (Var("chromium_git")) + '/chromium/dom-distiller/dist.git@232c293a4d3ebcbc4320f642af017ee054b3be93',
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
      (Var("chromium_git")) + '/chromium/third_party/errorprone.git@6a55852cd7d1ef2f05a75d300495143be2a051d4'
  },
  'src/third_party/ffmpeg':
    (Var("chromium_git")) + '/chromium/third_party/ffmpeg.git@62ff55193a061c21c039a2ba0b39641136912c2e',
  'src/third_party/findbugs': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/chromium/deps/findbugs.git@57f05238d3ac77ea0a194813d3065dd780c6e566'
  },
  'src/third_party/flac':
    (Var("chromium_git")) + '/chromium/deps/flac.git@7d0f5b3a173ffe98db08057d1f52b7787569e0a6',
  'src/third_party/flatbuffers/src':
    (Var("chromium_git")) + '/external/github.com/google/flatbuffers.git@01c50d57a67a52ee3cddd81b54d4647e9123a290',
  'src/third_party/fontconfig/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/fontconfig.git@f16c3118e25546c1b749f9823c51827a60aeb5c1'
  },
  'src/third_party/freetype/src':
    (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@c3a51e430ca5c7a9cc047548b446b044196da995',
  'src/third_party/gestures/gestures': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/gestures.git@74f55100df966280d305d5d5ada824605f875839'
  },
  'src/third_party/glslang/src':
    (Var("chromium_git")) + '/external/github.com/google/glslang.git@210c6bf4d8119dc5f8ac21da2d4c87184f7015e0',
  'src/third_party/gnu_binutils': {
    'condition':
      'checkout_win',
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
    (Var("chromium_git")) + '/external/github.com/google/googletest.git@7f8fefabedf2965980585be8c2bff97458f28e0b',
  'src/third_party/gperf': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/chromium/deps/gperf.git@d892d79f64f9449770443fb06da49b5a1e5d33c1'
  },
  'src/third_party/gvr-android-sdk/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/googlevr/gvr-android-sdk.git@ee5cb1c6138d0be57e82ddafc1b54d7d3e3e5560'
  },
  'src/third_party/hunspell_dictionaries':
    (Var("chromium_git")) + '/chromium/deps/hunspell_dictionaries.git@dc6e7c25bf47cbfb466e0701fd2728b4a12e79d5',
  'src/third_party/icu':
    (Var("chromium_git")) + '/chromium/deps/icu.git@7e19d270ee925d41f2953a3e33f9868b3960fbf1',
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
    (Var("chromium_git")) + '/external/leveldb.git@ca216e493f32278f50a823811ab95f64cf0f839b',
  'src/third_party/libFuzzer/src':
    (Var("chromium_git")) + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git@06fb50cc1f0197398c8a70658928a3b91912e68a',
  'src/third_party/libaddressinput/src':
    (Var("chromium_git")) + '/external/libaddressinput.git@87f69db0b1ca732ba148489d2f75b3af2f236632',
  'src/third_party/libdrm/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/third_party/libdrm.git@0ce18bedd3e62d4784fa755403801934ba171084'
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
    (Var("chromium_git")) + '/external/github.com/google/libprotobuf-mutator.git@52af4b0f6f55f9d8836acb605e902db49eb8e6b4',
  'src/third_party/libsrtp':
    (Var("chromium_git")) + '/chromium/deps/libsrtp.git@1d45b8e599dc2db6ea3ae22dbc94a8c504652423',
  'src/third_party/libsync/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/aosp/platform/system/core/libsync.git@aa6cda6f638bd57d3a024f0d201f723a5c3bb875'
  },
  'src/third_party/libvpx/source/libvpx':
    (Var("chromium_git")) + '/webm/libvpx.git@fe7b869104806752a26a262dc60923639d9a384f',
  'src/third_party/libwebm/source':
    (Var("chromium_git")) + '/webm/libwebm.git@4956b2dec65352af32dc71bab553acb631c64177',
  'src/third_party/libyuv':
    (Var("chromium_git")) + '/libyuv/libyuv.git@5b1af9a33545895ea12c52bf007f17914de19173',
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
      (Var("chromium_git")) + '/linux-syscall-support.git@63f24c8221a229f677d26ebe8f3d1528a9d787ac'
  },
  'src/third_party/material_design_icons/src': {
    'condition':
      'checkout_ios',
    'url':
      (Var("chromium_git")) + '/external/github.com/google/material-design-icons.git@5ab428852e35dc177a8c37a2df9dc9ccf768c65a'
  },
  'src/third_party/mesa/src':
    (Var("chromium_git")) + '/chromium/deps/mesa.git@ef811c6bd4de74e13e7035ca882cc77f85793fef',
  'src/third_party/mingw-w64/mingw/bin': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/native_client/deps/third_party/mingw-w64/mingw/bin.git@3cc8b140b883a9fe4986d12cfd46c16a093d3527'
  },
  'src/third_party/minigbm/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromiumos/platform/minigbm.git@27a7e6a24709564e18c3382d0aeda0b40c7ae03b'
  },
  'src/third_party/minizip/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/github.com/nmoinvaz/minizip@e07e141475220196b55294c8172b274cc32d642d'
  },
  'src/third_party/mockito/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/mockito/mockito.git@de83ad4598ad4cf5ea53c69a8a8053780b04b850'
  },
  'src/third_party/nacl_sdk_binaries': {
    'condition':
      'checkout_win',
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
  'src/third_party/openh264/src':
    (Var("chromium_git")) + '/external/github.com/cisco/openh264@a180c9d4d6f1a4830ca9eed9d159d54996bd63cb',
  'src/third_party/openmax_dl':
    (Var("webrtc_git")) + '/deps/third_party/openmax.git@7acede9c039ea5d14cf326f44aad1245b9e674a7',
  'src/third_party/pdfium':
    (Var("pdfium_git")) + '/pdfium.git@ff2a52cab523bc9de526f97bc060300e55f4506b',
  'src/third_party/pefile': {
    'condition':
      'checkout_win',
    'url':
      (Var("chromium_git")) + '/external/pefile.git@72c6ae42396cb913bcab63c15585dc3b5c3f92f1'
  },
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
    (Var("chromium_git")) + '/external/github.com/google/re2.git@ae9cb49a2e2ba95de4f0c6ec5a8afd039996d2c7',
  'src/third_party/requests/src': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/github.com/kennethreitz/requests.git@f172b30356d821d180fa4ecfa3e71c7274a32de4'
  },
  'src/third_party/robolectric/robolectric': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/external/robolectric.git@b02c65cc6d7465f58f0de48a39914aa905692afa'
  },
  'src/third_party/sfntly/src':
    (Var("chromium_git")) + '/external/github.com/googlei18n/sfntly.git@2439bd08ff93d4dce761dd6b825917938bd35a4f',
  'src/third_party/shaderc/src':
    (Var("chromium_git")) + '/external/github.com/google/shaderc.git@cd8793c34907073025af2622c28bcee64e9879a4',
  'src/third_party/skia':
    (Var("skia_git")) + '/skia.git@dbae7001c9805fb0a4b18fd0cbc889941cb39db4',
  'src/third_party/smhasher/src':
    (Var("chromium_git")) + '/external/smhasher.git@e87738e57558e0ec472b2fc3a643b838e5b6e88f',
  'src/third_party/snappy/src':
    (Var("chromium_git")) + '/external/github.com/google/snappy.git@b02bfa754ebf27921d8da3bd2517eab445b84ff9',
  'src/third_party/swiftshader':
    (Var("swiftshader_git")) + '/SwiftShader.git@ec5da193b1c29dc8bee19dcc8fe297901ff74911',
  'src/third_party/ub-uiautomator/lib': {
    'condition':
      'checkout_android',
    'url':
      (Var("chromium_git")) + '/chromium/third_party/ub-uiautomator.git@00270549ce3161ae72ceb24712618ea28b4f9434'
  },
  'src/third_party/usrsctp/usrsctplib':
    (Var("chromium_git")) + '/external/github.com/sctplab/usrsctp@f4819e1b177f7bfdd761c147f5a649b9f1a78c06',
  'src/third_party/visualmetrics/src':
    (Var("chromium_git")) + '/external/github.com/WPO-Foundation/visualmetrics.git@1edde9d2fe203229c895b648fdec355917200ad6',
  'src/third_party/wayland-protocols/src': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git@26c99346ab5f2273fe5581bc4f6397bbb834f747'
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
    (Var("chromium_git")) + '/external/khronosgroup/webgl.git@34842fa3c36988840c89f5bc6a68503175acf7d9',
  'src/third_party/webrtc':
    (Var("webrtc_git")) + '/src.git@dc5904661c4c8b357e19595d80b2881db3f4a71b',
  'src/third_party/xdg-utils': {
    'condition':
      'checkout_linux',
    'url':
      (Var("chromium_git")) + '/chromium/deps/xdg-utils.git@d80274d5869b17b8c9067a1022e4416ee7ed5e0d'
  },
  'src/third_party/yasm/source/patched-yasm':
    (Var("chromium_git")) + '/chromium/deps/yasm/patched-yasm.git@b98114e18d8b9b84586b10d24353ab8616d4c5fc',
  'src/tools/gyp':
    (Var("chromium_git")) + '/external/gyp.git@d61a9397e668fa9843c4aa7da9e79460fe590bfb',
  'src/tools/page_cycler/acid3':
    (Var("chromium_git")) + '/chromium/deps/acid3.git@6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',
  'src/tools/swarming_client':
    (Var("chromium_git")) + '/infra/luci/client-py.git@5e8001d9a710121ce7a68efd0804430a34b4f9e4',
  'src/v8':
    (Var("chromium_git")) + '/v8/v8.git@4d61cf35f14d59fd3e5d27bcbd6716c6ab554293'
}

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
      'nacltools'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--running-as-hook'
    ],
    'pattern':
      '.',
    'name':
      'sysroot',
    'condition':
      'checkout_linux'
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
      '--platform=win32',
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
      '--platform=darwin',
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
      '--platform=linux*',
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
      '--platform=win32',
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
      '--platform=darwin',
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
      '--platform=linux*',
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
      '--platform=win32',
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
      '--platform=win32',
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
      '--platform=darwin',
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
      '--platform=linux*',
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
      '--revision=190dbfe74c6f5b5913820fa66d9176877924d7c5',
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
      '--platform=win32',
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
      'src/third_party/WebKit/Source/devtools/scripts/local_node/node.py',
      '--running-as-hook',
      '--version'
    ],
    'name':
      'devtools_install_node'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--platform=linux*',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/6.9.4',
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
      '--platform=darwin',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/6.9.4',
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
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-nodejs/6.9.4',
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
      'python',
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
      '--platform=linux*',
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
      'chromium-binary-patching',
      '-d',
      'src/chrome/installer/zucchini/testdata'
    ],
    'pattern':
      '.',
    'name':
      'zucchini_testdata'
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
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'android_system_stubs',
      '-l',
      'third_party/android_system_sdk'
    ],
    'pattern':
      '.',
    'name':
      'android_system_sdk',
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
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-intellij',
      '-l',
      'third_party/intellij'
    ],
    'pattern':
      '.',
    'name':
      'intellij',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-javax-inject',
      '-l',
      'third_party/javax_inject'
    ],
    'pattern':
      '.',
    'name':
      'javax_inject',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-hamcrest',
      '-l',
      'third_party/hamcrest'
    ],
    'pattern':
      '.',
    'name':
      'hamcrest',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-guava',
      '-l',
      'third_party/guava'
    ],
    'pattern':
      '.',
    'name':
      'guava',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-android-support-test-runner',
      '-l',
      'third_party/android_support_test_runner'
    ],
    'pattern':
      '.',
    'name':
      'android_support_test_runner',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-byte-buddy',
      '-l',
      'third_party/byte_buddy'
    ],
    'pattern':
      '.',
    'name':
      'byte_buddy',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-espresso',
      '-l',
      'third_party/espresso'
    ],
    'pattern':
      '.',
    'name':
      'espresso',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-robolectric',
      '-l',
      'third_party/robolectric'
    ],
    'pattern':
      '.',
    'name':
      'robolectric_libs',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-apache-velocity',
      '-l',
      'third_party/apache_velocity'
    ],
    'pattern':
      '.',
    'name':
      'apache_velocity',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-ow2-asm',
      '-l',
      'third_party/ow2_asm'
    ],
    'pattern':
      '.',
    'name':
      'ow2_asm',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-android-tools/bazel/desugar',
      '-l',
      'third_party/bazel/desugar'
    ],
    'pattern':
      '.',
    'name':
      'desugar',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-android-tools/apk-patch-size-estimator',
      '-l',
      'third_party/apk-patch-size-estimator/lib'
    ],
    'pattern':
      '.',
    'name':
      'apk-patch-size-estimator',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-icu4j',
      '-l',
      'third_party/icu4j'
    ],
    'pattern':
      '.',
    'name':
      'icu4j',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-accessibility-test-framework',
      '-l',
      'third_party/accessibility_test_framework'
    ],
    'pattern':
      '.',
    'name':
      'accessibility_test_framework',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-bouncycastle',
      '-l',
      'third_party/bouncycastle'
    ],
    'pattern':
      '.',
    'name':
      'bouncycastle',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-sqlite4java',
      '-l',
      'third_party/sqlite4java'
    ],
    'pattern':
      '.',
    'name':
      'sqlite4java',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-gson',
      '-l',
      'third_party/gson'
    ],
    'pattern':
      '.',
    'name':
      'gson',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-objenesis',
      '-l',
      'third_party/objenesis'
    ],
    'pattern':
      '.',
    'name':
      'objenesis',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-robolectric',
      '-l',
      'third_party/xstream'
    ],
    'pattern':
      '.',
    'name':
      'xstream',
    'condition':
      'checkout_android'
  },
  {
    'action': [
      'python',
      'src/third_party/depot_tools/download_from_google_storage.py',
      '--no_resume',
      '--platform=linux*',
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
      '--platform=linux*',
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
      '--no_resume',
      '--platform=linux*',
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
      'src/build/fuchsia/update_sdk.py',
      'bb7a8c4be8001868468ba0a76389ce4ee111620c'
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
  },
  {
    'action': [
      'vpython',
      '-vpython-spec',
      'src/third_party/WebKit/Tools/Scripts/common.vpython',
      '-vpython-tool',
      'install'
    ],
    'pattern':
      '.',
    'name':
      'vpython_webkitpy'
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
  'src/third_party/android_tools',
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

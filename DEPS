vars = {
  'angle_revision':
    'e6d4e12c18f8c456ae8b95ad08f9dcf8e2558a59',
  'boringssl_revision':
    'de24aadc5bc01130b6a9d25582203bb5308fabe1',
  'buildspec_platforms':
    'win-pgo,',
  'buildtools_revision':
    'ecc8e253abac3b6186a97573871a084f4c0ca3ae',
  'chromium_git':
    'https://chromium.googlesource.com',
  'google_toolbox_for_mac_revision':
    'ce47a231ea0b238fbe95538e86cc61d74c234be6',
  'googlecode_url':
    'http://%s.googlecode.com/svn',
  'libvpx_revision':
    'aa9b5f1d6e226fbeae31b2e8984d6c135c61169f',
  'lighttpd_revision':
    '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  'lss_revision':
    '6f97298fe3794e92c8c896a6bc06e0b36e4c3de3',
  'nacl_revision':
    'b2c6249aa6eea12a4bd6ef09e2cfd6a5bd14ed61',
  'nss_revision':
    'aab0d08a298b29407397fbb1c4219f99e99431ed',
  'openmax_dl_revision':
    '22bb1085a6a0f6f3589a8c3d60ed0a9b82248275',
  'pdfium_revision':
    'ac9e977a913d134c5f536eeef60a2de6941f2863',
  'sfntly_revision':
    '1bdaae8fc788a5ac8936d68bf24f37d977a13dac',
  'skia_revision':
    '4417c7f8bb85aa1eae536cc50c70c0cc87f31171',
  'swarming_revision':
    'b39a448d8522392389b28f6997126a6ab04bfe87',
  'v8_revision':
    'bbcea65d2ff6c1575d6b692acf9e9fc468415e82',
  'webkit_revision':
    '65532c55361aa7f738e5203d10b041cb6f552412'
}

allowed_hosts = [
  'android.googlesource.com',
  'boringssl.googlesource.com',
  'chromium.googlesource.com',
  'pdfium.googlesource.com'
]

deps = {
  'src/breakpad/src':
    (Var("chromium_git")) + '/external/google-breakpad/src.git@0bccfcc38087bf8b71c04befe6cd83c683db980a',
  'src/buildtools':
    (Var("chromium_git")) + '/chromium/buildtools.git@ecc8e253abac3b6186a97573871a084f4c0ca3ae',
  'src/chrome/test/data/perf/canvas_bench':
    (Var("chromium_git")) + '/chromium/canvas_bench.git@a7b40ea5ae0239517d78845a5fc9b12976bfc732',
  'src/chrome/test/data/perf/frame_rate/content':
    (Var("chromium_git")) + '/chromium/frame_rate/content.git@c10272c88463efeef6bb19c9ec07c42bc8fe22b9',
  'src/media/cdm/ppapi/api':
    (Var("chromium_git")) + '/chromium/cdm.git@7377023e384f296cbb27644eb2c485275f1f92e8',
  'src/native_client':
    (Var("chromium_git")) + '/native_client/src/native_client.git@b2c6249aa6eea12a4bd6ef09e2cfd6a5bd14ed61',
  'src/sdch/open-vcdiff':
    (Var("chromium_git")) + '/external/open-vcdiff.git@438f2a5be6d809bc21611a94cd37bfc8c28ceb33',
  'src/testing/gmock':
    (Var("chromium_git")) + '/external/googlemock.git@29763965ab52f24565299976b936d1265cb6a271',
  'src/testing/gtest':
    (Var("chromium_git")) + '/external/googletest.git@23574bf2333f834ff665f894c97bef8a5b33a0a9',
  'src/third_party/WebKit':
    (Var("chromium_git")) + '/chromium/blink.git@690cbb705dee3d35cebe81eb97c519f9adef061f',
  'src/third_party/angle':
    (Var("chromium_git")) + '/angle/angle.git@e6d4e12c18f8c456ae8b95ad08f9dcf8e2558a59',
  'src/third_party/bidichecker':
    (Var("chromium_git")) + '/external/bidichecker/lib.git@97f2aa645b74c28c57eca56992235c79850fa9e0',
  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git@de24aadc5bc01130b6a9d25582203bb5308fabe1',
  'src/third_party/cld_2/src':
    (Var("chromium_git")) + '/external/cld2.git@14d9ef8d4766326f8aa7de54402d1b9c782d4481',
  'src/third_party/colorama/src':
    (Var("chromium_git")) + '/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/crashpad/crashpad':
    (Var("chromium_git")) + '/crashpad/crashpad.git@797adb320680a4a8ad39428075cca287e04b111f',
  'src/third_party/dom_distiller_js/dist':
    (Var("chromium_git")) + '/external/github.com/chromium/dom-distiller-dist.git@81e5b59da2a7a0a518b90b5ded58670322c98128',
  'src/third_party/ffmpeg':
    (Var("chromium_git")) + '/chromium/third_party/ffmpeg.git@1f2f7df0f4005fb620342d847bbaa1ec54122c68',
  'src/third_party/flac':
    (Var("chromium_git")) + '/chromium/deps/flac.git@0635a091379d9677f1ddde5f2eec85d0f096f219',
  'src/third_party/hunspell_dictionaries':
    (Var("chromium_git")) + '/chromium/deps/hunspell_dictionaries.git@c106afdcec5d3de2622e19f1b3294c47bbd8bd72',
  'src/third_party/icu':
    (Var("chromium_git")) + '/chromium/deps/icu.git@7fe225d77f307fdbe24695179a84336ef95c1253',
  'src/third_party/jsoncpp/source':
    (Var("chromium_git")) + '/external/github.com/open-source-parsers/jsoncpp.git@f572e8e42e22cfcf5ab0aea26574f408943edfa4',
  'src/third_party/leveldatabase/src':
    (Var("chromium_git")) + '/external/leveldb.git@40c17c0b84ac0b791fb434096fd5c05f3819ad55',
  'src/third_party/libaddressinput/src':
    (Var("chromium_git")) + '/external/libaddressinput.git@5eeeb797e79fa01503fcdcbebdc50036fac023ef',
  'src/third_party/libexif/sources':
    (Var("chromium_git")) + '/chromium/deps/libexif/sources.git@ed98343daabd7b4497f97fda972e132e6877c48a',
  'src/third_party/libjingle/source/talk':
    (Var("chromium_git")) + '/external/webrtc/trunk/talk.git@38891011af54bc0f5adeddb21918b44c415938fa',
  'src/third_party/libjpeg_turbo':
    (Var("chromium_git")) + '/chromium/deps/libjpeg_turbo.git@f4631b6ee8b1dbb05e51ae335a7886f9ac598ab6',
  'src/third_party/libphonenumber/src/phonenumbers':
    (Var("chromium_git")) + '/external/libphonenumber/cpp/src/phonenumbers.git@0d6e3e50e17c94262ad1ca3b7d52b11223084bca',
  'src/third_party/libphonenumber/src/resources':
    (Var("chromium_git")) + '/external/libphonenumber/resources.git@b6dfdc7952571ff7ee72643cd88c988cbe966396',
  'src/third_party/libphonenumber/src/test':
    (Var("chromium_git")) + '/external/libphonenumber/cpp/test.git@f351a7e007f9c9995494499120bbc361ca808a16',
  'src/third_party/libsrtp':
    (Var("chromium_git")) + '/chromium/deps/libsrtp.git@9c53f858cddd4d890e405e91ff3af0b48dfd90e6',
  'src/third_party/libvpx':
    (Var("chromium_git")) + '/chromium/deps/libvpx.git@aa9b5f1d6e226fbeae31b2e8984d6c135c61169f',
  'src/third_party/libyuv':
    (Var("chromium_git")) + '/libyuv/libyuv.git@6dde4f14bd3b3d1777a371feabbe8130bc043dfa',
  'src/third_party/mesa/src':
    (Var("chromium_git")) + '/chromium/deps/mesa.git@071d25db04c23821a12a8b260ab9d96a097402f0',
  'src/third_party/openmax_dl':
    (Var("chromium_git")) + '/external/webrtc/deps/third_party/openmax.git@22bb1085a6a0f6f3589a8c3d60ed0a9b82248275',
  'src/third_party/opus/src':
    (Var("chromium_git")) + '/chromium/deps/opus.git@cae696156f1e60006e39821e79a1811ae1933c69',
  'src/third_party/pdfium':
    'https://pdfium.googlesource.com/pdfium.git@ac9e977a913d134c5f536eeef60a2de6941f2863',
  'src/third_party/py_trace_event/src':
    (Var("chromium_git")) + '/external/py_trace_event.git@dd463ea9e2c430de2b9e53dea57a77b4c3ac9b30',
  'src/third_party/pyftpdlib/src':
    (Var("chromium_git")) + '/external/pyftpdlib.git@2be6d65e31c7ee6320d059f581f05ae8d89d7e45',
  'src/third_party/pywebsocket/src':
    (Var("chromium_git")) + '/external/pywebsocket/src.git@cb349e87ddb30ff8d1fa1a89be39cec901f4a29c',
  'src/third_party/safe_browsing/testing':
    (Var("chromium_git")) + '/external/google-safe-browsing/testing.git@9d7e8064f3ca2e45891470c9b5b1dce54af6a9d6',
  'src/third_party/scons-2.0.1':
    (Var("chromium_git")) + '/native_client/src/third_party/scons-2.0.1.git@1c1550e17fc26355d08627fbdec13d8291227067',
  'src/third_party/sfntly/cpp/src':
    (Var("chromium_git")) + '/external/sfntly/cpp/src.git@1bdaae8fc788a5ac8936d68bf24f37d977a13dac',
  'src/third_party/skia':
    (Var("chromium_git")) + '/skia.git@4417c7f8bb85aa1eae536cc50c70c0cc87f31171',
  'src/third_party/smhasher/src':
    (Var("chromium_git")) + '/external/smhasher.git@e87738e57558e0ec472b2fc3a643b838e5b6e88f',
  'src/third_party/snappy/src':
    (Var("chromium_git")) + '/external/snappy.git@762bb32f0c9d2f31ba4958c7c0933d22e80c20bf',
  'src/third_party/trace-viewer':
    (Var("chromium_git")) + '/external/trace-viewer.git@fac25e76cafbad757662cd3ac3ba9fba6f9732af',
  'src/third_party/usrsctp/usrsctplib':
    (Var("chromium_git")) + '/external/usrsctplib.git@36444a999739e9e408f8f587cb4c3ffeef2e50ac',
  'src/third_party/webdriver/pylib':
    (Var("chromium_git")) + '/external/selenium/py.git@5fd78261a75fe08d27ca4835fb6c5ce4b42275bd',
  'src/third_party/webgl/src':
    (Var("chromium_git")) + '/external/khronosgroup/webgl.git@8986f8bfa84547b1a30a9256ebdd665024d68d71',
  'src/third_party/webpagereplay':
    (Var("chromium_git")) + '/external/github.com/chromium/web-page-replay.git@5da5975950daa7b30a6938da73fd0b3200901b0c',
  'src/third_party/webrtc':
    (Var("chromium_git")) + '/external/webrtc/trunk/webrtc.git@e566ca36dcd4c86a7e370bf2666a35b005f31aed',
  'src/third_party/yasm/source/patched-yasm':
    (Var("chromium_git")) + '/chromium/deps/yasm/patched-yasm.git@4671120cd8558ce62ee8672ebf3eb6f5216f909b',
  'src/tools/deps2git':
    (Var("chromium_git")) + '/chromium/tools/deps2git.git@f04828eb0b5acd3e7ad983c024870f17f17b06d9',
  'src/tools/grit':
    (Var("chromium_git")) + '/external/grit-i18n.git@1dac9ae64b0224beb1547810933a6f9998d0d55e',
  'src/tools/gyp':
    (Var("chromium_git")) + '/external/gyp.git@5122240c5e5c4d8da12c543d82b03d6089eb77c5',
  'src/tools/page_cycler/acid3':
    (Var("chromium_git")) + '/chromium/deps/acid3.git@6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',
  'src/tools/swarming_client':
    (Var("chromium_git")) + '/external/swarming.client.git@b39a448d8522392389b28f6997126a6ab04bfe87',
  'src/v8':
    (Var("chromium_git")) + '/v8/v8.git@bbcea65d2ff6c1575d6b692acf9e9fc468415e82'
}

deps_os = {
  'android': {
    'src/third_party/android_protobuf/src':
      (Var("chromium_git")) + '/external/android_protobuf.git@999188d0dc72e97f7fe08bb756958a2cf090f4e7',
    'src/third_party/android_tools':
      (Var("chromium_git")) + '/android_tools.git@21f4bcbd6cd927e4b4227cfde7d5f13486be1236',
    'src/third_party/apache-mime4j':
      (Var("chromium_git")) + '/chromium/deps/apache-mime4j.git@28cb1108bff4b6cf0a2e86ff58b3d025934ebe3a',
    'src/third_party/appurify-python/src':
      (Var("chromium_git")) + '/external/github.com/appurify/appurify-python.git@ee7abd5c5ae3106f72b2a0b9d2cb55094688e867',
    'src/third_party/cardboard-java/src':
      (Var("chromium_git")) + '/external/github.com/googlesamples/cardboard-java.git@08ad25a04f2801bd822c3f2cd28301b68d74aef6',
    'src/third_party/custom_tabs_client/src':
      (Var("chromium_git")) + '/external/github.com/GoogleChrome/custom-tabs-client.git@47a8e598e6116a211c320d285d887e9fb9376524',
    'src/third_party/elfutils/src':
      (Var("chromium_git")) + '/external/elfutils.git@249673729a7e5dbd5de4f3760bdcaa3d23d154d7',
    'src/third_party/errorprone/lib':
      (Var("chromium_git")) + '/chromium/third_party/errorprone.git@6c66e56c0f9d750aef83190466df834f9d6af8ab',
    'src/third_party/findbugs':
      (Var("chromium_git")) + '/chromium/deps/findbugs.git@7f69fa78a6db6dc31866d09572a0e356e921bf12',
    'src/third_party/freetype-android/src':
      (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@e186230678ee8e4ea4ac4797ece8125761e3225a',
    'src/third_party/httpcomponents-client':
      (Var("chromium_git")) + '/chromium/deps/httpcomponents-client.git@285c4dafc5de0e853fa845dce5773e223219601c',
    'src/third_party/httpcomponents-core':
      (Var("chromium_git")) + '/chromium/deps/httpcomponents-core.git@9f7180a96f8fa5cab23f793c14b413356d419e62',
    'src/third_party/jarjar':
      (Var("chromium_git")) + '/chromium/deps/jarjar.git@2e1ead4c68c450e0b77fe49e3f9137842b8b6920',
    'src/third_party/jsr-305/src':
      (Var("chromium_git")) + '/external/jsr-305.git@642c508235471f7220af6d5df2d3210e3bfc0919',
    'src/third_party/junit/src':
      (Var("chromium_git")) + '/external/junit.git@45a44647e7306262162e1346b750c3209019f2e1',
    'src/third_party/lss':
      (Var("chromium_git")) + '/external/linux-syscall-support/lss.git@6f97298fe3794e92c8c896a6bc06e0b36e4c3de3',
    'src/third_party/mockito/src':
      (Var("chromium_git")) + '/external/mockito/mockito.git@ed99a52e94a84bd7c467f2443b475a22fcc6ba8e',
    'src/third_party/requests/src':
      (Var("chromium_git")) + '/external/github.com/kennethreitz/requests.git@f172b30356d821d180fa4ecfa3e71c7274a32de4',
    'src/third_party/robolectric/lib':
      (Var("chromium_git")) + '/chromium/third_party/robolectric.git@6b63c99a8b6967acdb42cbed0adb067c80efc810',
    'src/third_party/ub-uiautomator/lib':
      (Var("chromium_git")) + '/chromium/third_party/ub-uiautomator.git@e6f02481bada8bdbdfdd7987dd6e648c44a3adcb'
  },
  'ios': {
    'src/chrome/test/data/perf/canvas_bench': None,
    'src/chrome/test/data/perf/frame_rate/content': None,
    'src/ios/third_party/gcdwebserver/src':
      (Var("chromium_git")) + '/external/github.com/swisspol/GCDWebServer.git@3d5fd0b8281a7224c057deb2d17709b5bea64836',
    'src/native_client': None,
    'src/third_party/class-dump/src':
      (Var("chromium_git")) + '/external/github.com/nygard/class-dump.git@93e7c6a5419380d89656dcc511dc60d475199b67',
    'src/third_party/ffmpeg': None,
    'src/third_party/google_toolbox_for_mac/src':
      (Var("chromium_git")) + '/external/google-toolbox-for-mac.git@ce47a231ea0b238fbe95538e86cc61d74c234be6',
    'src/third_party/hunspell_dictionaries': None,
    'src/third_party/nss':
      (Var("chromium_git")) + '/chromium/deps/nss.git@aab0d08a298b29407397fbb1c4219f99e99431ed',
    'src/third_party/webgl': None
  },
  'mac': {
    'src/chrome/installer/mac/third_party/xz/xz':
      (Var("chromium_git")) + '/chromium/deps/xz.git@eecaf55632ca72e90eb2641376bce7cdbc7284f7',
    'src/chrome/tools/test/reference_build/chrome_mac':
      (Var("chromium_git")) + '/chromium/reference_builds/chrome_mac.git@8dc181329e7c5255f83b4b85dc2f71498a237955',
    'src/third_party/google_toolbox_for_mac/src':
      (Var("chromium_git")) + '/external/google-toolbox-for-mac.git@ce47a231ea0b238fbe95538e86cc61d74c234be6',
    'src/third_party/lighttpd':
      (Var("chromium_git")) + '/chromium/deps/lighttpd.git@9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
    'src/third_party/nss':
      (Var("chromium_git")) + '/chromium/deps/nss.git@aab0d08a298b29407397fbb1c4219f99e99431ed',
    'src/third_party/pdfsqueeze':
      (Var("chromium_git")) + '/external/pdfsqueeze.git@5936b871e6a087b7e50d4cbcb122378d8a07499f'
  },
  'unix': {
    'src/chrome/tools/test/reference_build/chrome_linux':
      (Var("chromium_git")) + '/chromium/reference_builds/chrome_linux64.git@033d053a528e820e1de3e2db766678d862a86b36',
    'src/third_party/chromite':
      (Var("chromium_git")) + '/chromiumos/chromite.git@8a83c625c57a1ad990ba8f5f95890b57d204501f',
    'src/third_party/cros_system_api':
      (Var("chromium_git")) + '/chromiumos/platform/system_api.git@ba08c21a075ab091ca9bdc783435c2aec8c6f807',
    'src/third_party/fontconfig/src':
      (Var("chromium_git")) + '/external/fontconfig.git@f16c3118e25546c1b749f9823c51827a60aeb5c1',
    'src/third_party/freetype2/src':
      (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@1dd5f5f4a909866f15c92a45c9702bce290a0151',
    'src/third_party/liblouis/src':
      (Var("chromium_git")) + '/external/liblouis-github.git@5f9c03f2a3478561deb6ae4798175094be8a26c2',
    'src/third_party/lss':
      (Var("chromium_git")) + '/external/linux-syscall-support/lss.git@6f97298fe3794e92c8c896a6bc06e0b36e4c3de3',
    'src/third_party/pyelftools':
      (Var("chromium_git")) + '/chromiumos/third_party/pyelftools.git@bdc1d380acd88d4bfaf47265008091483b0d614e',
    'src/third_party/stp/src':
      (Var("chromium_git")) + '/external/github.com/stp/stp.git@fc94a599207752ab4d64048204f0c88494811b62',
    'src/third_party/xdg-utils':
      (Var("chromium_git")) + '/chromium/deps/xdg-utils.git@d80274d5869b17b8c9067a1022e4416ee7ed5e0d'
  },
  'win': {
    'src/chrome/tools/test/reference_build/chrome_win':
      (Var("chromium_git")) + '/chromium/reference_builds/chrome_win.git@f8a3a845dfc845df6b14280f04f86a61959357ef',
    'src/third_party/bison':
      (Var("chromium_git")) + '/chromium/deps/bison.git@083c9a45e4affdd5464ee2b224c2df649c6e26c3',
    'src/third_party/cygwin':
      (Var("chromium_git")) + '/chromium/deps/cygwin.git@c89e446b273697fadf3a10ff1007a97c0b7de6df',
    'src/third_party/deqp/src':
      'https://android.googlesource.com/platform/external/deqp@194294e69d44eac48bc1fb063bd607189650aa5e',
    'src/third_party/gnu_binutils':
      (Var("chromium_git")) + '/native_client/deps/third_party/gnu_binutils.git@f4003433b61b25666565690caf3d7a7a1a4ec436',
    'src/third_party/gperf':
      (Var("chromium_git")) + '/chromium/deps/gperf.git@d892d79f64f9449770443fb06da49b5a1e5d33c1',
    'src/third_party/lighttpd':
      (Var("chromium_git")) + '/chromium/deps/lighttpd.git@9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
    'src/third_party/mingw-w64/mingw/bin':
      (Var("chromium_git")) + '/native_client/deps/third_party/mingw-w64/mingw/bin.git@3cc8b140b883a9fe4986d12cfd46c16a093d3527',
    'src/third_party/nacl_sdk_binaries':
      (Var("chromium_git")) + '/chromium/deps/nacl_sdk_binaries.git@759dfca03bdc774da7ecbf974f6e2b84f43699a5',
    'src/third_party/nss':
      (Var("chromium_git")) + '/chromium/deps/nss.git@aab0d08a298b29407397fbb1c4219f99e99431ed',
    'src/third_party/pefile':
      (Var("chromium_git")) + '/external/pefile.git@72c6ae42396cb913bcab63c15585dc3b5c3f92f1',
    'src/third_party/perl':
      (Var("chromium_git")) + '/chromium/deps/perl.git@ac0d98b5cee6c024b0cffeb4f8f45b6fc5ccdb78',
    'src/third_party/psyco_win32':
      (Var("chromium_git")) + '/chromium/deps/psyco_win32.git@f5af9f6910ee5a8075bbaeed0591469f1661d868',
    'src/third_party/yasm/binaries':
      (Var("chromium_git")) + '/chromium/deps/yasm/binaries.git@52f9b3f4b0aa06da24ef8b123058bb61ee468881'
  }
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
      'src/build/download_sdk_extras.py'
    ],
    'pattern':
      '.',
    'name':
      'sdkextras'
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
      'sysroot'
  },
  {
    'action': [
      'python',
      'src/build/vs_toolchain.py',
      'update'
    ],
    'pattern':
      '.',
    'name':
      'win_toolchain'
  },
  {
    'action': [
      'python',
      'src/third_party/binutils/download.py'
    ],
    'pattern':
      'src/third_party/binutils',
    'name':
      'binutils'
  },
  {
    'action': [
      'python',
      'src/tools/clang/scripts/update.py',
      '--if-needed'
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
      '-s',
      'src/third_party/WebKit',
      '-o',
      'src/build/util/LASTCHANGE.blink'
    ],
    'pattern':
      '.',
    'name':
      'lastchange'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'gn_win'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'gn_mac'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'gn_linux64'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'clang_format_win'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'clang_format_mac'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'clang_format_linux'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'luci-go_win'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'luci-go_mac'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'luci-go_linux'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket',
      'chromium-eu-strip',
      '-s',
      'src/build/linux/bin/eu-strip.sha1'
    ],
    'pattern':
      '.',
    'name':
      'eu-strip'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-drmemory',
      '-s',
      'src/third_party/drmemory/drmemory-windows-sfx.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'drmemory'
  },
  {
    'action': [
      'python',
      'src/build/get_syzygy_binaries.py',
      '--output-dir=src/third_party/syzygy/binaries',
      '--revision=e50a9822fc8aeb5e7902da5e2940ea135d732e57',
      '--overwrite'
    ],
    'pattern':
      '.',
    'name':
      'syzygy-binaries'
  },
  {
    'action': [
      'python',
      'src/build/get_syzygy_binaries.py',
      '--output-dir=src/third_party/kasko',
      '--revision=283aeaceeb22e2ba40a1753e3cb32454b59cc017',
      '--resource=kasko.zip',
      '--resource=kasko_symbols.zip',
      '--overwrite'
    ],
    'pattern':
      '.',
    'name':
      'kasko'
  },
  {
    'action': [
      'download_from_google_storage',
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
      'apache_win32'
  },
  {
    'action': [
      'python',
      'src/third_party/mojo/src/mojo/public/tools/download_shell_binary.py',
      '--tools-directory=../../../../../../tools'
    ],
    'pattern':
      '',
    'name':
      'download_mojo_shell'
  },
  {
    'action': [
      'python',
      'src/third_party/instrumented_libraries/scripts/download_binaries.py'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'instrumented_libraries'
  },
  {
    'action': [
      'python',
      'src/tools/remove_stale_pyc_files.py',
      'src/android_webview/tools',
      'src/gpu/gles2_conform_support',
      'src/ppapi',
      'src/printing',
      'src/third_party/closure_compiler/build',
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
      'src/build/gyp_chromium'
    ],
    'pattern':
      '.',
    'name':
      'gyp'
  },
  {
    'action': [
      'python',
      'src/tools/check_git_config.py',
      '--running-as-hook'
    ],
    'pattern':
      '.',
    'name':
      'check_git_config'
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

skip_child_includes = [
  'breakpad',
  'native_client_sdk',
  'out',
  'sdch',
  'skia',
  'testing',
  'v8',
  'win8'
]

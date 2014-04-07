#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 18.0.972.2
#  
#  
#  
vars =  {'webkit_trunk': 'http://svn.webkit.org/repository/webkit/trunk'} 

deps_os = {
   'win': {
      'src/third_party/yasm/binaries':
      '/trunk/deps/third_party/yasm/binaries@74228',
      'src/third_party/nacl_sdk_binaries':
      '/trunk/deps/third_party/nacl_sdk_binaries@111576',
      'src/third_party/pefile':
      'http://pefile.googlecode.com/svn/trunk@63',
      'src/third_party/swig/win':
      '/trunk/deps/third_party/swig/win@69281',
      'src/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@33727',
      'src/chrome/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/rlz':
      'http://rlz.googlecode.com/svn/trunk@49',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
      '/trunk/deps/third_party/ffmpeg/binaries/win@112050',
      'src/third_party/psyco_win32':
      '/trunk/deps/third_party/psyco_win32@79861',
      'src/third_party/mingw-w64/mingw/bin':
      'http://src.chromium.org/native_client/trunk/deps/third_party/mingw-w64/mingw/bin@7139',
      'src/chrome_frame/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/third_party/xulrunner-sdk':
      '/trunk/deps/third_party/xulrunner-sdk@71609',
      'src/third_party/cygwin':
      '/trunk/deps/third_party/cygwin@66844',
      'src/third_party/python_26':
      '/trunk/tools/third_party/python_26@89111',
      'src/third_party/syzygy/binaries':
      'http://sawbuck.googlecode.com/svn/trunk/syzygy/binaries@543',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@114554',
   },
   'mac': {
      'src/third_party/GTM':
      'http://google-toolbox-for-mac.googlecode.com/svn/trunk@459',
      'src/third_party/swig/mac':
      '/trunk/deps/third_party/swig/mac@69281',
      'src/third_party/pdfsqueeze':
      'http://pdfsqueeze.googlecode.com/svn/trunk@4',
      'src/chrome/installer/mac/third_party/xz/xz':
      '/trunk/deps/third_party/xz@87706',
      'src/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@33737',
      'src/chrome/tools/test/reference_build/chrome_mac':
      '/trunk/deps/reference_builds/chrome_mac@89574',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@114554',
   },
   'unix': {
      'src/third_party/cros_system_api':
      'http://git.chromium.org/chromiumos/platform/system_api.git@58d34158',
      'src/third_party/openssl':
      '/trunk/deps/third_party/openssl@105093',
      'src/third_party/WebKit/Tools/gdb':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/gdb@102802',
      'src/chrome/tools/test/reference_build/chrome_linux':
      '/trunk/deps/reference_builds/chrome_linux@89574',
      'src/third_party/swig/linux':
      '/trunk/deps/third_party/swig/linux@69281',
      'src/third_party/cros':
      'http://git.chromium.org/chromiumos/platform/cros.git@63b0d1fd',
      'src/third_party/xdg-utils':
      '/trunk/deps/third_party/xdg-utils@93299',
      'src/third_party/lss':
      'http://linux-syscall-support.googlecode.com/svn/trunk/lss@9',
   },
}

deps = {
   'src/third_party/mozc/chrome/chromeos/renderer':
      'http://mozc.googlecode.com/svn/trunk/src/chrome/chromeos/renderer@83',
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/trunk/include@2870',
   'build/scripts/private/data/reliability':
      '/trunk/src/chrome/test/data/reliability@109518',
   'src/third_party/flac':
      '/trunk/deps/third_party/flac@96404',
   'chromeos':
      '/trunk/src/tools/cros.DEPS@114108',
   'src/chrome/test/data/perf/frame_rate/content':
      '/trunk/deps/frame_rate/content@93671',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/workers@102802',
   'src/third_party/sfntly/cpp/src':
      'http://sfntly.googlecode.com/svn/trunk/cpp/src@118',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@162',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/xmlhttprequest@102802',
   'src/third_party/webgl_conformance':
      '/trunk/deps/third_party/webgl/sdk/tests@106432',
   'src/third_party/hunspell_dictionaries':
      '/trunk/deps/third_party/hunspell_dictionaries@79099',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/fast/events@102802',
   'build/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@58968',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-cg-mac/storage/domstorage@102802',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium/fast/workers@102802',
   'src/third_party/skia/third_party/glu':
      'http://skia.googlecode.com/svn/trunk/third_party/glu@2870',
   'src/third_party/scons-2.0.1':
      'http://src.chromium.org/native_client/trunk/src/third_party/scons-2.0.1@7139',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/js/resources@102802',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/resources@102802',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/fast/events':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-cg-mac/fast/events@102802',
   'src/third_party/libyuv':
      'http://libyuv.googlecode.com/svn/trunk@102',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/filesystem':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/filesystem@102802',
   'src/third_party/libphonenumber/src/phonenumbers':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/src/phonenumbers@407',
   'src/third_party/libphonenumber/src/resources':
      'http://libphonenumber.googlecode.com/svn/trunk/resources@407',
   'src/third_party/safe_browsing/testing':
      'http://google-safe-browsing.googlecode.com/svn/trunk/testing@106',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@40',
   'src/third_party/speex':
      '/trunk/deps/third_party/speex@111570',
   'src/third_party/WebKit/Source':
      Var("webkit_trunk")[:-6] + '/trunk/Source@102802',
   'build/third_party/gsutil':
      'http://gsutil.googlecode.com/svn/trunk/src@145',
   'src/third_party/pyftpdlib/src':
      'http://pyftpdlib.googlecode.com/svn/trunk@888',
   'src/third_party/WebKit/LayoutTests':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests@102802',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@76115',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/websocket/tests@102802',
   'src/third_party/webrtc':
      'http://webrtc.googlecode.com/svn/trunk/src@1165',
   'src/chrome/test/data/layout_tests/LayoutTests/media':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/media@102802',
   'src/third_party/libjpeg_turbo':
      '/trunk/deps/third_party/libjpeg_turbo@111873',
   'src/third_party/v8-i18n':
      'http://v8-i18n.googlecode.com/svn/trunk@7',
   'depot_tools':
      '/trunk/tools/depot_tools@114270',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/events@102802',
   'src/third_party/bidichecker':
      'http://bidichecker.googlecode.com/svn/trunk/lib@4',
   'src/third_party/snappy/src':
      'http://snappy.googlecode.com/svn/trunk@37',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@891',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@76',
   'src/third_party/pylib':
      'http://src.chromium.org/native_client/trunk/src/third_party/pylib@7139',
   'src/third_party/jsoncpp/source/src/lib_json':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/src/lib_json@246',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/workers@102802',
   'build/scripts/gsd_generate_index':
      '/trunk/tools/gsd_generate_index@110568',
   'src/chrome/browser/resources/software_rendering_list':
      '/trunk/deps/gpu/software_rendering_list@110790',
   'src/testing/gmock':
      'http://googlemock.googlecode.com/svn/trunk@374',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/trunk/src@2870',
   'src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin':
      'http://src.chromium.org/native_client/trunk/src/native_client/tests/prebuilt@7388',
   'src/third_party/smhasher/src':
      'http://smhasher.googlecode.com/svn/trunk@136',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell@110423',
   'src/native_client':
      'http://src.chromium.org/native_client/trunk/src/native_client@7388',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@102714',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/filesystem/workers@102802',
   'src/third_party/leveldatabase/src':
      'http://leveldb.googlecode.com/svn/trunk@55',
   'src/third_party/mozc/session':
      'http://mozc.googlecode.com/svn/trunk/src/session@83',
   'build':
      '/trunk/tools/build@114613',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@1104',
   'src/third_party/ffmpeg':
      '/trunk/deps/third_party/ffmpeg/source@112050',
   'src/third_party/webdriver/pylib':
      'http://selenium.googlecode.com/svn/trunk/py@13487',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/storage/domstorage@102802',
   'src/third_party/cacheinvalidation/files/src/google':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk/src/google@143',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/fast/workers@102802',
   'src/native_client_sdk/src/site_scons':
      'http://src.chromium.org/native_client/trunk/src/native_client/site_scons@7388',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/filesystem/resources@102802',
   'src/build/util/support':
      '/trunk/deps/support@20411',
   'src/third_party/jsoncpp/source/include':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/include@246',
   'src/third_party/libphonenumber/src/test':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/test@407',
   'src/third_party/WebKit/Tools/Scripts':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/Scripts@102802',
   'src/third_party/pymox/src':
      'http://pymox.googlecode.com/svn/trunk@70',
   'build/scripts/command_wrapper/bin':
      '/trunk/tools/command_wrapper/bin@111020',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@560',
   'src/third_party/libjingle/source':
      'http://libjingle.googlecode.com/svn/trunk@100',
   'src':
      '/branches/972/src@114757',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu46@113543',
   'src/tools/grit':
      'http://grit-i18n.googlecode.com/svn/trunk@9',
   'src/third_party/WebKit/Tools/DumpRenderTree':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/DumpRenderTree@102802',
   'src/seccompsandbox':
      'http://seccompsandbox.googlecode.com/svn/trunk@178',
   'src/third_party/yasm/source/patched-yasm':
      '/trunk/deps/third_party/yasm/patched-yasm@73761',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-cg-mac/http/tests/workers@102802',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/http/tests/workers@102802',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/appcache@102802',
   'src/third_party/WebKit/Tools/TestWebKitAPI':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/TestWebKitAPI@102802',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/trunk@911',
   'src/v8':
      'http://v8.googlecode.com/svn/branches/3.7@10163',
   'src/third_party/libvpx':
      '/trunk/deps/third_party/libvpx@109236',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/storage/domstorage@102802',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@69281',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'native_client', 'native_client_sdk', 'o3d', 'pdf', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/download_nacl_toolchains.py', '--no-pnacl', '--no-arm-trusted', '--x86-version', '7332', '--file-hash', 'mac_x86_newlib', '56d82b794c3523a43068a5b69236277a96de04c5', '--file-hash', 'win_x86_newlib', '40d182ac25fe6623cfbe5772623c15c663a42c93', '--file-hash', 'linux_x86_newlib', '06935bbb3ab4c55d9f6555ae54fa06e096464491', '--file-hash', 'mac_x86', '98da0b2c45a5ecc0a424b393e5ae6dec780940fa', '--file-hash', 'win_x86', 'c5af5469b2252f3d5bb9ce4a7758b4537e9c5b16', '--file-hash', 'linux_x86', '23f1945071467efeff373889b3ca036c10207522', '--save-downloads-dir', 'src/native_client_sdk/src/build_tools/toolchain_archives'], 'pattern': '.'}, {'action': ['python', 'src/tools/clang/scripts/update.py', '--mac-only'], 'pattern': '.'}, {'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
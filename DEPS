#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 16.0.912.53
#  
#  
#  
vars =  {'webkit_trunk': 'http://svn.webkit.org/repository/webkit/trunk'} 

deps_os = {
   'win': {
      'src/third_party/yasm/binaries':
      '/trunk/deps/third_party/yasm/binaries@74228',
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
      'src/third_party/xulrunner-sdk':
      '/trunk/deps/third_party/xulrunner-sdk@71609',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
      '/trunk/deps/third_party/ffmpeg/binaries/win@108357',
      'src/third_party/psyco_win32':
      '/trunk/deps/third_party/psyco_win32@79861',
      'src/third_party/mingw-w64/mingw/bin':
      'http://src.chromium.org/native_client/trunk/src/third_party/mingw-w64/mingw/bin@6504',
      'src/chrome_frame/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/third_party/cygwin':
      '/trunk/deps/third_party/cygwin@66844',
      'src/third_party/python_26':
      '/trunk/tools/third_party/python_26@89111',
      'src/third_party/syzygy/binaries':
      'http://sawbuck.googlecode.com/svn/trunk/syzygy/binaries@427',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@106031',
   },
   'mac': {
      'src/third_party/WebKit/WebKitLibraries':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/WebKitLibraries@101040',
      'src/third_party/GTM':
      'http://google-toolbox-for-mac.googlecode.com/svn/trunk@459',
      'src/third_party/pdfsqueeze':
      'http://pdfsqueeze.googlecode.com/svn/trunk@4',
      'src/chrome/installer/mac/third_party/xz/xz':
      '/trunk/deps/third_party/xz@87706',
      'src/third_party/swig/mac':
      '/trunk/deps/third_party/swig/mac@69281',
      'src/chrome/tools/test/reference_build/chrome_mac':
      '/trunk/deps/reference_builds/chrome_mac@89574',
      'src/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@33737',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@106031',
   },
   'unix': {
      'src/third_party/cros_system_api':
      'http://git.chromium.org/chromiumos/platform/system_api.git@8152d9bd',
      'src/third_party/openssl':
      '/trunk/deps/third_party/openssl@101988',
      'src/third_party/WebKit/Tools/gdb':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/Tools/gdb@101040',
      'src/chrome/tools/test/reference_build/chrome_linux':
      '/trunk/deps/reference_builds/chrome_linux@89574',
      'src/third_party/swig/linux':
      '/trunk/deps/third_party/swig/linux@69281',
      'src/third_party/cros':
      'http://git.chromium.org/chromiumos/platform/cros.git@b48469c4',
      'src/third_party/xdg-utils':
      '/trunk/deps/third_party/xdg-utils@93299',
      'src/third_party/lss':
      'http://linux-syscall-support.googlecode.com/svn/trunk/lss@8',
   },
}

deps = {
   'src/native_client':
      'http://src.chromium.org/native_client/branches/912/src/native_client@7016',
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/trunk/include@2480',
   'src/third_party/flac':
      '/trunk/deps/third_party/flac@96404',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/fast/workers@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/xmlhttprequest@101040',
   'src/third_party/sfntly/src/sfntly':
      'http://sfntly.googlecode.com/svn/trunk/cpp/src/sfntly@98',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-cg-mac/storage/domstorage@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium/fast/workers@101040',
   'src/third_party/webdriver/python/selenium/test':
      'http://selenium.googlecode.com/svn/trunk/py/test@13487',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/fast/js/resources@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/resources@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/filesystem':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/filesystem@101040',
   'src/third_party/safe_browsing/testing':
      'http://google-safe-browsing.googlecode.com/svn/trunk/testing@106',
   'src/third_party/pyftpdlib/src':
      'http://pyftpdlib.googlecode.com/svn/trunk@888',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@76115',
   'chromeos':
      '/trunk/src/tools/cros.DEPS@105204',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/fast/events@101040',
   'src/third_party/bidichecker':
      'http://bidichecker.googlecode.com/svn/trunk/lib@4',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@74',
   'src/third_party/WebKit/Source':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/Source@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/workers@101040',
   'src/testing/gmock':
      'http://googlemock.googlecode.com/svn/trunk@374',
   'src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin':
      'http://src.chromium.org/native_client/trunk/src/native_client/tests/prebuilt@6941',
   'src/third_party/smhasher/src':
      'http://smhasher.googlecode.com/svn/trunk@136',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/fast/filesystem/workers@101040',
   'src/third_party/leveldatabase/src':
      'http://leveldb.googlecode.com/svn/trunk@53',
   'src/third_party/mozc/session':
      'http://mozc.googlecode.com/svn/trunk/src/session@58',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@1075',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@40',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-win/storage/domstorage@101040',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@102714',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/fast/filesystem/resources@101040',
   'src/build/util/support':
      '/trunk/deps/support@20411',
   'src/third_party/libjingle/source':
      'http://libjingle.googlecode.com/svn/trunk@88',
   'src':
      '/branches/912/src@111507',
   'src/third_party/WebKit/Tools/DumpRenderTree':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/Tools/DumpRenderTree@101040',
   'src/chrome/browser/resources/software_rendering_list':
      '/trunk/deps/gpu/software_rendering_list@111394',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell@96723',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/appcache@101040',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@560',
   'src/v8':
      'http://v8.googlecode.com/svn/branches/3.6@9982',
   'src/third_party/libvpx':
      '/trunk/deps/third_party/libvpx@103347',
   'src/third_party/skia/third_party/glu':
      'http://skia.googlecode.com/svn/trunk/third_party/glu@2480',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@875',
   'src/third_party/v8-i18n':
      'http://v8-i18n.googlecode.com/svn/trunk@7',
   'src/chrome/test/data/perf/frame_rate/content':
      '/trunk/deps/frame_rate/content@93671',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@159',
   'src/third_party/webgl_conformance':
      '/trunk/deps/third_party/webgl/sdk/tests@93490',
   'src/third_party/hunspell_dictionaries':
      '/trunk/deps/third_party/hunspell_dictionaries@79099',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-win/fast/events@101040',
   'src/third_party/scons-2.0.1':
      'http://src.chromium.org/native_client/trunk/src/third_party/scons-2.0.1@6504',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-cg-mac/fast/events@101040',
   'src/third_party/webdriver/python/selenium':
      'http://selenium.googlecode.com/svn/trunk/py/selenium@13487',
   'src/third_party/pylib':
      'http://src.chromium.org/native_client/trunk/src/third_party/pylib@6504',
   'src/third_party/WebKit/LayoutTests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests@101040',
   'src/third_party/snappy/src':
      'http://snappy.googlecode.com/svn/trunk@37',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/http/tests/websocket/tests@101040',
   'src/third_party/webrtc':
      'http://webrtc.googlecode.com/svn/trunk/src@711',
   'src/chrome/test/data/layout_tests/LayoutTests/media':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/media@101040',
   'src/third_party/libjpeg_turbo':
      '/trunk/deps/third_party/libjpeg_turbo@105131',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/trunk/src@2480',
   'build':
      '/trunk/tools/build@109845',
   'src/third_party/ffmpeg':
      '/trunk/deps/third_party/ffmpeg/source@107826',
   'src/third_party/cacheinvalidation/files/src/google':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk/src/google@143',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-win/fast/workers@101040',
   'src/third_party/WebKit/Tools/Scripts':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/Tools/Scripts@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/storage/domstorage@101040',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu46@101184',
   'src/third_party/speex':
      '/trunk/deps/third_party/speex@83190',
   'src/tools/grit':
      'http://grit-i18n.googlecode.com/svn/trunk@6',
   'src/seccompsandbox':
      'http://seccompsandbox.googlecode.com/svn/trunk@170',
   'src/third_party/yasm/source/patched-yasm':
      '/trunk/deps/third_party/yasm/patched-yasm@73761',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-cg-mac/http/tests/workers@101040',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/LayoutTests/platform/chromium-win/http/tests/workers@101040',
   'src/third_party/WebKit/Tools/TestWebKitAPI':
      Var("webkit_trunk")[:-6] + '/branches/chromium/912/Tools/TestWebKitAPI@101040',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/branches/chrome_m16@886',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@69281',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'native_client', 'o3d', 'pdf', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/download_nacl_toolchains.py', '--x86-version', '6941', '--nacl-newlib-only', '--file-hash', 'mac_x86_newlib', 'da76f48d57b1e579e2a2e65fdfd9c03136b46e24', '--file-hash', 'win_x86_newlib', 'b6318df6af3e6bda15c8bf8aa373bb4fd1b6bd63', '--file-hash', 'linux_x86_newlib', 'd1001bba03ab37a7aab3c295da5660c5b7c45a98'], 'pattern': '.'}, {'action': ['python', 'src/tools/clang/scripts/update.py', '--mac-only'], 'pattern': '.'}, {'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
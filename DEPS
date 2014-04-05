#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 15.0.874.121
#  
#  
#  
vars =  {'webkit_trunk': 'http://svn.webkit.org/repository/webkit/trunk'} 

deps_os = {
   'win': {
      'src/chrome_frame/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/third_party/yasm/binaries':
      '/trunk/deps/third_party/yasm/binaries@74228',
      'src/chrome/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/third_party/cygwin':
      '/trunk/deps/third_party/cygwin@66844',
      'src/third_party/python_26':
      '/trunk/tools/third_party/python_26@89111',
      'src/third_party/swig/win':
      '/trunk/deps/third_party/swig/win@69281',
      'src/third_party/xulrunner-sdk':
      '/trunk/deps/third_party/xulrunner-sdk@71609',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
      '/trunk/deps/third_party/ffmpeg/binaries/win@108357',
      'src/third_party/pefile':
      'http://pefile.googlecode.com/svn/trunk@63',
      'src/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@33727',
      'src/third_party/psyco_win32':
      '/trunk/deps/third_party/psyco_win32@79861',
      'src/third_party/mingw-w64/mingw/bin':
      'http://src.chromium.org/native_client/trunk/src/third_party/mingw-w64/mingw/bin@6504',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@94921',
      'src/third_party/syzygy/binaries':
      'http://sawbuck.googlecode.com/svn/trunk/syzygy/binaries@398',
      'src/rlz':
      'http://rlz.googlecode.com/svn/trunk@47',
   },
   'mac': {
      'src/third_party/WebKit/WebKitLibraries':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/WebKitLibraries@100034',
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
      '/trunk/deps/third_party/nss@94921',
   },
   'unix': {
      'src/third_party/cros_system_api':
      'http://git.chromium.org/chromiumos/platform/system_api.git@c3de536a',
      'src/third_party/openssl':
      '/trunk/deps/third_party/openssl@94299',
      'src/third_party/WebKit/Tools/gdb':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/Tools/gdb@100034',
      'src/chrome/tools/test/reference_build/chrome_linux':
      '/trunk/deps/reference_builds/chrome_linux@89574',
      'src/third_party/swig/linux':
      '/trunk/deps/third_party/swig/linux@69281',
      'src/third_party/cros':
      'http://git.chromium.org/chromiumos/platform/cros.git@b9cc80a6',
      'src/third_party/xdg-utils':
      '/trunk/deps/third_party/xdg-utils@93299',
      'src/third_party/lss':
      'http://linux-syscall-support.googlecode.com/svn/trunk/lss@3',
   },
}

deps = {
   'src/native_client':
      'http://src.chromium.org/native_client/trunk/src/native_client@6631',
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/branches/chrome/874a/include@2481',
   'src/third_party/flac':
      '/trunk/deps/third_party/flac@96404',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/fast/workers@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/xmlhttprequest@100034',
   'src/third_party/sfntly/src/sfntly':
      'http://sfntly.googlecode.com/svn/trunk/cpp/src/sfntly@54',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-cg-mac/storage/domstorage@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium/fast/workers@100034',
   'src/third_party/webdriver/python/selenium/test':
      'http://selenium.googlecode.com/svn/trunk/py/test@13487',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/fast/js/resources@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/resources@100034',
   'src/third_party/skia/gpu':
      'http://skia.googlecode.com/svn/branches/chrome/874a/gpu@2481',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/filesystem':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/filesystem@100034',
   'src/third_party/safe_browsing/testing':
      'http://google-safe-browsing.googlecode.com/svn/trunk/testing@106',
   'src/third_party/pyftpdlib/src':
      'http://pyftpdlib.googlecode.com/svn/trunk@888',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@76115',
   'chromeos':
      '/trunk/src/tools/cros.DEPS@99930',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/fast/events@100034',
   'src/third_party/bidichecker':
      'http://bidichecker.googlecode.com/svn/trunk/lib@4',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@69',
   'src/third_party/WebKit/Source':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/Source@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/workers@100034',
   'src/testing/gmock':
      'http://googlemock.googlecode.com/svn/trunk@374',
   'src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin':
      'http://src.chromium.org/native_client/trunk/src/native_client/tests/prebuilt@6631',
   'src/third_party/smhasher/src':
      'http://smhasher.googlecode.com/svn/trunk@136',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/fast/filesystem/workers@100034',
   'src/third_party/leveldatabase/src':
      'http://leveldb.googlecode.com/svn/trunk@47',
   'src/third_party/mozc/session':
      'http://mozc.googlecode.com/svn/trunk/src/session@58',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@1031',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@28',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-win/storage/domstorage@100034',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@19546',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/fast/filesystem/resources@100034',
   'src/build/util/support':
      '/trunk/deps/support@20411',
   'src/third_party/libjingle/source':
      'http://libjingle.googlecode.com/svn/trunk@77',
   'src':
      '/branches/874/src@109964',
   'src/third_party/WebKit/Tools/DumpRenderTree':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/Tools/DumpRenderTree@100034',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell@96723',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/appcache@100034',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@560',
   'src/v8':
      'http://v8.googlecode.com/svn/branches/3.5@9918',
   'src/third_party/libvpx':
      '/trunk/deps/third_party/libvpx@97420',
   'src/third_party/skia/third_party/glu':
      'http://skia.googlecode.com/svn/branches/chrome/874a/third_party/glu@2481',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/branches/chrome_15/src@877',
   'src/third_party/v8-i18n':
      'http://v8-i18n.googlecode.com/svn/trunk@4',
   'src/chrome/test/data/perf/frame_rate/content':
      '/trunk/deps/frame_rate/content@93671',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@159',
   'src/third_party/webgl_conformance':
      '/trunk/deps/third_party/webgl/sdk/tests@93490',
   'src/third_party/hunspell_dictionaries':
      '/trunk/deps/third_party/hunspell_dictionaries@79099',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-win/fast/events@100034',
   'src/third_party/scons-2.0.1':
      'http://src.chromium.org/native_client/trunk/src/third_party/scons-2.0.1@6504',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-cg-mac/fast/events@100034',
   'src/third_party/webdriver/python/selenium':
      'http://selenium.googlecode.com/svn/trunk/py/selenium@13487',
   'src/third_party/pylib':
      'http://src.chromium.org/native_client/trunk/src/third_party/pylib@6504',
   'src/third_party/WebKit/LayoutTests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests@100034',
   'src/third_party/snappy/src':
      'http://snappy.googlecode.com/svn/trunk@37',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/http/tests/websocket/tests@100034',
   'src/third_party/webrtc':
      'http://webrtc.googlecode.com/svn/trunk/src@521',
   'src/chrome/test/data/layout_tests/LayoutTests/media':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/media@100034',
   'src/third_party/libjpeg_turbo':
      '/trunk/deps/third_party/libjpeg_turbo@95800',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/branches/chrome/874a/src@2490',
   'src/third_party/ffmpeg':
      '/trunk/deps/third_party/ffmpeg/source@107826',
   'src/third_party/cacheinvalidation/files/src/google':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk/src/google@135',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-win/fast/workers@100034',
   'src/third_party/WebKit/Tools/Scripts':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/Tools/Scripts@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/storage/domstorage@100034',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu46@98343',
   'src/third_party/speex':
      '/trunk/deps/third_party/speex@83190',
   'src/seccompsandbox':
      'http://seccompsandbox.googlecode.com/svn/trunk@168',
   'src/third_party/yasm/source/patched-yasm':
      '/trunk/deps/third_party/yasm/patched-yasm@73761',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-cg-mac/http/tests/workers@100034',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/874/LayoutTests/platform/chromium-win/http/tests/workers@100034',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/branches/chrome_m15@814',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@69281',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'native_client', 'o3d', 'pdf', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/download_nacl_toolchains.py', '--x86-version', '6494', '--nacl-newlib-only', '--file-hash', 'mac_x86_newlib', '1b0855435c03c435a011c6105a509624b2a4edaa', '--file-hash', 'win_x86_newlib', '5038a47b5a9a49acdc36cbe311aec7bce575c164', '--file-hash', 'linux_x86_newlib', '01e245dc6dca16bea5cf840dbc77e3aa138f234f'], 'pattern': '.'}, {'action': ['python', 'src/tools/clang/scripts/update.py', '--mac-only'], 'pattern': '.'}, {'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
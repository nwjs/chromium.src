#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 18.0.1025.169
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
      '/branches/ffmpeg/1025/binaries/win@123109',
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
      'http://sawbuck.googlecode.com/svn/trunk/syzygy/binaries@596',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@117974',
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
      '/trunk/deps/third_party/nss@115445',
   },
   'unix': {
      'src/third_party/cros_system_api':
      'http://git.chromium.org/chromiumos/platform/system_api.git@d9cd48f6',
      'src/third_party/openssl':
      '/trunk/deps/third_party/openssl@118977',
      'src/third_party/xdg-utils':
      '/trunk/deps/third_party/xdg-utils@93299',
      'src/chrome/tools/test/reference_build/chrome_linux':
      '/trunk/deps/reference_builds/chrome_linux@89574',
      'src/third_party/swig/linux':
      '/trunk/deps/third_party/swig/linux@69281',
      'src/third_party/cros':
      'http://git.chromium.org/chromiumos/platform/cros.git@aa50358c',
      'src/third_party/WebKit/Tools/gdb':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/Tools/gdb@114948',
      'src/third_party/lss':
      'http://linux-syscall-support.googlecode.com/svn/trunk/lss@9',
   },
}

deps = {
   'src/third_party/mozc/chrome/chromeos/renderer':
      'http://mozc.googlecode.com/svn/trunk/src/chrome/chromeos/renderer@83',
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/branches/chrome/1025/include@3755',
   'build/scripts/private/data/reliability':
      '/trunk/src/chrome/test/data/reliability@109518',
   'src/third_party/flac':
      '/trunk/deps/third_party/flac@118218',
   'chromeos':
      '/trunk/src/tools/cros.DEPS@119717',
   'src/chrome/test/data/perf/frame_rate/content':
      '/trunk/deps/frame_rate/content@93671',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/fast/workers@114948',
   'src/third_party/sfntly/cpp/src':
      'http://sfntly.googlecode.com/svn/trunk/cpp/src@118',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@168',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/xmlhttprequest@114948',
   'src/native_client_sdk/src/site_scons':
      'http://src.chromium.org/native_client/trunk/src/native_client/site_scons@7664',
   'src/third_party/webgl_conformance':
      '/trunk/deps/third_party/webgl/sdk/tests@106432',
   'src/third_party/hunspell_dictionaries':
      '/trunk/deps/third_party/hunspell_dictionaries@79099',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/platform/chromium-win/fast/events@114948',
   'build/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@58968',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/branches/chrome_m18@988',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/platform/chromium/fast/workers@114948',
   'src/third_party/skia/third_party/glu':
      'http://skia.googlecode.com/svn/branches/chrome/1025/third_party/glu@3220',
   'src/third_party/scons-2.0.1':
      'http://src.chromium.org/native_client/trunk/src/third_party/scons-2.0.1@7139',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/fast/js/resources@114948',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/resources@114948',
   'src/third_party/libyuv':
      'http://libyuv.googlecode.com/svn/trunk@159',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/platform/chromium-win/fast/workers@114948',
   'src/third_party/WebKit/Tools/DumpRenderTree':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/Tools/DumpRenderTree@114948',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/filesystem':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/filesystem@114948',
   'src/third_party/libphonenumber/src/phonenumbers':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/src/phonenumbers@407',
   'src/third_party/libphonenumber/src/resources':
      'http://libphonenumber.googlecode.com/svn/trunk/resources@407',
   'src/third_party/safe_browsing/testing':
      'http://google-safe-browsing.googlecode.com/svn/trunk/testing@106',
   'src/third_party/ffmpeg':
      '/branches/ffmpeg/1025/source@123109',
   'src/third_party/speex':
      '/trunk/deps/third_party/speex@111570',
   'src/third_party/pylib':
      'http://src.chromium.org/native_client/trunk/src/third_party/pylib@7139',
   'build/third_party/gsutil':
      'http://gsutil.googlecode.com/svn/trunk/src@145',
   'src/third_party/pyftpdlib/src':
      'http://pyftpdlib.googlecode.com/svn/trunk@977',
   'src/third_party/WebKit/LayoutTests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests@114948',
   'src/third_party/snappy/src':
      'http://snappy.googlecode.com/svn/trunk@37',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@76115',
   'src/third_party/webrtc':
      'http://webrtc.googlecode.com/svn/trunk/src@1538',
   'src/chrome/test/data/layout_tests/LayoutTests/media':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/media@114948',
   'src/third_party/libjpeg_turbo':
      '/trunk/deps/third_party/libjpeg_turbo@118072',
   'src/third_party/v8-i18n':
      'http://v8-i18n.googlecode.com/svn/trunk@7',
   'depot_tools':
      '/trunk/tools/depot_tools@119713',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/fast/events@114948',
   'src/third_party/bidichecker':
      'http://bidichecker.googlecode.com/svn/trunk/lib@4',
   'commit-queue':
      '/trunk/tools/commit-queue@119060',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@909',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@83',
   'src/third_party/WebKit/Source':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/Source@114948',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/indexeddb':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/storage/indexeddb@114948',
   'src/third_party/jsoncpp/source/src/lib_json':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/src/lib_json@248',
   'build/scripts/command_wrapper/bin':
      '/trunk/tools/command_wrapper/bin@111020',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/workers@114948',
   'build/scripts/gsd_generate_index':
      '/trunk/tools/gsd_generate_index@110568',
   'src/seccompsandbox':
      'http://seccompsandbox.googlecode.com/svn/trunk@178',
   'src/testing/gmock':
      'http://googlemock.googlecode.com/svn/trunk@374',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/branches/chrome/1025/src@3755',
   'src/tools/grit':
      'http://grit-i18n.googlecode.com/svn/trunk@9',
   'src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin':
      'http://src.chromium.org/native_client/trunk/src/native_client/tests/prebuilt@7664',
   'src/third_party/smhasher/src':
      'http://smhasher.googlecode.com/svn/trunk@136',
   'src/third_party/yasm/source/patched-yasm':
      '/trunk/deps/third_party/yasm/patched-yasm@73761',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@102714',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/fast/filesystem/workers@114948',
   'src/third_party/leveldatabase/src':
      'http://leveldb.googlecode.com/svn/trunk@55',
   'src/third_party/mozc/session':
      'http://mozc.googlecode.com/svn/trunk/src/session@83',
   'build':
      '/trunk/tools/build@119869',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@1185',
   'src/third_party/libsrtp':
      '/trunk/deps/third_party/libsrtp@119742',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@40',
   'src/third_party/webdriver/pylib':
      'http://selenium.googlecode.com/svn/trunk/py@13487',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/platform/chromium-win/storage/domstorage@114948',
   'src/third_party/cacheinvalidation/files/src/google':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk/src/google@185',
   'src/native_client':
      'http://src.chromium.org/native_client/branches/1025/src/native_client@8003',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/websocket/tests@114948',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/fast/filesystem/resources@114948',
   'src/build/util/support':
      '/trunk/deps/support@20411',
   'src/third_party/jsoncpp/source/include':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/include@248',
   'src/third_party/libphonenumber/src/test':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/test@407',
   'src/third_party/WebKit/Tools/Scripts':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/Tools/Scripts@114948',
   'src/third_party/libjingle/source':
      'http://libjingle.googlecode.com/svn/trunk@114',
   'src':
      '/branches/1025_166/src@134785',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu46@122360',
   'src/third_party/pymox/src':
      'http://pymox.googlecode.com/svn/trunk@70',
   'src/chrome/browser/resources/software_rendering_list':
      '/trunk/deps/gpu/software_rendering_list@115644',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell@110423',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/platform/chromium-win/http/tests/workers@114948',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/http/tests/appcache@114948',
   'src/third_party/WebKit/Tools/TestWebKitAPI':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/Tools/TestWebKitAPI@114948',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@560',
   'src/v8':
      'http://v8.googlecode.com/svn/branches/3.8@11287',
   'src/third_party/libvpx':
      '/trunk/deps/third_party/libvpx@117975',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/branches/chromium/1025/LayoutTests/storage/domstorage@114948',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@69281',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'native_client', 'native_client_sdk', 'o3d', 'pdf', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/download_nacl_toolchains.py', '--no-pnacl', '--no-arm-trusted', '--x86-version', '7537', '--file-hash', 'mac_x86_newlib', '36744d062e149563b948accf5f88221e4be4f4c2', '--file-hash', 'win_x86_newlib', 'b41a2e565716b3ddc0212d185afa79881e971c58', '--file-hash', 'linux_x86_newlib', 'd091970ba08e6d0349568184e6d91980447ff827', '--file-hash', 'mac_x86', 'ab054c9fad027ce705f6c7be17569c4a58ff0b73', '--file-hash', 'win_x86', '396ca98c3021666a08819b89eecb69a79fd7bad8', '--file-hash', 'linux_x86', '9db88d4b92913d494d47ceb936d8a0c4af9b7ddb', '--save-downloads-dir', 'src/native_client_sdk/src/build_tools/toolchain_archives', '--keep'], 'pattern': '.'}, {'action': ['python', 'src/tools/clang/scripts/update.py', '--mac-only'], 'pattern': '.'}, {'action': ['python', 'src/build/win/setup_cygwin_mount.py', '--win-only'], 'pattern': '.'}, {'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
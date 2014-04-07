#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 19.0.1054.0
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
      'http://rlz.googlecode.com/svn/trunk@66',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
      '/trunk/deps/third_party/ffmpeg/binaries/win@122092',
      'src/third_party/psyco_win32':
      '/trunk/deps/third_party/psyco_win32@79861',
      'src/third_party/mingw-w64/mingw/bin':
      'http://src.chromium.org/native_client/trunk/deps/third_party/mingw-w64/mingw/bin@7139',
      'src/chrome_frame/tools/test/reference_build/chrome_win':
      '/trunk/deps/reference_builds/chrome_win@89574',
      'src/third_party/xulrunner-sdk':
      '/trunk/deps/third_party/xulrunner-sdk@119756',
      'src/third_party/cygwin':
      '/trunk/deps/third_party/cygwin@66844',
      'src/third_party/python_26':
      '/trunk/tools/third_party/python_26@89111',
      'src/third_party/syzygy/binaries':
      'http://sawbuck.googlecode.com/svn/trunk/syzygy/binaries@640',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@122528',
   },
   'mac': {
      'src/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@33737',
      'src/third_party/GTM':
      'http://google-toolbox-for-mac.googlecode.com/svn/trunk@459',
      'src/third_party/pdfsqueeze':
      'http://pdfsqueeze.googlecode.com/svn/trunk@4',
      'src/chrome/installer/mac/third_party/xz/xz':
      '/trunk/deps/third_party/xz@87706',
      'src/third_party/swig/mac':
      '/trunk/deps/third_party/swig/mac@69281',
      'src/rlz':
      'http://rlz.googlecode.com/svn/trunk@66',
      'src/chrome/tools/test/reference_build/chrome_mac':
      '/trunk/deps/reference_builds/chrome_mac@89574',
      'src/third_party/nss':
      '/trunk/deps/third_party/nss@122528',
   },
   'unix': {
      'src/third_party/gold':
      '/trunk/deps/third_party/gold@120645',
      'src/third_party/swig/linux':
      '/trunk/deps/third_party/swig/linux@69281',
      'src/third_party/WebKit/Tools/gdb':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/gdb@108903',
      'src/third_party/openssl':
      '/trunk/deps/third_party/openssl@118977',
      'build/xvfb':
      '/trunk/tools/xvfb@121100',
      'src/third_party/cros':
      'http://git.chromium.org/chromiumos/platform/cros.git@2b826b46',
      'src/third_party/xdg-utils':
      '/trunk/deps/third_party/xdg-utils@93299',
      'src/third_party/lss':
      'http://linux-syscall-support.googlecode.com/svn/trunk/lss@9',
      'src/chrome/tools/test/reference_build/chrome_linux':
      '/trunk/deps/reference_builds/chrome_linux@89574',
      'src/third_party/cros_system_api':
      'http://git.chromium.org/chromiumos/platform/system_api.git@9527535f',
   },
}

deps = {
   'src/third_party/mozc/chrome/chromeos/renderer':
      'http://mozc.googlecode.com/svn/trunk/src/chrome/chromeos/renderer@83',
   'src/native_client':
      'http://src.chromium.org/native_client/trunk/src/native_client@7813',
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/trunk/include@3244',
   'build/scripts/private/data/reliability':
      '/trunk/src/chrome/test/data/reliability@109518',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/fast/events@108903',
   'src/third_party/flac':
      '/trunk/deps/third_party/flac@120197',
   'chromeos':
      '/trunk/src/tools/cros.DEPS@123215',
   'src/chrome/test/data/perf/frame_rate/content':
      '/trunk/deps/frame_rate/content@93671',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/workers@108903',
   'src/third_party/sfntly/cpp/src':
      'http://sfntly.googlecode.com/svn/trunk/cpp/src@118',
   'src/third_party/undoview':
      '/trunk/deps/third_party/undoview@119694',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@168',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/xmlhttprequest@108903',
   'src/native_client_sdk/src/site_scons':
      'http://src.chromium.org/native_client/trunk/src/native_client/site_scons@7813',
   'src/third_party/webgl_conformance':
      '/trunk/deps/third_party/webgl/sdk/tests@121363',
   'src/third_party/hunspell_dictionaries':
      '/trunk/deps/third_party/hunspell_dictionaries@79099',
   'build/third_party/lighttpd':
      '/trunk/deps/third_party/lighttpd@58968',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/trunk@993',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium/fast/workers@108903',
   'src/third_party/skia/third_party/glu':
      'http://skia.googlecode.com/svn/trunk/third_party/glu@3244',
   'src/third_party/scons-2.0.1':
      'http://src.chromium.org/native_client/trunk/src/third_party/scons-2.0.1@7139',
   'src/third_party/webdriver/pylib':
      'http://selenium.googlecode.com/svn/trunk/py@13487',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/resources@108903',
   'src/third_party/libyuv':
      'http://libyuv.googlecode.com/svn/trunk@190',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu46@122842',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/storage/domstorage@108903',
   'src/third_party/libphonenumber/src/phonenumbers':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/src/phonenumbers@425',
   'src/third_party/libphonenumber/src/resources':
      'http://libphonenumber.googlecode.com/svn/trunk/resources@425',
   'src/third_party/safe_browsing/testing':
      'http://google-safe-browsing.googlecode.com/svn/trunk/testing@106',
   'src/third_party/ffmpeg':
      '/trunk/deps/third_party/ffmpeg/source@122092',
   'src/third_party/pylib':
      'http://src.chromium.org/native_client/trunk/src/third_party/pylib@7139',
   'build/third_party/gsutil':
      'http://gsutil.googlecode.com/svn/trunk/src@145',
   'src/third_party/pyftpdlib/src':
      'http://pyftpdlib.googlecode.com/svn/trunk@977',
   'src/third_party/WebKit/LayoutTests':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests@108903',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@76115',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/websocket/tests@108903',
   'src/third_party/WebKit/Tools/DumpRenderTree':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/DumpRenderTree@108903',
   'src/chrome/test/data/layout_tests/LayoutTests/media':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/media@108903',
   'src/third_party/libjpeg_turbo':
      '/trunk/deps/third_party/libjpeg_turbo@119959',
   'src/third_party/v8-i18n':
      'http://v8-i18n.googlecode.com/svn/trunk@25',
   'src/chrome/test/data/perf/canvas_bench':
      '/trunk/deps/canvas_bench@122605',
   'depot_tools':
      '/trunk/tools/depot_tools@123399',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/events@108903',
   'src/third_party/bidichecker':
      'http://bidichecker.googlecode.com/svn/trunk/lib@4',
   'src/third_party/snappy/src':
      'http://snappy.googlecode.com/svn/trunk@37',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@922',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@82',
   'src/third_party/WebKit/Source':
      Var("webkit_trunk")[:-6] + '/trunk/Source@108903',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/indexeddb':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/storage/indexeddb@108903',
   'src/third_party/jsoncpp/source/src/lib_json':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/src/lib_json@248',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/workers@108903',
   'build/scripts/gsd_generate_index':
      '/trunk/tools/gsd_generate_index@110568',
   'src/seccompsandbox':
      'http://seccompsandbox.googlecode.com/svn/trunk@178',
   'src/testing/gmock':
      'http://googlemock.googlecode.com/svn/trunk@374',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/trunk/src@3244',
   'src/tools/grit':
      'http://grit-i18n.googlecode.com/svn/trunk@15',
   'src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin':
      'http://src.chromium.org/native_client/trunk/src/native_client/tests/prebuilt@7813',
   'src/third_party/smhasher/src':
      'http://smhasher.googlecode.com/svn/trunk@136',
   'src/third_party/webrtc':
      'http://webrtc.googlecode.com/svn/trunk/src@1538',
   'src/third_party/leveldatabase/src':
      'http://leveldb.googlecode.com/svn/trunk@58',
   'src/third_party/mozc/session':
      'http://mozc.googlecode.com/svn/trunk/src/session@83',
   'build':
      '/trunk/tools/build@123692',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@1233',
   'src/third_party/libsrtp':
      '/trunk/deps/third_party/libsrtp@121481',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/js/resources@108903',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@40',
   'src/third_party/speex':
      '/trunk/deps/third_party/speex@111570',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/storage/domstorage@108903',
   'src/third_party/cacheinvalidation/files/src/google':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk/src/google@185',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/fast/workers@108903',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@102714',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/fast/filesystem/resources@108903',
   'src/build/util/support':
      '/trunk/deps/support@20411',
   'src/third_party/jsoncpp/source/include':
      'http://jsoncpp.svn.sourceforge.net/svnroot/jsoncpp/trunk/jsoncpp/include@248',
   'src/third_party/libphonenumber/src/test':
      'http://libphonenumber.googlecode.com/svn/trunk/cpp/test@425',
   'src/third_party/WebKit/Tools/Scripts':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/Scripts@108903',
   'src/third_party/yasm/source/patched-yasm':
      '/trunk/deps/third_party/yasm/patched-yasm@73761',
   'build/scripts/command_wrapper/bin':
      '/trunk/tools/command_wrapper/bin@111020',
   'src/third_party/libjingle/source':
      'http://libjingle.googlecode.com/svn/trunk@117',
   'src':
      '/trunk/src@123710',
   'commit-queue':
      '/trunk/tools/commit-queue@123090',
   'src/third_party/pymox/src':
      'http://pymox.googlecode.com/svn/trunk@70',
   'src/chrome/browser/resources/software_rendering_list':
      '/trunk/deps/gpu/software_rendering_list@115644',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell@110423',
   'src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/platform/chromium-win/http/tests/workers@108903',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache':
      Var("webkit_trunk")[:-6] + '/trunk/LayoutTests/http/tests/appcache@108903',
   'src/third_party/WebKit/Tools/TestWebKitAPI':
      Var("webkit_trunk")[:-6] + '/trunk/Tools/TestWebKitAPI@108903',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@560',
   'src/v8':
      'http://v8.googlecode.com/svn/trunk@10829',
   'src/third_party/libvpx':
      '/trunk/deps/third_party/libvpx@122996',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@69281',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'native_client', 'native_client_sdk', 'o3d', 'pdf', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/download_nacl_toolchains.py', '--no-arm-trusted', '--optional-pnacl', '--pnacl-version', '7799', '--file-hash', 'pnacl_linux_i686', 'a1be3e4ecea3f3d3becfff6860e8a593ec014d81', '--file-hash', 'pnacl_linux_x86_64', 'f34c8efb57ca0d1f9fb3f71d9e343bfa7026bd03', '--file-hash', 'pnacl_darwin_i386', 'fadb4949172a2cc918ddcadca06ec524f19b29b8', '--x86-version', '7799', '--file-hash', 'mac_x86_newlib', '039a39d2323c8e9380252607bcf9bca1a1cfa7d6', '--file-hash', 'win_x86_newlib', 'bd8dd553c9998e7bf826f5746c8ef69c3c5e5ac7', '--file-hash', 'linux_x86_newlib', '23697d53dce9a506f65a713d6913e99ecb882f18', '--file-hash', 'mac_x86', 'ea06ee0e55d5e4e252fc85c916474a40b6cad023', '--file-hash', 'win_x86', '6a8fd328bcbdaa88887a39fd9a83a006129b44dd', '--file-hash', 'linux_x86', '030324df172abc67f69dca80012c2a37ccd9ebb0', '--save-downloads-dir', 'src/native_client_sdk/src/build_tools/toolchain_archives', '--keep'], 'pattern': '.'}, {'action': ['python', 'src/tools/clang/scripts/update.py', '--mac-only'], 'pattern': '.'}, {'action': ['python', 'src/build/win/setup_cygwin_mount.py', '--win-only'], 'pattern': '.'}, {'action': ['python', 'src/build/util/lastchange.py', '-o', 'src/build/util/LASTCHANGE'], 'pattern': '.'}, {'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
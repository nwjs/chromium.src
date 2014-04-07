#  
#  To use this DEPS file to re-create a Chromium release you
#  need the tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 6.0.415.1
#  
#  
#  
deps_os = {
   'win': {
      'src/third_party/xulrunner-sdk':
         '/trunk/deps/third_party/xulrunner-sdk@17887',
      'src/chrome_frame/tools/test/reference_build/chrome':
         '/trunk/deps/reference_builds/chrome_frame@33840',
      'src/third_party/cygwin':
         '/trunk/deps/third_party/cygwin@11984',
      'src/third_party/python_24':
         '/trunk/deps/third_party/python_24@22967',
      'src/third_party/swig/win':
         '/trunk/deps/third_party/swig/win@40423',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
         '/trunk/deps/third_party/ffmpeg/binaries/win@47712',
      'src/third_party/lighttpd':
         '/trunk/deps/third_party/lighttpd@33727',
      'src/third_party/pefile':
         'http://pefile.googlecode.com/svn/trunk@63',
      'src/chrome/tools/test/reference_build/chrome':
         '/trunk/deps/reference_builds/chrome@41984',
      'src/third_party/mingw-w64/mingw/bin':
         'http://nativeclient.googlecode.com/svn/trunk/src/third_party/mingw-w64/mingw/bin@2235',
      'src/third_party/nss':
         '/trunk/deps/third_party/nss@45059',
   },
   'mac': {
      'src/third_party/GTM':
         'http://google-toolbox-for-mac.googlecode.com/svn/trunk@330',
      'src/third_party/pdfsqueeze':
         'http://pdfsqueeze.googlecode.com/svn/trunk@2',
      'src/third_party/yasm/source/patched-yasm':
         '/trunk/deps/third_party/yasm/patched-yasm@29937',
      'src/third_party/swig/mac':
         '/trunk/deps/third_party/swig/mac@40423',
      'src/third_party/lighttpd':
         '/trunk/deps/third_party/lighttpd@33737',
      'src/chrome/tools/test/reference_build/chrome_mac':
         '/trunk/deps/reference_builds/chrome_mac@41963',
   },
   'unix': {
      'src/third_party/cros':
         'http://src.chromium.org/git/cros.git@3d4e6dd4',
      'src/third_party/yasm/source/patched-yasm':
         '/trunk/deps/third_party/yasm/patched-yasm@29937',
      'src/third_party/swig/linux':
         '/trunk/deps/third_party/swig/linux@40423',
      'src/third_party/chromeos_login_manager':
         'http://src.chromium.org/git/login_manager.git@c51b4b0a',
      'src/third_party/xdg-utils':
         '/trunk/deps/third_party/xdg-utils@29103',
      'src/chrome/tools/test/reference_build/chrome_linux':
         '/trunk/deps/reference_builds/chrome_linux@41515',
   },
}

deps = {
   'src/third_party/skia/include':
      'http://skia.googlecode.com/svn/trunk/include@562',
   'src/third_party/ots':
      'http://ots.googlecode.com/svn/trunk@30',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@568',
   'chromeos':
      '/trunk/cros_deps@48136',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@134',
   'src/third_party/protobuf2/src':
      'http://protobuf.googlecode.com/svn/trunk@327',
   'src/v8':
      'http://v8.googlecode.com/svn/trunk@4660',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@818',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@28',
   'src/third_party/libvpx/lib':
      '/trunk/deps/third_party/libvpx/lib@47701',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@19546',
   'src/build/util/support':
      '/trunk/deps/support@48215',
   'src/third_party/angle':
      'http://angleproject.googlecode.com/svn/trunk@305',
   'src/third_party/ffmpeg/source/patched-ffmpeg-mt':
      '/trunk/deps/third_party/ffmpeg/patched-ffmpeg-mt@47712',
   'src/third_party/skia/src':
      'http://skia.googlecode.com/svn/trunk/src@562',
   'src/third_party/libvpx/include':
      '/trunk/deps/third_party/libvpx/include@47701',
   'src/third_party/cacheinvalidation/files':
      'http://google-cache-invalidation-api.googlecode.com/svn/trunk@13',
   'src/third_party/ppapi':
      'http://ppapi.googlecode.com/svn/trunk@50',
   'src':
      '/branches/415/src@48139',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu42@47106',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@33467',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell128@46230',
   'src/third_party/swig/Lib':
      '/trunk/deps/third_party/swig/Lib@40423',
   'src/native_client':
      'http://nativeclient.googlecode.com/svn/trunk/src/native_client@2235',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@408',
   'src/third_party/py/simplejson':
      'http://simplejson.googlecode.com/svn/trunk/simplejson@217',
}

skip_child_includes =  ['breakpad', 'chrome_frame', 'gears', 'native_client', 'o3d', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/gyp_chromium'], 'pattern': '.'}, {'action': ['python', 'src/build/mac/clobber_generated_headers.py'], 'pattern': '.'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing']
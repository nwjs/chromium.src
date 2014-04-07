#  
#  To use this DEPS file to re-create a Chromium release you need the
#  tools from http://dev.chromium.org installed.
#  
#  This DEPS file corresponds to Chromium 4.0.212.1
#  
#  
#  
deps_os = {
   'win': {
      'src/third_party/cygwin':
         '/trunk/deps/third_party/cygwin@11984',
      'src/third_party/pthreads-win32':
         '/trunk/deps/third_party/pthreads-win32@26716',
      'src/third_party/python_24':
         '/trunk/deps/third_party/python_24@22967',
      'src/third_party/ffmpeg/binaries/chromium/win/ia32':
         '/trunk/deps/third_party/ffmpeg/binaries/win@26428',
   },
   'mac': {
      'src/third_party/GTM':
         'http://google-toolbox-for-mac.googlecode.com/svn/trunk@223',
      'src/third_party/pdfsqueeze':
         'http://pdfsqueeze.googlecode.com/svn/trunk@2',
      'src/third_party/WebKit/WebKit/mac':
         'http://svn.webkit.org/repository/webkit/trunk/WebKit/mac@48585',
      'src/third_party/WebKit/WebKitLibraries':
         'http://svn.webkit.org/repository/webkit/trunk/WebKitLibraries@48585',
      'src/third_party/ffmpeg/binaries/chromium/mac/ia32_dbg':
         '/trunk/deps/third_party/ffmpeg/binaries/mac_dbg@26428',
      'src/third_party/ffmpeg/binaries/chromium/mac/ia32':
         '/trunk/deps/third_party/ffmpeg/binaries/mac@26428',
   },
   'unix': {
      'src/third_party/xdg-utils':
         '/trunk/deps/third_party/xdg-utils@26314',
      'src/third_party/ffmpeg/binaries/chromium/linux/x64_dbg':
         '/trunk/deps/third_party/ffmpeg/binaries/linux_64_dbg@26428',
      'src/third_party/ffmpeg/binaries/chromium/linux/ia32':
         '/trunk/deps/third_party/ffmpeg/binaries/linux@26428',
      'src/third_party/ffmpeg/binaries/chromium/linux/ia32_dbg':
         '/trunk/deps/third_party/ffmpeg/binaries/linux_dbg@26428',
      'src/third_party/ffmpeg/binaries/chromium/linux/x64':
         '/trunk/deps/third_party/ffmpeg/binaries/linux_64@26428',
   },
}

deps = {
   'src/chrome/test/data/layout_tests/LayoutTests/fast/events':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/fast/events@48585',
   'src/third_party/WebKit/WebCore':
      'http://svn.webkit.org/repository/webkit/trunk/WebCore@48585',
   'src/breakpad/src':
      'http://google-breakpad.googlecode.com/svn/trunk/src@391',
   'src/third_party/skia':
      'http://skia.googlecode.com/svn/trunk@341',
   'src/chrome/test/data/layout_tests/LayoutTests/fast/workers':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/fast/workers@48585',
   'src/third_party/tcmalloc/tcmalloc':
      'http://google-perftools.googlecode.com/svn/trunk@74',
   'src/googleurl':
      'http://google-url.googlecode.com/svn/trunk@116',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/http/tests/workers@48585',
   'src/third_party/protobuf2/src':
      'http://protobuf.googlecode.com/svn/trunk@219',
   'src/native_client':
      'http://nativeclient.googlecode.com/svn/trunk/src/native_client@730',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/http/tests/xmlhttprequest@48585',
   'src/tools/gyp':
      'http://gyp.googlecode.com/svn/trunk@657',
   'src/sdch/open-vcdiff':
      'http://open-vcdiff.googlecode.com/svn/trunk@26',
   'src/third_party/WebKit/JavaScriptCore':
      'http://svn.webkit.org/repository/webkit/trunk/JavaScriptCore@48585',
   'src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/http/tests/resources@48585',
   'src/tools/page_cycler/acid3':
      '/trunk/deps/page_cycler/acid3@19546',
   'src/build/util/support':
      '/trunk/deps/support@26866',
   'src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests/storage/domstorage@48585',
   'src/chrome/tools/test/reference_build':
      '/trunk/deps/reference_builds@25513',
   'src/third_party/WebKit/LayoutTests':
      'http://svn.webkit.org/repository/webkit/trunk/LayoutTests@48585',
   'src':
      '/branches/212/src@26852',
   'src/third_party/icu':
      '/trunk/deps/third_party/icu42@26673',
   'src/third_party/xulrunner-sdk':
      '/trunk/deps/third_party/xulrunner-sdk/@26810',
   'src/third_party/gecko-sdk':
      '/trunk/deps/third_party/gecko-sdk@26810',
   'src/third_party/WebKit':
      '/trunk/deps/third_party/WebKit@20601',
   'src/third_party/hunspell':
      '/trunk/deps/third_party/hunspell128@26238',
   'src/testing/gtest':
      'http://googletest.googlecode.com/svn/trunk@267',
   'src/v8':
      'http://v8.googlecode.com/svn/trunk@2901',
}

skip_child_includes =  ['breakpad', 'gears', 'native_client', 'o3d', 'sdch', 'skia', 'testing', 'third_party', 'v8'] 

hooks =  [{'action': ['python', 'src/build/gyp_chromium'], 'pattern': '\\.gypi?$|[/\\\\]src[/\\\\]tools[/\\\\]gyp[/\\\\]|[/\\\\]src[/\\\\]build[/\\\\]gyp_chromium$'}, {'action': ['python', 'src/build/win/clobber_generated_headers.py', '$matching_files'], 'pattern': '\\.grd$'}] 

include_rules =  ['+base', '+build', '+ipc', '+unicode', '+testing', '+webkit/port/platform/graphics/skia/public', '+chrome/app', '+chrome/browser', '+chrome/renderer', '+chrome/views', '+skia/include', '+grit', '+webkit/glue']
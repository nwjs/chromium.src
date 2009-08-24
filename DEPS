vars = {
  "webkit_trunk":
    "http://svn.webkit.org/repository/webkit/trunk",
  "webkit_revision": "46143",
}


deps = {
  "src/breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@346",

  "src/build/util/support":
    "/trunk/deps/support@19914",

  "src/googleurl":
    "http://google-url.googlecode.com/svn/trunk@110",

  "src/sdch/open-vcdiff":
    "http://open-vcdiff.googlecode.com/svn/trunk@26",

  "src/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@267",

  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@20601",

  "src/third_party/icu38":
    "/trunk/deps/third_party/icu38@20192",

  "src/third_party/protobuf2/src":
    "http://protobuf.googlecode.com/svn/trunk@154",

  # TODO(mark): Remove once this has moved into depot_tools.
  "src/tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@553",

  "src/v8":
    "http://v8.googlecode.com/svn/branches/1.2@2530",

  "src/native_client":
    "http://nativeclient.googlecode.com/svn/trunk/src/native_client@385",

  "src/third_party/skia":
    "http://skia.googlecode.com/svn/trunk@250",

  "src/third_party/WebKit/LayoutTests":
    "svn://chrome-svn/chrome/branches/WebKit/195/LayoutTests@22623",

  "src/third_party/WebKit/JavaScriptCore":
    "svn://chrome-svn/chrome/branches/WebKit/195/JavaScriptCore@22621",

  "src/third_party/WebKit/WebCore":
    "svn://chrome-svn/chrome/branches/WebKit/195/WebCore@22620",

  "src/third_party/tcmalloc/tcmalloc":
    "http://google-perftools.googlecode.com/svn/trunk@74",

  "src/tools/page_cycler/acid3":
    "/trunk/deps/page_cycler/acid3@19546",

  # TODO(jianli): Remove this once we do not need to run worker's layout tests
  # in ui test.
  "src/chrome/test/data/workers/LayoutTests/fast/workers":
    "svn://chrome-svn/chrome/branches/WebKit/195/LayoutTests/fast/workers@22623",
  "src/chrome/test/data/workers/LayoutTests/http/tests/resources":
    "svn://chrome-svn/chrome/branches/WebKit/195/LayoutTests/http/tests/resources@22623",
  "src/chrome/test/data/workers/LayoutTests/http/tests/workers":
    "svn://chrome-svn/chrome/branches/WebKit/195/LayoutTests/http/tests/workers@22623",
  "src/chrome/test/data/workers/LayoutTests/http/tests/xmlhttprequest":
    "svn://chrome-svn/chrome/branches/WebKit/195/LayoutTests/http/tests/xmlhttprequest@22623",
}


deps_os = {
  "win": {
    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@11984",

    "src/third_party/python_24":
      "/trunk/deps/third_party/python_24@19441",

    "src/third_party/ffmpeg/binaries/chromium":
      "/trunk/deps/third_party/ffmpeg/binaries/win@22838",
  },
  "mac": {
    "src/third_party/GTM":
      "http://google-toolbox-for-mac.googlecode.com/svn/trunk@154",
    "src/third_party/pdfsqueeze":
      "http://pdfsqueeze.googlecode.com/svn/trunk@2",
    "src/third_party/WebKit/WebKit/mac":
      Var("webkit_trunk") + "/WebKit/mac@" + Var("webkit_revision"),
    "src/third_party/WebKit/WebKitLibraries":
      Var("webkit_trunk") + "/WebKitLibraries@" + Var("webkit_revision"),
    "src/third_party/ffmpeg/binaries/chromium":
      "/trunk/deps/third_party/ffmpeg/binaries/mac@22838",
  },
  "unix": {
    # Linux, really.
    "src/third_party/xdg-utils":
      "/trunk/deps/third_party/xdg-utils@20073",
    "src/third_party/ffmpeg/binaries/chromium":
      "/trunk/deps/third_party/ffmpeg/binaries/linux@22838",
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  "+ipc",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode",
  "+testing",

  # Allow anybody to include files from the "public" Skia directory in the
  # webkit port. This is shared between the webkit port and Chrome.
  "+webkit/port/platform/graphics/skia/public",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "breakpad",
   "gears",
   "native_client",
   "o3d",
   "sdch",
   "skia",
   "testing",
   "third_party",
   "v8",
]


hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself shound run the generator.
    "pattern": "\\.gypi?$|[/\\\\]src[/\\\\]tools[/\\\\]gyp[/\\\\]",
    "action": ["python", "src/tools/gyp/gyp_chromium"],
  },
]


# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'nw_product_name': 'node-webkit',
  },
  'targets': [
    {
      'target_name': 'nw_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_app',
        'content_browser',
        'content_common',
        'content_gpu',
        'content_plugin',
        'content_ppapi_plugin',
        'content_renderer',
        'content_utility',
        'content_worker',
        'content_resources.gyp:content_resources',
        'nw_resources',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../skia/skia.gyp:skia',
        '../third_party/node/node.gyp:node',
        '../ui/ui.gyp:ui',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/support/webkit_support.gyp:webkit_support',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'nw/src/common/zip.cc',
        'nw/src/common/zip.h',
        'nw/src/common/zip_reader.cc',
        'nw/src/common/zip_reader.h',
        'nw/src/common/zip_internal.cc',
        'nw/src/common/zip_internal.h',
        'nw/src/geolocation/shell_access_token_store.cc',
        'nw/src/geolocation/shell_access_token_store.h',
        'nw/src/nw_version.h',
        'nw/src/nw_package.h',
        'nw/src/nw_package.cc',
        'nw/src/paths_mac.h',
        'nw/src/paths_mac.mm',
        'nw/src/shell.cc',
        'nw/src/shell.h',
        'nw/src/shell_android.cc',
        'nw/src/shell_aura.cc',
        'nw/src/shell_gtk.cc',
        'nw/src/shell_mac.mm',
        'nw/src/shell_win.cc',
        'nw/src/shell_application_mac.h',
        'nw/src/shell_application_mac.mm',
        'nw/src/shell_browser_context.cc',
        'nw/src/shell_browser_context.h',
        'nw/src/shell_browser_main.cc',
        'nw/src/shell_browser_main.h',
        'nw/src/shell_browser_main_parts.cc',
        'nw/src/shell_browser_main_parts.h',
        'nw/src/shell_browser_main_parts_mac.mm',
        'nw/src/shell_content_browser_client.cc',
        'nw/src/shell_content_browser_client.h',
        'nw/src/shell_content_client.cc',
        'nw/src/shell_content_client.h',
        'nw/src/shell_content_renderer_client.cc',
        'nw/src/shell_content_renderer_client.h',
        'nw/src/shell_devtools_delegate.cc',
        'nw/src/shell_devtools_delegate_android.cc',
        'nw/src/shell_devtools_delegate.h',
        'nw/src/shell_download_manager_delegate.cc',
        'nw/src/shell_download_manager_delegate.h',
        'nw/src/shell_javascript_dialog_creator.cc',
        'nw/src/shell_javascript_dialog_creator.h',
        'nw/src/shell_javascript_dialog_gtk.cc',
        'nw/src/shell_javascript_dialog_mac.mm',
        'nw/src/shell_javascript_dialog_win.cc',
        'nw/src/shell_javascript_dialog.h',
        'nw/src/shell_login_dialog_gtk.cc',
        'nw/src/shell_login_dialog_mac.mm',
        'nw/src/shell_login_dialog.cc',
        'nw/src/shell_login_dialog.h',
        'nw/src/shell_main_delegate.cc',
        'nw/src/shell_main_delegate.h',
        'nw/src/shell_messages.cc',
        'nw/src/shell_messages.h',
        'nw/src/shell_network_delegate.cc',
        'nw/src/shell_network_delegate.h',
        'nw/src/shell_render_process_observer.cc',
        'nw/src/shell_render_process_observer.h',
        'nw/src/shell_resource_context.cc',
        'nw/src/shell_resource_context.h',
        'nw/src/shell_resource_dispatcher_host_delegate.cc',
        'nw/src/shell_resource_dispatcher_host_delegate.h',
        'nw/src/shell_switches.cc',
        'nw/src/shell_switches.h',
        'nw/src/shell_url_request_context_getter.cc',
        'nw/src/shell_url_request_context_getter.h',
        'nw/src/prerenderer/prerenderer_client.cc',
        'nw/src/prerenderer/prerenderer_client.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'dependencies': [
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],  # OS=="win"
        ['OS=="android"', {
          'dependencies': [
            'nw_jni_headers',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/content/nw',
          ],
          'sources!': [
            'nw/src/shell_devtools_delegate.cc',
          ],
        }, {  # else: OS!="android"
          'dependencies': [
            # This dependency is for running DRT against the content shell, and
            # this combination is not yet supported on Android.
            '../webkit/support/webkit_support.gyp:webkit_support',
          ],
        }],  # OS=="android"
        ['os_posix==1 and use_aura==1 and linux_use_tcmalloc==1', {
          'dependencies': [
            # This is needed by content/app/content_main_runner.cc
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:aura',
            '../ui/base/strings/ui_strings.gyp:ui_strings',
            '../ui/views/views.gyp:views',
            '../ui/ui.gyp:ui_resources',
          ],
          'sources/': [
            ['exclude', 'nw/src/shell_gtk.cc'],
            ['exclude', 'nw/src/shell_win.cc'],
          ],
        }],  # use_aura==1
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
           ],
        }], # chromeos==1
        ['inside_chromium_build==0 or component!="shared_library"', {
          'dependencies': [
            '<(webkit_src_dir)/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore_test_support',
            '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
          ],
          'include_dirs': [
            # Required for WebTestingSupport.cpp to find our custom config.h.
            'nw/src/',
            '<(webkit_src_dir)/Source/WebKit/chromium/public',
            # WARNING: Do not view this particular case as a precedent for
            # including WebCore headers in the content shell.
            '<(webkit_src_dir)/Source/WebCore/testing/v8', # for WebCoreTestSupport.h needed  to link in window.internals code.
          ],
          'sources': [
            'nw/src/config.h',
            '<(webkit_src_dir)/Source/WebKit/chromium/src/WebTestingSupport.cpp',
            '<(webkit_src_dir)/Source/WebKit/chromium/public/WebTestingSupport.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'nw_resources',
      'type': 'none',
      'dependencies': [
        'generate_nw_resources',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/content/nw_resources.pak'
          ],
        },
      ],
    },
    {
      'target_name': 'generate_nw_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'actions': [
        {
          'action_name': 'nw_resources',
          'variables': {
            'grit_grd_file': 'nw/src/nw_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
    },
    {
      # We build a minimal set of resources so WebKit in nw has
      # access to necessary resources.
      'target_name': 'nw_pak',
      'type': 'none',
      'dependencies': [
        'browser/debugger/devtools_resources.gyp:devtools_resources',
        'nw_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
      ],
      'variables': {
        'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
      },
      'actions': [
        {
          'action_name': 'repack_nw_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/nw_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/app_locale_settings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings/ui_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
            ],
          },
          'inputs': [
            '<(repack_path)',
            '<@(pak_inputs)',
          ],
          'action': ['python', '<(repack_path)', '<@(_outputs)',
                     '<@(pak_inputs)'],
          'conditions': [
            ['OS!="android"', {
              'outputs': [
                '<(PRODUCT_DIR)/nw.pak',
              ],
            }, {
              'outputs': [
                '<(PRODUCT_DIR)/nw/assets/nw.pak',
              ],
            }],
          ],
        },
      ],
    },
    {
      'target_name': 'nw',
      'type': 'executable',
      'mac_bundle': 1,
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'nw_lib',
        'nw_pak',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/startup_helper_win.cc',
        'nw/src/shell_main.cc',
      ],
      'mac_bundle_resources': [
        'nw/src/mac/app.icns',
        'nw/src/mac/app-Info.plist',
      ],
      # TODO(mark): Come up with a fancier way to do this.  It should only
      # be necessary to list app-Info.plist once, not the three times it is
      # listed here.
      'mac_bundle_resources!': [
        'nw/src/mac/app-Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'nw/src/mac/app-Info.plist',
      },
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'nw/src/shell.rc',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],  # OS=="win"
        ['OS == "win" or (toolkit_uses_gtk == 1 and selinux == 0)', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],  # OS=="win" or (toolkit_uses_gtk == 1 and selinux == 0)
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],  # toolkit_uses_gtk
        ['OS=="mac"', {
          'product_name': '<(nw_product_name)',
          'dependencies!': [
            'nw_lib',
          ],
          'dependencies': [
            'nw_framework',
            'nw_helper_app',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(nw_product_name).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(nw_product_name) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(nw_product_name) Framework.framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(nw_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '/Library/Frameworks/<(nw_product_name) Framework.framework/Versions/A/<(nw_product_name) Framework',
                '@executable_path/../Frameworks/<(nw_product_name) Framework.framework/<(nw_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--svn=0'],
            },
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # Content Shell Helper EH.app and Content Shell Helper NP.app are
              # created from Content Shell Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                '<(nw_product_name)',
              ],
            },
            {
              # Make sure there isn't any Objective-C in the shell's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
        }],  # OS=="mac"
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'nw_framework',
          'type': 'shared_library',
          'product_name': '<(nw_product_name) Framework',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            'nw/src/mac/English.lproj/HttpAuth.xib',
            'nw/src/mac/English.lproj/MainMenu.xib',
            '<(PRODUCT_DIR)/nw.pak'
          ],
          'dependencies': [
            'nw_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'nw/src/shell_content_main.cc',
            'nw/src/shell_content_main.h',
          ],
          'copies': [
            {
              # Copy FFmpeg binaries for audio/video support.
              'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
              'files': [
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
          ],
        },  # target nw_framework
        {
          'target_name': 'nw_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(nw_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'nw_framework',
          ],
          'sources': [
            'nw/src/shell_main.cc',
            'nw/src/mac/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'nw/src/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'nw/src/mac/helper-Info.plist',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # nw_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '/Library/Frameworks/<(nw_product_name) Framework.framework/Versions/A/<(nw_product_name) Framework',
                '@executable_path/../../../../Frameworks/<(nw_product_name) Framework.framework/<(nw_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --svn=0
              # are used because Breakpad, Keystone, and Subversion keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--svn=0'],
            },
            {
              # Make sure there isn't any Objective-C in the helper app's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
          'conditions': [
            ['component=="shared_library"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [
                  # Get back from Content Shell.app/Contents/Frameworks/
                  #                                 Helper.app/Contents/MacOS
                  '@loader_path/../../../../../..',
                ],
              },
            }],
          ],
        },  # target nw_helper_app
      ],
    }],  # OS=="mac"
    ['OS=="android"', {
      'targets': [
        {
          # TODO(jrg): Update this action and other jni generators to only
          # require specifying the java directory and generate the rest.
          'target_name': 'nw_jni_headers',
          'type': 'none',
          'sources': [
            'nw/src/android/java/src/org/chromium/nw/ShellManager.java',
            'nw/src/android/java/src/org/chromium/nw/Shell.java',
          ],
          'variables': {
            'jni_gen_dir': 'content/nw',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libnw_content_view',
          'type': 'shared_library',
          'dependencies': [
            'nw_jni_headers',
            'nw_lib',
            'nw_pak',
            # Skia is necessary to ensure the dependencies needed by
            # WebContents are included.
            '../skia/skia.gyp:skia',
            '<(DEPTH)/media/media.gyp:player_android',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/content/nw',
          ],
          'sources': [
            'nw/src/android/draw_context.cc',
            'nw/src/android/draw_context.h',
            'nw/src/android/shell_library_loader.cc',
            'nw/src/android/shell_library_loader.h',
            'nw/src/android/shell_manager.cc',
            'nw/src/android/shell_manager.h',
          ],
          'sources!': [
            'nw/src/shell_main.cc',
            'nw/src/shell_main.h',
          ],
          'conditions': [
            ['android_build_type==1', {
              'ldflags': [
                '-lgabi++',  # For rtti
              ],
            }],
          ],
        },
        {
          'target_name': 'nw_apk',
          'type': 'none',
          'actions': [
            {
              'action_name': 'copy_base_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base.jar'],
              'outputs': ['<(PRODUCT_DIR)/nw/java/libs/chromium_base.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_net_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_net.jar'],
              'outputs': ['<(PRODUCT_DIR)/nw/java/libs/chromium_net.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_media_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_media.jar'],
              'outputs': ['<(PRODUCT_DIR)/nw/java/libs/chromium_media.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_content_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_content.jar'],
              'outputs': ['<(PRODUCT_DIR)/nw/java/libs/chromium_content.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_and_strip_so',
              'inputs': ['<(SHARED_LIB_DIR)/libnw_content_view.so'],
              'outputs': ['<(PRODUCT_DIR)/nw/libs/<(android_app_abi)/libnw_content_view.so'],
              'action': [
                '<!(/bin/echo -n $STRIP)',
                '--strip-unneeded',  # All symbols not needed for relocation.
                '<@(_inputs)',
                '-o',
                '<@(_outputs)',
              ],
            },
            {
              'action_name': 'nw_apk',
              'inputs': [
                'nw/src/android/java/nw_apk.xml',
                'nw/src/android/java/AndroidManifest.xml',
                '<!@(find shell/android/java -name "*.java")',
                '<!@(find shell/android/res -name "*")',
                '<(PRODUCT_DIR)/nw/java/libs/chromium_base.jar',
                '<(PRODUCT_DIR)/nw/java/libs/chromium_net.jar',
                '<(PRODUCT_DIR)/nw/java/libs/chromium_media.jar',
                '<(PRODUCT_DIR)/nw/java/libs/chromium_content.jar',
                '<(PRODUCT_DIR)/nw/assets/nw.pak',
                '<(PRODUCT_DIR)/nw/libs/<(android_app_abi)/libnw_content_view.so',
              ],
              'outputs': [
                # Awkwardly, we build a Debug APK even when gyp is in
                # Release mode.  I don't think it matters (e.g. we're
                # probably happy to not codesign) but naming should be
                # fixed.
                '<(PRODUCT_DIR)/nw/ContentShell-debug.apk',
              ],
              'action': [
                # Pass the build type to ant. Currently it only assumes
                # debug mode in java. Release mode will break the current
                # workflow.
                'nw/src/content_shell_ant_helper.sh',
                'ant',
                '-DPRODUCT_DIR=<(ant_build_out)',
                '-DAPP_ABI=<(android_app_abi)',
                '-DANDROID_SDK=<(android_sdk)',
                '-DANDROID_SDK_ROOT=<(android_sdk_root)',
                '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
                '-DANDROID_SDK_VERSION=<(android_sdk_version)',
                '-DANDROID_TOOLCHAIN=<(android_toolchain)',
                '-buildfile',
                'nw/src/android/java/nw_apk.xml',
                '<(CONFIGURATION_NAME)',
              ],
              'dependencies': [
                'content_java',
              ],
            }
          ],
        },
      ],
    }],  # OS=="android"
  ]
}

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    # Dummy target to allow chrome to require chrome_dll to build
    # without actually linking to the library
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'chrome_dll_dependency_shim',
          'type': 'executable',
          'dependencies': [
            'chrome_dll',
          ],
          # In release, we end up with a strip step that is unhappy if there is
          # no binary. Rather than check in a new file for this hack, just
          # generate a source file on the fly.
          'actions': [
            {
              'action_name': 'generate_stub_main',
              'process_outputs_as_sources': 1,
              'inputs': [],
              'outputs': [ '<(INTERMEDIATE_DIR)/dummy_main.c' ],
              'action': [
                'bash', '-c',
                'echo "int main() { return 0; }" > <(INTERMEDIATE_DIR)/dummy_main.c'
              ],
            },
          ],
        },
      ],
     },
    ],
    ['OS=="mac" or OS=="win" or OS=="linux"', {
      'targets': [
        {
          # GN version: //chrome:chrome_dll
          'target_name': 'chrome_dll',
          'type': 'none',
          'dependencies': [
            'chrome_main_dll',
          ],
          'conditions': [
            ['OS=="mac" and component=="shared_library"', {
              'type': 'shared_library',
              'includes': [ 'chrome_dll_bundle.gypi' ],
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-Wl,-reexport_library,<(PRODUCT_DIR)/libchrome_main_dll.dylib',
                ],
              },
            }],  # OS=="mac"
            ['chrome_multiple_dll==1', {
              'dependencies': [
                'chrome_child_dll',
              ],
            }],
            ['incremental_chrome_dll==1', {
              # Linking to a different directory and then hardlinking back
              # to OutDir is a workaround to avoid having the .ilk for
              # chrome.exe and chrome.dll conflicting. See crbug.com/92528
              # for more information. Done on the dll instead of the exe so
              # that people launching from VS don't need to modify
              # $(TargetPath) for the exe.
              'actions': [
                {
                  'action_name': 'hardlink_to_output',
                  'inputs': [
                    '$(OutDir)\\initial\\nw.dll',
                  ],
                  'outputs': [
                    '$(OutDir)\\nw.dll',
                  ],
                  'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                             '$(OutDir)\\initial\\nw.dll',
                             '$(OutDir)\\nw.dll'],
                },
              ],
              'conditions': [
                # Only hardlink pdb if we're generating debug info.
                ['fastbuild==0 or win_z7!=0', {
                  'actions': [
                    {
                      'action_name': 'hardlink_pdb_to_output',
                      'inputs': [
                        # Not the pdb, since gyp doesn't know about it
                        '$(OutDir)\\initial\\nw.dll',
                      ],
                      'outputs': [
                        '$(OutDir)\\nw.dll.pdb',
                      ],
                      'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                                 '$(OutDir)\\initial\\nw.dll.pdb',
                                 '$(OutDir)\\nw.dll.pdb'],
                    }
                  ]
                }]
              ],
            }],
          ]
        },
        {
          # GN version: //chrome:main_dll
          'target_name': 'chrome_main_dll',
          'type': 'shared_library',
          'variables': {
            'enable_wexit_time_destructors': 1,
          },
          'sources': [
            '../base/win/dllmain.cc',
            'app/chrome_command_ids.h',
            'app/chrome_dll_resource.h',
            'app/chrome_main.cc',
            'app/chrome_main_delegate.cc',
            'app/chrome_main_delegate.h',
            'app/chrome_main_mac.h',
            'app/chrome_main_mac.mm',
            'app/delay_load_hook_win.cc',
            'app/delay_load_hook_win.h',
          ],
          'dependencies': [
            '<@(chromium_browser_dependencies)',
            '../content/content.gyp:content_app_browser',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(DEPTH)/chrome_elf/chrome_elf.gyp:chrome_elf',
              ],
            }],
            ['OS=="win" and configuration_policy==1', {
              'dependencies': [
                '<(DEPTH)/components/components.gyp:policy',
              ],
            }],
            ['use_aura==1', {
              'dependencies': [
                '../ui/compositor/compositor.gyp:compositor',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              # Add a dependency to custom import library for user32 delay
              # imports only in x86 builds.
              'dependencies': [
                'chrome_user32_delay_imports',
              ],
            },],
            ['OS=="linux"', {
              'product_name': 'nw'
            }],
            ['OS=="win"', {
              'product_name': 'nw',
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_version_resources',
                '../base/trace_event/etw_manifest/etw_manifest.gyp:etw_manifest',
                '../chrome/chrome_resources.gyp:chrome_unscaled_resources',
                '../content/app/resources/content_resources.gyp:content_resources',
                '../crypto/crypto.gyp:crypto',
                '../net/net.gyp:net_resources',
                '../ui/views/views.gyp:views',
              ],
              'sources': [
                'app/chrome_dll.rc',

                # ETW Manifest.
                '<(SHARED_INTERMEDIATE_DIR)/base/trace_event/etw_manifest/chrome_events_win.rc',

                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',

                # Cursors.
                '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
              ],
              'include_dirs': [
                '<(DEPTH)/third_party/wtl/include',
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
              'msvs_settings': {
                'VCLinkerTool': {
                  # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                  'SubSystem': '2',
                  'conditions': [
                    ['incremental_chrome_dll==1', {
                      'OutputFile': '$(OutDir)\\initial\\nw.dll',
                      'UseLibraryDependencyInputs': "true",
                    }],
                    ['target_arch=="ia32"', {
                      # Don't set an x64 base address (to avoid breaking HE-ASLR).
                      'BaseAddress': '0x01c30000',
                      # Link against the XP-constrained user32 import library
                      # instead of the platform-SDK provided one to avoid
                      # inadvertently taking dependencies on post-XP user32
                      # exports.
                      'AdditionalDependencies!': [
                        'user32.lib',
                      ],
                      'IgnoreDefaultLibraryNames': [
                        'user32.lib',
                      ],
                      # Remove user32 delay load for chrome.dll.
                      'DelayLoadDLLs!': [
                        'user32.dll',
                      ],
                      'AdditionalDependencies': [
                        'user32.winxp.lib',
                      ],
                      'DelayLoadDLLs': [
                        'user32-delay.dll',
                      ],
                      'AdditionalLibraryDirectories': [
                        '<(DEPTH)/build/win/importlibs/x86',
                      ],
                      'ForceSymbolReferences': [
                        # Force the inclusion of the delay load hook in this
                        # binary.
                        '_ChromeDelayLoadHook@8',
                      ],
                    }],
                  ],
                  'DelayLoadDLLs': [
                    'comdlg32.dll',
                    'crypt32.dll',
                    'cryptui.dll',
                    'dhcpcsvc.dll',
                    'imagehlp.dll',
                    'imm32.dll',
                    'iphlpapi.dll',
                    'setupapi.dll',
                    'urlmon.dll',
                    'winhttp.dll',
                    'wininet.dll',
                    'winspool.drv',
                    'ws2_32.dll',
                    'wsock32.dll',
                  ],
                },
                'VCManifestTool': {
                  'AdditionalManifestFiles': [
                    '$(ProjectDir)\\app\\nw.dll.manifest',
                  ],
                },
              },
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
                ['enable_basic_printing==1 or enable_print_preview==1', {
                  'dependencies': [
                    '../printing/printing.gyp:printing',
                  ],
                }],
              ]
            }],
            ['chrome_multiple_dll==1', {
              'defines': [
                'CHROME_MULTIPLE_DLL_BROWSER',
              ],
            }, {
              'dependencies': [
                '<@(chromium_child_dependencies)',
                '../content/content.gyp:content_app_both',
              ],
              'dependencies!': [
                '../content/content.gyp:content_app_browser',
              ],
            }],
            ['chrome_multiple_dll==0 and enable_plugins==1', {
              'dependencies': [
                '../pdf/pdf.gyp:pdf',
              ],
            }],
            ['cld_version==1', {
              'dependencies': [
                '<(DEPTH)/third_party/cld/cld.gyp:cld',
              ],
            }],
            ['cld_version==2', {
              'dependencies': [
                #'<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
              ],
            }],
            ['OS=="mac" and component!="shared_library"', {
              'includes': [ 'chrome_dll_bundle.gypi' ],
            }],
            ['OS=="mac" and component=="shared_library"', {
              'xcode_settings': { 'OTHER_LDFLAGS': [ '-Wl,-ObjC' ], },
            }],
            ['OS=="mac"', {
              'dependencies': [
                '../components/components.gyp:crash_component',
                '../components/components.gyp:policy',
                '../third_party/crashpad/crashpad/handler/handler.gyp:crashpad_handler',
              ],
              'sources': [
                'app/chrome_crash_reporter_client.cc',
                'app/chrome_crash_reporter_client.h',
                'app/chrome_crash_reporter_client_mac.mm',
              ],
              'xcode_settings': {
                # Define the order of symbols within the framework.  This
                # sets -order_file.
                'ORDER_FILE': 'app/framework.order',
              },
              'include_dirs': [
                '<(grit_out_dir)',
              ],
            }],
            # This step currently fails when using LTO. TODO(pcc): Re-enable.
            ['OS=="macdisable" and use_lto==0 and component=="static_library" and asan==0', {
              'postbuilds': [
                {
                  # This step causes an error to be raised if the .order file
                  # does not account for all global text symbols.  It
                  # validates the completeness of the .order file.
                  'postbuild_name': 'Verify global text symbol order',
                  'variables': {
                    'verify_order_path': 'tools/build/mac/verify_order',
                  },
                  'action': [
                    '<(verify_order_path)',
                    '_ChromeMain',
                    '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
                  ],
                },
              ],
            }],  # OS=="mac"
          ],  # conditions
        },  # target chrome_main_dll
      ],  # targets
    }],  # OS=="mac" or OS=="win"
    ['chrome_multiple_dll', {
      'targets': [
        {
          # GN version: //chrome:chrome_child
          'target_name': 'chrome_child_dll',
          'type': 'shared_library',
          'product_name': 'nw_child',
          'variables': {
            'enable_wexit_time_destructors': 1,
          },
          'dependencies': [
            '<@(chromium_child_dependencies)',
            '../components/components.gyp:browser_watcher_client',
            '../content/content.gyp:content_app_child',
            '../third_party/kasko/kasko.gyp:kasko',
            'chrome_version_resources',
            'policy_path_parser',
          ],
          'defines': [
            'CHROME_MULTIPLE_DLL_CHILD',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',
            'app/chrome_main.cc',
            'app/chrome_main_delegate.cc',
            'app/chrome_main_delegate.h',
          ],
          'conditions': [
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
            ['OS=="win"', {
              'conditions': [
                ['chrome_pgo_phase==1', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '2',
                      'AdditionalOptions': [
                        '/PogoSafeMode',
                      ],
                    },
                  },
                }],
                ['chrome_pgo_phase==2', {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkTimeCodeGeneration': '3',
                    },
                  },
                }],
              ]
            }],
            ['OS=="win" and configuration_policy==1', {
              'dependencies': [
                '<(DEPTH)/components/components.gyp:policy',
              ],
            }],
            ['configuration_policy==1', {
              'dependencies': [
                'policy_path_parser',
              ],
            }],
            ['enable_plugins==1', {
              'dependencies': [
                '../pdf/pdf.gyp:pdf',
              ],
            }],
          ],
        },  # target chrome_child_dll
      ],
    }],
  ],
}

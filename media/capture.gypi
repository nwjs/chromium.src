# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'defines': [
    'MEDIA_IMPLEMENTATION',
  ],
  'variables': {
    # GN version: //media/capture:capture
    'capture_sources': [
      'capture/content/animated_content_sampler.cc',
      'capture/content/animated_content_sampler.h',
      'capture/content/capture_resolution_chooser.cc',
      'capture/content/capture_resolution_chooser.h',
      'capture/content/feedback_signal_accumulator.cc',
      'capture/content/feedback_signal_accumulator.h',
      'capture/content/screen_capture_device_core.cc',
      'capture/content/screen_capture_device_core.h',
      'capture/content/thread_safe_capture_oracle.cc',
      'capture/content/thread_safe_capture_oracle.h',
      'capture/content/smooth_event_sampler.cc',
      'capture/content/smooth_event_sampler.h',
      'capture/content/video_capture_oracle.cc',
      'capture/content/video_capture_oracle.h',
      'capture/device_monitor_mac.h',
      'capture/device_monitor_mac.mm',
      'capture/video/android/video_capture_device_android.cc',
      'capture/video/android/video_capture_device_android.h',
      'capture/video/android/video_capture_device_factory_android.cc',
      'capture/video/android/video_capture_device_factory_android.h',
      'capture/video/fake_video_capture_device.cc',
      'capture/video/fake_video_capture_device.h',
      'capture/video/fake_video_capture_device_factory.cc',
      'capture/video/fake_video_capture_device_factory.h',
      'capture/video/file_video_capture_device.cc',
      'capture/video/file_video_capture_device.h',
      'capture/video/file_video_capture_device_factory.cc',
      'capture/video/file_video_capture_device_factory.h',
      'capture/video/linux/v4l2_capture_delegate.cc',
      'capture/video/linux/v4l2_capture_delegate.h',
      'capture/video/linux/video_capture_device_chromeos.cc',
      'capture/video/linux/video_capture_device_chromeos.h',
      'capture/video/linux/video_capture_device_factory_linux.cc',
      'capture/video/linux/video_capture_device_factory_linux.h',
      'capture/video/linux/video_capture_device_linux.cc',
      'capture/video/linux/video_capture_device_linux.h',
      'capture/video/mac/platform_video_capturing_mac.h',
      'capture/video/mac/video_capture_device_avfoundation_mac.h',
      'capture/video/mac/video_capture_device_avfoundation_mac.mm',
      'capture/video/mac/video_capture_device_decklink_mac.h',
      'capture/video/mac/video_capture_device_decklink_mac.mm',
      'capture/video/mac/video_capture_device_factory_mac.h',
      'capture/video/mac/video_capture_device_factory_mac.mm',
      'capture/video/mac/video_capture_device_mac.h',
      'capture/video/mac/video_capture_device_mac.mm',
      # 'capture/video/mac/video_capture_device_qtkit_mac.h',
      # 'capture/video/mac/video_capture_device_qtkit_mac.mm',
      'capture/video/video_capture_device.cc',
      'capture/video/video_capture_device.h',
      'capture/video/video_capture_device_factory.cc',
      'capture/video/video_capture_device_factory.h',
      'capture/video/video_capture_device_info.cc',
      'capture/video/video_capture_device_info.h',
      'capture/video/win/capability_list_win.cc',
      'capture/video/win/capability_list_win.h',
      'capture/video/win/filter_base_win.cc',
      'capture/video/win/filter_base_win.h',
      'capture/video/win/pin_base_win.cc',
      'capture/video/win/pin_base_win.h',
      'capture/video/win/sink_filter_observer_win.h',
      'capture/video/win/sink_filter_win.cc',
      'capture/video/win/sink_filter_win.h',
      'capture/video/win/sink_input_pin_win.cc',
      'capture/video/win/sink_input_pin_win.h',
      'capture/video/win/video_capture_device_factory_win.cc',
      'capture/video/win/video_capture_device_factory_win.h',
      'capture/video/win/video_capture_device_mf_win.cc',
      'capture/video/win/video_capture_device_mf_win.h',
      'capture/video/win/video_capture_device_win.cc',
      'capture/video/win/video_capture_device_win.h'
    ],

    # GN version: //media/capture:unittests
    'capture_unittests_sources': [
      'capture/content/animated_content_sampler_unittest.cc',
      'capture/content/capture_resolution_chooser_unittest.cc',
      'capture/content/feedback_signal_accumulator_unittest.cc',
      'capture/content/smooth_event_sampler_unittest.cc',
      'capture/content/video_capture_oracle_unittest.cc',
      'capture/video/fake_video_capture_device_unittest.cc',
      'capture/video/video_capture_device_unittest.cc'
    ],

    # The following files lack the correct platform suffixes.
    'conditions': [
      ['OS=="linux" and use_udev==1', {
        'capture_sources': [
          'capture/device_monitor_udev.cc',
          'capture/device_monitor_udev.h',
        ],
      }],

      ['OS=="mac"', {
        'capture_unittests_sources': [
          'capture/video/mac/video_capture_device_factory_mac_unittest.mm',
        ]
      }],

      ['OS=="mac" and nwjs_mas!=1', {
        'capture_sources': [
          'capture/video/mac/video_capture_device_qtkit_mac.h',
          'capture/video/mac/video_capture_device_qtkit_mac.mm',
        ]
      }]
    ],
  },
}

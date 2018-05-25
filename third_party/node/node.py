#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import platform
import subprocess
import sys
import atexit
import os
import signal

is_win = (platform.system() == 'Windows')

def GetBinaryPath():
  return os_path.join(os_path.dirname(__file__), *{
    'Darwin': ('mac', 'node-darwin-x64', 'bin', 'node'),
    'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
    'Windows': ('win', 'node.exe'),
  }[platform.system()])

def RunNode(cmd_parts, stdout=None):
  cmd = " ".join([GetBinaryPath()] + cmd_parts)
  process = None
  if is_win:
    process = subprocess.Popen(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  else:
    process = subprocess.Popen(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, preexec_fn=os.setsid)
  pgid = None
  if not is_win:
    pgid = os.getpgid(process.pid)
  @atexit.register
  def kill_process(*args):
      try:
        if is_win:
          os.popen('taskkill /T /F /PID %d' % process.pid)
        else:
          os.kill(process.pid, signal.SIGTERM)
          os.killpg(pgid, signal.SIGTERM)
        process.wait()
      except OSError:
        pass
  signal.signal(signal.SIGTERM, kill_process)
  stdout, stderr = process.communicate()

  if stderr:
    raise RuntimeError('%s failed: %s' % (cmd, stderr))

  return stdout

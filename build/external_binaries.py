#!/usr/bin/env python

import subprocess
import os
import sys
import shutil

if sys.platform.startswith('darwin'):
  try:
    shutil.rmtree('src/content/nw/external_binaries')
  except OSError:
    pass

if sys.platform.startswith('darwin'):
	subprocess.check_output(['unzip', 'src/build/external_binaries.zip', '-d', 'src/content/nw/external_binaries'], stderr=subprocess.STDOUT, env=os.environ)

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import operator

from appengine_wrappers import GetAppVersion
from object_store_creator import ObjectStoreCreator

class ChannelInfo(object):
  def __init__(self, channel, branch, version):
    self.channel = channel
    self.branch = branch
    self.version = version

class BranchUtility(object):
  def __init__(self, fetch_url, fetcher, object_store=None):
    self._fetch_url = fetch_url
    self._fetcher = fetcher
    if object_store is None:
      object_store = (ObjectStoreCreator.SharedFactory(GetAppVersion())
          .Create(BranchUtility).Create())
    self._object_store = object_store

  @staticmethod
  def GetAllBranchNames():
    return ['stable', 'beta', 'dev', 'trunk']

  def GetAllBranchNumbers(self):
    return [(branch, self.GetBranchNumberForChannelName(branch))
            for branch in BranchUtility.GetAllBranchNames()]

  @staticmethod
  def SplitChannelNameFromPath(path):
    '''Splits the channel name out of |path|, returning the tuple
    (channel_name, real_path). If the channel cannot be determined then returns
    (None, path).
    '''
    if '/' in path:
      first, second = path.split('/', 1)
    else:
      first, second = (path, '')
    if first in BranchUtility.GetAllChannelNames():
      return (first, second)
    return (None, path)

  def GetBranchNumberForChannelName(self, channel_name):
    """Returns the branch number for a channel name.
    """
    if channel_name == 'trunk':
      return 'trunk'

    if data_type == 'branch':
      object_store = self._branch_object_store
    elif data_type == 'version':
      object_store = self._version_object_store

    data = object_store.Get(channel_name).Get()
    if data is not None:
      return data

    try:
      version_json = json.loads(self._fetch_result.Get().content)
    except Exception as e:
      # This can happen if omahaproxy is misbehaving, which we've seen before.
      # Quick hack fix: just serve from trunk until it's fixed.
      logging.error('Failed to fetch or parse branch from omahaproxy: %s! '
                    'Falling back to "trunk".' % e)
      return 'trunk'

    numbers = {}
    for entry in version_json:
      if entry['os'] not in ['win', 'linux', 'mac', 'cros']:
        continue
      for version in entry['versions']:
        if version['channel'] != channel_name:
          continue
        if data_type == 'branch':
          number = version['version'].split('.')[2]
        elif data_type == 'version':
          number = version['version'].split('.')[0]
        if number not in numbers:
          numbers[number] = 0
        else:
          numbers[number] += 1

    sorted_numbers = sorted(numbers.iteritems(),
                            None,
                            operator.itemgetter(1),
                            True)
    object_store.Set(channel_name, int(sorted_numbers[0][0]))
    return int(sorted_numbers[0][0])

  def GetBranchForVersion(self, version):
    '''Returns the most recent branch for a given chrome version number using
    data stored on omahaproxy (see url_constants).
    '''
    if version == 'trunk':
      return 'trunk'

    branch = self._branch_object_store.Get(version).Get()
    if branch is not None:
      return branch

    version_json = json.loads(self._history_result.Get().content)
    for entry in version_json['events']:
      # Here, entry['title'] looks like: '<title> - <version>.##.<branch>.##'
      version_title = entry['title'].split(' - ')[1].split('.')
      if version_title[0] == str(version):
        self._branch_object_store.Set(str(version), version_title[2])
        return int(version_title[2])

    raise ValueError('The branch for %s could not be found.' % version)

  def GetChannelForVersion(self, version):
    '''Returns the name of the development channel corresponding to a given
    version number.
    '''
    for channel_info in self.GetAllChannelInfo():
      if channel_info.channel == 'stable' and version <= channel_info.version:
        return channel_info.channel
      if version == channel_info.version:
        return channel_info.channel

  def GetLatestVersionNumber(self):
    '''Returns the most recent version number found using data stored on
    omahaproxy.
    '''
    latest_version = self._version_object_store.Get('latest').Get()
    if latest_version is not None:
      return latest_version

    version_json = json.loads(self._history_result.Get().content)
    latest_version = 0
    for entry in version_json['events']:
      version_title = entry['title'].split(' - ')[1].split('.')
      version = int(version_title[0])
      if version > latest_version:
        latest_version = version

    self._version_object_store.Set('latest', latest_version)
    return latest_version

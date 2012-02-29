# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import chrome_test, cros_ui, cros_ui_test
from blacklists import blacklist, blacklist_vm

SKIP_DEPS_ARG = 'skip_deps'
GDBSERVER_ARG = 'gdbserver'
GTEST_FILTER_ARG = 'gtest_filter='
PASS_TO_TESTS_ARG = 'pass_to_tests='

GDBSERVER_SETUP_CMD = '/sbin/iptables -A INPUT  -p tcp --dport 1234 -j ACCEPT'
GDBSERVER_STRING = 'gdbserver PORT:1234'

def extract_named_args(name, arguments=[]):
    # return a list of arguments with the named part removed
    return [x[len(name):]
            for x in arguments
            # if the argument starts with the given name
            if name == x[:len(name)]]


def get_binary_prefix(arguments=[]):
    if GDBSERVER_ARG in arguments:
        cros_ui.xsystem(GDBSERVER_SETUP_CMD, ignore_status=True)
        return GDBSERVER_STRING + ' '
    return ''


class desktopui_BrowserTest(chrome_test.ChromeTestBase, cros_ui_test.UITest):
    version = 1
    binary_to_run = 'browser_tests'

    MAX_TESTS_TO_RUN = 100

    def initialize(self, creds='$default', arguments=[]):
        if SKIP_DEPS_ARG in arguments:
            skip_deps = True
        else:
            skip_deps = False
        chrome_test.ChromeTestBase.initialize(self, False, skip_deps=skip_deps)
        cros_ui_test.UITest.initialize(self, creds)

    def cleanup(self):
        cros_ui_test.UITest.cleanup(self)
        chrome_test.ChromeTestBase.cleanup(self)

    def get_tests_to_run(self, group=0, total_groups=1, arguments=[]):
        # Tests specified in arguments override default test behavior
        tests_to_run = extract_named_args(GTEST_FILTER_ARG, arguments)
        if not tests_to_run:
            tests_to_run = self.filter_bad_tests(
                self.generate_test_list(self.binary_to_run, group,
                                        total_groups),
                blacklist + blacklist_vm)
        return tests_to_run

    def split_tests_into_smaller_arrays(self, tests):
        """Returns a list of lists with tests partitioned into smaller arrays.

        This splits the number of tests to smaller arrays that gtest can parse.
        With > 500 tests, we reach the maxiumum character limit for strings and
        our filters get cut off.
        """
        for index in xrange(0, len(tests), self.MAX_TESTS_TO_RUN):
            yield tests[index:index + self.MAX_TESTS_TO_RUN]

    def run_once(self, group=0, total_groups=1, arguments=[]):
        tests_to_run = self.get_tests_to_run(group, total_groups, arguments)
        last_error_message = None
        for tests in self.split_tests_into_smaller_arrays(tests_to_run):
          test_args = '--gtest_filter=%s' % ':'.join(tests)
          args_to_pass = ' --'.join(extract_named_args(PASS_TO_TESTS_ARG,
                                                       arguments))
          if args_to_pass:
              test_args += ' --' + args_to_pass

          try:
              self.run_chrome_test(self.binary_to_run,
                                   test_args,
                                   prefix=get_binary_prefix(arguments))
          except error.TestFail as test_error:
              # We only track the last_error message as we rely on gtest_runnner
              # to parse the failures for us when run. Right now all this
              # suppresses is multiple browser_tests failed messages.
              last_error_message = test_error.message

        if last_error_message:
            raise error.TestError(last_error_message)

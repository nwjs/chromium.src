// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GTEST_PROD_UTIL_H_
#define BASE_GTEST_PROD_UTIL_H_
#pragma once

#include  "testing/gtest/include/gtest/gtest_prod.h"

// This is a wrapper for gtest's FRIEND_TEST macro that friends
// test with all possible prefixes. This is very helpful when changing the test
// prefix, because the friend declarations don't need to be updated.
//
// Example usage:
//
// class MyClass {
//  private:
//   void MyMethod();
//   FRIEND_TEST_ALL_PREFIXES(MyClassTest, MyMethod);
// };

#if defined(GOOGLE_CHROME_BUILD)

#undef FRIEND_TEST
// Provide a no-op that can live in a class definition.
// We can't use an expression, so we define a useless local class name.

#define FRIEND_TEST_PASTE(a, b) a##b

#define FRIEND_TEST_EXPAND_LINE_AND_PASTE(test_name, line)                     \
    FRIEND_TEST_PASTE(BogusFrientTestNamePrefix_##test_name, line)

// One file in CrOS uses a leading "::" in their test case name, so we don't use
// that to create our bogus class name.
#define FRIEND_TEST(test_case_name, test_name)                                 \
    class FRIEND_TEST_EXPAND_LINE_AND_PASTE(test_name, __LINE__) {             \
      int garbage;                                                             \
    }

#endif

#define FRIEND_TEST_ALL_PREFIXES(test_case_name, test_name) \
  FRIEND_TEST(test_case_name, test_name); \
  FRIEND_TEST(test_case_name, DISABLED_##test_name); \
  FRIEND_TEST(test_case_name, FLAKY_##test_name); \
  FRIEND_TEST(test_case_name, FAILS_##test_name)

#endif  // BASE_GTEST_PROD_UTIL_H_

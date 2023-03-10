// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hstring_compare.h"

#include "base/win/hstring_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kTestString12[] = L"12";
constexpr wchar_t kTestString123[] = L"123";
constexpr wchar_t kTestString1234[] = L"1234";

}  // namespace

TEST(HStringCompareTest, FirstStringBeforeSecondString) {
  ASSERT_TRUE(HStringReference::ResolveCoreWinRTStringDelayload());

  const HStringReference string12(kTestString12);
  const HStringReference string123(kTestString123);
  INT32 result;
  HRESULT hr = HStringCompare(string12.Get(), string123.Get(), &result);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(-1, result);
}

TEST(HStringCompareTest, StringsEqual) {
  ASSERT_TRUE(HStringReference::ResolveCoreWinRTStringDelayload());

  const HStringReference string123(kTestString123);
  INT32 result;
  HRESULT hr = HStringCompare(string123.Get(), string123.Get(), &result);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(0, result);
}

TEST(HStringCompareTest, FirstStringAfterSecondString) {
  ASSERT_TRUE(HStringReference::ResolveCoreWinRTStringDelayload());

  const HStringReference string123(kTestString123);
  const HStringReference string1234(kTestString1234);
  INT32 result;
  HRESULT hr = HStringCompare(string1234.Get(), string123.Get(), &result);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(1, result);
}

}  // namespace base::win

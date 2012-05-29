// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_TEST_SUITE_H_
#define CONTENT_TEST_CONTENT_TEST_SUITE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/win/scoped_com_initializer.h"
#include "content/test/content_test_suite_base.h"

namespace content {

class ContentTestSuite : public ContentTestSuiteBase {
 public:
  ContentTestSuite(int argc, char** argv);
  virtual ~ContentTestSuite();

 protected:
  virtual void Initialize() OVERRIDE;

  virtual ContentClient* CreateClientForInitialization() OVERRIDE;

 private:
  base::win::ScopedCOMInitializer com_initializer_;

  DISALLOW_COPY_AND_ASSIGN(ContentTestSuite);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_TEST_SUITE_H_

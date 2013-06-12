// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/memory/scoped_ptr.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/browser_frame_win.h"
#endif

class BrowserViewTest : public BrowserWithTestWindowTest {
 public:
  BrowserViewTest();
  virtual ~BrowserViewTest() {}

  // BrowserWithTestWindowTest overrides:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE;

  void Init();
  BrowserView* browser_view() { return browser_view_; }

 private:
  BrowserView* browser_view_;  // Not owned.
  scoped_ptr<ScopedTestingLocalState> local_state_;
  DISALLOW_COPY_AND_ASSIGN(BrowserViewTest);
};

BrowserViewTest::BrowserViewTest()
    : browser_view_(NULL) {
}

void BrowserViewTest::SetUp() {
  Init();
  // Memory ownership is tricky here. BrowserView has taken ownership of
  // |browser|, so BrowserWithTestWindowTest cannot continue to own it.
  ASSERT_TRUE(release_browser());
}

void BrowserViewTest::TearDown() {
  // Ensure the Browser is reset before BrowserWithTestWindowTest cleans up
  // the Profile.
  browser_view_->GetWidget()->CloseNow();
  browser_view_ = NULL;
  BrowserWithTestWindowTest::TearDown();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  local_state_.reset(NULL);
}

BrowserWindow* BrowserViewTest::CreateBrowserWindow() {
  // Allow BrowserWithTestWindowTest to use Browser to create the default
  // BrowserView and BrowserFrame.
  return NULL;
}

void BrowserViewTest::Init() {
  local_state_.reset(
      new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManager);
#endif
  BrowserWithTestWindowTest::SetUp();
  browser_view_ = static_cast<BrowserView*>(browser()->window());
}

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // The window is owned by the native widget, not the test class.
  EXPECT_FALSE(window());
  // |browser_view_| owns the Browser, not the test class.
  EXPECT_FALSE(browser());
  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  EXPECT_FALSE(browser_view()->IsOffTheRecord());
  EXPECT_EQ(IDR_OTR_ICON, browser_view()->GetOTRIconResourceID());
  EXPECT_FALSE(browser_view()->IsGuestSession());
  EXPECT_FALSE(browser_view()->ShouldShowAvatar());
  EXPECT_TRUE(browser_view()->IsBrowserTypeNormal());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarAnimating());

  // Ensure we've initialized enough to run Layout().
  browser_view()->Layout();
  // TDOO(jamescook): Layout assertions.
}

#if defined(OS_WIN) && !defined(USE_AURA)

// This class provides functionality to test the incognito window/normal window
// switcher button which is added to Windows 8 metro Chrome.
// We create the BrowserView ourselves in the
// BrowserWithTestWindowTest::CreateBrowserWindow function override and add the
// switcher button to the view. We also provide an incognito profile to ensure
// that the switcher button is visible.
class BrowserViewIncognitoSwitcherTest : public BrowserViewTest {
 public:
  // Subclass of BrowserView, which overrides the GetRestoreBounds/IsMaximized
  // functions to return dummy values. This is needed because we create the
  // BrowserView instance ourselves and initialize it with the created Browser
  // instance. These functions get called before the underlying Widget is
  // initialized which causes a crash while dereferencing a null native_widget_
  // pointer in the Widget class.
  class TestBrowserView : public BrowserView {
   public:
    virtual ~TestBrowserView() {}

    virtual gfx::Rect GetRestoredBounds() const OVERRIDE {
      return gfx::Rect();
    }
    virtual bool IsMaximized() const OVERRIDE {
      return false;
    }
  };

  BrowserViewIncognitoSwitcherTest()
      : browser_view_(NULL) {}
  virtual ~BrowserViewIncognitoSwitcherTest() {}

  virtual void SetUp() OVERRIDE {
    Init();
    browser_view_->Init(browser());
    (new BrowserFrame(browser_view_))->InitBrowserFrame();
    browser_view_->SetBounds(gfx::Rect(10, 10, 500, 500));
    browser_view_->Show();
    // Memory ownership is tricky here. BrowserView has taken ownership of
    // |browser|, so BrowserWithTestWindowTest cannot continue to own it.
    ASSERT_TRUE(release_browser());
  }

  virtual void TearDown() OVERRIDE {
    // ok to release the window_ pointer because BrowserViewTest::TearDown
    // deletes the BrowserView instance created.
    release_browser_window();
    BrowserViewTest::TearDown();
    browser_view_ = NULL;
  }

  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE {
    // We need an incognito profile for the window switcher button to be
    // visible.
    // This profile instance is owned by the TestingProfile instance within the
    // BrowserWithTestWindowTest class.
    TestingProfile* incognito_profile = new TestingProfile();
    incognito_profile->set_incognito(true);
    GetProfile()->SetOffTheRecordProfile(incognito_profile);

    browser_view_ = new TestBrowserView();
    browser_view_->SetWindowSwitcherButton(
        MakeWindowSwitcherButton(NULL, false));
    return browser_view_;
  }

 private:
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewIncognitoSwitcherTest);
};

// Test whether the windows incognito/normal browser window switcher button
// is the event handler for a point within its bounds. The event handler for
// a point in the View class is dependent on the order in which children are
// added to it. This test ensures that we don't regress in the window switcher
// functionality when additional children are added to the BrowserView class.
TEST_F(BrowserViewIncognitoSwitcherTest,
       BrowserViewIncognitoSwitcherEventHandlerTest) {
  // |browser_view_| owns the Browser, not the test class.
  EXPECT_FALSE(browser());
  EXPECT_TRUE(browser_view()->browser());
  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  // Validate whether the window switcher button is the target for the position
  // passed in.
  gfx::Point switcher_point(browser_view()->window_switcher_button()->x() + 2,
                            browser_view()->window_switcher_button()->y());
  EXPECT_EQ(browser_view()->GetEventHandlerForPoint(switcher_point),
            browser_view()->window_switcher_button());
}
#endif

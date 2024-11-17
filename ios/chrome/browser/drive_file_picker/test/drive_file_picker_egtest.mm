// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/test/drive_file_picker_app_interface.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Matcher for the drive file picker navigation view controller.
id<GREYMatcher> DriveFilePickerNavigationViewControllerMatcher() {
  return grey_accessibilityID(kDriveFilePickerAccessibilityIdentifier);
}

// Matcher for the confirm button, depending on an enabled/disabled state.
id<GREYMatcher> ConfirmButtonMatcher(BOOL enabled) {
  id<GREYMatcher> enabled_matcher =
      enabled ? grey_enabled() : grey_not(grey_enabled());
  return grey_allOf(
      grey_accessibilityID(kDriveFilePickerConfirmButtonIdentifier),
      enabled_matcher, nil);
}

id<GREYMatcher> SearchBarMatcher() {
  return grey_allOf(grey_accessibilityID(kDriveFilePickerSearchBarIdentifier),
                    grey_interactable(), nil);
}

// Matcher for the sort button, depending on an enabled/disabled state.
id<GREYMatcher> SortButtonMatcher(BOOL enabled) {
  id<GREYMatcher> enabled_matcher =
      enabled ? grey_enabled() : grey_not(grey_enabled());
  return grey_allOf(grey_accessibilityID(kDriveFilePickerSortButtonIdentifier),
                    enabled_matcher, nil);
}

// Matcher for the filter button, depending on an enabled/disabled state.
id<GREYMatcher> FilterButtonMatcher(BOOL enabled) {
  id<GREYMatcher> enabled_matcher =
      enabled ? grey_enabled() : grey_not(grey_enabled());
  return grey_allOf(
      grey_accessibilityID(kDriveFilePickerFilterButtonIdentifier),
      enabled_matcher, nil);
}

// Matcher for the identity button based on a given email.
id<GREYMatcher> IdentityButtonMatcher(NSString* email) {
  return grey_allOf(grey_accessibilityID(kDriveFilePickerIdentityIdentifier),
                    chrome_test_util::ButtonWithAccessibilityLabel(email),
                    grey_enabled(), nil);
}

}  // namespace

@interface DriveFilePickerTestCase : ChromeTestCase
@end

@implementation DriveFilePickerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSChooseFromDrive);
  return config;
}

- (void)tearDown {
  [DriveFilePickerAppInterface hideDriveFilePicker];
  [super tearDown];
}

#pragma mark - Tests

// Tests the presence of the different buttons in the drive file picker.
- (void)testDriveFilePicker {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];
  [[EarlGrey selectElementWithMatcher:ConfirmButtonMatcher(NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SortButtonMatcher(NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:FilterButtonMatcher(NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcher(fakeIdentity.userEmail)]
      assertWithMatcher:grey_notNil()];
}

// Tests identity change from the root.
- (void)testIdentityChangeFromTheRoot {
  FakeSystemIdentity* primaryIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:primaryIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:primaryIdentity];

  FakeSystemIdentity* secondaryIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:secondaryIdentity];

  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];

  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcher(primaryIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     secondaryIdentity.userEmail)] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:IdentityButtonMatcher(
                                          secondaryIdentity.userEmail)]
      assertWithMatcher:grey_notNil()];
}

// Tests identity change when browsing a drive folder.
- (void)testIdentityChangeAfterBrowsing {
  FakeSystemIdentity* primaryIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:primaryIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:primaryIdentity];

  FakeSystemIdentity* secondaryIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:secondaryIdentity];

  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kDriveFilePickerMyDriveItemIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:IdentityButtonMatcher(primaryIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     secondaryIdentity.userEmail)] performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kDriveFilePickerRootTitleAccessibilityIdentifier)];
  [[EarlGrey selectElementWithMatcher:IdentityButtonMatcher(
                                          secondaryIdentity.userEmail)]
      assertWithMatcher:grey_notNil()];
}

// Tests the sort button context menu options are present.
- (void)testSortButtonContextMenuItems {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kDriveFilePickerMyDriveItemIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SortButtonMatcher(YES)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_DRIVE_SORT_BY_NAME)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_DRIVE_SORT_BY_MODIFICATION)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_DRIVE_SORT_BY_OPENING)]
      assertWithMatcher:grey_notNil()];
}

// Tests the filter button context menu options are present.
- (void)testFilterButtonContextMenuItems {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kDriveFilePickerMyDriveItemIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FilterButtonMatcher(YES)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     l10n_util::GetNSString(
                         IDS_IOS_DRIVE_FILE_PICKER_FILTER_MORE_OPTIONS))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     l10n_util::GetNSString(
                         IDS_IOS_DRIVE_FILE_PICKER_FILTER_ARCHIVES))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabel(
              l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_FILTER_AUDIO))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabel(
              l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_FILTER_VIDEOS))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabel(
              l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_FILTER_IMAGES))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContextMenuItemWithAccessibilityLabel(
              l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_FILTER_PDF))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     l10n_util::GetNSString(
                         IDS_IOS_DRIVE_FILE_PICKER_FILTER_ALL_FILES))]
      assertWithMatcher:grey_notNil()];
}

// Tests that toolbar items are still interactable when search bar is focused.
- (void)testToolbarAboveKeyboardDuringSearch {
  // Initialize the Drive file picker.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [DriveFilePickerAppInterface startChoosingFilesInCurrentWebState];
  [DriveFilePickerAppInterface showDriveFilePicker];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      DriveFilePickerNavigationViewControllerMatcher()];
  // Tap the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBarMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Assert that toolbar items are interactable.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kDriveFilePickerSortButtonIdentifier)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kDriveFilePickerIdentityIdentifier)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kDriveFilePickerFilterButtonIdentifier)]
      assertWithMatcher:grey_interactable()];
}

@end

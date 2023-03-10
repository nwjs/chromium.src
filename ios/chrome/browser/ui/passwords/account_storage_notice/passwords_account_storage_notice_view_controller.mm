// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_view_controller.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - PasswordsAccountStorageNoticeViewController

@interface PasswordsAccountStorageNoticeViewController () <
    UIAdaptivePresentationControllerDelegate,
    UITextViewDelegate>

@property(nonatomic, strong, readonly) NSString* accountStoringPasswords;

@end

@implementation PasswordsAccountStorageNoticeViewController

@dynamic actionHandler;

- (instancetype)initWithActionHandler:
                    (id<PasswordsAccountStorageNoticeActionHandler>)
                        actionHandler
              accountStoringPasswords:(NSString*)accountStoringPasswords {
  self = [super initWithNibName:nil bundle:nil];
  if (!self) {
    return nil;
  }

  _accountStoringPasswords = accountStoringPasswords;
  self.actionHandler = actionHandler;
  self.presentationController.delegate = self;
  if (@available(iOS 15, *)) {
    self.modalPresentationStyle = UIModalPresentationPageSheet;
    self.sheetPresentationController.preferredCornerRadius = 20;
    // Both prefersEdgeAttachedInCompactHeight and mediumDetent serve to keep
    // the sheet at half-height.
    self.sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    self.sheetPresentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
    ];
  } else {
    self.modalPresentationStyle = UIModalPresentationFormSheet;
  }
  return self;
}

- (void)viewDidLoad {
  self.image =
      [UIImage imageNamed:@"passwords_account_storage_notice_illustration"];
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.customSpacingAfterImage = 32;
  self.customSpacingBeforeImageIfNoNavigationBar = 16;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_TITLE);
  self.subtitleString = [self subtitleStringWithTag].string;
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_BUTTON_TEXT);

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.delegate = self;
  // Makes the link clickable.
  subtitle.selectable = YES;
  // Inherits the default styling already applied to `subtitle`.
  NSMutableAttributedString* newSubtitle = [[NSMutableAttributedString alloc]
      initWithAttributedString:subtitle.attributedText];
  // The URL value is arbitrary because the click will be handled via the
  // UITextViewDelegate implementation.
  [newSubtitle addAttribute:NSLinkAttributeName
                      value:@""
                      range:[self subtitleStringWithTag].range];
  subtitle.attributedText = newSubtitle;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.actionHandler confirmationAlertSettingsAction];
  // `self` might be deleted.
  return NO;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.actionHandler confirmationAlertSwipeDismissAction];
  // `self` might be deleted.
}

#pragma mark - Private

- (StringWithTag)subtitleStringWithTag {
  StringWithTags stringWithTags = ParseStringWithLinks(l10n_util::GetNSStringF(
      IDS_IOS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_SUBTITLE,
      base::SysNSStringToUTF16(self.accountStoringPasswords)));
  DCHECK_EQ(stringWithTags.ranges.size(), 1u);
  return {stringWithTags.string, stringWithTags.ranges[0]};
}

@end

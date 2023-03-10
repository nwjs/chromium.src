// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_NAMES_H_
#define IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_NAMES_H_

#import <UIKit/UIKit.h>

#import "build/build_config.h"
#import "ios/chrome/browser/ui/icons/buildflags.h"

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

// Custom symbol names.
extern NSString* const kPrivacySymbol;
extern NSString* const kSyncDisabledSymbol;
extern NSString* const kSafetyCheckSymbol;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kGoogleIconSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kArrowClockWiseSymbol;
extern NSString* const kSquareNumberSymbol;
extern NSString* const kTranslateSymbol;
extern NSString* const kCameraSymbol;
extern NSString* const kCameraFillSymbol;
extern NSString* const kPasswordManagerSymbol;
extern NSString* const kPlusCircleFillSymbol;
extern NSString* const kPopupBadgeMinusSymbol;
extern NSString* const kPhotoBadgePlusSymbol;
extern NSString* const kPhotoBadgeMagnifyingglassSymbol;
extern NSString* const kReadingListSymbol;
extern NSString* const kRecentTabsSymbol;
extern NSString* const kLanguageSymbol;
extern NSString* const kPasswordSymbol;
#if !BUILDFLAG(IS_IOS_MACCATALYST)
extern NSString* const kMulticolorPasswordSymbol;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
extern NSString* const kCameraLensSymbol;
extern NSString* const kDownTrendSymbol;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kGoogleShieldSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kShieldSymbol;
extern NSString* const kIncognitoSymbol;

// Custom symbol names which can be configured with a color palette. iOS 15+
// only.
extern NSString* const kIncognitoCircleFillSymbol;
extern NSString* const kNewTabSymbol;

// Custom symbol to replace "palette" symbols on iOS 14. Cannot be used with a
// palette.
extern NSString* const kIncognitoCircleFilliOS14Symbol;

// Default symbol names.
extern NSString* const kSyncEnabledSymbol;
extern NSString* const kDefaultBrowserSymbol;
extern NSString* const kDefaultBrowseriOS14Symbol;
extern NSString* const kDiscoverSymbol;
extern NSString* const kBellSymbol;
extern NSString* const kCachedDataSymbol;
extern NSString* const kAutofillDataSymbol;
extern NSString* const kSecureLocationBarSymbol;
extern NSString* const kNavigateToTabSymbol;
extern NSString* const kRefineQuerySymbol;
extern NSString* const kLinkActionSymbol;
extern NSString* const kQRCodeFinderActionSymbol;
extern NSString* const kNewTabActionSymbol;
extern NSString* const kNewTabCircleActionSymbol;
extern NSString* const kClipboardActionSymbol;
extern NSString* const kDeleteActionSymbol;
extern NSString* const kEditActionSymbol;
extern NSString* const kMarkAsUnreadActionSymbol;
extern NSString* const kMarkAsReadActionSymbol;
extern NSString* const kReadLaterActionSymbol;
extern NSString* const kAddBookmarkActionSymbol;
extern NSString* const kCopyActionSymbol;
extern NSString* const kNewWindowActionSymbol;
extern NSString* const kShowActionSymbol;
extern NSString* const kHideActionSymbol;
extern NSString* const kFindInPageActionSymbol;
extern NSString* const kZoomTextActionSymbol;
extern NSString* const kSaveImageActionSymbol;
extern NSString* const kOpenImageActionSymbol;
extern NSString* const kQRCodeSymbol;
extern NSString* const kPrinterSymbol;
extern NSString* const kCreditCardSymbol;
extern NSString* const kMicrophoneFillSymbol;
extern NSString* const kMicrophoneSymbol;
extern NSString* const kEllipsisCircleFillSymbol;
extern NSString* const kPinSymbol;
extern NSString* const kPinFillSymbol;
extern NSString* const kPinSlashSymbol;
extern NSString* const kSettingsSymbol;
extern NSString* const kSettingsFilledSymbol;
extern NSString* const kShareSymbol;
extern NSString* const kXMarkSymbol;
extern NSString* const kPlusSymbol;
extern NSString* const kSearchSymbol;
extern NSString* const kCheckmarkSymbol;
extern NSString* const kDownloadSymbol;
extern NSString* const kSecureSymbol;
extern NSString* const kWarningSymbol;
extern NSString* const kWarningFillSymbol;
extern NSString* const kHelpSymbol;
extern NSString* const kCheckmarkCircleSymbol;
extern NSString* const kCheckmarkCircleFillSymbol;
extern NSString* const kErrorCircleFillSymbol;
extern NSString* const kTrashSymbol;
extern NSString* const kInfoCircleSymbol;
extern NSString* const kHistorySymbol;
extern NSString* const kCheckmarkSealSymbol;
extern NSString* const kWifiSymbol;
extern NSString* const kBookmarksSymbol;
extern NSString* const kSyncErrorSymbol;
extern NSString* const kMenuSymbol;
extern NSString* const kSortSymbol;
extern NSString* const kBackSymbol;
extern NSString* const kForwardSymbol;
extern NSString* const kPersonFillSymbol;
extern NSString* const kMailFillSymbol;
extern NSString* const kPhoneFillSymbol;
extern NSString* const kDownloadPromptFillSymbol;
extern NSString* const kDownloadPromptFilliOS14Symbol;
extern NSString* const kDownloadDocFillSymbol;
extern NSString* const kOpenInDownloadsSymbol;
extern NSString* const kOpenInDownloadsiOS14Symbol;
extern NSString* const kExternalLinkSymbol;
extern NSString* const kChevronForwardSymbol;
extern NSString* const kGlobeAmericasSymbol;
extern NSString* const kGlobeSymbol;
extern NSString* const kPersonCropCircleSymbol;

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
extern NSString* const kIPhoneSymbol;
extern NSString* const kIPadSymbol;
extern NSString* const kLaptopSymbol;
extern NSString* const kDesktopSymbol;

#endif  // IOS_CHROME_BROWSER_UI_ICONS_SYMBOL_NAMES_H_

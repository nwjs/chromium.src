// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Snapping states for the bottom sheet.
typedef NS_ENUM(NSInteger, BottomSheetSnappingState) {
  BottomSheetSnappingStateCollapsed,
  BottomSheetSnappingStateExpanded,
};

// Animation duration for wallpaper transition.
constexpr CGFloat kBackgroundImageAnimationDuration = 0.25;

// Minimum velocity needed for a user drag to trigger bottom sheet state change.
constexpr CGFloat kMinimumDragVelocityToChangeState = 500;
}  // namespace

@interface NewTabPageRedesignViewController () <UIGestureRecognizerDelegate>

// Properties conformed to by `NewTabPageConsumer`
@property(nonatomic, assign, readwrite) CGFloat collectionShiftingOffset;
@property(nonatomic, assign, readwrite) BOOL scrolledToMinimumHeight;

@end

@implementation NewTabPageRedesignViewController {
  HomeCustomizationImageView* _backgroundImageView;
  UIImage* _backgroundImage;
  HomeCustomizationFramingCoordinates* _framingCoordinates;

  UIVisualEffectView* _bottomSheetView;
  UIView* _dragHandle;
  FakeLocationBarView* _fakeLocationBar;

  NSLayoutConstraint* _bottomSheetTopConstraint;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:@"ntp_background_color"];

  _sheetState = BottomSheetSnappingStateCollapsed;

  _backgroundImageView = [[HomeCustomizationImageView alloc] init];
  _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_backgroundImageView];
  AddSameConstraints(_backgroundImageView, self.view);

  // Create bottom sheet view.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  _bottomSheetView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  _bottomSheetView.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomSheetView.layer.cornerRadius = 24.0;
  _bottomSheetView.layer.masksToBounds = YES;
  [self.view addSubview:_bottomSheetView];

  // Pin leading, trailing, bottom of bottom sheet to root view.
  [NSLayoutConstraint activateConstraints:@[
    [_bottomSheetView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_bottomSheetView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_bottomSheetView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // The top constraint will be updated during panning and layout.
  _bottomSheetTopConstraint = [_bottomSheetView.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:[self collapsedOffset]];
  _bottomSheetTopConstraint.active = YES;

  // Add drag handle to bottom sheet.
  _dragHandle = [[UIView alloc] init];
  _dragHandle.translatesAutoresizingMaskIntoConstraints = NO;
  _dragHandle.backgroundColor = [UIColor colorWithWhite:0.5 alpha:0.3];
  _dragHandle.layer.cornerRadius = 2.5;
  [_bottomSheetView.contentView addSubview:_dragHandle];

  [NSLayoutConstraint activateConstraints:@[
    [_dragHandle.centerXAnchor
        constraintEqualToAnchor:_bottomSheetView.contentView.centerXAnchor],
    [_dragHandle.topAnchor
        constraintEqualToAnchor:_bottomSheetView.contentView.topAnchor
                       constant:8],
    [_dragHandle.widthAnchor constraintEqualToConstant:36],
    [_dragHandle.heightAnchor constraintEqualToConstant:5],
  ]];

  // Add fake location bar.
  _fakeLocationBar = [[FakeLocationBarView alloc] init];
  _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_fakeLocationBar addTarget:self
                       action:@selector(fakeLocationBarTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _fakeLocationBar.isAccessibilityElement = YES;
  _fakeLocationBar.accessibilityIdentifier = @"ntp-redesign-fake-omnibox";
  [_bottomSheetView.contentView addSubview:_fakeLocationBar];

  CGFloat fakeLocationBarHeight = 56.0;
  [NSLayoutConstraint activateConstraints:@[
    [_fakeLocationBar.topAnchor constraintEqualToAnchor:_dragHandle.bottomAnchor
                                               constant:16],
    [_fakeLocationBar.leadingAnchor
        constraintEqualToAnchor:_bottomSheetView.contentView.leadingAnchor
                       constant:16],
    [_fakeLocationBar.trailingAnchor
        constraintEqualToAnchor:_bottomSheetView.contentView.trailingAnchor
                       constant:-16],
    [_fakeLocationBar.heightAnchor
        constraintEqualToConstant:fakeLocationBarHeight],
  ]];
  _fakeLocationBar.layer.cornerRadius = fakeLocationBarHeight / 2.0;

  // Add search icon and placeholder text inside the fake location bar.
  UIImage* searchIconImage =
      DefaultSymbolTemplateWithPointSize(kMagnifyingglassSymbol, 18);
  UIImageView* searchIcon = [[UIImageView alloc] initWithImage:searchIconImage];
  searchIcon.translatesAutoresizingMaskIntoConstraints = NO;
  searchIcon.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  [_fakeLocationBar addSubview:searchIcon];

  UILabel* hintLabel = [[UILabel alloc] init];
  hintLabel.translatesAutoresizingMaskIntoConstraints = NO;
  hintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  hintLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [_fakeLocationBar addSubview:hintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchIcon.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:16],
    [searchIcon.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
    [searchIcon.widthAnchor constraintEqualToConstant:18],
    [searchIcon.heightAnchor constraintEqualToConstant:18],

    [hintLabel.leadingAnchor constraintEqualToAnchor:searchIcon.trailingAnchor
                                            constant:8],
    [hintLabel.trailingAnchor
        constraintEqualToAnchor:_fakeLocationBar.trailingAnchor
                       constant:-16],
    [hintLabel.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
  ]];

  // Set initial accessibility label for fake location bar.
  NSString* askGoogleString = l10n_util::GetNSStringF(
      IDS_OMNIBOX_EMPTY_ASK_HINT_WITH_DSE_NAME, std::u16string(u"Google"));
  _fakeLocationBar.accessibilityLabel = askGoogleString;
  hintLabel.text = askGoogleString;

  [_fakeLocationBar applyBackgroundTheme];
  [_fakeLocationBar updateColorsWithProgress:0.0 colorPalette:nil];

  // Add pan gesture recognizer.
  UIPanGestureRecognizer* panGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  panGesture.delegate = self;
  [_bottomSheetView addGestureRecognizer:panGesture];

  if (_searchEngineLogoView) {
    [self addSearchEngineLogoView];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (!CGSizeEqualToSize(_lastSize, self.view.bounds.size)) {
    _lastSize = self.view.bounds.size;
    [self updateBottomSheetPositionAnimated:NO];
  }
}

- (void)invalidate {
  self.mutator = nil;
  self.searchEngineLogoView = nil;
  self.NTPContentDelegate = nil;
}

#pragma mark - Public

- (void)focusOmnibox {
  [self.NTPContentDelegate focusOmnibox];
}

#pragma mark - Action Targets

- (void)fakeLocationBarTapped {
  [self focusOmnibox];
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  return self.view.bounds.size.height * 0.55;
}

- (CGFloat)expandedOffset {
  return 20.0 + self.view.safeAreaInsets.top;
}

#pragma mark - Bottom Sheet Snapping and Panning

- (void)updateBottomSheetPositionAnimated:(BOOL)animated {
  CGFloat targetConstant = (_sheetState == BottomSheetSnappingStateCollapsed)
                               ? [self collapsedOffset]
                               : [self expandedOffset];

  if (!animated) {
    _bottomSheetTopConstraint.constant = targetConstant;
  } else {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.3
                          delay:0
         usingSpringWithDamping:0.8
          initialSpringVelocity:0.5
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       NewTabPageRedesignViewController* strongSelf = weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       strongSelf->_bottomSheetTopConstraint.constant =
                           targetConstant;
                       [strongSelf.view layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  CGPoint translation = [gesture translationInView:self.view];
  CGPoint velocity = [gesture velocityInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstant = _bottomSheetTopConstraint.constant;
  }

  CGFloat newConstant = _initialConstant + translation.y;
  CGFloat minOffset = [self expandedOffset];
  CGFloat maxOffset = [self collapsedOffset];

  if (newConstant < minOffset) {
    newConstant = minOffset;
  } else if (newConstant > maxOffset) {
    newConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = newConstant;

  if (gesture.state == UIGestureRecognizerStateEnded) {
    CGFloat collapsedOffset = [self collapsedOffset];
    CGFloat expandedOffset = [self expandedOffset];
    CGFloat midPoint = (collapsedOffset + expandedOffset) / 2.0;

    BottomSheetSnappingState targetState;
    if (velocity.y > kMinimumDragVelocityToChangeState) {
      targetState = BottomSheetSnappingStateCollapsed;
    } else if (velocity.y < -kMinimumDragVelocityToChangeState) {
      targetState = BottomSheetSnappingStateExpanded;
    } else {
      if (newConstant > midPoint) {
        targetState = BottomSheetSnappingStateCollapsed;
      } else {
        targetState = BottomSheetSnappingStateExpanded;
      }
    }

    _sheetState = targetState;
    [self updateBottomSheetPositionAnimated:YES];
  }
}

#pragma mark - NewTabPageConsumer

- (void)omniboxDidBecomeFirstResponder {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)omniboxWillResignFirstResponder {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)omniboxDidEndEditing {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)restoreScrollPosition:(CGFloat)scrollPosition {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)restoreScrollPositionToTopOfFeed {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (CGFloat)heightAboveFeed {
  return 0.0;
}

- (CGFloat)scrollPosition {
  return 0.0;
}

- (CGFloat)pinnedOffsetY {
  return 0.0;
}

- (void)setBackgroundImage:(UIImage*)backgroundImage
        framingCoordinates:
            (HomeCustomizationFramingCoordinates*)framingCoordinates {
  _backgroundImage = backgroundImage;
  _framingCoordinates = framingCoordinates;

  __weak HomeCustomizationImageView* view = _backgroundImageView;
  __weak UIImage* image = _backgroundImage;
  __weak HomeCustomizationFramingCoordinates* weakFramingCoordinates =
      _framingCoordinates;
  [UIView transitionWithView:view
                    duration:kBackgroundImageAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [view setImage:image
                        framingCoordinates:weakFramingCoordinates];
                  }
                  completion:nil];
}

- (void)setAIMAllowed:(BOOL)allowed {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

#pragma mark - Setters

- (void)setSearchEngineLogoView:(UIView*)searchEngineLogoView {
  if (_searchEngineLogoView == searchEngineLogoView) {
    return;
  }
  if (_searchEngineLogoView && _searchEngineLogoView.superview) {
    [_searchEngineLogoView removeFromSuperview];
  }
  _searchEngineLogoView = searchEngineLogoView;
  if (_searchEngineLogoView && _bottomSheetView) {
    [self addSearchEngineLogoView];
  }
}

#pragma mark - Private

- (void)addSearchEngineLogoView {
  if (!_searchEngineLogoView || !_bottomSheetView) {
    return;
  }
  [self.view insertSubview:_searchEngineLogoView belowSubview:_bottomSheetView];
  _searchEngineLogoView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_searchEngineLogoView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_searchEngineLogoView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:40],
    [_searchEngineLogoView.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor
                     multiplier:0.8],
    [_searchEngineLogoView.heightAnchor constraintEqualToConstant:120]
  ]];
}

#pragma mark - SearchEngineLogoConsumer

- (void)searchEngineLogoStateDidChange:(SearchEngineLogoState)logoState {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

#pragma mark - NewTabPageHeaderViewDelegate

- (BOOL)shouldPinFakeOmnibox {
  return NO;
}

- (void)didChangeOmniboxPosition:(NewTabPageHeaderView*)headerView {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

@end

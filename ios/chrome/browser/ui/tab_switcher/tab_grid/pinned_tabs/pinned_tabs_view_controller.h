// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"

@class PinnedTabsViewController;
@protocol GridImageDataSource;
@protocol TabCollectionDragDropHandler;
@protocol TabContextMenuProvider;

// Protocol used to relay relevant user interactions from the
// PinnedTabsViewController.
@protocol PinnedTabsViewControllerDelegate

// Tells the delegate that the item with `itemID` in `pinnedTabsViewController`
// was selected.
- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
             didSelectItemWithID:(NSString*)itemID;

// Tells the delegate that the the number of items in `pinnedTabsViewController`
// changed to `count`.
- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
              didChangeItemCount:(NSUInteger)count;

// Tells the delegate that the `pinnedTabsViewController` is hidden.
- (void)pinnedTabsViewControllerDidHide;

@end

// UICollectionViewController used to display pinned tabs.
@interface PinnedTabsViewController
    : UICollectionViewController <TabCollectionConsumer>

// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;

// Delegate used to to relay relevant user interactions.
@property(nonatomic, weak) id<PinnedTabsViewControllerDelegate> delegate;

// Provides context menus.
@property(nonatomic, weak) id<TabContextMenuProvider> menuProvider;

// Handles drag and drop interactions that involved the model layer.
@property(nonatomic, weak) id<TabCollectionDragDropHandler> dragDropHandler;

// Tracks if a drop animation is in progress.
@property(nonatomic, assign) BOOL dropAnimationInProgress;

// Updates the view when starting or ending a drag action.
- (void)dragSessionEnabled:(BOOL)enabled;

// Makes the pinned tabs view available. The pinned view should only be
// available when the regular tabs grid is displayed.
- (void)pinnedTabsAvailable:(BOOL)available;

// Updates the view when the drop animation did end.
- (void)dropAnimationDidEnd;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout
    NS_UNAVAILABLE;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentWillDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

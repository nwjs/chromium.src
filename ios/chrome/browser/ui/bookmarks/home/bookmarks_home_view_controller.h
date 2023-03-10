// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <set>
#include <vector>

#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol ApplicationCommands;
@class BookmarksHomeViewController;
class Browser;
namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks
class GURL;
@protocol SnackbarCommands;

@protocol BookmarksHomeViewControllerDelegate
// The view controller wants to be dismissed. If `urls` is not empty, then
// the user has selected to navigate to those URLs in the current tab mode.
- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarksHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls;

// The view controller wants to be dismissed. If `urls` is not empty, then
// the user has selected to navigate to those URLs with specified tab mode.
- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarksHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab;

@end

// Class to navigate the bookmark hierarchy.
@interface BookmarksHomeViewController
    : ChromeTableViewController <KeyCommandActions,
                                 UIAdaptivePresentationControllerDelegate>

// Delegate for presenters. Note that this delegate is currently being set only
// in case of handset, and not tablet. In the future it will be used by both
// cases.
@property(nonatomic, weak) id<BookmarksHomeViewControllerDelegate> homeDelegate;

// Handler for Application Commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// Handler for Snackbar Commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Initializers.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)tableViewStyle NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

// Setter to set _rootNode value.
- (void)setRootNode:(const bookmarks::BookmarkNode*)rootNode;

// Returns an array of BookmarksHomeViewControllers, one per BookmarkNode in the
// path from this view controller's node to the latest cached node (as
// determined by BookmarkPathCache).  Includes `self` as the first element of
// the returned array.  Sets the cached scroll position for the last element of
// the returned array, if appropriate.
- (NSArray<BookmarksHomeViewController*>*)cachedViewControllerStack;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_VIEW_CONTROLLER_H_

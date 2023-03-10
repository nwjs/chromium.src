// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabContextMenuHelper ()

@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;
@property(nonatomic, assign) BOOL incognito;

@end

@implementation TabContextMenuHelper

#pragma mark - TabContextMenuProvider

- (instancetype)initWithBrowser:(Browser*)browser
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _contextMenuDelegate = tabContextMenuDelegate;
    _incognito = _browser->GetBrowserState()->IsOffTheRecord();
  }
  return self;
}

- (void)dealloc {
  self.browser = nullptr;
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabCell:(TabCell*)cell
                          menuScenario:(MenuScenarioHistogram)scenario {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        TabContextMenuHelper* strongSelf = weakSelf;
        if (!strongSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        NSArray<UIMenuElement*>* menuElements =
            [strongSelf menuElementsForTabCell:cell menuScenario:scenario];
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (NSArray<UIMenuElement*>*)menuElementsForTabCell:(TabCell*)cell
                                      menuScenario:
                                          (MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  const BOOL pinned = scenario == MenuScenarioHistogram::kPinnedTabsEntry;

  TabItem* item = [self tabItemForIdentifier:cell.itemIdentifier pinned:pinned];

  if (!item) {
    return @[];
  }

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

    if (IsPinnedTabsEnabled()) {
      if (pinned) {
        if ([self.contextMenuDelegate
                respondsToSelector:@selector(unpinTabWithIdentifier:)]) {
          [menuElements addObject:[actionFactory actionToUnpinTabWithBlock:^{
                          [self.contextMenuDelegate
                              unpinTabWithIdentifier:cell.itemIdentifier];
                        }]];
        }
      } else {
        if ([self.contextMenuDelegate
                respondsToSelector:@selector(pinTabWithIdentifier:)]) {
          [menuElements addObject:[actionFactory actionToPinTabWithBlock:^{
                          [self.contextMenuDelegate
                              pinTabWithIdentifier:cell.itemIdentifier];
                        }]];
        }
      }
    }

    if (!IsURLNewTabPage(item.URL)) {
      if ([self.contextMenuDelegate respondsToSelector:@selector
                                    (shareURL:title:scenario:fromView:)]) {
        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [self.contextMenuDelegate
                            shareURL:item.URL
                               title:item.title
                            scenario:ActivityScenario::TabGridItem
                            fromView:cell];
                      }]];
      }

      if (item.URL.SchemeIsHTTPOrHTTPS() &&
          [self.contextMenuDelegate
              respondsToSelector:@selector(addToReadingListURL:title:)]) {
        [menuElements
            addObject:[actionFactory actionToAddToReadingListWithBlock:^{
              [self.contextMenuDelegate addToReadingListURL:item.URL
                                                      title:item.title];
            }]];
      }

      UIAction* bookmarkAction;
      const BOOL currentlyBookmarked = [self isTabItemBookmarked:item];
      if (currentlyBookmarked) {
        if ([self.contextMenuDelegate
                respondsToSelector:@selector(editBookmarkWithURL:)]) {
          bookmarkAction = [actionFactory actionToEditBookmarkWithBlock:^{
            [self.contextMenuDelegate editBookmarkWithURL:item.URL];
          }];
        }
      } else {
        if ([self.contextMenuDelegate
                respondsToSelector:@selector(bookmarkURL:title:)]) {
          bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
            [self.contextMenuDelegate bookmarkURL:item.URL title:item.title];
          }];
        }
      }
      // Bookmarking can be disabled from prefs (from an enterprise policy),
      // if that's the case grey out the option in the menu.
      if (self.browser) {
        BOOL isEditBookmarksEnabled =
            self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
                bookmarks::prefs::kEditBookmarksEnabled);
        if (!isEditBookmarksEnabled && bookmarkAction) {
          bookmarkAction.attributes = UIMenuElementAttributesDisabled;
        }
        if (bookmarkAction) {
          [menuElements addObject:bookmarkAction];
        }
      }
    }

  // Thumb strip, pinned tabs and search results menus don't support tab
  // selection.
  BOOL scenarioDisablesSelection =
      scenario == MenuScenarioHistogram::kTabGridSearchResult ||
      scenario == MenuScenarioHistogram::kPinnedTabsEntry ||
      scenario == MenuScenarioHistogram::kThumbStrip;
  if (!scenarioDisablesSelection &&
      [self.contextMenuDelegate respondsToSelector:@selector(selectTabs)]) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [self.contextMenuDelegate selectTabs];
                  }]];
  }

  if ([self.contextMenuDelegate respondsToSelector:@selector
                                (closeTabWithIdentifier:incognito:pinned:)]) {
    [menuElements addObject:[actionFactory actionToCloseTabWithBlock:^{
                    [self.contextMenuDelegate
                        closeTabWithIdentifier:cell.itemIdentifier
                                     incognito:self.incognito
                                        pinned:pinned];
                  }]];
  }
  return menuElements;
}

#pragma mark - Private

// Returns `YES` if the tab `item` is already bookmarked.
- (BOOL)isTabItemBookmarked:(TabItem*)item {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          _browser->GetBrowserState());
  return item && bookmarkModel &&
         bookmarkModel->GetMostRecentlyAddedUserNodeForURL(item.URL);
}

// Returns the TabItem object representing the tab with `identifier.
// `pinned` tracks the pinned state of
// the tab we are looking for.
- (TabItem*)tabItemForIdentifier:(NSString*)identifier pinned:(BOOL)pinned {
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(_browser->GetBrowserState());
  std::set<Browser*> browsers = _incognito ? browserList->AllIncognitoBrowsers()
                                           : browserList->AllRegularBrowsers();
  for (Browser* browser : browsers) {
    WebStateList* webStateList = browser->GetWebStateList();
    TabItem* item = GetTabItem(webStateList, identifier, /*pinned=*/pinned);
    if (item != nil) {
      return item;
    }
  }
  return nil;
}

@end

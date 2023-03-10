// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains business logic for power bookmarks side panel content.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';

import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo} from './shopping_list.mojom-webui.js';

export interface Label {
  label: string;
  icon: string;
  active: boolean;
}

interface PowerBookmarksDelegate {
  setCurrentUrl(url: string|undefined): void;
  setCompactDescription(
      bookmark: chrome.bookmarks.BookmarkTreeNode, description: string): void;
  setExpandedDescription(
      bookmark: chrome.bookmarks.BookmarkTreeNode, description: string): void;
  onBookmarksLoaded(): void;
  onBookmarkChanged(id: string, changedInfo: chrome.bookmarks.ChangeInfo): void;
  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode): void;
  onBookmarkMoved(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      oldParent: chrome.bookmarks.BookmarkTreeNode,
      newParent: chrome.bookmarks.BookmarkTreeNode): void;
  onBookmarkRemoved(bookmark: chrome.bookmarks.BookmarkTreeNode): void;
}

export class PowerBookmarksService {
  private delegate_: PowerBookmarksDelegate;
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private listeners_ = new Map<string, Function>();
  private folders_: chrome.bookmarks.BookmarkTreeNode[] = [];
  private productInfos_ = new Map<string, BookmarkProductInfo>();
  private shoppingListenerIds_: number[] = [];

  constructor(delegate: PowerBookmarksDelegate) {
    this.delegate_ = delegate;
  }

  /**
   * Creates listeners for all relevant bookmark and shopping information.
   * Invoke during setup.
   */
  startListening() {
    this.bookmarksApi_.getActiveUrl().then(
        url => this.delegate_.setCurrentUrl(url));
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;
      this.folders_.forEach(bookmark => {
        this.findBookmarkDescriptions_(bookmark, true);
      });
      this.addListener_(
          'onChanged',
          (id: string, changedInfo: chrome.bookmarks.ChangeInfo) =>
              this.onChanged_(id, changedInfo));
      this.addListener_(
          'onCreated',
          (_id: string, node: chrome.bookmarks.BookmarkTreeNode) =>
              this.onCreated_(node));
      this.addListener_(
          'onMoved',
          (_id: string, movedInfo: chrome.bookmarks.MoveInfo) =>
              this.onMoved_(movedInfo));
      this.addListener_('onRemoved', (id: string) => this.onRemoved_(id));
      this.addListener_('onTabActivated', (_info: chrome.tabs.ActiveInfo) => {
        this.bookmarksApi_.getActiveUrl().then(
            url => this.delegate_.setCurrentUrl(url));
      });
      this.addListener_(
          'onTabUpdated',
          (_tabId: number, _changeInfo: object, tab: chrome.tabs.Tab) => {
            if (tab.active) {
              this.delegate_.setCurrentUrl(tab.url);
            }
          });
      this.delegate_.onBookmarksLoaded();
    });
    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product =>
              this.productInfos_.set(product.bookmarkId.toString(), product));
      if (this.productInfos_.size > 0) {
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
      }
    });
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            () => this.onBookmarkPriceTracked_()),
    );
  }

  /**
   * Cleans up any listeners created by the startListening method.
   * Invoke during teardown.
   */
  stopListening() {
    for (const [eventName, callback] of this.listeners_.entries()) {
      this.bookmarksApi_.callbackRouter[eventName]!.removeListener(callback);
    }
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));
  }

  /**
   * Returns a list of all root bookmark folders.
   */
  getFolders() {
    return this.folders_;
  }

  /**
   * Returns a list of all bookmarks defaulted to if no filter criteria are
   * provided.
   */
  getTopLevelBookmarks() {
    return this.filterBookmarks(undefined, 0, undefined, []);
  }

  /**
   * Returns a list of bookmarks and folders filtered by the provided criteria.
   */
  filterBookmarks(
      activeFolder: chrome.bookmarks.BookmarkTreeNode|undefined,
      activeSortIndex: number, searchQuery: string|undefined,
      labels: Label[]): chrome.bookmarks.BookmarkTreeNode[] {
    let shownBookmarks;
    if (activeFolder) {
      shownBookmarks = activeFolder.children!.slice();
    } else {
      let topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[] = [];
      this.folders_.forEach(
          folder => topLevelBookmarks = topLevelBookmarks.concat(
              (folder.id === loadTimeData.getString('bookmarksBarId')) ?
                  [folder] :
                  folder.children!));
      shownBookmarks = topLevelBookmarks;
    }
    if (searchQuery) {
      shownBookmarks = this.applySearchQuery_(searchQuery!, shownBookmarks);
    }
    shownBookmarks = shownBookmarks.filter(
        (b: chrome.bookmarks.BookmarkTreeNode) =>
            this.nodeMatchesContentFilters_(b, labels));
    const sortChangedPosition =
        this.sortBookmarks(shownBookmarks, activeSortIndex);
    if (sortChangedPosition) {
      return shownBookmarks.slice();
    } else {
      return shownBookmarks;
    }
  }

  /**
   * Apply the current active sort type to the given bookmarks list. Returns
   * true if any elements in the list changed position.
   */
  sortBookmarks(
      bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      activeSortIndex: number): boolean {
    let changedPosition = false;
    bookmarks.sort(function(
        a: chrome.bookmarks.BookmarkTreeNode,
        b: chrome.bookmarks.BookmarkTreeNode) {
      // Always sort by folders first
      if (!a.url && b.url) {
        return -1;
      } else if (a.url && !b.url) {
        changedPosition = true;
        return 1;
      } else {
        let toReturn;
        if (activeSortIndex === 0) {
          // Newest first
          toReturn = b.dateAdded! - a.dateAdded!;
        } else if (activeSortIndex === 1) {
          // Oldest first
          toReturn = a.dateAdded! - b.dateAdded!;
        } else if (activeSortIndex === 2) {
          // Alphabetical
          toReturn = a.title!.localeCompare(b.title);
        } else {
          // Reverse alphabetical
          toReturn = b.title!.localeCompare(a.title);
        }
        if (toReturn > 0) {
          changedPosition = true;
        }
        return toReturn;
      }
    });
    return changedPosition;
  }

  /**
   * Returns the BookmarkTreeNode with the given id, or undefined if one does
   * not exist.
   */
  findBookmarkWithId(id: string): chrome.bookmarks.BookmarkTreeNode|undefined {
    const path = this.findPathToId_(id);
    if (path) {
      return path[path.length - 1];
    }
    return undefined;
  }

  /**
   * Returns true if the given url is not already present in the given folder.
   * If the folder is undefined, will default to the "Other Bookmarks" folder.
   */
  canAddUrl(
      url: string|undefined,
      folder: chrome.bookmarks.BookmarkTreeNode|undefined): boolean {
    if (!folder) {
      folder =
          this.findBookmarkWithId(loadTimeData.getString('otherBookmarksId'));
      if (!folder) {
        return false;
      }
    }
    return folder.children!.findIndex(b => b.url === url) === -1;
  }

  /**
   * Returns shopping information associated with the given bookmark.
   */
  getProductInfo(bookmark: chrome.bookmarks.BookmarkTreeNode):
      BookmarkProductInfo|undefined {
    return this.productInfos_.get(bookmark.id);
  }

  private addListener_(eventName: string, callback: Function): void {
    this.bookmarksApi_.callbackRouter[eventName]!.addListener(callback);
    this.listeners_.set(eventName, callback);
  }

  private onChanged_(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    const bookmark = this.findBookmarkWithId(id)!;
    Object.assign(bookmark, changedInfo);
    this.findBookmarkDescriptions_(bookmark, false);
    this.delegate_.onBookmarkChanged(id, changedInfo);
  }

  private onCreated_(node: chrome.bookmarks.BookmarkTreeNode) {
    const parent = this.findBookmarkWithId(node.parentId as string)!;
    if (!node.url && !node.children) {
      // Newly created folders in this session may not have an array of
      // children yet, so create an empty one.
      node.children = [];
    }
    parent.children!.splice(node.index!, 0, node);
    this.delegate_.onBookmarkCreated(node, parent);
    this.findBookmarkDescriptions_(parent, false);
    this.findBookmarkDescriptions_(node, false);
  }

  private onMoved_(movedInfo: chrome.bookmarks.MoveInfo) {
    // Remove node from oldParent at oldIndex.
    const oldParent = this.findBookmarkWithId(movedInfo.oldParentId)!;
    const movedNode = oldParent!.children![movedInfo.oldIndex]!;
    Object.assign(
        movedNode, {index: movedInfo.index, parentId: movedInfo.parentId});
    oldParent.children!.splice(movedInfo.oldIndex, 1);

    // Add the node to the new parent at index.
    const newParent = this.findBookmarkWithId(movedInfo.parentId)!;
    if (!newParent.children) {
      newParent.children = [];
    }
    newParent.children!.splice(movedInfo.index, 0, movedNode);
    this.delegate_.onBookmarkMoved(movedNode, oldParent, newParent);

    if (movedInfo.oldParentId !== movedInfo.parentId) {
      this.findBookmarkDescriptions_(oldParent, false);
      this.findBookmarkDescriptions_(newParent, false);
    }
  }

  private onRemoved_(id: string) {
    const oldPath = this.findPathToId_(id);
    const removedNode = oldPath.pop()!;
    const oldParent = oldPath[oldPath.length - 1]!;
    oldParent.children!.splice(oldParent.children!.indexOf(removedNode), 1);
    this.productInfos_.delete(id);
    this.delegate_.onBookmarkRemoved(removedNode);
    this.findBookmarkDescriptions_(oldParent, false);
  }

  /**
   * Finds the node within all bookmarks and returns the path to the node in
   * the tree.
   */
  private findPathToId_(id: string): chrome.bookmarks.BookmarkTreeNode[] {
    const path: chrome.bookmarks.BookmarkTreeNode[] = [];

    function findPathByIdInternal(
        id: string, node: chrome.bookmarks.BookmarkTreeNode) {
      if (node.id === id) {
        path.push(node);
        return true;
      }

      if (!node.children) {
        return false;
      }

      path.push(node);
      const foundInChildren =
          node.children.some(child => findPathByIdInternal(id, child));
      if (!foundInChildren) {
        path.pop();
      }

      return foundInChildren;
    }

    this.folders_.some(bookmark => findPathByIdInternal(id, bookmark));
    return path;
  }

  /**
   * Assigns a text description for the given bookmark, to be displayed
   * following the bookmark title. Also assigns a description to all
   * descendants if recurse is true.
   */
  private findBookmarkDescriptions_(
      bookmark: chrome.bookmarks.BookmarkTreeNode, recurse: boolean) {
    if (bookmark.url) {
      const url = new URL(bookmark.url);
      // Show chrome:// if it's a chrome internal url
      if (url.protocol === 'chrome:') {
        this.delegate_.setExpandedDescription(
            bookmark, 'chrome://' + url.hostname);
      } else {
        this.delegate_.setExpandedDescription(bookmark, url.hostname);
      }
    } else {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'bookmarkFolderChildCount',
              bookmark.children ? bookmark.children.length : 0)
          .then(pluralString => {
            this.delegate_.setCompactDescription(bookmark, pluralString);
          });
    }
    if (recurse && bookmark.children) {
      bookmark.children.forEach(
          child => this.findBookmarkDescriptions_(child, recurse));
    }
  }

  // Return an array that includes folder and all its descendants.
  private expandFolder_(folder: chrome.bookmarks.BookmarkTreeNode):
      chrome.bookmarks.BookmarkTreeNode[] {
    let expanded: chrome.bookmarks.BookmarkTreeNode[] = [folder];
    if (folder.children) {
      folder.children.forEach((child: chrome.bookmarks.BookmarkTreeNode) => {
        expanded = expanded.concat(this.expandFolder_(child));
      });
    }
    return expanded;
  }

  private applySearchQuery_(
      searchQuery: string,
      shownBookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    let searchSpace: chrome.bookmarks.BookmarkTreeNode[] = [];
    // Search space should include all descendants of the shown bookmarks, in
    // addition to the shown bookmarks themselves.
    shownBookmarks.forEach((bookmark: chrome.bookmarks.BookmarkTreeNode) => {
      searchSpace = searchSpace.concat(this.expandFolder_(bookmark));
    });
    return searchSpace.filter(
        (bookmark: chrome.bookmarks.BookmarkTreeNode) =>
            (bookmark.title &&
             bookmark.title.toLocaleLowerCase().includes(searchQuery)) ||
            (bookmark.url &&
             bookmark.url.toLocaleLowerCase().includes(searchQuery)));
  }

  private nodeMatchesContentFilters_(
      bookmark: chrome.bookmarks.BookmarkTreeNode, labels: Label[]): boolean {
    // Price tracking label
    if (labels[0] && labels[0]!.active &&
        !this.productInfos_.has(bookmark.id)) {
      return false;
    }
    return true;
  }

  private onBookmarkPriceTracked_() {
    if (this.productInfos_.size > 0) {
      return;
    }
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
  }
}

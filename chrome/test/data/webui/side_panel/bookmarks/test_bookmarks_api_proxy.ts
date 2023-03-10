// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionSource} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxy} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  private folders_: chrome.bookmarks.BookmarkTreeNode[] = [];
  callbackRouter: {
    onChanged: FakeChromeEvent,
    onChildrenReordered: FakeChromeEvent,
    onCreated: FakeChromeEvent,
    onMoved: FakeChromeEvent,
    onRemoved: FakeChromeEvent,
    onTabActivated: FakeChromeEvent,
    onTabUpdated: FakeChromeEvent,
  };

  constructor() {
    super([
      'getActiveUrl',
      'getFolders',
      'bookmarkCurrentTabInFolder',
      'openBookmark',
      'cutBookmark',
      'contextMenuOpenBookmarkInNewTab',
      'contextMenuOpenBookmarkInNewWindow',
      'contextMenuOpenBookmarkInIncognitoWindow',
      'contextMenuAddToBookmarksBar',
      'contextMenuRemoveFromBookmarksBar',
      'contextMenuDelete',
      'copyBookmark',
      'createFolder',
      'deleteBookmarks',
      'pasteToBookmark',
      'renameBookmark',
      'showContextMenu',
      'showUi',
      'undo',
    ]);

    this.callbackRouter = {
      onChanged: new FakeChromeEvent(),
      onChildrenReordered: new FakeChromeEvent(),
      onCreated: new FakeChromeEvent(),
      onMoved: new FakeChromeEvent(),
      onRemoved: new FakeChromeEvent(),
      onTabActivated: new FakeChromeEvent(),
      onTabUpdated: new FakeChromeEvent(),
    };
  }

  getActiveUrl() {
    this.methodCalled('getActiveUrl');
    return Promise.resolve('http://www.test.com');
  }

  getFolders() {
    this.methodCalled('getFolders');
    return Promise.resolve(this.folders_);
  }

  bookmarkCurrentTabInFolder() {
    this.methodCalled('bookmarkCurrentTabInFolder');
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.methodCalled('openBookmark', id, depth, clickModifiers, source);
  }

  setFolders(folders: chrome.bookmarks.BookmarkTreeNode[]) {
    this.folders_ = folders;
  }

  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInNewTab', ids, source);
  }

  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInNewWindow', ids, source);
  }

  contextMenuOpenBookmarkInIncognitoWindow(
      ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInIncognitoWindow', ids, source);
  }

  contextMenuAddToBookmarksBar(id: string, source: ActionSource) {
    this.methodCalled('contextMenuAddToBookmarksBar', id, source);
  }

  contextMenuRemoveFromBookmarksBar(id: string, source: ActionSource) {
    this.methodCalled('contextMenuRemoveFromBookmarksBar', id, source);
  }

  contextMenuDelete(id: string, source: ActionSource) {
    this.methodCalled('contextMenuDelete', id, source);
  }

  copyBookmark(id: string): Promise<void> {
    this.methodCalled('copyBookmark', id);
    return Promise.resolve();
  }

  createFolder(parentId: string, title: string) {
    this.methodCalled('createFolder', parentId, title);
  }

  cutBookmark(id: string) {
    this.methodCalled('cutBookmark', id);
  }

  deleteBookmarks(ids: string[]) {
    this.methodCalled('deleteBookmarks', ids);
    return Promise.resolve();
  }

  pasteToBookmark(parentId: string, destinationId?: string): Promise<void> {
    this.methodCalled('pasteToBookmark', parentId, destinationId);
    return Promise.resolve();
  }

  renameBookmark(id: string, title: string) {
    this.methodCalled('renameBookmark', id, title);
  }

  showContextMenu(id: string, x: number, y: number, source: ActionSource) {
    this.methodCalled('showContextMenu', id, x, y, source);
  }

  showUi() {
    this.methodCalled('showUi');
  }

  undo() {
    this.methodCalled('undo');
  }
}

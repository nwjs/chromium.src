// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Implements some actions for the Feed */
public class CreatorActionDelegateImpl implements FeedActionDelegate {
    private static final String TAG = "Cormorant";

    private final Context mActivityContext;
    private final Profile mProfile;
    private final SnackbarManager mSnackbarManager;
    private CreatorCoordinator mCreatorCoordinator;

    public CreatorActionDelegateImpl(Context activityContext, Profile profile,
            SnackbarManager snackbarManager, CreatorCoordinator creatorCoordinator) {
        mActivityContext = activityContext;
        mProfile = profile;
        mSnackbarManager = snackbarManager;
        mCreatorCoordinator = creatorCoordinator;
    }

    @Override
    public void downloadPage(String url) {
        // Unimplemented.
    }

    @Override
    public void openSuggestionUrl(int disposition, LoadUrlParams params, boolean inGroup,
            Runnable onPageLoaded, Callback<VisitResult> onVisitComplete) {
        // Back-of-card actions
        if (disposition == WindowOpenDisposition.NEW_FOREGROUND_TAB
                || disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD) {
            boolean offTheRecord = (disposition == WindowOpenDisposition.OFF_THE_RECORD);
            new TabDelegate(offTheRecord).createNewTab(params, TabLaunchType.FROM_LINK, null);
            return;
        } else if (disposition == WindowOpenDisposition.CURRENT_TAB) {
            mCreatorCoordinator.requestOpenSheet(new GURL(params.getUrl()));
            return;
        }
        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
        Log.w(TAG, "OpenSuggestionUrl: Unhandled disposition " + disposition);
    }

    @Override
    public void openUrl(int disposition, LoadUrlParams params) {}

    @Override
    public void openHelpPage() {
        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
    }

    @Override
    public void addToReadingList(String title, String url) {
        // TODO(crbug/1399617) Eliminate code duplication with
        //     FeedActionDelegateImpl
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(mProfile);
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            assert ThreadUtils.runningOnUiThread();
            BookmarkUtils.addToReadingList(
                    new GURL(url), title, mSnackbarManager, bookmarkModel, mActivityContext);
        });
    }

    @Override
    public void openCrow(String url) {
        // Unused; feature is deprecated and does not launch from the feed.
    }

    @Override
    public void openWebFeed(String webFeedName) {
        // Unused; the Creator feed does not need to navigate to another feed.
    }

    @Override
    public void onContentsChanged() {
        // Not sure if this needs to be implemented.
    }

    @Override
    public void onStreamCreated() {
        // Not sure if this needs to be implemented.
    }

    @Override
    public void showSignInActivity() {
        // TODO(crbug.com/1395449)
    }
}

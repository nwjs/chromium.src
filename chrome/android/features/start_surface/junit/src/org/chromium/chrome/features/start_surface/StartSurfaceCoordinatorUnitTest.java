// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Tests for {@link StartSurfaceCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
@Features.DisableFeatures({ChromeFeatureList.WEB_FEED, ChromeFeatureList.SHOPPING_LIST,
        ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
public class StartSurfaceCoordinatorUnitTest {
    private static final long MILLISECONDS_PER_MINUTE = TimeUtils.SECONDS_PER_MINUTE * 1000;
    private static final String START_SURFACE_TIME_SPENT = "StartSurface.TimeSpent";
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";
    private static final String USER_ACTION_START_SURFACE_MVT_CLICK =
            "Suggestions.Tile.Tapped.StartSurface";
    private static final String TEST_URL = "https://www.example.com/";

    @Mock
    private UmaRecorder mUmaRecorder;
    @Mock
    private Callback mOnVisitComplete;
    @Mock
    private Runnable mOnPageLoaded;

    @Rule
    public StartSurfaceCoordinatorUnitTestRule mTestRule =
            new StartSurfaceCoordinatorUnitTestRule();

    StartSurfaceCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        UmaRecorderHolder.resetForTesting();
        mCoordinator = mTestRule.getCoordinator();
        mCoordinator.initWithNative();
    }

    @Test
    public void testCleanUpMVTilesAfterHiding() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);

        Assert.assertFalse(mCoordinator.isMVTilesCleanedUpForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertTrue(mCoordinator.isMVTilesCleanedUpForTesting());
    }

    @Test
    public void testMVTilesInitialized() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertTrue(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());
    }

    @Test
    public void testDoNotInitializeSecondaryTasksSurfaceWithoutOpenGridTabSwitcher() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertTrue(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertTrue(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertFalse(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());
    }

    @Test
    public void testStartWithBehaviouralTargeting() {
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("model_mv_tiles");
        StartSurfaceConfiguration.USER_CLICK_THRESHOLD.setForTesting(1);
        StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.setForTesting(2);

        ReturnToChromeUtil.userBehaviourSupported();
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Verifies that the START_NEXT_SHOW_ON_STARTUP_DECISION_MS has been set.
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeUtil.INVALID_DECISION_TIMESTAMP);
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));

        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeUtil.ShowChromeStartSegmentationResult.DONT_SHOW);

        StartSurfaceConfiguration.USER_CLICK_THRESHOLD.setForTesting(1);
        int clicksHigherThreshold = StartSurfaceConfiguration.USER_CLICK_THRESHOLD.getValue();
        Assert.assertEquals(1, clicksHigherThreshold);
        ReturnToChromeUtil.onMVTileOpened();
        // Verifies that userBehaviourSupported() returns the same result before the next decision
        // time arrives.
        Assert.assertFalse(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertEquals(nextDecisionTime,
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeUtil.INVALID_DECISION_TIMESTAMP));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(1, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison"));

        // Verifies if the next decision time past and the clicks of MV tiles is higher than the
        // threshold, userBehaviourSupported() returns true. Besides, the next decision time is set
        // to NUM_DAYS_KEEP_SHOW_START_AT_STARTUP day's later, and MV tiles count is reset.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP.getValue());
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison",
                        ReturnToChromeUtil.ShowChromeStartSegmentationResultComparison
                                .SEGMENTATION_DISABLED_LOGIC_ENABLED));

        // Verifies if the next decision time past and the clicks of MV tiles is lower than the
        // threshold, userBehaviourSupported() returns false. Besides, the next decision time is
        // set to NUM_DAYS_USER_CLICK_BELOW_THRESHOLD day's later.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeUtil.ShowChromeStartSegmentationResult.SHOW);
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));
        Assert.assertFalse(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Startup.Android.ShowChromeStartSegmentationResultComparison",
                        ReturnToChromeUtil.ShowChromeStartSegmentationResultComparison
                                .SEGMENTATION_ENABLED_LOGIC_DISABLED));

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("feeds");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_new_tab");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_history");
        verifyBehaviourTypeRecordedAndChecked(manager);

        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("open_recent_tabs");
        verifyBehaviourTypeRecordedAndChecked(manager);

        // Verifies if the key doesn't match the value of
        // StartSurfaceConfiguration.BEHAVIOURAL_TARGETING, e.g., the value isn't set, onUIClicked()
        // doesn't record or increase the count.
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("");
        String type = "feeds";
        String key = ReturnToChromeUtil.getBehaviourTypeKeyForTesting(type);
        ReturnToChromeUtil.onUIClicked(key);
        Assert.assertEquals(0, manager.readInt(key, 0));

        // Verifies the combination case that BEHAVIOURAL_TARGETING is set to "all".
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("all");
        String type1 = "open_history";
        String type2 = "open_recent_tabs";
        String key1 = ReturnToChromeUtil.getBehaviourTypeKeyForTesting(type1);
        String key2 = ReturnToChromeUtil.getBehaviourTypeKeyForTesting(type2);
        Assert.assertEquals(0, manager.readInt(key1, 0));
        Assert.assertEquals(0, manager.readInt(key2, 0));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Increase the count of one key.
        ReturnToChromeUtil.onHistoryOpened();
        Assert.assertEquals(1, manager.readInt(key1, 0));

        // Verifies that userBehaviourSupported() return true due to the count of this key is higher
        // or equal to the threshold.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertEquals(0, manager.readInt(key1, 0));
        Assert.assertEquals(0, manager.readInt(key2, 0));
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Resets the decision.
        manager.writeBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false);
    }

    @Test
    public void testStartSegmentationUsage() {
        StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.setForTesting("model");

        ReturnToChromeUtil.userBehaviourSupported();
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Verifies that the START_NEXT_SHOW_ON_STARTUP_DECISION_MS has been set.
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeUtil.INVALID_DECISION_TIMESTAMP);
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        Assert.assertEquals(0, manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT, 0));

        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeUtil.ShowChromeStartSegmentationResult.SHOW);

        // Verifies that userBehaviourSupported() returns the same result before the next decision
        // time arrives.
        Assert.assertFalse(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertEquals(nextDecisionTime,
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeUtil.INVALID_DECISION_TIMESTAMP));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Verifies if the next decision time past, userBehaviourSupported() returns true. Besides,
        // the next decision time is set to NUM_DAYS_KEEP_SHOW_START_AT_STARTUP day's later.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP.getValue());

        // Verifies if the next decision time past and segmentation says dont show,
        // userBehaviourSupported() returns false. Besides, the next decision time is set to
        // NUM_DAYS_USER_CLICK_BELOW_THRESHOLD day's later.
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeUtil.ShowChromeStartSegmentationResult.DONT_SHOW);
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertFalse(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());

        // Verifies that if segmentation stops returning results, then we continue to use the
        // previous result.
        manager.writeInt(ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT,
                ReturnToChromeUtil.ShowChromeStartSegmentationResult.UNINITIALIZED);
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertFalse(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));
        verifyNextDecisionTimeStampInDays(
                manager, StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue());
    }

    @Test
    @MediumTest
    public void testFeedSwipeLayoutVisibility() {
        assert mCoordinator.getStartSurfaceState() == StartSurfaceState.NOT_SHOWN;
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertEquals(
                View.VISIBLE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        mCoordinator.showOverview(false);
        Assert.assertEquals(
                View.VISIBLE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());
    }

    /**
     * Tests the logic of recording time spend in start surface.
     */
    @Test
    public void testRecordTimeSpendInStart() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        mCoordinator.onHide();
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(START_SURFACE_TIME_SPENT));
    }

    /**
     * Test whether the clicking action on MV tiles in {@link StartSurface} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramMostVisitedItemClick_StartSurface() {
        Tile tileForTest =
                new Tile(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"),
                                 TileTitleSource.TITLE_TAG, TileSource.TOP_SITES,
                                 TileSectionType.PERSONALIZED),
                        0);
        TileGroupDelegateImpl tileGroupDelegate = mCoordinator.getTileGroupDelegateForTesting();

        // Test clicking on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click on MV tiles.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in new tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(
                WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded "
                        + "correctly when long press then open in new tab on MV tiles.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in other window on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.NEW_WINDOW, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " shouldn't be recorded when long press then open in other window "
                        + "on MV tiles.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then download link on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then download link "
                        + "on MV tiles.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in Incognito tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded correctly "
                        + "when long press then open in Incognito tab on MV tiles.",
                4,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));
    }

    /**
     * Test whether the clicking action on MV tiles in {@link StartSurface} is been recorded
     * as user actions correctly.
     */
    @Test
    @SmallTest
    public void testRecordUserActionMostVisitedItemClick_StartSurface() {
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);

        Tile tileForTest =
                new Tile(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"),
                                 TileTitleSource.TITLE_TAG, TileSource.TOP_SITES,
                                 TileSectionType.PERSONALIZED),
                        0);
        TileGroupDelegateImpl tileGroupDelegate = mCoordinator.getTileGroupDelegateForTesting();

        // Test clicking on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tileForTest);
        verify(mUmaRecorder, times(1))
                .recordUserAction(eq(USER_ACTION_START_SURFACE_MVT_CLICK), anyLong());

        // Test long press then open in new tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(
                WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
        verify(mUmaRecorder, times(1))
                .recordUserAction(eq(USER_ACTION_START_SURFACE_MVT_CLICK), anyLong());

        // Test long press then open in other window on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.NEW_WINDOW, tileForTest);
        verify(mUmaRecorder, times(1))
                .recordUserAction(eq(USER_ACTION_START_SURFACE_MVT_CLICK), anyLong());

        // Test long press then download link on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
        verify(mUmaRecorder, times(1))
                .recordUserAction(eq(USER_ACTION_START_SURFACE_MVT_CLICK), anyLong());

        // Test long press then open in Incognito tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
        verify(mUmaRecorder, times(2))
                .recordUserAction(eq(USER_ACTION_START_SURFACE_MVT_CLICK), anyLong());

        UmaRecorderHolder.resetForTesting();
    }

    /**
     * Test whether the clicking action on Feeds in {@link StartSurface} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramFeedClick_StartSurface() {
        FeedActionDelegate feedActionDelegate =
                mCoordinator.getMediatorForTesting().getFeedActionDelegateForTesting();
        // Test click on Feeds or long press then check about this source & topic on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click on Feeds or "
                        + "long press then check about this source & topic on Feeds.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test long press then open in new tab on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.NEW_BACKGROUND_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then open in "
                        + "new tab on Feeds.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test long press then open in incognito tab on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.OFF_THE_RECORD,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then open in incognito tab "
                        + "on Feeds.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test manage activity or manage interests on Feeds.
        feedActionDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.LINK));
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " shouldn't be recorded when manage activity or manage interests "
                        + "on Feeds.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test click Learn More button on Feeds.
        feedActionDelegate.openHelpPage();
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click Learn More button on Feeds.",
                4,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));
    }

    /**
     * Check that the next decision time is within |numOfDays| from now.
     * @param numOfDays Number of days to check.
     */
    private void verifyNextDecisionTimeStampInDays(
            SharedPreferencesManager manager, int numOfDays) {
        long approximateTime =
                System.currentTimeMillis() + numOfDays * ReturnToChromeUtil.MILLISECONDS_PER_DAY;
        long nextDecisionTime =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        ReturnToChromeUtil.INVALID_DECISION_TIMESTAMP);

        Assert.assertThat("new decision time lower bound",
                approximateTime - MILLISECONDS_PER_MINUTE,
                Matchers.lessThanOrEqualTo(nextDecisionTime));

        Assert.assertThat("new decision time upper bound",
                approximateTime + MILLISECONDS_PER_MINUTE,
                Matchers.greaterThanOrEqualTo(nextDecisionTime));
    }

    private void verifyBehaviourTypeRecordedAndChecked(SharedPreferencesManager manager) {
        String key = ReturnToChromeUtil.getBehaviourTypeKeyForTesting(
                StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue());
        Assert.assertEquals(0, manager.readInt(key, 0));

        // Increase the count of the key.
        ReturnToChromeUtil.onUIClicked(key);
        Assert.assertEquals(1, manager.readInt(key, 0));
        Assert.assertFalse(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Verifies that userBehaviourSupported() return true due to the count of this key is higher
        // or equal to the threshold.
        manager.writeLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                System.currentTimeMillis() - 1);
        Assert.assertTrue(ReturnToChromeUtil.userBehaviourSupported());
        Assert.assertEquals(0, manager.readInt(key, 0));
        Assert.assertTrue(manager.readBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false));

        // Resets the decision.
        manager.writeBoolean(ChromePreferenceKeys.START_SHOW_ON_STARTUP, false);
    }
}

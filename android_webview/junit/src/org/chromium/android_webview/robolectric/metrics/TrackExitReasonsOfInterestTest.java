// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.metrics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AppState;
import org.chromium.android_webview.metrics.TrackExitReasonsOfInterest;
import org.chromium.android_webview.metrics.TrackExitReasonsOfInterest.AppStateData;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem.ExitReason;

import java.io.FileInputStream;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Junit tests for TrackExitReasonsOfInterest. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, manifest = Config.NONE)
public class TrackExitReasonsOfInterestTest {
    private static final String TAG = "ExitReasonsTest";
    private MockAwContentsLifecycleNotifier mMockNotifier = new MockAwContentsLifecycleNotifier();

    @Before
    public void setUp() {
        // Needed in case the data directory is not initialized for testing
        PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
    }

    public static class MockAwContentsLifecycleNotifier {
        public @AppState int mState;

        public MockAwContentsLifecycleNotifier() {
            mState = AppState.UNKNOWN;
        }

        public MockAwContentsLifecycleNotifier(@AppState int state) {
            mState = state;
        }

        @AppState
        int getAppState() {
            return mState;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAppStateDataConstructor() {
        int pid = 42;
        long timeMillis = 5L;
        for (@AppState int state = AppState.UNKNOWN; state <= AppState.STARTUP; state++) {
            AppStateData data = new AppStateData(pid, timeMillis, state);
            assertEquals("AppStateData process id should match", pid, data.mPid);
            assertEquals("AppStateData app state should match", state, data.mState);
            assertEquals("AppStateData time should match", timeMillis, data.mTimeMillis);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReadDataWhenThereIsNoFile() {
        assertFalse(
                "File should initially not exist", TrackExitReasonsOfInterest.getFile().exists());
        List<AppStateData> dataList = TrackExitReasonsOfInterest.readData();
        assertEquals("Data list should be empty because there is no file", 0, dataList.size());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWriteThenReadAppStateData() {
        int pid = 42;
        long timeMillis = 5L;
        @AppState int state = AppState.BACKGROUND;
        TrackExitReasonsOfInterest.setPidForTest(pid);
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(timeMillis);
        TrackExitReasonsOfInterest.writeState(state);

        assertTrue(
                "File should exist after writing to it",
                TrackExitReasonsOfInterest.getFile().exists());
        List<AppStateData> dataList = TrackExitReasonsOfInterest.readData();
        assertEquals("Data list should have one entry after writing app state", 1, dataList.size());
        AppStateData data = dataList.get(0);
        assertEquals("Process id should be stored in file", pid, data.mPid);
        assertEquals("Time should be stored in file", timeMillis, data.mTimeMillis);
        assertEquals("State should be stored in file", state, data.mState);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testValidSystemReasonsAndStatesAreLogged() {
        int previousPid = 42;
        long previousTimeMillis = 5L;
        long currentTimeMillis = 7L;
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(currentTimeMillis);

        for (int systemReason = 0; systemReason < ExitReason.NUM_ENTRIES; systemReason++) {
            for (@AppState int state = 0; state <= AppState.STARTUP; state++) {
                ProcessExitReasonFromSystem.setActivityManagerForTest(
                        createMockActivityManager(previousPid, systemReason));

                AppStateData data = new AppStateData(previousPid, previousTimeMillis, state);

                // There should be logs for recognized system exit reasons.
                var histogramWatcher =
                        HistogramWatcher.newBuilder()
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_COUNTS
                                                + "."
                                                + TrackExitReasonsOfInterest.sUmaSuffixMap.get(
                                                        state),
                                        ProcessExitReasonFromSystem
                                                .convertApplicationExitInfoToExitReason(
                                                        systemReason))
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_COUNTS,
                                        ProcessExitReasonFromSystem
                                                .convertApplicationExitInfoToExitReason(
                                                        systemReason))
                                .expectIntRecord(
                                        TrackExitReasonsOfInterest.UMA_DELTA,
                                        (int) (currentTimeMillis - previousTimeMillis))
                                .build();

                assertNotEquals(-1, TrackExitReasonsOfInterest.findExitReasonAndLog(data));
                histogramWatcher.assertExpected();
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUnexpectedSystemReasonsAreNotLogged() {
        int pid = 1;
        long timeMillis = 5L;
        int unexpectedSystemReason = 1337;
        ProcessExitReasonFromSystem.setActivityManagerForTest(
                createMockActivityManager(pid, unexpectedSystemReason));
        AppStateData data = new AppStateData(pid, timeMillis, AppState.DESTROYED);

        // There should be nothing logged for unexpected system exit reasons.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TrackExitReasonsOfInterest.UMA_COUNTS + ".DESTROYED")
                        .expectNoRecords(TrackExitReasonsOfInterest.UMA_COUNTS)
                        .expectNoRecords(TrackExitReasonsOfInterest.UMA_DELTA)
                        .build();

        assertEquals(-1, TrackExitReasonsOfInterest.findExitReasonAndLog(data));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUpdateAppStateWritesFileOnlyIfAppStateChanged() throws TimeoutException {
        TrackExitReasonsOfInterest.setStateSupplier(mMockNotifier::getAppState);
        mMockNotifier.mState = AppState.UNKNOWN;
        long timeMillis = 5L;
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(timeMillis);

        // The first call writes the file with the initial state.
        CallbackHelper writeFinished = new CallbackHelper();
        Callback<Boolean> resultCallback =
                result -> {
                    writeFinished.notifyCalled();
                };
        int calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.updateAppState(resultCallback);
        writeFinished.waitForCallback(calls);

        AppStateData data = TrackExitReasonsOfInterest.readData().get(0);
        assertEquals(timeMillis, data.mTimeMillis);
        assertEquals(AppState.UNKNOWN, data.mState);

        // The second call also writes the file because the app state has changed.
        mMockNotifier.mState = AppState.FOREGROUND;
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(++timeMillis);
        calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.updateAppState(resultCallback);
        writeFinished.waitForCallback(calls);

        data = TrackExitReasonsOfInterest.readData().get(0);
        assertEquals(timeMillis, data.mTimeMillis);
        assertEquals(AppState.FOREGROUND, data.mState);

        // The third call does not write the file because the app state has not changed, the
        // previous time is still in the file.
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(++timeMillis);
        calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.updateAppState(resultCallback);
        writeFinished.waitForCallback(calls);

        data = TrackExitReasonsOfInterest.readData().get(0);
        assertEquals(timeMillis - 1, data.mTimeMillis);
        assertEquals(AppState.FOREGROUND, data.mState);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testStartTrackingStartupMultipleTimes() throws TimeoutException, IOException {
        // If an app exits during early startup repeatedly, new data keeps getting added to the file
        // without getting logged to UMA and removed from the file. We do not want the file to have
        // unbounded growth, so there is a limit.
        for (int i = 1; i <= TrackExitReasonsOfInterest.MAX_DATA_LIST_SIZE + 1; i++) {
            TrackExitReasonsOfInterest.setPidForTest(i);
            TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(i);

            CallbackHelper writeFinished = new CallbackHelper();
            Callback<Boolean> resultCallback =
                    result -> {
                        writeFinished.notifyCalled();
                    };
            int calls = writeFinished.getCallCount();
            TrackExitReasonsOfInterest.startTrackingStartup(resultCallback);
            writeFinished.waitForCallback(calls);

            if (i <= TrackExitReasonsOfInterest.MAX_DATA_LIST_SIZE) {
                assertEquals(i, TrackExitReasonsOfInterest.readData().size());
            } else {
                assertEquals(
                        TrackExitReasonsOfInterest.MAX_DATA_LIST_SIZE,
                        TrackExitReasonsOfInterest.readData().size());
            }
            try (FileInputStream fis = new FileInputStream(TrackExitReasonsOfInterest.getFile())) {
                assertTrue(
                        "File should not be larger than 4KB",
                        FileUtils.readStream(fis).length <= 4096);
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testStartAndFinishTrackingStartup() throws TimeoutException {
        int previousPid = 42;
        int currentPid = 1337;
        long previousTimeMillis = 5L;
        long currentTimeMillis = 7L;
        int systemReason = ApplicationExitInfo.REASON_CRASH;

        CallbackHelper writeFinished = new CallbackHelper();
        Callback<Boolean> resultCallback =
                result -> {
                    writeFinished.notifyCalled();
                };
        int calls;

        List<AppStateData> dataList;
        TrackExitReasonsOfInterest.setStateSupplier(mMockNotifier::getAppState);

        ProcessExitReasonFromSystem.setActivityManagerForTest(
                createMockActivityManager(previousPid, systemReason));

        // Set previous state and write it to file, this overwrites its contents.
        TrackExitReasonsOfInterest.setPidForTest(previousPid);
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(previousTimeMillis);
        mMockNotifier.mState = AppState.DESTROYED;
        calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.updateAppState(resultCallback);
        writeFinished.waitForCallback(calls);

        dataList = TrackExitReasonsOfInterest.readData();
        assertEquals(1, dataList.size());
        assertEquals(previousPid, dataList.get(0).mPid);
        assertEquals(previousTimeMillis, dataList.get(0).mTimeMillis);
        assertEquals(AppState.DESTROYED, dataList.get(0).mState);

        // Set current state and start tracking startup, the current state is appended
        // to the file and it is STARTUP.
        TrackExitReasonsOfInterest.setPidForTest(currentPid);
        TrackExitReasonsOfInterest.setCurrentTimeMillisForTest(currentTimeMillis);
        calls = writeFinished.getCallCount();
        TrackExitReasonsOfInterest.startTrackingStartup(resultCallback);
        writeFinished.waitForCallback(calls);

        dataList = TrackExitReasonsOfInterest.readData();
        assertEquals(2, dataList.size());
        AppStateData currentData = null;
        for (AppStateData data : dataList) {
            if (data.mPid == currentPid) currentData = data;
        }
        assertNotNull(currentData);
        assertEquals(AppState.STARTUP, currentData.mState);

        // Finish tracking startup, the previous process state is logged, and the file
        // is rewritten to contain just the current state.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TrackExitReasonsOfInterest.UMA_COUNTS + ".DESTROYED",
                                ProcessExitReasonFromSystem.convertApplicationExitInfoToExitReason(
                                        systemReason))
                        .expectIntRecord(
                                TrackExitReasonsOfInterest.UMA_COUNTS,
                                ProcessExitReasonFromSystem.convertApplicationExitInfoToExitReason(
                                        systemReason))
                        .expectIntRecord(
                                TrackExitReasonsOfInterest.UMA_DELTA,
                                (int) (currentTimeMillis - previousTimeMillis))
                        .build();

        calls = writeFinished.getCallCount();
        mMockNotifier.mState = AppState.FOREGROUND;
        TrackExitReasonsOfInterest.finishTrackingStartup(
                mMockNotifier::getAppState, resultCallback);
        writeFinished.waitForCallback(calls);

        histogramWatcher.assertExpected();
        dataList = TrackExitReasonsOfInterest.readData();
        assertEquals(1, dataList.size());
        assertEquals(currentPid, dataList.get(0).mPid);
        assertEquals(currentTimeMillis, dataList.get(0).mTimeMillis);
        assertEquals(AppState.FOREGROUND, dataList.get(0).mState);
    }

    private static ActivityManager createMockActivityManager(int pid, int systemReason) {
        ActivityManager mockActivityManager = Mockito.mock(ActivityManager.class);
        ApplicationExitInfo exitInfo = Mockito.mock(ApplicationExitInfo.class);
        when(exitInfo.getPid()).thenReturn(pid);
        when(exitInfo.getReason()).thenReturn(systemReason);
        when(mockActivityManager.getHistoricalProcessExitReasons(null, pid, 1))
                .thenReturn(List.of(exitInfo));
        return mockActivityManager;
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.ChromeModernDesign;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests related to displaying contextual suggestions in a bottom sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BOTTOM_SHEET)
@ChromeModernDesign.Enable
public class ContextualSuggestionsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ContextualSuggestionsDependenciesRule mContextualSuggestionsDeps =
            new ContextualSuggestionsDependenciesRule();
    @Rule
    public TestRule mChromeModernDesignStateRule = new ChromeModernDesign.Processor();
    @Rule
    public TestRule mFeaturesProcessor = new Features.InstrumentationProcessor();

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    private EmbeddedTestServer mTestServer;
    private ContextualSuggestionsCoordinator mCoordinator;
    private ContextualSuggestionsMediator mMediator;
    private ContextualSuggestionsModel mModel;
    private BottomSheet mBottomSheet;

    @Before
    public void setUp() throws Exception {
        mContextualSuggestionsDeps.getFactory().suggestionsSource =
                new FakeContextualSuggestionsSource();
        mContextualSuggestionsDeps.getFactory().fetchHelper = new FakeFetchHelper();

        FakeEnabledStateMonitor stateMonitor = new FakeEnabledStateMonitor();
        mContextualSuggestionsDeps.getFactory().enabledStateMonitor = new FakeEnabledStateMonitor();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator =
                    mActivityTestRule.getActivity().getContextualSuggestionsCoordinatorForTesting();
            mMediator = mCoordinator.getMediatorForTesting();
            mModel = mCoordinator.getModelForTesting();
            stateMonitor.setObserver(mMediator);
        });

        mBottomSheet = mActivityTestRule.getActivity().getBottomSheet();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // TODO(twellington): Add more tests!
}

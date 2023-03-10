// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * A TestRule that sets up the mocks and contains helper methods for JUnit/Robolectric tests scoped
 * to the Partial Custom Tabs logic.
 */
public class PartialCustomTabTestRule implements TestRule {
    // Pixel 3 XL metrics
    static final int DEVICE_HEIGHT = 2960;
    static final int DEVICE_WIDTH = 1440;
    static final int DEVICE_HEIGHT_LANDSCAPE = DEVICE_WIDTH;
    static final int DEVICE_WIDTH_LANDSCAPE = DEVICE_HEIGHT;
    static final int NAVBAR_HEIGHT = 160;
    static final int STATUS_BAR_HEIGHT = 68;
    static final int FULL_HEIGHT = DEVICE_HEIGHT - NAVBAR_HEIGHT;

    @Mock
    Activity mActivity;
    @Mock
    Window mWindow;
    @Mock
    WindowManager mWindowManager;
    @Mock
    Resources mResources;
    @Mock
    Configuration mConfiguration;
    WindowManager.LayoutParams mAttributes;
    @Mock
    View mDecorView;
    @Mock
    View mRootView;
    @Mock
    Display mDisplay;
    @Mock
    PartialCustomTabHeightStrategy.OnResizedCallback mOnResizedCallback;
    @Mock
    ViewGroup mCoordinatorLayout;
    @Mock
    ViewGroup mContentFrame;
    @Mock
    FullscreenManager mFullscreenManager;
    @Mock
    ViewStub mHandleViewStub;
    @Mock
    ImageView mHandleView;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    LinearLayout mNavbar;
    @Mock
    ViewPropertyAnimator mViewAnimator;
    @Mock
    ImageView mSpinnerView;
    @Mock
    CircularProgressDrawable mSpinner;
    @Mock
    View mToolbarView;
    @Mock
    View mToolbarCoordinator;
    @Mock
    View mDragBar;
    @Mock
    View mDragHandlebar;
    @Mock
    GradientDrawable mDragBarBackground;

    DisplayMetrics mMetrics;
    Context mContext;
    List<WindowManager.LayoutParams> mAttributeResults;
    DisplayMetrics mRealMetrics;

    FrameLayout.LayoutParams mLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    FrameLayout.LayoutParams mCoordinatorLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);

    private void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getWindowManager()).thenReturn(mWindowManager);
        when(mActivity.findViewById(R.id.coordinator)).thenReturn(mCoordinatorLayout);
        when(mActivity.findViewById(android.R.id.content)).thenReturn(mContentFrame);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view_stub)).thenReturn(mHandleViewStub);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view)).thenReturn(mHandleView);
        when(mActivity.findViewById(R.id.drag_bar)).thenReturn(mDragBar);
        when(mActivity.findViewById(R.id.drag_handlebar)).thenReturn(mDragHandlebar);
        mAttributes = new WindowManager.LayoutParams();
        when(mWindow.getAttributes()).thenReturn(mAttributes);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getContext()).thenReturn(mActivity);
        when(mDecorView.getRootView()).thenReturn(mRootView);
        when(mRootView.getLayoutParams()).thenReturn(mAttributes);
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mContentFrame.getLayoutParams()).thenReturn(mLayoutParams);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mCoordinatorLayout.getLayoutParams()).thenReturn(mCoordinatorLayoutParams);
        when(mHandleView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mToolbarCoordinator.getLayoutParams()).thenReturn(mLayoutParams);
        when(mNavbar.animate()).thenReturn(mViewAnimator);
        when(mViewAnimator.alpha(anyFloat())).thenReturn(mViewAnimator);
        when(mViewAnimator.setDuration(anyLong())).thenReturn(mViewAnimator);
        when(mViewAnimator.setListener(anyObject())).thenReturn(mViewAnimator);
        when(mSpinnerView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mSpinnerView.getParent()).thenReturn(mContentFrame);
        when(mSpinnerView.animate()).thenReturn(mViewAnimator);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;

        mAttributeResults = new ArrayList<>();
        doAnswer(invocation -> {
            WindowManager.LayoutParams attributes = new WindowManager.LayoutParams();
            attributes.copyFrom((WindowManager.LayoutParams) invocation.getArgument(0));
            mAttributes.copyFrom(attributes);
            mAttributeResults.add(attributes);
            return null;
        })
                .when(mWindow)
                .setAttributes(any(WindowManager.LayoutParams.class));

        mRealMetrics = new DisplayMetrics();
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mRealMetrics);
            return null;
        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));

        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
    }

    private void commonTearDown() {
        // Reset the multi-window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
    }

    public void configPortraitMode() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mDisplay.getRotation()).thenReturn(Surface.ROTATION_90);
    }

    public void configLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
    }

    public void configLandscapeMode(int direction) {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_WIDTH);
        when(mDisplay.getRotation()).thenReturn(direction);
    }

    public void verifyWindowFlagsSet() {
        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                try {
                    statement.evaluate();
                } finally {
                    commonTearDown();
                }
            }
        };
    }
}
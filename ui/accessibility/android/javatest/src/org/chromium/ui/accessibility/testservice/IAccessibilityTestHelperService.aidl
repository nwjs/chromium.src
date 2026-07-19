// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.os.Bundle;
import org.chromium.ui.accessibility.testservice.NodeMatcher;
import org.chromium.ui.accessibility.testservice.WaitForEventParams;

interface IAccessibilityTestHelperService {
    /**
     * Waits for an accessibility event matching the given query parameters.
     * Returns true if the event is received within the timeout, false otherwise.
     *
     * @param params The event query parameters.
     */
    boolean waitForEvent(in WaitForEventParams params);

    /**
     * Finds a node matching the matcher and performs the given action on it.
     *
     * @param matcher The node matching criteria.
     * @param action The action to perform (e.g., AccessibilityNodeInfo.ACTION_HOVER_ENTER).
     * @param arguments The arguments bundle, which can be null.
     * @return true if the action was performed successfully.
     */
    boolean performActionOnNode(
            in NodeMatcher matcher, int action, in @nullable Bundle arguments);

    /**
     * Dumps the accessibility tree to a String.
     *
     * @return The accessibility tree as a String.
     */
    String dumpWebContentsAccessibilityTree();
}

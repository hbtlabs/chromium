// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.historyreport;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeApplication;

/** Base class for reporting entities to App Indexing. */
public class AppIndexingReporter {
    private static final String TAG = "AppIndexingReporter";
    private static AppIndexingReporter sInstance;

    public static AppIndexingReporter getInstance() {
        if (sInstance == null) {
            sInstance = ((ChromeApplication) ContextUtils.getApplicationContext())
                                .createAppIndexingReporter();
        }
        return sInstance;
    }

    /**
     * Reports provided entity to on-device index.
     * Base class does not implement any reporting, and call is a no-op. Child classes should
     * implement this functionality.
     */
    public void reportEntityJsonLd(String url, String json) {
        // Overriden by private class. Base class does nothing.
    }
}

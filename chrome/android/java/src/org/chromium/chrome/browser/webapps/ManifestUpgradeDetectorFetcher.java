// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * Downloads the Web Manifest if the web site still uses the {@link manifestUrl} passed to the
 * constructor.
 */
public class ManifestUpgradeDetectorFetcher extends EmptyTabObserver {

    /**
     * Called once the Web Manifest has been downloaded.
     */
    public interface Callback {
        void onGotManifestData(FetchedManifestData fetchedData);
    }

    /**
     * Fetched Web Manifest data.
     */
    public static class FetchedManifestData {
        public String startUrl;
        public String scopeUrl;
        public String name;
        public String shortName;
        public String bestIconUrl;

        // Hash of untransformed icon bytes. The hash should have been taken prior to any
        // encoding/decoding.
        public String bestIconMurmur2Hash;

        public Bitmap bestIcon;
        public String[] iconUrls;
        public int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
    }

    /**
     * Pointer to the native side ManifestUpgradeDetectorFetcher. The Java side owns the native side
     * ManifestUpgradeDetectorFetcher.
     */
    private long mNativePointer;

    /** The tab that is being observed. */
    private final Tab mTab;

    private Callback mCallback;

    public ManifestUpgradeDetectorFetcher(Tab tab, String scopeUrl, String manifestUrl) {
        mTab = tab;
        mNativePointer = nativeInitialize(scopeUrl, manifestUrl);
    }

    /**
     * Starts fetching the web manifest resources.
     * @param callback Called once the Web Manifest has been downloaded.
     */
    public boolean start(Callback callback) {
        if (mTab == null || mTab.getWebContents() == null) return false;
        mCallback = callback;
        mTab.addObserver(this);
        nativeStart(mNativePointer, mTab.getWebContents());
        return true;
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        mTab.removeObserver(this);
        nativeDestroy(mNativePointer);
        mNativePointer = 0;
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad,
            boolean didFinishLoad) {
        updatePointers();
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePointers();
    }

    /**
     * Updates which WebContents the native ManifestUpgradeDetectorFetcher is monitoring.
     */
    private void updatePointers() {
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Called when the updated Web Manifest has been fetched.
     */
    @CalledByNative
    private void onDataAvailable(String startUrl, String scopeUrl, String name, String shortName,
            String bestIconUrl, String bestIconMurmur2Hash, Bitmap bestIconBitmap,
            String[] iconUrls, int displayMode, int orientation, long themeColor,
            long backgroundColor) {
        if (TextUtils.isEmpty(scopeUrl)) {
            scopeUrl = ShortcutHelper.getScopeFromUrl(startUrl);
        }

        FetchedManifestData fetchedData = new FetchedManifestData();
        fetchedData.startUrl = startUrl;
        fetchedData.scopeUrl = scopeUrl;
        fetchedData.name = name;
        fetchedData.shortName = shortName;
        fetchedData.bestIconUrl = bestIconUrl;
        fetchedData.bestIconMurmur2Hash = bestIconMurmur2Hash;
        fetchedData.bestIcon = bestIconBitmap;
        fetchedData.iconUrls = iconUrls;
        fetchedData.displayMode = displayMode;
        fetchedData.orientation = orientation;
        fetchedData.themeColor = themeColor;
        fetchedData.backgroundColor = backgroundColor;

        mCallback.onGotManifestData(fetchedData);
    }

    private native long nativeInitialize(String scope, String webManifestUrl);
    private native void nativeReplaceWebContents(
            long nativeManifestUpgradeDetectorFetcher, WebContents webContents);
    private native void nativeDestroy(long nativeManifestUpgradeDetectorFetcher);
    private native void nativeStart(
            long nativeManifestUpgradeDetectorFetcher, WebContents webContents);
}

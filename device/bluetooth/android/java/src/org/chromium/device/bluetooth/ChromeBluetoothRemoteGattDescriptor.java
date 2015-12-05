// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.BluetoothGattDescriptor;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.bluetooth.BluetoothGattDescriptor as necessary
 * for C++ device::BluetoothRemoteGattDescriptorAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattDescriptorAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattDescriptor {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattDescriptorAndroid;
    final Wrappers.BluetoothGattDescriptorWrapper mDescriptor;
    final ChromeBluetoothRemoteGattCharacteristic mChromeCharacteristic;

    private ChromeBluetoothRemoteGattDescriptor(long nativeBluetoothRemoteGattDescriptorAndroid,
            Wrappers.BluetoothGattDescriptorWrapper descriptorWrapper,
            ChromeBluetoothRemoteGattCharacteristic chromeCharacteristic) {
        mNativeBluetoothRemoteGattDescriptorAndroid = nativeBluetoothRemoteGattDescriptorAndroid;
        mDescriptor = descriptorWrapper;
        mChromeCharacteristic = chromeCharacteristic;

        mChromeCharacteristic.mChromeBluetoothDevice.mWrapperToChromeDescriptorsMap.put(
                descriptorWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattDescriptorAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattDescriptor Destroyed.");
        mChromeBluetoothDevice.mBluetoothGatt.setDescriptorNotification(mDescriptor, false);
        mNativeBluetoothRemoteGattDescriptorAndroid = 0;
        mChromeCharacteristic.mChromeBluetoothDevice.mWrapperToChromeDescriptorsMap.remove(
                mDescriptor);
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattDescriptorAndroid methods implemented in java:

    // Implements BluetoothRemoteGattDescriptorAndroid::Create.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    @CalledByNative
    private static ChromeBluetoothRemoteGattDescriptor create(
            long nativeBluetoothRemoteGattDescriptorAndroid,
            Object bluetoothGattCarachteristicWrapper, Object chromeBluetoothDevice) {
        return new ChromeBluetoothRemoteGattDescriptor(nativeBluetoothRemoteGattDescriptorAndroid,
                (Wrappers.BluetoothGattDescriptorWrapper) bluetoothGattCarachteristicWrapper,
                (ChromeBluetoothDevice) chromeBluetoothDevice);
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:
}

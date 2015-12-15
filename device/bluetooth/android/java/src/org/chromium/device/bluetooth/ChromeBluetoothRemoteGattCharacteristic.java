// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;
import java.util.UUID;

/**
 * Exposes android.bluetooth.BluetoothGattCharacteristic as necessary
 * for C++ device::BluetoothRemoteGattCharacteristicAndroid.
 *
 * Lifetime is controlled by
 * device::BluetoothRemoteGattCharacteristicAndroid.
 */
@JNINamespace("device")
final class ChromeBluetoothRemoteGattCharacteristic {
    private static final String TAG = "Bluetooth";

    private long mNativeBluetoothRemoteGattCharacteristicAndroid;
    final Wrappers.BluetoothGattCharacteristicWrapper mCharacteristic;
    final String mInstanceId;
    final ChromeBluetoothDevice mChromeBluetoothDevice;

    private ChromeBluetoothRemoteGattCharacteristic(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Wrappers.BluetoothGattCharacteristicWrapper characteristicWrapper, String instanceId,
            ChromeBluetoothDevice chromeBluetoothDevice) {
        mNativeBluetoothRemoteGattCharacteristicAndroid =
                nativeBluetoothRemoteGattCharacteristicAndroid;
        mCharacteristic = characteristicWrapper;
        mInstanceId = instanceId;
        mChromeBluetoothDevice = chromeBluetoothDevice;

        mChromeBluetoothDevice.mWrapperToChromeCharacteristicsMap.put(characteristicWrapper, this);

        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic created.");
    }

    /**
     * Handles C++ object being destroyed.
     */
    @CalledByNative
    private void onBluetoothRemoteGattCharacteristicAndroidDestruction() {
        Log.v(TAG, "ChromeBluetoothRemoteGattCharacteristic Destroyed.");
        mChromeBluetoothDevice.mBluetoothGatt.setCharacteristicNotification(mCharacteristic, false);
        mNativeBluetoothRemoteGattCharacteristicAndroid = 0;
        mChromeBluetoothDevice.mWrapperToChromeCharacteristicsMap.remove(mCharacteristic);
    }

    void onCharacteristicChanged() {
        Log.i(TAG, "onCharacteristicChanged");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnChanged(
                    mNativeBluetoothRemoteGattCharacteristicAndroid, mCharacteristic.getValue());
        }
    }

    void onCharacteristicRead(int status) {
        Log.i(TAG, "onCharacteristicRead status:%d==%s", status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnRead(mNativeBluetoothRemoteGattCharacteristicAndroid, status,
                    mCharacteristic.getValue());
        }
    }

    void onCharacteristicWrite(int status) {
        Log.i(TAG, "onCharacteristicWrite status:%d==%s", status,
                status == android.bluetooth.BluetoothGatt.GATT_SUCCESS ? "OK" : "Error");
        if (mNativeBluetoothRemoteGattCharacteristicAndroid != 0) {
            nativeOnWrite(mNativeBluetoothRemoteGattCharacteristicAndroid, status);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothRemoteGattCharacteristicAndroid methods implemented in java:

    // Implements BluetoothRemoteGattCharacteristicAndroid::Create.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    @CalledByNative
    private static ChromeBluetoothRemoteGattCharacteristic create(
            long nativeBluetoothRemoteGattCharacteristicAndroid,
            Object bluetoothGattCharacteristicWrapper, String instanceId,
            Object chromeBluetoothDevice) {
        return new ChromeBluetoothRemoteGattCharacteristic(
                nativeBluetoothRemoteGattCharacteristicAndroid,
                (Wrappers.BluetoothGattCharacteristicWrapper) bluetoothGattCharacteristicWrapper,
                instanceId, (ChromeBluetoothDevice) chromeBluetoothDevice);
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetUUID.
    @CalledByNative
    private String getUUID() {
        return mCharacteristic.getUuid().toString();
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::GetProperties.
    @CalledByNative
    private int getProperties() {
        // TODO(scheib): Must read Extended Properties Descriptor. crbug.com/548449
        return mCharacteristic.getProperties();
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::StartNotifySession.
    @CalledByNative
    private boolean startNotifySession() {
        if (!mChromeBluetoothDevice.mBluetoothGatt.setCharacteristicNotification(
                    mCharacteristic, true)) {
            Log.i(TAG, "startNotifySession setCharacteristicNotification failed.");
            return false;
        }

        Wrappers.BluetoothGattDescriptorWrapper clientCharacteristicConfigurationDescriptor =
                mCharacteristic.getDescriptor(UUID.fromString(
                        "00002902-0000-1000-8000-00805F9B34FB" /* Config's standard UUID*/));

        if ((mCharacteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0) {
            if (!clientCharacteristicConfigurationDescriptor.setValue(
                        BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)) {
                Log.v(TAG, "startNotifySession NOTIFY failed!");
                return false;
            }
            Log.v(TAG, "startNotifySession NOTIFY.");
        } else if ((mCharacteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_INDICATE)
                != 0) {
            if (!clientCharacteristicConfigurationDescriptor.setValue(
                        BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)) {
                Log.v(TAG, "startNotifySession INDICATE failed!");
                return false;
            }
            Log.v(TAG, "startNotifySession INDICATE.");
        } else {
            Log.v(TAG,
                    "startNotifySession failed! Characteristic has neither PROPERTY_NOTIFY or PROPERTY_INDICATE.");
            return false;
        }

        if (!mChromeBluetoothDevice.mBluetoothGatt.writeDescriptor(
                    clientCharacteristicConfigurationDescriptor)) {
            Log.i(TAG, "startNotifySession writeDescriptor failed!");
            return false;
        }

        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::ReadRemoteCharacteristic.
    @CalledByNative
    private boolean readRemoteCharacteristic() {
        if (!mChromeBluetoothDevice.mBluetoothGatt.readCharacteristic(mCharacteristic)) {
            Log.i(TAG, "readRemoteCharacteristic readCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::WriteRemoteCharacteristic.
    @CalledByNative
    private boolean writeRemoteCharacteristic(byte[] value) {
        if (!mCharacteristic.setValue(value)) {
            Log.i(TAG, "writeRemoteCharacteristic setValue failed.");
            return false;
        }
        if (!mChromeBluetoothDevice.mBluetoothGatt.writeCharacteristic(mCharacteristic)) {
            Log.i(TAG, "writeRemoteCharacteristic writeCharacteristic failed.");
            return false;
        }
        return true;
    }

    // Implements BluetoothRemoteGattCharacteristicAndroid::EnsureDescriptorsCreated
    @CalledByNative
    private void ensureDescriptorsCreated() {
        List<Wrappers.BluetoothGattDescriptorWrapper> descriptors =
                mCharacteristic.getDescriptors();
        for (Wrappers.BluetoothGattDescriptorWrapper descriptor : descriptors) {
            // Create an adapter unique descriptor ID. getInstanceId only differs between
            // descriptor instances with the same UUID on this service.
            String descriptorInstanceId = mInstanceId + "/" + descriptor.getUuid().toString();
            nativeCreateGattRemoteDescriptor(mNativeBluetoothRemoteGattCharacteristicAndroid,
                    descriptorInstanceId, descriptor, mChromeBluetoothDevice);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterDevice C++ methods declared for access from java:

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnChanged.
    native void nativeOnChanged(long nativeBluetoothRemoteGattCharacteristicAndroid, byte[] value);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnRead.
    native void nativeOnRead(
            long nativeBluetoothRemoteGattCharacteristicAndroid, int status, byte[] value);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::OnWrite.
    native void nativeOnWrite(long nativeBluetoothRemoteGattCharacteristicAndroid, int status);

    // Binds to BluetoothRemoteGattCharacteristicAndroid::CreateGattRemoteDescriptor.
    // TODO(http://crbug.com/505554): Replace 'Object' with specific type when JNI fixed.
    private native void nativeCreateGattRemoteDescriptor(
            long nativeBluetoothRemoteGattCharacteristicAndroid, String instanceId,
            Object bluetoothGattDescriptorWrapper, Object chromeBluetoothCharacteristic);
}

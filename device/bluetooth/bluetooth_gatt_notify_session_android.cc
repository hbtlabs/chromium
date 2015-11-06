// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session_android.h"

namespace device {

BluetoothGattNotifySessionAndroid::BluetoothGattNotifySessionAndroid(
    const std::string& characteristic_identifier)
    : characteristic_id_(characteristic_identifier) {}

BluetoothGattNotifySessionAndroid::~BluetoothGattNotifySessionAndroid() {}

}  // namespace device

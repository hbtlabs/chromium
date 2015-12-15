// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_characteristic.h"

#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
class BluetoothGattDescriptorTest : public BluetoothTest {
};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothGattDescriptorTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = DiscoverLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  ASSERT_EQ(1u, device->GetGattServices().size());
  BluetoothGattService* service = device->GetGattServices()[0];

  SimulateGattCharacteristic(service, "00000000-0000-1000-8000-00805f9b34fb", /* properties */ 0);
  ASSERT_EQ(1u, service->GetCharacteristics().size());
  BluetoothGattCharacteristic* characteristic = service_->GetCharacteristics()[0];

  // Create 2 descriptors. Two of them are duplicates.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattDescriptor(characteristic, uuid_str1);
  SimulateGattDescriptor(characteristic, uuid_str2);
  ASSERT_EQ(1u, characteristic->GetDescriptors().size());
  BluetoothGattDescriptor* descriptor1 = service->GetDescriptors()[0];
  BluetoothGattDescriptor* descriptor2 = service->GetDescriptors()[1];

  // Swap as needed to have descriptor1 be the one with uuid1.
  if (descriptor2->GetUUID() == uuid1) 
    std::swap(descriptor1, descriptor2);

  EXPECT_EQ(uuid1, descriptor1->GetUUID());
  EXPECT_EQ(uuid2, descriptor2->GetUUID());
}
#endif  // defined(OS_ANDROID)

}  // namespace device

Bluetooth
=========

`device/bluetooth` abstracts
[Bluetooth Classic](https://en.wikipedia.org/wiki/Bluetooth) and
[Low Energy](https://en.wikipedia.org/wiki/Bluetooth_low_energy) features
across multiple platforms.

Classic and Low Energy based profiles differ substantially. Platform
implementations may support only one or the other. even though several classes
have interfaces for both, e.g. `BluetoothAdapter` & `BluetoothDevice`.

  |          | Classic |  Low Energy |
  |----------|:-------:|:-----------:|
  | Android  |    no   | in progress |
  | ChromeOS |   yes   |     yes     |
  | Linux    |   yes   |     yes     |
  | Mac OSX  |   yes   | in progress |
  | Windows  |  some   | in progress |

ChromeOS and Linux are supported via BlueZ, see `*_bluez` files.


--------------------------------------------------------------------------------
Maintainer History
--------------------------------------------------------------------------------

Initial implementation OWNERS were youngki@chromium.org, keybuk@chromium.org,
armansito@chromium.org, and rpaquay@chromium.org. They no longer contribute to
chromium fulltime. They were responsible for support for ChromeOS Bluetooth
features and the Chrome Apps APIs:

* [bluetooth](https://developer.chrome.com/apps/bluetooth)
* [bluetoothLowEnergy](https://developer.chrome.com/apps/bluetoothLowEnergy)
* [bluetoothSocket](https://developer.chrome.com/apps/bluetoothSocket)

Active development in 2015 & 2016 is focused on enabling GATT features for:

* [Web Bluetooth](https://crbug.com/419413)

Known future work is tracked in the
[Refactoring meta issue](https://crbug.com/580406).

--------------------------------------------------------------------------------
Testing
--------------------------------------------------------------------------------

Implementation of the Bluetooth component is tested via unittests. Client code
uses Mock Bluetooth objects:


### Cross Platform Unit Tests

New feature development uses cross platform unit tests. This reduces test code
redundancy and produces consistency across all implementations.

`test/bluetooth_test.h` defines a cross platform test fixture
`BluetoothTestBase`. Platform implementations provide subclasses, such as
`test/bluetooth_test_android.h` and typedef to the name `BluetoothTest`.

[More information](https://docs.google.com/document/d/1mBipxn1sJu6jMqP0RQZpkYXC1o601bzLCpCxwTA2yGA/edit?usp=sharing)

### Legacy Platform Specific Unit Tests

Early code (Classic on most platforms, and Low Energy on BlueZ) was tested with
platform specific unit tests, e.g. `bluetooth_bluez_unittest.cc` &
`bluetooth_adapter_win_unittest.cc`.

Maintenance of these earlier implementations should update tests in place. Long
term these tests should be
[refactored into cross platform tests](https://crbug.com/580403).


### Mock Bluetooth Objects

`test/mock_bluetooth_*` files provide GMOCK based fake objects for use in client
code.


--------------------------------------------------------------------------------
Android
--------------------------------------------------------------------------------

The android implementation requires crossing from C++ to Java using
[__JNI__](https://www.chromium.org/developers/design-documents/android-jni).

Object ownership is rooted in the C++ classes, starting with the Adapter, which
owns Devices, Services, etc. Java counter parts interface with the Android
Bluetooth objects. E.g.

For testing, the Android objects are __wrapped__ in:
`android/java/src/org/chromium/device/bluetooth/Wrappers.java`. <br>
and __fakes__ implemenated in:
`test/android/java/src/org/chromium/device/bluetooth/Fakes.java`.

Thus:

* `bluetooth_adapter_android.h` owns:
    * `android/.../ChromeBluetoothAdapter.java` uses:
        * `android/.../Wrappers.java`: `BluetoothAdapterWrapper`
            * Which under test is a `FakeBluetoothAdapter`
    * `bluetooth_device_android.h` owns:
        * `android/.../ChromeBluetoothDevice.java` uses:
            * `android/.../Wrappers.java`: `BluetoothDeviceWrapper`
                * Which under test is a `FakeBluetoothDevice`
        * `bluetooth_gatt_service_android.h` owns:
            * `android/.../ChromeBluetoothService.java` uses:
                * `android/.../Wrappers.java`: `BluetoothServiceWrapper`
                    * Which under test is a `FakeBluetoothService`

Fake objects are controlled by `bluetooth_test_android.cc`.

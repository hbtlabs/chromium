# Bluetooth

`device/bluetooth` abstracts 
[Bluetooth Classic](https://en.wikipedia.org/wiki/Bluetooth) and 
[Low Energy](https://en.wikipedia.org/wiki/Bluetooth_low_energy) features 
across multiple platforms.

Classic and Low Energy based profiles differ substantially. Platform 
implementations may support only one or the other. even though several 
classes have interfaces for both, e.g. `BluetoothAdapter` & 
`BluetoothDevice`.

  |          | Classic |  Low Energy |
  |----------|:-------:|:-----------:|
  | Android  |    no   | in progress |
  | ChromeOS |   yes   |     yes     |
  | Linux    |   yes   |     yes     |
  | Mac OSX  |   yes   | in progress |
  | Windows  |   yes   | in progress |

ChromeOS and Linux are supported via BlueZ, see `*_bluez` files.

# Testing



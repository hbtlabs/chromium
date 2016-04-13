// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/hash.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_uuid.h"

int main(int argc, char** argv) {
  if (argc <= 1) {
    std::cout << "Generates hash values given UUIDs using the same method\n"
              << "as in bluetooth_metrics.cc.\n"
              << "\n"
              << "Usage: " << argv[0] << " <uuid> [uuid2 ...]\n"
              << "       The UUIDs may be short UUIDs, and will be made\n"
              << "       canonical before being hashed.\n";
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    std::string input_string(argv[i]);
    device::BluetoothUUID uuid(input_string);
    std::string uuid_canonical_string = uuid.canonical_value();
    uint32_t hash = base::SuperFastHash(uuid_canonical_string.data(),
                                        uuid_canonical_string.size());

    // Strip off the signed bit because UMA doesn't support negative values,
    // but takes a signed int as input.
    hash &= 0x7fffffff;
    std::cout << input_string << "\t" << uuid_canonical_string << "\t" << hash
              << std::endl;
  }
  return 0;
}

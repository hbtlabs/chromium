// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_DYNAMICALLY_INITIALIZED_MIDI_MANAGER_WIN_H_
#define MEDIA_MIDI_DYNAMICALLY_INITIALIZED_MIDI_MANAGER_WIN_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "media/midi/midi_manager.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace midi {

// New backend for legacy Windows that support dynamic instantiation.
class DynamicallyInitializedMidiManagerWin final
    : public MidiManager,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit DynamicallyInitializedMidiManagerWin(MidiService* service);
  ~DynamicallyInitializedMidiManagerWin() override;

  // Posts a reply task to the I/O thread that hosts MidiManager instance, runs
  // it safely, and ensures that the instance keeps alive while the task is
  // running.
  void PostReplyTask(const base::Closure&);

  // MidiManager overrides:
  void StartInitialization() override;
  void Finalize() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) override;

  // base::SystemMonitor::DevicesChangedObserver overrides:
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  class InPort;
  class OutPort;

  // Posts a task to TaskRunner, and ensures that the instance keeps alive while
  // the task is running.
  void PostTask(const base::Closure&);

  // Initializes instance asynchronously on TaskRunner.
  void InitializeOnTaskRunner();

  // Updates device lists on TaskRunner.
  // Returns true if device lists were changed.
  void UpdateDeviceListOnTaskRunner();

  // Reflect active port list to a device list.
  template <typename T>
  void ReflectActiveDeviceList(DynamicallyInitializedMidiManagerWin* manager,
                               std::vector<T>* known_ports,
                               std::vector<T>* active_ports);

  // Holds an unique instance ID.
  const int instance_id_;

  // Keeps a TaskRunner for the I/O thread.
  scoped_refptr<base::SingleThreadTaskRunner> thread_runner_;

  // Following members should be accessed only on TaskRunner.

  // Holds all MIDI input or output ports connected once.
  std::vector<std::unique_ptr<InPort>> input_ports_;
  std::vector<std::unique_ptr<OutPort>> output_ports_;

  DISALLOW_COPY_AND_ASSIGN(DynamicallyInitializedMidiManagerWin);
};

}  // namespace midi

#endif  // MEDIA_MIDI_DYNAMICALLY_INITIALIZED_MIDI_MANAGER_WIN_H_

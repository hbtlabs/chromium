// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_MUS_DEMO_EXTERNAL_H_
#define SERVICES_UI_DEMO_MUS_DEMO_EXTERNAL_H_

#include <memory>
#include <vector>

#include "services/ui/demo/mus_demo.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"

namespace ui {
namespace demo {

// MusDemoExternal demonstrates Mus operating in "external" mode: A new platform
// window (and hence acceleratedWidget) is created for each aura window.
class MusDemoExternal : public MusDemo {
 public:
  MusDemoExternal();
  ~MusDemoExternal() final;

 private:
  // ui::demo::MusDemo:
  void OnStartImpl(std::unique_ptr<aura::WindowTreeClient>* window_tree_client,
                   std::unique_ptr<WindowTreeData>* window_tree_data) final;

  // aura::WindowTreeClientDelegate:
  void OnEmbed(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) final;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) final;

  mojom::WindowTreeHostFactoryPtr window_tree_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusDemoExternal);
};

}  // namespace demo
}  // namespace ui

#endif  // SERVICES_UI_DEMO_MUS_DEMO_EXTERNAL_H_

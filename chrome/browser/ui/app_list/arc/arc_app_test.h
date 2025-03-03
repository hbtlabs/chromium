// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/arc/common/app.mojom.h"

namespace arc {
namespace mojom {
class AppInfo;
class ArcPackageInfo;
}
class ArcSessionManager;
class FakeArcBridgeService;
class FakeAppInstance;
}

namespace chromeos {
class FakeChromeUserManager;
class ScopedUserManagerEnabler;
}

namespace user_manager {
class User;
}

class ArcAppListPrefs;
class Profile;

// Helper class to initialize arc bridge to work with arc apps in unit tests.
class ArcAppTest {
 public:
  ArcAppTest();
  virtual ~ArcAppTest();

  void SetUp(Profile* profile);
  void TearDown();

  // Public methods to modify AppInstance for unit_tests.
  void StopArcInstance();
  void RestartArcInstance();

  static std::string GetAppId(const arc::mojom::AppInfo& app_info);
  static std::string GetAppId(const arc::mojom::ShortcutInfo& shortcut);

  const std::vector<arc::mojom::ArcPackageInfo>& fake_packages() const {
    return fake_packages_;
  }

  void AddPackage(const arc::mojom::ArcPackageInfo& package);

  void RemovePackage(const arc::mojom::ArcPackageInfo& package);

  // The 0th item is sticky but not the followings.
  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return fake_apps_;
  }

  const std::vector<arc::mojom::AppInfo>& fake_default_apps() const {
    return fake_default_apps_;
  }

  const std::vector<arc::mojom::ShortcutInfo>& fake_shortcuts() const {
    return fake_shortcuts_;
  }

  chromeos::FakeChromeUserManager* GetUserManager();

  arc::FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  ArcAppListPrefs* arc_app_list_prefs() { return arc_app_list_pref_; }

  arc::ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  const user_manager::User* CreateUserAndLogin();
  bool FindPackage(const arc::mojom::ArcPackageInfo& package);

  // Unowned pointer.
  Profile* profile_ = nullptr;

  ArcAppListPrefs* arc_app_list_pref_ = nullptr;

  std::unique_ptr<arc::FakeArcBridgeService> bridge_service_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  std::vector<arc::mojom::AppInfo> fake_apps_;
  std::vector<arc::mojom::AppInfo> fake_default_apps_;
  std::vector<arc::mojom::ArcPackageInfo> fake_packages_;
  std::vector<arc::mojom::ShortcutInfo> fake_shortcuts_;

  bool dbus_thread_manager_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppTest);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_

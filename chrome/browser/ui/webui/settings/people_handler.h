// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/driver/sync_service_observer.h"

class LoginUIService;
class SigninManagerBase;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace content {
class WebContents;
class WebUI;
}  // namespace content

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace syncer {
class SyncSetupInProgressHandle;
}  // namespace syncer

namespace settings {

class PeopleHandler : public SettingsPageUIHandler,
                      public SigninManagerBase::Observer,
                      public SyncStartupTracker::Observer,
                      public LoginUIService::LoginUI,
                      public syncer::SyncServiceObserver {
 public:
  // TODO(tommycli): Remove these strings and instead use WebUIListener events.
  // These string constants are used from JavaScript (sync_browser_proxy.js).
  static const char kSpinnerPageStatus[];
  static const char kConfigurePageStatus[];
  static const char kTimeoutPageStatus[];
  static const char kDonePageStatus[];
  static const char kPassphraseFailedPageStatus[];

  explicit PeopleHandler(Profile* profile);
  ~PeopleHandler() override;

  // Initializes the sync setup flow and shows the setup UI.
  void OpenSyncSetup(bool creating_supervised_user);

  // Terminates the sync setup flow.
  void CloseSyncSetup();

 protected:
  bool is_configuring_sync() const { return configuring_sync_; }

 private:
  friend class PeopleHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           DisplayConfigureWithBackendDisabledAndCancel);
  FRIEND_TEST_ALL_PREFIXES(
      PeopleHandlerTest,
      DisplayConfigureWithBackendDisabledAndSyncStartupCompleted);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, HandleSetupUIWhenSyncDisabled);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, SelectCustomEncryption);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, ShowSyncSetupWhenNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, SuccessfullySetPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncEverything);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncNothing);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncAllManually);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestPassphraseStillRequired);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TestSyncIndividualTypes);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest,
                           EnterExistingFrozenImplicitPassword);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, SetNewCustomPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterWrongExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, EnterBlankExistingPassphrase);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerTest, TurnOnEncryptAllDisallowed);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest,
                           UnrecoverableErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest, GaiaErrorInitializingSync);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest, HandleCaptcha);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest, HandleGaiaAuthFailure);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerNonCrosTest,
                           SubmitAuthWithInvalidUsername);
  FRIEND_TEST_ALL_PREFIXES(PeopleHandlerFirstSigninTest, DisplayBasicLogin);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // SyncStartupTracker::Observer implementation.
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  // LoginUIService::LoginUI implementation.
  void FocusUI() override;

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged() override;

  // Returns a newly created dictionary with a number of properties that
  // correspond to the status of sync.
  std::unique_ptr<base::DictionaryValue> GetSyncStatusDictionary();

  // Helper routine that gets the ProfileSyncService associated with the parent
  // profile.
  browser_sync::ProfileSyncService* GetSyncService() const;

  // Returns the LoginUIService for the parent profile.
  LoginUIService* GetLoginUIService() const;

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::ListValue* args);
  void OnDidClosePage(const base::ListValue* args);
  void HandleSetDatatypes(const base::ListValue* args);
  void HandleSetEncryption(const base::ListValue* args);
  void HandleShowSetupUI(const base::ListValue* args);
  void HandleAttemptUserExit(const base::ListValue* args);
  void HandleStartSignin(const base::ListValue* args);
  void HandleStopSyncing(const base::ListValue* args);
  void HandleGetSyncStatus(const base::ListValue* args);
  void HandleManageOtherPeople(const base::ListValue* args);

#if !defined(OS_CHROMEOS)
  // Displays the GAIA login form.
  void DisplayGaiaLogin(signin_metrics::AccessPoint access_point);

  // When web-flow is enabled, displays the Gaia login form in a new tab.
  // This function is virtual so that tests can override.
  virtual void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point);
#endif

  // Displays spinner-only UI indicating that something is going on in the
  // background.
  // TODO(kochi): better to show some message that the user can understand what
  // is running in the background.
  void DisplaySpinner();

  // Displays an error dialog which shows timeout of starting the sync backend.
  void DisplayTimeout();

  // Closes the associated sync settings page.
  void CloseUI();

  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Sends the current sync status to the JavaScript WebUI code.
  void UpdateSyncStatus();

  // Suppresses any further signin promos, since the user has signed in once.
  void MarkFirstSetupComplete();

  // Weak pointer.
  Profile* profile_;

  // Helper object used to wait for the sync backend to startup.
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Set to true whenever the sync configure UI is visible. This is used to tell
  // what stage of the setup wizard the user was in and to update the UMA
  // histograms in the case that the user cancels out.
  bool configuring_sync_;

  // The OneShotTimer object used to timeout of starting the sync backend
  // service.
  std::unique_ptr<base::OneShotTimer> backend_start_timer_;

  // Used to listen for pref changes to allow or disallow signin.
  PrefChangeRegistrar profile_pref_registrar_;

  // Manages observer lifetimes.
  ScopedObserver<SigninManagerBase, PeopleHandler> signin_observer_;
  ScopedObserver<browser_sync::ProfileSyncService, PeopleHandler>
      sync_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(PeopleHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PEOPLE_HANDLER_H_

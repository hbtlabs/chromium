// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration_win.h"

#include <windows.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <propkey.h>  // Needs to come after shobjidl.h.
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/win/settings_app_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/shell_handler_win.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/scoped_user_protocol_entry.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_mojo_client.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace shell_integration {

namespace {

// Helper function for GetAppId to generates profile id
// from profile path. "profile_id" is composed of sanitized basenames of
// user data dir and profile dir joined by a ".".
base::string16 GetProfileIdFromPath(const base::FilePath& profile_path) {
  // Return empty string if profile_path is empty
  if (profile_path.empty())
    return base::string16();

  base::FilePath default_user_data_dir;
  // Return empty string if profile_path is in default user data
  // dir and is the default profile.
  if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir) &&
      profile_path.DirName() == default_user_data_dir &&
      profile_path.BaseName().value() ==
          base::ASCIIToUTF16(chrome::kInitialProfile)) {
    return base::string16();
  }

  // Get joined basenames of user data dir and profile.
  base::string16 basenames = profile_path.DirName().BaseName().value() +
      L"." + profile_path.BaseName().value();

  base::string16 profile_id;
  profile_id.reserve(basenames.size());

  // Generate profile_id from sanitized basenames.
  for (size_t i = 0; i < basenames.length(); ++i) {
    if (base::IsAsciiAlpha(basenames[i]) ||
        base::IsAsciiDigit(basenames[i]) ||
        basenames[i] == L'.')
      profile_id += basenames[i];
  }

  return profile_id;
}

base::string16 GetAppListAppName() {
  static const base::char16 kAppListAppNameSuffix[] = L"AppList";
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 app_name(dist->GetBaseAppId());
  app_name.append(kAppListAppNameSuffix);
  return app_name;
}

// Gets expected app id for given Chrome (based on |command_line| and
// |is_per_user_install|).
base::string16 GetExpectedAppId(const base::CommandLine& command_line,
                                bool is_per_user_install) {
  base::FilePath user_data_dir;
  if (command_line.HasSwitch(switches::kUserDataDir))
    user_data_dir = command_line.GetSwitchValuePath(switches::kUserDataDir);
  else
    chrome::GetDefaultUserDataDirectory(&user_data_dir);
  // Adjust with any policy that overrides any other way to set the path.
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
  DCHECK(!user_data_dir.empty());

  base::FilePath profile_subdir;
  if (command_line.HasSwitch(switches::kProfileDirectory)) {
    profile_subdir =
        command_line.GetSwitchValuePath(switches::kProfileDirectory);
  } else {
    profile_subdir =
        base::FilePath(base::ASCIIToUTF16(chrome::kInitialProfile));
  }
  DCHECK(!profile_subdir.empty());

  base::FilePath profile_path = user_data_dir.Append(profile_subdir);
  base::string16 app_name;
  if (command_line.HasSwitch(switches::kApp)) {
    app_name = base::UTF8ToUTF16(web_app::GenerateApplicationNameFromURL(
        GURL(command_line.GetSwitchValueASCII(switches::kApp))));
  } else if (command_line.HasSwitch(switches::kAppId)) {
    app_name = base::UTF8ToUTF16(
        web_app::GenerateApplicationNameFromExtensionId(
            command_line.GetSwitchValueASCII(switches::kAppId)));
  } else if (command_line.HasSwitch(switches::kShowAppList)) {
    app_name = GetAppListAppName();
  } else {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    app_name = ShellUtil::GetBrowserModelId(dist, is_per_user_install);
  }
  DCHECK(!app_name.empty());

  return win::GetAppModelIdForProfile(app_name, profile_path);
}

void MigrateTaskbarPinsCallback() {
  // This should run on the file thread.
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;

  base::FilePath pins_path;
  if (!PathService::Get(base::DIR_TASKBAR_PINS, &pins_path)) {
    NOTREACHED();
    return;
  }

  win::MigrateShortcutsInPathInternal(chrome_exe, pins_path);
}

// Windows 8 introduced a new protocol->executable binding system which cannot
// be retrieved in the HKCR registry subkey method implemented below. We call
// AssocQueryString with the new Win8-only flag ASSOCF_IS_PROTOCOL instead.
base::string16 GetAppForProtocolUsingAssocQuery(const GURL& url) {
  base::string16 url_scheme = base::ASCIIToUTF16(url.scheme());
  // Don't attempt to query protocol association on an empty string.
  if (url_scheme.empty())
    return base::string16();

  // Query AssocQueryString for a human-readable description of the program
  // that will be invoked given the provided URL spec. This is used only to
  // populate the external protocol dialog box the user sees when invoking
  // an unknown external protocol.
  wchar_t out_buffer[1024];
  DWORD buffer_size = arraysize(out_buffer);
  HRESULT hr = AssocQueryString(ASSOCF_IS_PROTOCOL,
                                ASSOCSTR_FRIENDLYAPPNAME,
                                url_scheme.c_str(),
                                NULL,
                                out_buffer,
                                &buffer_size);
  if (FAILED(hr)) {
    DLOG(WARNING) << "AssocQueryString failed!";
    return base::string16();
  }
  return base::string16(out_buffer);
}

base::string16 GetAppForProtocolUsingRegistry(const GURL& url) {
  base::string16 command_to_launch;

  // First, try and extract the application's display name.
  base::string16 cmd_key_path = base::ASCIIToUTF16(url.scheme());
  base::win::RegKey cmd_key_name(HKEY_CLASSES_ROOT, cmd_key_path.c_str(),
                                 KEY_READ);
  if (cmd_key_name.ReadValue(NULL, &command_to_launch) == ERROR_SUCCESS &&
      !command_to_launch.empty()) {
    return command_to_launch;
  }

  // Otherwise, parse the command line in the registry, and return the basename
  // of the program path if it exists.
  cmd_key_path = base::ASCIIToUTF16(url.scheme() + "\\shell\\open\\command");
  base::win::RegKey cmd_key_exe(HKEY_CLASSES_ROOT, cmd_key_path.c_str(),
                                KEY_READ);
  if (cmd_key_exe.ReadValue(NULL, &command_to_launch) == ERROR_SUCCESS) {
    base::CommandLine command_line(
        base::CommandLine::FromString(command_to_launch));
    return command_line.GetProgram().BaseName().value();
  }

  return base::string16();
}

DefaultWebClientState GetDefaultWebClientStateFromShellUtilDefaultState(
    ShellUtil::DefaultState default_state) {
  switch (default_state) {
    case ShellUtil::NOT_DEFAULT:
      return DefaultWebClientState::NOT_DEFAULT;
    case ShellUtil::IS_DEFAULT:
      return DefaultWebClientState::IS_DEFAULT;
    default:
      DCHECK_EQ(ShellUtil::UNKNOWN_DEFAULT, default_state);
      return DefaultWebClientState::UNKNOWN_DEFAULT;
  }
}

// A recorder of user actions in the Windows Settings app.
class DefaultBrowserActionRecorder : public win::SettingsAppMonitor::Delegate {
 public:
  // Creates the recorder and the monitor that drives it. |continuation| will be
  // run once the monitor's initialization completes (regardless of success or
  // failure).
  explicit DefaultBrowserActionRecorder(base::Closure continuation)
      : continuation_(std::move(continuation)), settings_app_monitor_(this) {}

 private:
  // win::SettingsAppMonitor::Delegate:
  void OnInitialized(HRESULT result) override {
    UMA_HISTOGRAM_BOOLEAN("SettingsAppMonitor.InitializationResult",
                          SUCCEEDED(result));
    if (SUCCEEDED(result)) {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.Initialized"));
    }
    continuation_.Run();
    continuation_ = base::Closure();
  }

  void OnAppFocused() override {
    base::RecordAction(
        base::UserMetricsAction("SettingsAppMonitor.AppFocused"));
  }

  void OnChooserInvoked() override {
    base::RecordAction(
        base::UserMetricsAction("SettingsAppMonitor.ChooserInvoked"));
  }

  void OnBrowserChosen(const base::string16& browser_name) override {
    if (browser_name ==
        BrowserDistribution::GetDistribution()->GetDisplayName()) {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.ChromeBrowserChosen"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.OtherBrowserChosen"));
    }
  }

  // A closure to be run once initialization completes.
  base::Closure continuation_;

  // Monitors user interaction with the Windows Settings app for the sake of
  // reporting user actions.
  win::SettingsAppMonitor settings_app_monitor_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserActionRecorder);
};

// A function bound up in a callback with a DefaultBrowserActionRecorder and
// a closure to keep the former alive until the time comes to run the latter.
void OnSettingsAppFinished(
    std::unique_ptr<DefaultBrowserActionRecorder> recorder,
    const base::Closure& on_finished_callback) {
  recorder.reset();
  on_finished_callback.Run();
}

// There is no way to make sure the user is done with the system settings, but a
// signal that the interaction is finished is needed for UMA. A timer of 2
// minutes is used as a substitute. The registry keys for the protocol
// association with an app are also monitored to signal the end of the
// interaction early when it is clear that the user made a choice (e.g. http
// and https for default browser).
//
// This helper class manages both the timer and the registry watchers and makes
// sure the callback for the end of the settings interaction is only run once.
// This class also manages its own lifetime.
class OpenSystemSettingsHelper {
 public:
  // Begin the monitoring and will call |on_finished_callback| when done.
  // Takes in a null-terminated array of |protocols| whose registry keys must be
  // watched. The array must contain at least one element.
  static void Begin(const wchar_t* const protocols[],
                    const base::Closure& on_finished_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    delete instance_;
    instance_ = new OpenSystemSettingsHelper(protocols, on_finished_callback);
  }

 private:
  // The reason the settings interaction concluded. Do not modify the ordering
  // because it is used for UMA.
  enum ConcludeReason { REGISTRY_WATCHER, TIMEOUT, NUM_CONCLUDE_REASON_TYPES };

  OpenSystemSettingsHelper(const wchar_t* const protocols[],
                           const base::Closure& on_finished_callback)
      : scoped_user_protocol_entry_(protocols[0]),
        on_finished_callback_(on_finished_callback),
        weak_ptr_factory_(this) {
    static const wchar_t kUrlAssociationFormat[] =
        L"SOFTWARE\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
        L"%ls\\UserChoice";

    // Remember the start time.
    start_time_ = base::TimeTicks::Now();

    for (const wchar_t* const* scan = &protocols[0]; *scan != nullptr; ++scan) {
      AddRegistryKeyWatcher(
          base::StringPrintf(kUrlAssociationFormat, *scan).c_str());
    }
    // Only the watchers that were succesfully initialized are counted.
    registry_watcher_count_ = registry_key_watchers_.size();

    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMinutes(2),
        base::Bind(&OpenSystemSettingsHelper::ConcludeInteraction,
                   weak_ptr_factory_.GetWeakPtr(), ConcludeReason::TIMEOUT));
  }

  // Called when a change is detected on one of the registry keys being watched.
  // Note: All types of modification to the registry key will trigger this
  //       function even if the value change is the only one that matters. This
  //       is good enough for now.
  void OnRegistryKeyChanged() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    // Make sure all the registry watchers have fired.
    if (--registry_watcher_count_ == 0) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.SettingsInteraction.RegistryWatcherDuration",
          base::TimeTicks::Now() - start_time_);

      ConcludeInteraction(ConcludeReason::REGISTRY_WATCHER);
    }
  }

  // Ends the monitoring with the system settings. Will call
  // |on_finished_callback_| and then dispose of this class instance to make
  // sure the callback won't get called subsequently.
  void ConcludeInteraction(ConcludeReason conclude_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    UMA_HISTOGRAM_ENUMERATION(
        "DefaultBrowser.SettingsInteraction.ConcludeReason", conclude_reason,
        NUM_CONCLUDE_REASON_TYPES);
    on_finished_callback_.Run();
    delete instance_;
    instance_ = nullptr;
  }

  // Helper function to create a registry watcher for a given |key_path|. Do
  // nothing on initialization failure.
  void AddRegistryKeyWatcher(const wchar_t* key_path) {
    auto reg_key = base::MakeUnique<base::win::RegKey>(HKEY_CURRENT_USER,
                                                       key_path, KEY_NOTIFY);

    if (reg_key->Valid() &&
        reg_key->StartWatching(
            base::Bind(&OpenSystemSettingsHelper::OnRegistryKeyChanged,
                       weak_ptr_factory_.GetWeakPtr()))) {
      registry_key_watchers_.push_back(std::move(reg_key));
    }
  }

  // Used to make sure only one instance is alive at the same time.
  static OpenSystemSettingsHelper* instance_;

  // This is needed to make sure that Windows displays an entry for the protocol
  // inside the "Choose default apps by protocol" settings page.
  ScopedUserProtocolEntry scoped_user_protocol_entry_;

  // The function to call when the interaction with the system settings is
  // finished.
  base::Closure on_finished_callback_;

  // The number of time the registry key watchers must fire.
  int registry_watcher_count_ = 0;

  // There can be multiple registry key watchers as some settings modify
  // multiple protocol associations. e.g. Changing the default browser modifies
  // the http and https associations.
  std::vector<std::unique_ptr<base::win::RegKey>> registry_key_watchers_;

  base::OneShotTimer timer_;

  // Records the time it takes for the final registry watcher to get signaled.
  base::TimeTicks start_time_;

  // Weak ptrs are used to bind this class to the callbacks of the timer and the
  // registry watcher. This makes it possible to self-delete after one of the
  // callbacks is executed to cancel the remaining ones.
  base::WeakPtrFactory<OpenSystemSettingsHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenSystemSettingsHelper);
};

OpenSystemSettingsHelper* OpenSystemSettingsHelper::instance_ = nullptr;

void RecordPinnedToTaskbarProcessError(bool error) {
  UMA_HISTOGRAM_BOOLEAN("Windows.IsPinnedToTaskbar.ProcessError", error);
}

// Record the UMA histogram when a response is received. The callback that binds
// to this function holds a reference to the ShellHandlerClient to keep it alive
// until invokation.
void OnIsPinnedToTaskbarResult(
    content::UtilityProcessMojoClient<chrome::mojom::ShellHandler>* client,
    bool succeeded,
    bool is_pinned_to_taskbar) {
  // Clean up the utility process.
  delete client;

  RecordPinnedToTaskbarProcessError(false);

  enum Result { NOT_PINNED, PINNED, FAILURE, NUM_RESULTS };

  Result result = FAILURE;
  if (succeeded)
    result = is_pinned_to_taskbar ? PINNED : NOT_PINNED;
  UMA_HISTOGRAM_ENUMERATION("Windows.IsPinnedToTaskbar", result, NUM_RESULTS);
}

// Called when a connection error happen with the shell handler process. A call
// to this function is mutially exclusive with a call to
// OnIsPinnedToTaskbarResult().
void OnShellHandlerConnectionError(
    content::UtilityProcessMojoClient<chrome::mojom::ShellHandler>* client) {
  // Clean up the utility process.
  delete client;

  RecordPinnedToTaskbarProcessError(true);
}

}  // namespace

bool SetAsDefaultBrowser() {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  // From UI currently we only allow setting default browser for current user.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::MakeChromeDefault(dist, ShellUtil::CURRENT_USER, chrome_exe,
                                    true /* elevate_if_not_admin */)) {
    LOG(ERROR) << "Chrome could not be set as default browser.";
    return false;
  }

  VLOG(1) << "Chrome registered as default browser.";
  return true;
}

bool SetAsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return false;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::MakeChromeDefaultProtocolClient(dist, chrome_exe,
                                                  wprotocol)) {
    LOG(ERROR) << "Chrome could not be set as default handler for "
               << protocol << ".";
    return false;
  }

  VLOG(1) << "Chrome registered as default handler for " << protocol << ".";
  return true;
}

DefaultWebClientSetPermission GetDefaultWebClientSetPermission() {
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  if (distribution->GetDefaultBrowserControlPolicy() !=
          BrowserDistribution::DEFAULT_BROWSER_FULL_CONTROL)
    return SET_DEFAULT_NOT_ALLOWED;
  if (ShellUtil::CanMakeChromeDefaultUnattended())
    return SET_DEFAULT_UNATTENDED;
  // Windows 8 and 10 both introduced a new way to set the default web client
  // which require user interaction.
  return SET_DEFAULT_INTERACTIVE;
}

bool IsElevationNeededForSettingDefaultProtocolClient() {
  return base::win::GetVersion() < base::win::VERSION_WIN8;
}

base::string16 GetApplicationNameForProtocol(const GURL& url) {
  base::string16 application_name;
  // Windows 8 or above has a new protocol association query.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    application_name = GetAppForProtocolUsingAssocQuery(url);
    if (!application_name.empty())
      return application_name;
  }

  return GetAppForProtocolUsingRegistry(url);
}

DefaultWebClientState GetDefaultBrowser() {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultState());
}

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Firefox is default browser by checking these
// locations and returns true if Firefox traces are found there. In case of
// error (or if Firefox is not found)it returns the default value which
// is false.
bool IsFirefoxDefaultBrowser() {
  bool ff_default = false;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    base::string16 app_cmd;
    base::win::RegKey key(HKEY_CURRENT_USER,
                          ShellUtil::kRegVistaUrlPrefs, KEY_READ);
    if (key.Valid() && (key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS) &&
        app_cmd == L"FirefoxURL")
      ff_default = true;
  } else {
    base::string16 key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    base::win::RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    base::string16 app_cmd;
    if (key.Valid() && (key.ReadValue(L"", &app_cmd) == ERROR_SUCCESS) &&
        base::string16::npos !=
        base::ToLowerASCII(app_cmd).find(L"firefox"))
      ff_default = true;
  }
  return ff_default;
}

DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultProtocolClientState(
          base::UTF8ToUTF16(protocol)));
}

namespace win {

bool SetAsDefaultBrowserUsingIntentPicker() {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::ShowMakeChromeDefaultSystemUI(dist, chrome_exe)) {
    LOG(ERROR) << "Failed to launch the set-default-browser Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-browser Windows UI completed.";
  return true;
}

void SetAsDefaultBrowserUsingSystemSettings(
    const base::Closure& on_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    on_finished_callback.Run();
    return;
  }

  // Create an action recorder that will open the settings app once it has
  // initialized.
  std::unique_ptr<DefaultBrowserActionRecorder> recorder(
      new DefaultBrowserActionRecorder(base::Bind(
          base::IgnoreResult(&ShellUtil::ShowMakeChromeDefaultSystemUI),
          base::Unretained(BrowserDistribution::GetDistribution()),
          chrome_exe)));

  // The helper manages its own lifetime. Bind the action recorder
  // into the finished callback to keep it alive throughout the
  // interaction.
  static const wchar_t* const kProtocols[] = {L"http", L"https", nullptr};
  OpenSystemSettingsHelper::Begin(
      kProtocols, base::Bind(&OnSettingsAppFinished, base::Passed(&recorder),
                             on_finished_callback));
}

bool SetAsDefaultProtocolClientUsingIntentPicker(const std::string& protocol) {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  if (!ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(dist, chrome_exe,
                                                              wprotocol)) {
    LOG(ERROR) << "Failed to launch the set-default-client Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-client Windows UI completed.";
  return true;
}

void SetAsDefaultProtocolClientUsingSystemSettings(
    const std::string& protocol,
    const base::Closure& on_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    on_finished_callback.Run();
    return;
  }

  // The helper manages its own lifetime.
  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  const wchar_t* const kProtocols[] = {wprotocol.c_str(), nullptr};
  OpenSystemSettingsHelper::Begin(kProtocols, on_finished_callback);

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(dist, chrome_exe,
                                                         wprotocol);
}

base::string16 GetAppModelIdForProfile(const base::string16& app_name,
                                       const base::FilePath& profile_path) {
  std::vector<base::string16> components;
  components.push_back(app_name);
  const base::string16 profile_id(GetProfileIdFromPath(profile_path));
  if (!profile_id.empty())
    components.push_back(profile_id);
  return ShellUtil::BuildAppModelId(components);
}

base::string16 GetChromiumModelIdForProfile(
    const base::FilePath& profile_path) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return dist->GetBaseAppId();
  }
  return GetAppModelIdForProfile(
      ShellUtil::GetBrowserModelId(dist,
                                   InstallUtil::IsPerUserInstall(chrome_exe)),
      profile_path);
}

void MigrateTaskbarPins() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // This needs to happen eventually (e.g. so that the appid is fixed and the
  // run-time Chrome icon is merged with the taskbar shortcut), but this is not
  // urgent and shouldn't delay Chrome startup.
  static const int64_t kMigrateTaskbarPinsDelaySeconds = 15;
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MigrateTaskbarPinsCallback),
      base::TimeDelta::FromSeconds(kMigrateTaskbarPinsDelaySeconds));
}

void RecordIsPinnedToTaskbarHistogram() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // The code to check if Chrome is pinned to the taskbar brings in shell
  // extensions which can hinder stability so it is executed in a utility
  // process.
  content::UtilityProcessMojoClient<chrome::mojom::ShellHandler>* client =
      new content::UtilityProcessMojoClient<chrome::mojom::ShellHandler>(
          l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_SHELL_HANDLER_NAME));

  client->set_error_callback(
      base::Bind(&OnShellHandlerConnectionError, client));
  client->set_disable_sandbox();
  client->Start();

  client->service()->IsPinnedToTaskbar(
      base::Bind(&OnIsPinnedToTaskbarResult, client));
}

int MigrateShortcutsInPathInternal(const base::FilePath& chrome_exe,
                                   const base::FilePath& path) {
  DCHECK(base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Enumerate all pinned shortcuts in the given path directly.
  base::FileEnumerator shortcuts_enum(
      path, false,  // not recursive
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.lnk"));

  bool is_per_user_install = InstallUtil::IsPerUserInstall(chrome_exe);

  int shortcuts_migrated = 0;
  base::FilePath target_path;
  base::string16 arguments;
  base::win::ScopedPropVariant propvariant;
  for (base::FilePath shortcut = shortcuts_enum.Next(); !shortcut.empty();
       shortcut = shortcuts_enum.Next()) {
    // TODO(gab): Use ProgramCompare instead of comparing FilePaths below once
    // it is fixed to work with FilePaths with spaces.
    if (!base::win::ResolveShortcut(shortcut, &target_path, &arguments) ||
        chrome_exe != target_path) {
      continue;
    }
    base::CommandLine command_line(
        base::CommandLine::FromString(base::StringPrintf(
            L"\"%ls\" %ls", target_path.value().c_str(), arguments.c_str())));

    // Get the expected AppId for this Chrome shortcut.
    base::string16 expected_app_id(
        GetExpectedAppId(command_line, is_per_user_install));
    if (expected_app_id.empty())
      continue;

    // Load the shortcut.
    base::win::ScopedComPtr<IShellLink> shell_link;
    base::win::ScopedComPtr<IPersistFile> persist_file;
    if (FAILED(shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER)) ||
        FAILED(persist_file.QueryFrom(shell_link.get())) ||
        FAILED(persist_file->Load(shortcut.value().c_str(), STGM_READ))) {
      DLOG(WARNING) << "Failed loading shortcut at " << shortcut.value();
      continue;
    }

    // Any properties that need to be updated on the shortcut will be stored in
    // |updated_properties|.
    base::win::ShortcutProperties updated_properties;

    // Validate the existing app id for the shortcut.
    base::win::ScopedComPtr<IPropertyStore> property_store;
    propvariant.Reset();
    if (FAILED(property_store.QueryFrom(shell_link.get())) ||
        property_store->GetValue(PKEY_AppUserModel_ID, propvariant.Receive()) !=
            S_OK) {
      // When in doubt, prefer not updating the shortcut.
      NOTREACHED();
      continue;
    } else {
      switch (propvariant.get().vt) {
        case VT_EMPTY:
          // If there is no app_id set, set our app_id if one is expected.
          if (!expected_app_id.empty())
            updated_properties.set_app_id(expected_app_id);
          break;
        case VT_LPWSTR:
          if (expected_app_id != base::string16(propvariant.get().pwszVal))
            updated_properties.set_app_id(expected_app_id);
          break;
        default:
          NOTREACHED();
          continue;
      }
    }

    // Clear dual_mode property from any shortcuts that previously had it (it
    // was only ever installed on shortcuts with the
    // |default_chromium_model_id|).
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    base::string16 default_chromium_model_id(
        ShellUtil::GetBrowserModelId(dist, is_per_user_install));
    if (expected_app_id == default_chromium_model_id) {
      propvariant.Reset();
      if (property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                   propvariant.Receive()) != S_OK) {
        // When in doubt, prefer to not update the shortcut.
        NOTREACHED();
        continue;
      }
      if (propvariant.get().vt == VT_BOOL &&
                 !!propvariant.get().boolVal) {
        updated_properties.set_dual_mode(false);
      }
    }

    persist_file.Release();
    shell_link.Release();

    // Update the shortcut if some of its properties need to be updated.
    if (updated_properties.options &&
        base::win::CreateOrUpdateShortcutLink(
            shortcut, updated_properties,
            base::win::SHORTCUT_UPDATE_EXISTING)) {
      ++shortcuts_migrated;
    }
  }
  return shortcuts_migrated;
}

base::FilePath GetStartMenuShortcut(const base::FilePath& chrome_exe) {
  static const int kFolderIds[] = {
    base::DIR_COMMON_START_MENU,
    base::DIR_START_MENU,
  };
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const base::string16 shortcut_name(dist->GetShortcutName() +
                                     installer::kLnkExt);
  base::FilePath programs_folder;
  base::FilePath shortcut;

  // Check both the common and the per-user Start Menu folders for system-level
  // installs.
  size_t folder = InstallUtil::IsPerUserInstall(chrome_exe) ? 1 : 0;
  for (; folder < arraysize(kFolderIds); ++folder) {
    if (!PathService::Get(kFolderIds[folder], &programs_folder)) {
      NOTREACHED();
      continue;
    }

    shortcut = programs_folder.Append(shortcut_name);
    if (base::PathExists(shortcut))
      return shortcut;
  }

  return base::FilePath();
}

}  // namespace win

}  // namespace shell_integration

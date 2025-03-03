// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system_impl.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_tokenizer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/shared_user_script_master.h"
#include "chrome/browser/extensions/state_store_notification_observer.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/update_install_gate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/uninstall_ping_sender.h"
#include "extensions/browser/value_store/value_store_factory_impl.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_url_handlers.h"

#if defined(ENABLE_NOTIFICATIONS)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "ui/message_center/notifier_settings.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_install_gate.h"
#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/chromeos/extensions/signin_screen_policy_provider.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

// Helper to serve as an UninstallPingSender::Filter callback.
UninstallPingSender::FilterResult ShouldSendUninstallPing(
    const Extension* extension,
    UninstallReason reason) {
  if (extension && (extension->from_webstore() ||
                    ManifestURL::UpdatesFromGallery(extension))) {
    return UninstallPingSender::SEND_PING;
  }
  return UninstallPingSender::DO_NOT_SEND_PING;
}

}  // namespace

//
// ExtensionSystemImpl::Shared
//

ExtensionSystemImpl::Shared::Shared(Profile* profile)
    : profile_(profile) {
}

ExtensionSystemImpl::Shared::~Shared() {
}

void ExtensionSystemImpl::Shared::InitPrefs() {
  store_factory_ = new ValueStoreFactoryImpl(profile_->GetPath());

  // Two state stores. The latter, which contains declarative rules, must be
  // loaded immediately so that the rules are ready before we issue network
  // requests.
  state_store_.reset(new StateStore(
      profile_, store_factory_, ValueStoreFrontend::BackendType::STATE, true));
  state_store_notification_observer_.reset(
      new StateStoreNotificationObserver(state_store_.get()));

  rules_store_.reset(new StateStore(
      profile_, store_factory_, ValueStoreFrontend::BackendType::RULES, false));

#if defined(OS_CHROMEOS)
  // We can not perform check for Signin Profile here, as it would result in
  // recursive call upon creation of Signin Profile, so we will create
  // SigninScreenPolicyProvider lazily in RegisterManagementPolicyProviders.

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  policy::DeviceLocalAccount::Type device_local_account_type;
  if (user &&
      policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                       &device_local_account_type)) {
    device_local_account_management_policy_provider_.reset(
        new chromeos::DeviceLocalAccountManagementPolicyProvider(
            device_local_account_type));
  }
#endif
}

void ExtensionSystemImpl::Shared::RegisterManagementPolicyProviders() {
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());

#if defined(OS_CHROMEOS)
  // Lazy creation of SigninScreenPolicyProvider.
  if (!signin_screen_policy_provider_) {
    if (chromeos::ProfileHelper::IsSigninProfile(profile_)) {
      signin_screen_policy_provider_.reset(
          new chromeos::SigninScreenPolicyProvider());
    }
  }

  if (device_local_account_management_policy_provider_) {
    management_policy_->RegisterProvider(
        device_local_account_management_policy_provider_.get());
  }
  if (signin_screen_policy_provider_)
    management_policy_->RegisterProvider(signin_screen_policy_provider_.get());
#endif  // defined(OS_CHROMEOS)

  management_policy_->RegisterProvider(InstallVerifier::Get(profile_));
}

void ExtensionSystemImpl::Shared::InitInstallGates() {
  update_install_gate_.reset(new UpdateInstallGate(extension_service_.get()));
  extension_service_->RegisterInstallGate(
      ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE, update_install_gate_.get());
  extension_service_->RegisterInstallGate(
      ExtensionPrefs::DELAY_REASON_GC,
      ExtensionGarbageCollector::Get(profile_));
  extension_service_->RegisterInstallGate(
      ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
      extension_service_->shared_module_service());
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode()) {
    kiosk_app_update_install_gate_.reset(
        new chromeos::KioskAppUpdateInstallGate(profile_));
    extension_service_->RegisterInstallGate(
        ExtensionPrefs::DELAY_REASON_WAIT_FOR_OS_UPDATE,
        kiosk_app_update_install_gate_.get());
  }
#endif
}

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  TRACE_EVENT0("browser,startup", "ExtensionSystemImpl::Shared::Init");
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  navigation_observer_.reset(new NavigationObserver(profile_));

  bool allow_noisy_errors = !command_line->HasSwitch(switches::kNoErrorDialogs);
  ExtensionErrorReporter::Init(allow_noisy_errors);

  content_verifier_ = new ContentVerifier(
      profile_, new ChromeContentVerifierDelegate(profile_));

  service_worker_manager_.reset(new ServiceWorkerManager(profile_));

  shared_user_script_master_.reset(new SharedUserScriptMaster(profile_));

  // ExtensionService depends on RuntimeData.
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));

  bool autoupdate_enabled = !profile_->IsGuestSession() &&
                            !profile_->IsSystemProfile();
#if defined(OS_CHROMEOS)
  if (!extensions_enabled)
    autoupdate_enabled = false;
#endif  // defined(OS_CHROMEOS)
  extension_service_.reset(new ExtensionService(
      profile_, base::CommandLine::ForCurrentProcess(),
      profile_->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefs::Get(profile_), Blacklist::Get(profile_),
      autoupdate_enabled, extensions_enabled, &ready_));

  uninstall_ping_sender_.reset(new UninstallPingSender(
      ExtensionRegistry::Get(profile_), base::Bind(&ShouldSendUninstallPing)));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    InstallVerifier::Get(profile_)->Init();
    ContentVerifierDelegate::Mode mode =
        ChromeContentVerifierDelegate::GetDefaultMode();
#if defined(OS_CHROMEOS)
    mode = std::max(mode, ContentVerifierDelegate::BOOTSTRAP);
#endif  // defined(OS_CHROMEOS)
    if (mode >= ContentVerifierDelegate::BOOTSTRAP)
      content_verifier_->Start();
    info_map()->SetContentVerifier(content_verifier_.get());

    management_policy_.reset(new ManagementPolicy);
    RegisterManagementPolicyProviders();
  }

  bool skip_session_extensions = false;
#if defined(OS_CHROMEOS)
  // Skip loading session extensions if we are not in a user session.
  skip_session_extensions = !chromeos::LoginState::Get()->IsUserLoggedIn();
  if (chrome::IsRunningInForcedAppMode()) {
    extension_service_->component_loader()->
        AddDefaultComponentExtensionsForKioskMode(skip_session_extensions);
  } else {
    extension_service_->component_loader()->AddDefaultComponentExtensions(
        skip_session_extensions);
  }
#else
  extension_service_->component_loader()->AddDefaultComponentExtensions(
      skip_session_extensions);
#endif
  if (command_line->HasSwitch(switches::kLoadComponentExtension)) {
    base::CommandLine::StringType path_list =
        command_line->GetSwitchValueNative(switches::kLoadComponentExtension);
    base::StringTokenizerT<base::CommandLine::StringType,
                           base::CommandLine::StringType::const_iterator>
        t(path_list, FILE_PATH_LITERAL(","));
    while (t.GetNext()) {
      // Load the component extension manifest synchronously.
      // Blocking the UI thread is acceptable here since
      // this flag designated for developers.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      extension_service_->component_loader()->AddOrReplace(
          base::FilePath(t.token()));
    }
  }

  app_sorting_.reset(new ChromeAppSorting(profile_));

  InitInstallGates();

  extension_service_->Init();

  // Make sure ExtensionSyncService is created.
  ExtensionSyncService::Get(profile_);

  // Make the chrome://extension-icon/ resource available.
  content::URLDataSource::Add(profile_, new ExtensionIconSource(profile_));

  quota_service_.reset(new QuotaService);
}

void ExtensionSystemImpl::Shared::Shutdown() {
  if (content_verifier_.get())
    content_verifier_->Shutdown();
  if (extension_service_)
    extension_service_->Shutdown();
}

ServiceWorkerManager* ExtensionSystemImpl::Shared::service_worker_manager() {
  return service_worker_manager_.get();
}

StateStore* ExtensionSystemImpl::Shared::state_store() {
  return state_store_.get();
}

StateStore* ExtensionSystemImpl::Shared::rules_store() {
  return rules_store_.get();
}

scoped_refptr<ValueStoreFactory> ExtensionSystemImpl::Shared::store_factory()
    const {
  return store_factory_;
}

ExtensionService* ExtensionSystemImpl::Shared::extension_service() {
  return extension_service_.get();
}

RuntimeData* ExtensionSystemImpl::Shared::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* ExtensionSystemImpl::Shared::management_policy() {
  return management_policy_.get();
}

SharedUserScriptMaster*
ExtensionSystemImpl::Shared::shared_user_script_master() {
  return shared_user_script_master_.get();
}

InfoMap* ExtensionSystemImpl::Shared::info_map() {
  if (!extension_info_map_.get())
    extension_info_map_ = new InfoMap();
  return extension_info_map_.get();
}

QuotaService* ExtensionSystemImpl::Shared::quota_service() {
  return quota_service_.get();
}

AppSorting* ExtensionSystemImpl::Shared::app_sorting() {
  return app_sorting_.get();
}

ContentVerifier* ExtensionSystemImpl::Shared::content_verifier() {
  return content_verifier_.get();
}

//
// ExtensionSystemImpl
//

ExtensionSystemImpl::ExtensionSystemImpl(Profile* profile)
    : profile_(profile) {
  shared_ = ExtensionSystemSharedFactory::GetForBrowserContext(profile);

  if (!profile->IsOffTheRecord()) {
    shared_->InitPrefs();
  }
}

ExtensionSystemImpl::~ExtensionSystemImpl() {
}

void ExtensionSystemImpl::Shutdown() {
}

void ExtensionSystemImpl::InitForRegularProfile(bool extensions_enabled) {
  TRACE_EVENT0("browser,startup", "ExtensionSystemImpl::InitForRegularProfile");
  DCHECK(!profile_->IsOffTheRecord());
  if (shared_user_script_master() || extension_service())
    return;  // Already initialized.

  // The InfoMap needs to be created before the ProcessManager.
  shared_->info_map();
  shared_->Init(extensions_enabled);
}

ExtensionService* ExtensionSystemImpl::extension_service() {
  return shared_->extension_service();
}

RuntimeData* ExtensionSystemImpl::runtime_data() {
  return shared_->runtime_data();
}

ManagementPolicy* ExtensionSystemImpl::management_policy() {
  return shared_->management_policy();
}

ServiceWorkerManager* ExtensionSystemImpl::service_worker_manager() {
  return shared_->service_worker_manager();
}

SharedUserScriptMaster* ExtensionSystemImpl::shared_user_script_master() {
  return shared_->shared_user_script_master();
}

StateStore* ExtensionSystemImpl::state_store() {
  return shared_->state_store();
}

StateStore* ExtensionSystemImpl::rules_store() {
  return shared_->rules_store();
}

scoped_refptr<ValueStoreFactory> ExtensionSystemImpl::store_factory() {
  return shared_->store_factory();
}

InfoMap* ExtensionSystemImpl::info_map() { return shared_->info_map(); }

const OneShotEvent& ExtensionSystemImpl::ready() const {
  return shared_->ready();
}

QuotaService* ExtensionSystemImpl::quota_service() {
  return shared_->quota_service();
}

AppSorting* ExtensionSystemImpl::app_sorting() {
  return shared_->app_sorting();
}

ContentVerifier* ExtensionSystemImpl::content_verifier() {
  return shared_->content_verifier();
}

std::unique_ptr<ExtensionSet> ExtensionSystemImpl::GetDependentExtensions(
    const Extension* extension) {
  return extension_service()->shared_module_service()->GetDependentExtensions(
      extension);
}

void ExtensionSystemImpl::InstallUpdate(const std::string& extension_id,
                                        const base::FilePath& temp_dir) {
  NOTREACHED() << "Not yet implemented";
  base::DeleteFile(temp_dir, true /* recursive */);
}

void ExtensionSystemImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension,
    const base::Closure& callback) {
  base::Time install_time;
  if (extension->location() != Manifest::COMPONENT) {
    install_time = ExtensionPrefs::Get(profile_)->
        GetInstallTime(extension->id());
  }
  bool incognito_enabled = util::IsIncognitoEnabled(extension->id(), profile_);

  bool notifications_disabled = false;
#if defined(ENABLE_NOTIFICATIONS)
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION,
      extension->id());

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile_);
  notifications_disabled =
      !notifier_state_tracker->IsNotifierEnabled(notifier_id);
#endif

  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 base::RetainedRef(extension), install_time, incognito_enabled,
                 notifications_disabled),
      callback);
}

void ExtensionSystemImpl::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::RemoveExtension, info_map(), extension_id, reason));
}

}  // namespace extensions

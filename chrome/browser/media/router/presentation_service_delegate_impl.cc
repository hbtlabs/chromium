// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_service_delegate_impl.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/small_map.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/presentation_media_sinks_observer.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::PresentationServiceDelegateImpl);

using content::RenderFrameHost;

namespace media_router {

namespace {

using DelegateObserver = content::PresentationServiceDelegate::Observer;

// Returns the unique identifier for the supplied RenderFrameHost.
RenderFrameHostId GetRenderFrameHostId(RenderFrameHost* render_frame_host) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();
  return RenderFrameHostId(render_process_id, render_frame_id);
}

// Gets the last committed URL for the render frame specified by
// |render_frame_host_id|.
GURL GetLastCommittedURLForFrame(RenderFrameHostId render_frame_host_id) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      render_frame_host_id.first, render_frame_host_id.second);
  // TODO(crbug.com/632623): Use url::Origin in place of GURL for origins
  return render_frame_host
             ? render_frame_host->GetLastCommittedOrigin().GetURL()
             : GURL();
}

// Observes messages originating from the MediaSink connected to a MediaRoute
// that represents a presentation. Converts the messages into
// content::PresentationSessionMessages and dispatches them via the provided
// PresentationSessionMessageCallback.
class PresentationSessionMessagesObserver : public RouteMessageObserver {
 public:
  // |message_cb|: The callback to invoke whenever messages are received.
  // |route_id|: ID of MediaRoute to listen for messages.
  PresentationSessionMessagesObserver(
      MediaRouter* router, const MediaRoute::Id& route_id,
      const content::PresentationSessionMessageCallback& message_cb)
      : RouteMessageObserver(router, route_id), message_cb_(message_cb) {
    DCHECK(!message_cb_.is_null());
  }

  ~PresentationSessionMessagesObserver() final {}

  void OnMessagesReceived(const std::vector<RouteMessage>& messages) final {
    DVLOG(2) << __FUNCTION__ << ", number of messages : " << messages.size();
    ScopedVector<content::PresentationSessionMessage> presentation_messages;
    for (const RouteMessage& message : messages) {
      if (message.type == RouteMessage::TEXT && message.text) {
        presentation_messages.push_back(new content::PresentationSessionMessage(
            content::PresentationMessageType::TEXT));
        presentation_messages.back()->message = *message.text;
      } else if (message.type == RouteMessage::BINARY && message.binary) {
        presentation_messages.push_back(new content::PresentationSessionMessage(
            content::PresentationMessageType::ARRAY_BUFFER));
        presentation_messages.back()->data.reset(
            new std::vector<uint8_t>(*message.binary));
      }
    }
    // TODO(miu): Remove second argument from PresentationSessionMessageCallback
    // since it's always true now.
    message_cb_.Run(presentation_messages, true);
  }

 private:
  const content::PresentationSessionMessageCallback message_cb_;

  DISALLOW_COPY_AND_ASSIGN(PresentationSessionMessagesObserver);
};

}  // namespace

// Used by PresentationServiceDelegateImpl to manage
// listeners and default presentation info in a render frame.
// Its lifetime:
//  * PresentationFrameManager AddDelegateObserver
//  * Reset 0+ times.
//  * PresentationFrameManager.RemoveDelegateObserver.
class PresentationFrame {
 public:
  PresentationFrame(const RenderFrameHostId& render_frame_host_id,
                    content::WebContents* web_contents,
                    MediaRouter* router);
  ~PresentationFrame();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool HasScreenAvailabilityListenerForTest(
      const MediaSource::Id& source_id) const;
  std::string GetDefaultPresentationId() const;
  void ListenForConnectionStateChange(
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const content::PresentationSessionInfo& session,
      const content::PresentationSessionMessageCallback& message_cb);

  void Reset();
  void RemoveConnection(const std::string& presentation_id,
                        const MediaRoute::Id& route_id);

  const MediaRoute::Id GetRouteId(const std::string& presentation_id) const;
  const std::vector<MediaRoute::Id> GetRouteIds() const;

  void OnPresentationSessionStarted(
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);
  void OnPresentationServiceDelegateDestroyed() const;

  void set_delegate_observer(DelegateObserver* observer) {
    delegate_observer_ = observer;
  }

 private:
  MediaSource GetMediaSourceFromListener(
      content::PresentationScreenAvailabilityListener* listener) const;
  base::SmallMap<std::map<std::string, MediaRoute::Id>>
      presentation_id_to_route_id_;
  base::SmallMap<
      std::map<std::string, std::unique_ptr<PresentationMediaSinksObserver>>>
      url_to_sinks_observer_;
  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateSubscription>>
      connection_state_subscriptions_;
  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationSessionMessagesObserver>>
      session_messages_observers_;

  RenderFrameHostId render_frame_host_id_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  const content::WebContents* web_contents_;
  MediaRouter* router_;

  DelegateObserver* delegate_observer_;
};

PresentationFrame::PresentationFrame(
    const RenderFrameHostId& render_frame_host_id,
    content::WebContents* web_contents,
    MediaRouter* router)
    : render_frame_host_id_(render_frame_host_id),
      web_contents_(web_contents),
      router_(router),
      delegate_observer_(nullptr) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrame::~PresentationFrame() {
}

void PresentationFrame::OnPresentationServiceDelegateDestroyed() const {
  if (delegate_observer_)
    delegate_observer_->OnDelegateDestroyed();
}

void PresentationFrame::OnPresentationSessionStarted(
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  presentation_id_to_route_id_[session.presentation_id] =
      route.media_route_id();
}

const MediaRoute::Id PresentationFrame::GetRouteId(
    const std::string& presentation_id) const {
  auto it = presentation_id_to_route_id_.find(presentation_id);
  return it != presentation_id_to_route_id_.end() ? it->second : "";
}

const std::vector<MediaRoute::Id> PresentationFrame::GetRouteIds() const {
  std::vector<MediaRoute::Id> route_ids;
  for (const auto& e : presentation_id_to_route_id_)
    route_ids.push_back(e.second);
  return route_ids;
}

bool PresentationFrame::SetScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source(GetMediaSourceFromListener(listener));
  auto& sinks_observer = url_to_sinks_observer_[source.id()];
  if (sinks_observer && sinks_observer->listener() == listener)
    return false;

  sinks_observer.reset(new PresentationMediaSinksObserver(
      router_, listener, source,
      GetLastCommittedURLForFrame(render_frame_host_id_).GetOrigin()));

  if (!sinks_observer->Init()) {
    url_to_sinks_observer_.erase(source.id());
    listener->OnScreenAvailabilityNotSupported();
    return false;
  }

  return true;
}

bool PresentationFrame::RemoveScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source(GetMediaSourceFromListener(listener));
  auto sinks_observer_it = url_to_sinks_observer_.find(source.id());
  if (sinks_observer_it != url_to_sinks_observer_.end() &&
      sinks_observer_it->second->listener() == listener) {
    url_to_sinks_observer_.erase(sinks_observer_it);
    return true;
  }
  return false;
}

bool PresentationFrame::HasScreenAvailabilityListenerForTest(
    const MediaSource::Id& source_id) const {
  return url_to_sinks_observer_.find(source_id) != url_to_sinks_observer_.end();
}

void PresentationFrame::Reset() {
  for (const auto& pid_route_id : presentation_id_to_route_id_)
    router_->DetachRoute(pid_route_id.second);

  presentation_id_to_route_id_.clear();
  url_to_sinks_observer_.clear();
  connection_state_subscriptions_.clear();
  session_messages_observers_.clear();
}

void PresentationFrame::RemoveConnection(const std::string& presentation_id,
                                         const MediaRoute::Id& route_id) {
  // Remove the presentation id mapping so a later call to Reset is a no-op.
  presentation_id_to_route_id_.erase(presentation_id);

  // We no longer need to observe route messages.
  session_messages_observers_.erase(route_id);

  // We keep the PresentationConnectionStateChangedCallback registered with MR
  // so the MRP can tell us when terminate() completed.
}

void PresentationFrame::ListenForConnectionStateChange(
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto it = presentation_id_to_route_id_.find(connection.presentation_id);
  if (it == presentation_id_to_route_id_.end()) {
    DLOG(ERROR) << __func__ << "route id not found for presentation: "
                << connection.presentation_id;
    return;
  }

  const MediaRoute::Id& route_id = it->second;
  if (connection_state_subscriptions_.find(route_id) !=
      connection_state_subscriptions_.end()) {
    DLOG(ERROR) << __func__
                << "Already listening connection state change for route: "
                << route_id;
    return;
  }

  connection_state_subscriptions_.insert(std::make_pair(
      route_id, router_->AddPresentationConnectionStateChangedCallback(
                    it->second, state_changed_cb)));
}

void PresentationFrame::ListenForSessionMessages(
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  auto it = presentation_id_to_route_id_.find(session.presentation_id);
  if (it == presentation_id_to_route_id_.end()) {
    DVLOG(2) << "ListenForSessionMessages: no route for "
             << session.presentation_id;
    return;
  }

  const MediaRoute::Id& route_id = it->second;
  if (session_messages_observers_.find(route_id) !=
      session_messages_observers_.end()) {
    DLOG(ERROR) << __func__
                << "Already listening for session messages for route: "
                << route_id;
    return;
  }

  session_messages_observers_.insert(std::make_pair(
      route_id, base::MakeUnique<PresentationSessionMessagesObserver>(
          router_, it->second, message_cb)));
}

MediaSource PresentationFrame::GetMediaSourceFromListener(
    content::PresentationScreenAvailabilityListener* listener) const {
  // If the default presentation URL is empty then fall back to tab mirroring.
  return listener->GetAvailabilityUrl().is_empty()
             ? MediaSourceForTab(SessionTabHelper::IdForTab(web_contents_))
             : MediaSourceForPresentationUrl(listener->GetAvailabilityUrl());
}

// Used by PresentationServiceDelegateImpl to manage PresentationFrames.
class PresentationFrameManager {
 public:
  PresentationFrameManager(content::WebContents* web_contents,
                           MediaRouter* router);
  ~PresentationFrameManager();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  void ListenForConnectionStateChange(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const content::PresentationSessionMessageCallback& message_cb);

  // Sets or clears the default presentation request and callback for the given
  // frame. Also sets / clears the default presentation requests for the owning
  // tab WebContents.
  void SetDefaultPresentationUrl(
      const RenderFrameHostId& render_frame_host_id,
      const GURL& default_presentation_url,
      const content::PresentationSessionStartedCallback& callback);
  void AddDelegateObserver(const RenderFrameHostId& render_frame_host_id,
                           DelegateObserver* observer);
  void RemoveDelegateObserver(const RenderFrameHostId& render_frame_host_id);
  void AddDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void RemoveDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void Reset(const RenderFrameHostId& render_frame_host_id);
  void RemoveConnection(const RenderFrameHostId& render_frame_host_id,
                        const MediaRoute::Id& route_id,
                        const std::string& presentation_id);
  bool HasScreenAvailabilityListenerForTest(
      const RenderFrameHostId& render_frame_host_id,
      const MediaSource::Id& source_id) const;
  void SetMediaRouterForTest(MediaRouter* router);

  void OnPresentationSessionStarted(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);
  void OnDefaultPresentationSessionStarted(
      const PresentationRequest& request,
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);

  const MediaRoute::Id GetRouteId(const RenderFrameHostId& render_frame_host_id,
                                  const std::string& presentation_id) const;
  const std::vector<MediaRoute::Id> GetRouteIds(
      const RenderFrameHostId& render_frame_host_id) const;

  const PresentationRequest* default_presentation_request() const {
    return default_presentation_request_.get();
  }

 private:
  PresentationFrame* GetOrAddPresentationFrame(
      const RenderFrameHostId& render_frame_host_id);

  // Sets the default presentation request for the owning WebContents and
  // notifies observers of changes.
  void SetDefaultPresentationRequest(
      const PresentationRequest& default_presentation_request);

  // Clears the default presentation request for the owning WebContents and
  // notifies observers of changes. Also resets
  // |default_presentation_started_callback_|.
  void ClearDefaultPresentationRequest();

  bool IsMainFrame(const RenderFrameHostId& render_frame_host_id) const;

  // Maps a frame identifier to a PresentationFrame object for frames
  // that are using presentation API.
  std::unordered_map<RenderFrameHostId, std::unique_ptr<PresentationFrame>,
                     RenderFrameHostIdHasher>
      presentation_frames_;

  // Default presentation request for the owning tab WebContents.
  std::unique_ptr<PresentationRequest> default_presentation_request_;

  // Callback to invoke when default presentation has started.
  content::PresentationSessionStartedCallback
      default_presentation_started_callback_;

  // References to the observers listening for changes to this tab WebContent's
  // default presentation.
  base::ObserverList<
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver>
      default_presentation_request_observers_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  MediaRouter* router_;
  content::WebContents* web_contents_;
};

PresentationFrameManager::PresentationFrameManager(
    content::WebContents* web_contents,
    MediaRouter* router)
    : router_(router), web_contents_(web_contents) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrameManager::~PresentationFrameManager() {
  for (auto& frame : presentation_frames_)
    frame.second->OnPresentationServiceDelegateDestroyed();
}

void PresentationFrameManager::OnPresentationSessionStarted(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->OnPresentationSessionStarted(session, route);
}

void PresentationFrameManager::OnDefaultPresentationSessionStarted(
    const PresentationRequest& request,
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  const auto it = presentation_frames_.find(request.render_frame_host_id());
  if (it != presentation_frames_.end())
    it->second->OnPresentationSessionStarted(session, route);

  if (default_presentation_request_ &&
      default_presentation_request_->Equals(request)) {
    default_presentation_started_callback_.Run(session);
  }
}

const MediaRoute::Id PresentationFrameManager::GetRouteId(
    const RenderFrameHostId& render_frame_host_id,
    const std::string& presentation_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end()
      ? it->second->GetRouteId(presentation_id) : MediaRoute::Id();
}

const std::vector<MediaRoute::Id> PresentationFrameManager::GetRouteIds(
    const RenderFrameHostId& render_frame_host_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() ? it->second->GetRouteIds()
                                          : std::vector<MediaRoute::Id>();
}

bool PresentationFrameManager::SetScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  return presentation_frame->SetScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::RemoveScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->RemoveScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::HasScreenAvailabilityListenerForTest(
    const RenderFrameHostId& render_frame_host_id,
    const MediaSource::Id& source_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->HasScreenAvailabilityListenerForTest(source_id);
}

void PresentationFrameManager::ListenForConnectionStateChange(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->ListenForConnectionStateChange(connection, state_changed_cb);
}

void PresentationFrameManager::ListenForSessionMessages(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it == presentation_frames_.end()) {
    DVLOG(2) << "ListenForSessionMessages: PresentationFrame does not exist "
             << "for: (" << render_frame_host_id.first << ", "
             << render_frame_host_id.second << ")";
    return;
  }
  it->second->ListenForSessionMessages(session, message_cb);
}

void PresentationFrameManager::SetDefaultPresentationUrl(
    const RenderFrameHostId& render_frame_host_id,
    const GURL& default_presentation_url,
    const content::PresentationSessionStartedCallback& callback) {
  if (!IsMainFrame(render_frame_host_id))
    return;

  if (default_presentation_url.is_empty()) {
    ClearDefaultPresentationRequest();
  } else {
    DCHECK(!callback.is_null());
    GURL frame_url(GetLastCommittedURLForFrame(render_frame_host_id));
    PresentationRequest request(render_frame_host_id,
                                std::vector<GURL>({default_presentation_url}),
                                frame_url);
    default_presentation_started_callback_ = callback;
    SetDefaultPresentationRequest(request);
  }
}

void PresentationFrameManager::AddDelegateObserver(
    const RenderFrameHostId& render_frame_host_id,
    DelegateObserver* observer) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->set_delegate_observer(observer);
}

void PresentationFrameManager::RemoveDelegateObserver(
    const RenderFrameHostId& render_frame_host_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end()) {
    it->second->set_delegate_observer(nullptr);
    presentation_frames_.erase(it);
  }
}

void PresentationFrameManager::AddDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.AddObserver(observer);
}

void PresentationFrameManager::RemoveDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.RemoveObserver(observer);
}

void PresentationFrameManager::Reset(
    const RenderFrameHostId& render_frame_host_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->Reset();

  if (default_presentation_request_ &&
      render_frame_host_id ==
          default_presentation_request_->render_frame_host_id()) {
    ClearDefaultPresentationRequest();
  }
}

void PresentationFrameManager::RemoveConnection(
    const RenderFrameHostId& render_frame_host_id,
    const MediaRoute::Id& route_id,
    const std::string& presentation_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemoveConnection(route_id, presentation_id);
}

PresentationFrame* PresentationFrameManager::GetOrAddPresentationFrame(
    const RenderFrameHostId& render_frame_host_id) {
  std::unique_ptr<PresentationFrame>& presentation_frame =
      presentation_frames_[render_frame_host_id];
  if (!presentation_frame) {
    presentation_frame.reset(new PresentationFrame(
        render_frame_host_id, web_contents_, router_));
  }
  return presentation_frame.get();
}

void PresentationFrameManager::ClearDefaultPresentationRequest() {
  default_presentation_started_callback_.Reset();
  if (!default_presentation_request_)
    return;

  default_presentation_request_.reset();
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationRemoved();
}

bool PresentationFrameManager::IsMainFrame(
    const RenderFrameHostId& render_frame_host_id) const {
  RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  return main_frame && GetRenderFrameHostId(main_frame) == render_frame_host_id;
}

void PresentationFrameManager::SetDefaultPresentationRequest(
    const PresentationRequest& default_presentation_request) {
  if (default_presentation_request_ &&
      default_presentation_request_->Equals(default_presentation_request))
    return;

  default_presentation_request_.reset(
      new PresentationRequest(default_presentation_request));
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationChanged(*default_presentation_request_);
}

void PresentationFrameManager::SetMediaRouterForTest(MediaRouter* router) {
  router_ = router;
}

PresentationServiceDelegateImpl*
PresentationServiceDelegateImpl::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  PresentationServiceDelegateImpl::CreateForWebContents(web_contents);
  return PresentationServiceDelegateImpl::FromWebContents(web_contents);
}

PresentationServiceDelegateImpl::PresentationServiceDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      router_(MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext())),
      frame_manager_(new PresentationFrameManager(web_contents, router_)),
      weak_factory_(this) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationServiceDelegateImpl::~PresentationServiceDelegateImpl() {
}

void PresentationServiceDelegateImpl::AddObserver(int render_process_id,
                                                  int render_frame_id,
                                                  DelegateObserver* observer) {
  DCHECK(observer);
  frame_manager_->AddDelegateObserver(
      RenderFrameHostId(render_process_id, render_frame_id), observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(int render_process_id,
                                                     int render_frame_id) {
  frame_manager_->RemoveDelegateObserver(
      RenderFrameHostId(render_process_id, render_frame_id));
}

bool PresentationServiceDelegateImpl::AddScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  return frame_manager_->SetScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::RemoveScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  frame_manager_->RemoveScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::Reset(int render_process_id,
                                            int render_frame_id) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->Reset(render_frame_host_id);
}

void PresentationServiceDelegateImpl::SetDefaultPresentationUrls(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& default_presentation_urls,
    const content::PresentationSessionStartedCallback& callback) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  if (default_presentation_urls.empty()) {
    frame_manager_->SetDefaultPresentationUrl(render_frame_host_id, GURL(),
                                              callback);
  } else {
    // TODO(crbug.com/627655): Handle multiple URLs.
    frame_manager_->SetDefaultPresentationUrl(
        render_frame_host_id, default_presentation_urls[0], callback);
  }
}

void PresentationServiceDelegateImpl::OnJoinRouteResponse(
    int render_process_id,
    int render_frame_id,
    const GURL& presentation_url,
    const std::string& presentation_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb,
    const RouteRequestResult& result) {
  if (!result.route()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND, result.error()));
  } else {
    DVLOG(1) << "OnJoinRouteResponse: "
             << "route_id: " << result.route()->media_route_id()
             << ", presentation URL: " << presentation_url
             << ", presentation ID: " << presentation_id;
    DCHECK_EQ(presentation_id, result.presentation_id());
    content::PresentationSessionInfo session(presentation_url,
                                             result.presentation_id());
    frame_manager_->OnPresentationSessionStarted(
        RenderFrameHostId(render_process_id, render_frame_id), session,
        *result.route());
    success_cb.Run(session);
  }
}

void PresentationServiceDelegateImpl::OnStartSessionSucceeded(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionInfo& new_session,
    const MediaRoute& route) {
  DVLOG(1) << "OnStartSessionSucceeded: "
           << "route_id: " << route.media_route_id()
           << ", presentation URL: " << new_session.presentation_url
           << ", presentation ID: " << new_session.presentation_id;
  frame_manager_->OnPresentationSessionStarted(
      RenderFrameHostId(render_process_id, render_frame_id), new_session,
      route);
  success_cb.Run(new_session);
}

void PresentationServiceDelegateImpl::StartSession(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& presentation_urls,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  if (presentation_urls.empty()) {
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Invalid presentation arguments."));
    return;
  }

  // TODO(crbug.com/627655): Handle multiple URLs.
  const GURL& presentation_url = presentation_urls[0];
  if (presentation_url.is_empty() ||
      !IsValidPresentationUrl(presentation_url)) {
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Invalid presentation arguments."));
    return;
  }

  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  std::unique_ptr<CreatePresentationConnectionRequest> request(
      new CreatePresentationConnectionRequest(
          render_frame_host_id, presentation_url,
          GetLastCommittedURLForFrame(render_frame_host_id),
          base::Bind(&PresentationServiceDelegateImpl::OnStartSessionSucceeded,
                     weak_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id, success_cb),
          error_cb));
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents_);
  if (!controller->ShowMediaRouterDialogForPresentation(std::move(request))) {
    LOG(ERROR) << "Media router dialog already exists. Ignoring StartSession.";
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Unable to create dialog."));
    return;
  }
}

void PresentationServiceDelegateImpl::JoinSession(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& presentation_urls,
    const std::string& presentation_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  if (presentation_urls.empty()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
        "Invalid presentation arguments."));
  }

  // TODO(crbug.com/627655): Handle multiple URLs.
  const GURL& presentation_url = presentation_urls[0];
  bool incognito = web_contents_->GetBrowserContext()->IsOffTheRecord();
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(
      base::Bind(&PresentationServiceDelegateImpl::OnJoinRouteResponse,
                 weak_factory_.GetWeakPtr(), render_process_id, render_frame_id,
                 presentation_url, presentation_id, success_cb, error_cb));
  router_->JoinRoute(
      MediaSourceForPresentationUrl(presentation_url).id(), presentation_id,
      GetLastCommittedURLForFrame(
          RenderFrameHostId(render_process_id, render_frame_id))
          .GetOrigin(),
      web_contents_, route_response_callbacks, base::TimeDelta(), incognito);
}

void PresentationServiceDelegateImpl::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }

  router_->DetachRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
  // TODO(mfoltz): close() should always succeed so there is no need to keep the
  // state_changed_cb around - remove it and fire the ChangeEvent on the
  // PresentationConnection in Blink.
}

void PresentationServiceDelegateImpl::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }
  router_->TerminateRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
}

void PresentationServiceDelegateImpl::ListenForSessionMessages(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  frame_manager_->ListenForSessionMessages(
      RenderFrameHostId(render_process_id, render_frame_id), session,
      message_cb);
}

void PresentationServiceDelegateImpl::SendMessage(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    std::unique_ptr<content::PresentationSessionMessage> message,
    const SendMessageCallback& send_message_cb) {
  const MediaRoute::Id& route_id = frame_manager_->GetRouteId(
      RenderFrameHostId(render_process_id, render_frame_id),
      session.presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for  " << session.presentation_id;
    send_message_cb.Run(false);
    return;
  }

  if (message->is_binary()) {
    router_->SendRouteBinaryMessage(route_id, std::move(message->data),
                                    send_message_cb);
  } else {
    router_->SendRouteMessage(route_id, message->message, send_message_cb);
  }
}

void PresentationServiceDelegateImpl::ListenForConnectionStateChange(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  frame_manager_->ListenForConnectionStateChange(
      RenderFrameHostId(render_process_id, render_frame_id), connection,
      state_changed_cb);
}

void PresentationServiceDelegateImpl::OnRouteResponse(
    const PresentationRequest& presentation_request,
    const RouteRequestResult& result) {
  if (!result.route())
    return;

  content::PresentationSessionInfo session_info(
      presentation_request.presentation_url(), result.presentation_id());
  frame_manager_->OnDefaultPresentationSessionStarted(
      presentation_request, session_info, *result.route());
}

void PresentationServiceDelegateImpl::AddDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->AddDefaultPresentationRequestObserver(observer);
}

void PresentationServiceDelegateImpl::RemoveDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->RemoveDefaultPresentationRequestObserver(observer);
}

PresentationRequest
PresentationServiceDelegateImpl::GetDefaultPresentationRequest() const {
  DCHECK(HasDefaultPresentationRequest());
  return *frame_manager_->default_presentation_request();
}

bool PresentationServiceDelegateImpl::HasDefaultPresentationRequest() const {
  return frame_manager_->default_presentation_request() != nullptr;
}

base::WeakPtr<PresentationServiceDelegateImpl>
PresentationServiceDelegateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PresentationServiceDelegateImpl::SetMediaRouterForTest(
    MediaRouter* router) {
  router_ = router;
  frame_manager_->SetMediaRouterForTest(router);
}

bool PresentationServiceDelegateImpl::HasScreenAvailabilityListenerForTest(
    int render_process_id,
    int render_frame_id,
    const MediaSource::Id& source_id) const {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  return frame_manager_->HasScreenAvailabilityListenerForTest(
      render_frame_host_id, source_id);
}

}  // namespace media_router

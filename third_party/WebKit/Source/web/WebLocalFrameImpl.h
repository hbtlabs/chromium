/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLocalFrameImpl_h
#define WebLocalFrameImpl_h

#include "core/editing/VisiblePosition.h"
#include "core/frame/LocalFrame.h"
#include "platform/geometry/FloatRect.h"
#include "platform/heap/SelfKeepAlive.h"
#include "public/platform/WebFileSystemType.h"
#include "public/web/WebLocalFrame.h"
#include "web/FrameLoaderClientImpl.h"
#include "web/UserMediaClientImpl.h"
#include "web/WebExport.h"
#include "web/WebFrameImplBase.h"
#include "web/WebFrameWidgetBase.h"
#include "wtf/Compiler.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ChromePrintContext;
class IntSize;
class KURL;
class Range;
class ScrollableArea;
class SharedWorkerRepositoryClientImpl;
class TextFinder;
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;
class WebAutofillClient;
class WebDataSourceImpl;
class WebDevToolsAgentImpl;
class WebDevToolsFrontendImpl;
class WebFrameClient;
class WebInputMethodControllerImpl;
class WebNode;
class WebPerformance;
class WebPlugin;
class WebPluginContainerImpl;
class WebScriptExecutionCallback;
class WebSuspendableTask;
class WebView;
class WebViewImpl;
enum class WebFrameLoadType;
struct FrameLoadRequest;
struct WebPrintParams;

template <typename T>
class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class WEB_EXPORT WebLocalFrameImpl final
    : public WebFrameImplBase,
      WTF_NON_EXPORTED_BASE(public WebLocalFrame) {
 public:
  // WebFrame methods:
  void close() override;
  WebString uniqueName() const override;
  WebString assignedName() const override;
  void setName(const WebString&) override;
  WebVector<WebIconURL> iconURLs(int iconTypesMask) const override;
  void setRemoteWebLayer(WebLayer*) override;
  void setContentSettingsClient(WebContentSettingsClient*) override;
  void setSharedWorkerRepositoryClient(
      WebSharedWorkerRepositoryClient*) override;
  WebSize scrollOffset() const override;
  void setScrollOffset(const WebSize&) override;
  WebSize contentsSize() const override;
  bool hasVisibleContent() const override;
  WebRect visibleContentRect() const override;
  bool hasHorizontalScrollbar() const override;
  bool hasVerticalScrollbar() const override;
  WebView* view() const override;
  WebDocument document() const override;
  WebPerformance performance() const override;
  void dispatchUnloadEvent() override;
  void executeScript(const WebScriptSource&) override;
  void executeScriptInIsolatedWorld(int worldID,
                                    const WebScriptSource* sources,
                                    unsigned numSources,
                                    int extensionGroup) override;
  void setIsolatedWorldSecurityOrigin(int worldID,
                                      const WebSecurityOrigin&) override;
  void setIsolatedWorldContentSecurityPolicy(int worldID,
                                             const WebString&) override;
  void setIsolatedWorldHumanReadableName(int worldID,
                                         const WebString&) override;
  void addMessageToConsole(const WebConsoleMessage&) override;
  void collectGarbage() override;
  v8::Local<v8::Value> executeScriptAndReturnValue(
      const WebScriptSource&) override;
  void requestExecuteScriptAndReturnValue(const WebScriptSource&,
                                          bool userGesture,
                                          WebScriptExecutionCallback*) override;
  void requestExecuteV8Function(v8::Local<v8::Context>,
                                v8::Local<v8::Function>,
                                v8::Local<v8::Value> receiver,
                                int argc,
                                v8::Local<v8::Value> argv[],
                                WebScriptExecutionCallback*) override;
  void executeScriptInIsolatedWorld(
      int worldID,
      const WebScriptSource* sourcesIn,
      unsigned numSources,
      int extensionGroup,
      WebVector<v8::Local<v8::Value>>* results) override;
  void requestExecuteScriptInIsolatedWorld(
      int worldID,
      const WebScriptSource* sourceIn,
      unsigned numSources,
      int extensionGroup,
      bool userGesture,
      WebScriptExecutionCallback*) override;
  v8::Local<v8::Value> callFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) override;
  v8::Local<v8::Context> mainWorldScriptContext() const override;
  void reload(WebFrameLoadType) override;
  void reloadWithOverrideURL(const WebURL& overrideUrl,
                             WebFrameLoadType) override;
  void reloadImage(const WebNode&) override;
  void reloadLoFiImages() override;
  void loadRequest(const WebURLRequest&) override;
  void loadHTMLString(const WebData& html,
                      const WebURL& baseURL,
                      const WebURL& unreachableURL,
                      bool replace) override;
  void stopLoading() override;
  WebDataSource* provisionalDataSource() const override;
  WebDataSource* dataSource() const override;
  void enableViewSourceMode(bool enable) override;
  bool isViewSourceModeEnabled() const override;
  void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
  void dispatchWillSendRequest(WebURLRequest&) override;
  WebAssociatedURLLoader* createAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) override;
  unsigned unloadListenerCount() const override;
  void setMarkedText(const WebString&,
                     unsigned location,
                     unsigned length) override;
  void unmarkText() override;
  bool hasMarkedText() const override;
  WebRange markedRange() const override;
  bool firstRectForCharacterRange(unsigned location,
                                  unsigned length,
                                  WebRect&) const override;
  size_t characterIndexForPoint(const WebPoint&) const override;
  bool executeCommand(const WebString&) override;
  bool executeCommand(const WebString&, const WebString& value) override;
  bool isCommandEnabled(const WebString&) const override;
  void enableSpellChecking(bool) override;
  bool isSpellCheckingEnabled() const override;
  void replaceMisspelledRange(const WebString&) override;
  void removeSpellingMarkers() override;
  bool hasSelection() const override;
  WebRange selectionRange() const override;
  WebString selectionAsText() const override;
  WebString selectionAsMarkup() const override;
  bool selectWordAroundCaret() override;
  void selectRange(const WebPoint& base, const WebPoint& extent) override;
  void selectRange(const WebRange&) override;
  WebString rangeAsText(const WebRange&) override;
  void moveRangeSelectionExtent(const WebPoint&) override;
  void moveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = CharacterGranularity) override;
  void moveCaretSelection(const WebPoint&) override;
  bool setEditableSelectionOffsets(int start, int end) override;
  bool setCompositionFromExistingText(
      int compositionStart,
      int compositionEnd,
      const WebVector<WebCompositionUnderline>& underlines) override;
  void extendSelectionAndDelete(int before, int after) override;
  void deleteSurroundingText(int before, int after) override;
  void setCaretVisible(bool) override;
  int printBegin(const WebPrintParams&,
                 const WebNode& constrainToNode) override;
  float printPage(int pageToPrint, WebCanvas*) override;
  float getPrintPageShrink(int page) override;
  void printEnd() override;
  bool isPrintScalingDisabledForPlugin(const WebNode&) override;
  bool getPrintPresetOptionsForPlugin(const WebNode&,
                                      WebPrintPresetOptions*) override;
  bool hasCustomPageSizeStyle(int pageIndex) override;
  bool isPageBoxVisible(int pageIndex) override;
  void pageSizeAndMarginsInPixels(int pageIndex,
                                  WebDoubleSize& pageSize,
                                  int& marginTop,
                                  int& marginRight,
                                  int& marginBottom,
                                  int& marginLeft) override;
  WebString pageProperty(const WebString& propertyName, int pageIndex) override;
  void printPagesWithBoundaries(WebCanvas*, const WebSize&) override;

  void dispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intendedTargetOrigin,
      const WebDOMEvent&) override;

  WebRect selectionBoundsRect() const override;

  WebString layerTreeAsText(bool showDebugInfo = false) const override;

  WebFrameImplBase* toImplBase() override { return this; }

  // WebLocalFrame methods:
  void setAutofillClient(WebAutofillClient*) override;
  WebAutofillClient* autofillClient() override;
  void setDevToolsAgentClient(WebDevToolsAgentClient*) override;
  WebDevToolsAgent* devToolsAgent() override;
  WebLocalFrameImpl* localRoot() override;
  void sendPings(const WebURL& destinationURL) override;
  bool dispatchBeforeUnloadEvent(bool) override;
  WebURLRequest requestFromHistoryItem(const WebHistoryItem&,
                                       WebCachePolicy) const override;
  WebURLRequest requestForReload(WebFrameLoadType,
                                 const WebURL&) const override;
  void load(const WebURLRequest&,
            WebFrameLoadType,
            const WebHistoryItem&,
            WebHistoryLoadType,
            bool isClientRedirect) override;
  void loadData(const WebData&,
                const WebString& mimeType,
                const WebString& textEncoding,
                const WebURL& baseURL,
                const WebURL& unreachableURL,
                bool replace,
                WebFrameLoadType,
                const WebHistoryItem&,
                WebHistoryLoadType,
                bool isClientRedirect) override;
  bool maybeRenderFallbackContent(const WebURLError&) const override;
  bool isLoading() const override;
  bool isFrameDetachedForSpecialOneOffStopTheCrashingHackBug561873()
      const override;
  bool isNavigationScheduledWithin(double interval) const override;
  void setCommittedFirstRealLoad() override;
  void setHasReceivedUserGesture() override;
  void sendOrientationChangeEvent() override;
  WebSandboxFlags effectiveSandboxFlags() const override;
  void forceSandboxFlags(WebSandboxFlags) override;
  void requestRunTask(WebSuspendableTask*) const override;
  void didCallAddSearchProvider() override;
  void didCallIsSearchProviderInstalled() override;
  void replaceSelection(const WebString&) override;
  void requestFind(int identifier,
                   const WebString& searchText,
                   const WebFindOptions&) override;
  bool find(int identifier,
            const WebString& searchText,
            const WebFindOptions&,
            bool wrapWithinFrame,
            bool* activeNow = nullptr) override;
  void stopFinding(StopFindAction) override;
  void increaseMatchCount(int count, int identifier) override;
  int findMatchMarkersVersion() const override;
  WebFloatRect activeFindMatchRect() override;
  void findMatchRects(WebVector<WebFloatRect>&) override;
  int selectNearestFindMatch(const WebFloatPoint&,
                             WebRect* selectionRect) override;
  float distanceToNearestFindMatch(const WebFloatPoint&) override;
  void setTickmarks(const WebVector<WebRect>&) override;
  WebFrameWidgetBase* frameWidget() const override;
  void copyImageAt(const WebPoint&) override;
  void saveImageAt(const WebPoint&) override;
  void clearActiveFindMatch() override;
  void usageCountChromeLoadTimes(const WebString& metric) override;

  // WebFrameImplBase methods:
  void initializeCoreFrame(FrameHost*,
                           FrameOwner*,
                           const AtomicString& name,
                           const AtomicString& uniqueName) override;
  LocalFrame* frame() const override { return m_frame.get(); }

  void willBeDetached();
  void willDetachParent();

  static WebLocalFrameImpl* create(WebTreeScopeType,
                                   WebFrameClient*,
                                   WebFrame* opener);
  static WebLocalFrameImpl* createProvisional(WebFrameClient*,
                                              WebRemoteFrame*,
                                              WebSandboxFlags);
  ~WebLocalFrameImpl() override;

  LocalFrame* createChildFrame(const FrameLoadRequest&,
                               const AtomicString& name,
                               HTMLFrameOwnerElement*);

  void didChangeContentsSize(const IntSize&);

  void createFrameView();

  static WebLocalFrameImpl* fromFrame(LocalFrame*);
  static WebLocalFrameImpl* fromFrame(LocalFrame&);
  static WebLocalFrameImpl* fromFrameOwnerElement(Element*);

  // If the frame hosts a PluginDocument, this method returns the
  // WebPluginContainerImpl that hosts the plugin.
  static WebPluginContainerImpl* pluginContainerFromFrame(LocalFrame*);

  // If the frame hosts a PluginDocument, this method returns the
  // WebPluginContainerImpl that hosts the plugin. If the provided node is a
  // plugin, then it runs its WebPluginContainerImpl. Otherwise, uses the
  // currently focused element (if any).
  static WebPluginContainerImpl* currentPluginContainer(LocalFrame*,
                                                        Node* = nullptr);

  WebViewImpl* viewImpl() const;

  FrameView* frameView() const { return frame() ? frame()->view() : 0; }

  WebDevToolsAgentImpl* devToolsAgentImpl() const {
    return m_devToolsAgent.get();
  }

  // Getters for the impls corresponding to Get(Provisional)DataSource. They
  // may return 0 if there is no corresponding data source.
  WebDataSourceImpl* dataSourceImpl() const;
  WebDataSourceImpl* provisionalDataSourceImpl() const;

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void setFindEndstateFocusAndSelection();

  void didFail(const ResourceError&, bool wasProvisional, HistoryCommitType);
  void didFinish();

  // Sets whether the WebLocalFrameImpl allows its document to be scrolled.
  // If the parameter is true, allow the document to be scrolled.
  // Otherwise, disallow scrolling.
  void setCanHaveScrollbars(bool) override;

  WebFrameClient* client() const { return m_client; }
  void setClient(WebFrameClient* client) { m_client = client; }

  WebContentSettingsClient* contentSettingsClient() {
    return m_contentSettingsClient;
  }
  SharedWorkerRepositoryClientImpl* sharedWorkerRepositoryClient() const {
    return m_sharedWorkerRepositoryClient.get();
  }

  void setInputEventsTransformForEmulation(const IntSize&, float);

  static void selectWordAroundPosition(LocalFrame*, VisiblePosition);

  TextFinder* textFinder() const;
  // Returns the text finder object if it already exists.
  // Otherwise creates it and then returns.
  TextFinder& ensureTextFinder();

  // Returns a hit-tested VisiblePosition for the given point
  VisiblePosition visiblePositionForViewportPoint(const WebPoint&);

  void setFrameWidget(WebFrameWidgetBase*);

  // DevTools front-end bindings.
  void setDevToolsFrontend(WebDevToolsFrontendImpl* frontend) {
    m_webDevToolsFrontend = frontend;
  }
  WebDevToolsFrontendImpl* devToolsFrontend() { return m_webDevToolsFrontend; }

  WebNode contextMenuNode() const { return m_contextMenuNode.get(); }
  void setContextMenuNode(Node* node) { m_contextMenuNode = node; }
  void clearContextMenuNode() { m_contextMenuNode.clear(); }

  WebInputMethodControllerImpl* inputMethodController() const;

  DECLARE_TRACE();

 private:
  friend class FrameLoaderClientImpl;

  WebLocalFrameImpl(WebTreeScopeType, WebFrameClient*);
  WebLocalFrameImpl(WebRemoteFrame*, WebFrameClient*);

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebLocalFrameImpl.
  bool isWebLocalFrame() const override;
  WebLocalFrame* toWebLocalFrame() override;
  bool isWebRemoteFrame() const override;
  WebRemoteFrame* toWebRemoteFrame() override;

  // Sets the local core frame and registers destruction observers.
  void setCoreFrame(LocalFrame*);

  void loadJavaScriptURL(const KURL&);

  HitTestResult hitTestResultForVisualViewportPos(const IntPoint&);

  WebPlugin* focusedPluginIfInputMethodSupported();
  ScrollableArea* layoutViewportScrollableArea() const;

  // Returns true if the frame is focused.
  bool isFocused() const;

  Member<FrameLoaderClientImpl> m_frameLoaderClientImpl;

  // The embedder retains a reference to the WebCore LocalFrame while it is
  // active in the DOM. This reference is released when the frame is removed
  // from the DOM or the entire page is closed.  FIXME: These will need to
  // change to WebFrame when we introduce WebFrameProxy.
  Member<LocalFrame> m_frame;

  Member<WebDevToolsAgentImpl> m_devToolsAgent;

  // This is set if the frame is the root of a local frame tree, and requires a
  // widget for layout.
  WebFrameWidgetBase* m_frameWidget;

  WebFrameClient* m_client;
  WebAutofillClient* m_autofillClient;
  WebContentSettingsClient* m_contentSettingsClient;
  std::unique_ptr<SharedWorkerRepositoryClientImpl>
      m_sharedWorkerRepositoryClient;

  // Will be initialized after first call to ensureTextFinder().
  Member<TextFinder> m_textFinder;

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  Member<ChromePrintContext> m_printContext;

  // Stores the additional input events offset and scale when device metrics
  // emulation is enabled.
  IntSize m_inputEventsOffsetForEmulation;
  float m_inputEventsScaleFactorForEmulation;

  WebDevToolsFrontendImpl* m_webDevToolsFrontend;

  Member<Node> m_contextMenuNode;

  std::unique_ptr<WebInputMethodControllerImpl> m_inputMethodController;

  // Oilpan: WebLocalFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebLocalFrameImpl> m_selfKeepAlive;
};

DEFINE_TYPE_CASTS(WebLocalFrameImpl,
                  WebFrame,
                  frame,
                  frame->isWebLocalFrame(),
                  frame.isWebLocalFrame());

}  // namespace blink

#endif

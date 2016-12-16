/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLMediaElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/dom/Attribute.h"
#include "core/dom/DOMException.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ElementVisibilityObserver.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/AutoplayUmaHelper.h"
#include "core/html/HTMLMediaSource.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLTrackElement.h"
#include "core/html/MediaError.h"
#include "core/html/MediaFragmentURIParser.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/AudioTrack.h"
#include "core/html/track/AudioTrackList.h"
#include "core/html/track/AutomaticTrackSelection.h"
#include "core/html/track/CueTimeline.h"
#include "core/html/track/InbandTextTrack.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/html/track/TextTrackList.h"
#include "core/html/track/VideoTrack.h"
#include "core/html/track/VideoTrackList.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/api/LayoutMediaItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/NetworkStateNotifier.h"
#include "platform/ContentType.h"
#include "platform/Histogram.h"
#include "platform/LayoutTestSupport.h"
#include "platform/MIMETypeFromURL.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAudioSourceProvider.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebInbandTextTrack.h"
#include "public/platform/WebMediaPlayerSource.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include <limits>

#ifndef BLINK_MEDIA_LOG
#define BLINK_MEDIA_LOG DVLOG(3)
#endif

#ifndef LOG_MEDIA_EVENTS
// Default to not logging events because so many are generated they can
// overwhelm the rest of the logging.
#define LOG_MEDIA_EVENTS 0
#endif

#ifndef LOG_CACHED_TIME_WARNINGS
// Default to not logging warnings about excessive drift in the cached media
// time because it adds a fair amount of overhead and logging.
#define LOG_CACHED_TIME_WARNINGS 0
#endif

namespace blink {

using namespace HTMLNames;

using WeakMediaElementSet = HeapHashSet<WeakMember<HTMLMediaElement>>;
using DocumentElementSetMap =
    HeapHashMap<WeakMember<Document>, Member<WeakMediaElementSet>>;

namespace {

enum MediaControlsShow {
  MediaControlsShowAttribute = 0,
  MediaControlsShowFullscreen,
  MediaControlsShowNoScript,
  MediaControlsShowNotShown,
  MediaControlsShowMax
};

String urlForLoggingMedia(const KURL& url) {
  static const unsigned maximumURLLengthForLogging = 128;

  if (url.getString().length() < maximumURLLengthForLogging)
    return url.getString();
  return url.getString().substring(0, maximumURLLengthForLogging) + "...";
}

const char* boolString(bool val) {
  return val ? "true" : "false";
}

DocumentElementSetMap& documentToElementSetMap() {
  DEFINE_STATIC_LOCAL(DocumentElementSetMap, map, (new DocumentElementSetMap));
  return map;
}

void addElementToDocumentMap(HTMLMediaElement* element, Document* document) {
  DocumentElementSetMap& map = documentToElementSetMap();
  WeakMediaElementSet* set = nullptr;
  auto it = map.find(document);
  if (it == map.end()) {
    set = new WeakMediaElementSet;
    map.add(document, set);
  } else {
    set = it->value;
  }
  set->add(element);
}

void removeElementFromDocumentMap(HTMLMediaElement* element,
                                  Document* document) {
  DocumentElementSetMap& map = documentToElementSetMap();
  auto it = map.find(document);
  DCHECK(it != map.end());
  WeakMediaElementSet* set = it->value;
  set->remove(element);
  if (set->isEmpty())
    map.remove(it);
}

class AudioSourceProviderClientLockScope {
  STACK_ALLOCATED();

 public:
  AudioSourceProviderClientLockScope(HTMLMediaElement& element)
      : m_client(element.audioSourceNode()) {
    if (m_client)
      m_client->lock();
  }
  ~AudioSourceProviderClientLockScope() {
    if (m_client)
      m_client->unlock();
  }

 private:
  Member<AudioSourceProviderClient> m_client;
};

const AtomicString& AudioKindToString(
    WebMediaPlayerClient::AudioTrackKind kind) {
  switch (kind) {
    case WebMediaPlayerClient::AudioTrackKindNone:
      return emptyAtom;
    case WebMediaPlayerClient::AudioTrackKindAlternative:
      return AudioTrack::alternativeKeyword();
    case WebMediaPlayerClient::AudioTrackKindDescriptions:
      return AudioTrack::descriptionsKeyword();
    case WebMediaPlayerClient::AudioTrackKindMain:
      return AudioTrack::mainKeyword();
    case WebMediaPlayerClient::AudioTrackKindMainDescriptions:
      return AudioTrack::mainDescriptionsKeyword();
    case WebMediaPlayerClient::AudioTrackKindTranslation:
      return AudioTrack::translationKeyword();
    case WebMediaPlayerClient::AudioTrackKindCommentary:
      return AudioTrack::commentaryKeyword();
  }

  NOTREACHED();
  return emptyAtom;
}

const AtomicString& VideoKindToString(
    WebMediaPlayerClient::VideoTrackKind kind) {
  switch (kind) {
    case WebMediaPlayerClient::VideoTrackKindNone:
      return emptyAtom;
    case WebMediaPlayerClient::VideoTrackKindAlternative:
      return VideoTrack::alternativeKeyword();
    case WebMediaPlayerClient::VideoTrackKindCaptions:
      return VideoTrack::captionsKeyword();
    case WebMediaPlayerClient::VideoTrackKindMain:
      return VideoTrack::mainKeyword();
    case WebMediaPlayerClient::VideoTrackKindSign:
      return VideoTrack::signKeyword();
    case WebMediaPlayerClient::VideoTrackKindSubtitles:
      return VideoTrack::subtitlesKeyword();
    case WebMediaPlayerClient::VideoTrackKindCommentary:
      return VideoTrack::commentaryKeyword();
  }

  NOTREACHED();
  return emptyAtom;
}

bool canLoadURL(const KURL& url, const ContentType& contentType) {
  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

  String contentMIMEType = contentType.type().lower();
  String contentTypeCodecs = contentType.parameter(codecs);

  // If the MIME type is missing or is not meaningful, try to figure it out from
  // the URL.
  if (contentMIMEType.isEmpty() ||
      contentMIMEType == "application/octet-stream" ||
      contentMIMEType == "text/plain") {
    if (url.protocolIsData())
      contentMIMEType = mimeTypeFromDataURL(url.getString());
  }

  // If no MIME type is specified, always attempt to load.
  if (contentMIMEType.isEmpty())
    return true;

  // 4.8.10.3 MIME types - In the absence of a specification to the contrary,
  // the MIME type "application/octet-stream" when used with parameters, e.g.
  // "application/octet-stream;codecs=theora", is a type that the user agent
  // knows it cannot render.
  if (contentMIMEType != "application/octet-stream" ||
      contentTypeCodecs.isEmpty()) {
    WebMimeRegistry::SupportsType supported =
        Platform::current()->mimeRegistry()->supportsMediaMIMEType(
            contentMIMEType, contentTypeCodecs);
    return supported > WebMimeRegistry::IsNotSupported;
  }

  return false;
}

String preloadTypeToString(WebMediaPlayer::Preload preloadType) {
  switch (preloadType) {
    case WebMediaPlayer::PreloadNone:
      return "none";
    case WebMediaPlayer::PreloadMetaData:
      return "metadata";
    case WebMediaPlayer::PreloadAuto:
      return "auto";
  }

  NOTREACHED();
  return String();
}

}  // anonymous namespace

class HTMLMediaElement::AutoplayHelperClientImpl
    : public AutoplayExperimentHelper::Client {
 public:
  static AutoplayHelperClientImpl* create(HTMLMediaElement* element) {
    return new AutoplayHelperClientImpl(element);
  }

  virtual ~AutoplayHelperClientImpl();

  using RecordMetricsBehavior = HTMLMediaElement::RecordMetricsBehavior;

  double currentTime() const override { return m_element->currentTime(); }
  double duration() const override { return m_element->duration(); }
  bool paused() const override { return m_element->paused(); }
  bool ended() const override { return m_element->ended(); }
  bool muted() const override { return m_element->muted(); }
  void setMuted(bool muted) override { m_element->setMuted(muted); }
  void playInternal() override { m_element->playInternal(); }
  void pauseInternal() override { m_element->pauseInternal(); }
  bool isLockedPendingUserGesture() const override {
    return m_element->isLockedPendingUserGesture();
  }
  void unlockUserGesture() override { m_element->unlockUserGesture(); }
  void recordAutoplayMetric(AutoplayMetrics metric) override {
    m_element->recordAutoplayMetric(metric);
  }
  bool shouldAutoplay() override {
    return m_element->shouldAutoplay(RecordMetricsBehavior::DoNotRecord);
  }
  bool isHTMLVideoElement() const override {
    return m_element->isHTMLVideoElement();
  }
  bool isHTMLAudioElement() const override {
    return m_element->isHTMLAudioElement();
  }

  // Document
  bool isLegacyViewportType() override;
  PageVisibilityState pageVisibilityState() const override;
  String autoplayExperimentMode() const override;

  // Frame
  bool isCrossOrigin() const override {
    const LocalFrame* frame = m_element->document().frame();
    return frame && frame->isCrossOriginSubframe();
  }

  bool isAutoplayAllowedPerSettings() const override;

  // LayoutObject
  void setRequestPositionUpdates(bool) override;
  IntRect absoluteBoundingBoxRect() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_element);
    Client::trace(visitor);
  }

 private:
  AutoplayHelperClientImpl(HTMLMediaElement* element) : m_element(element) {}

  Member<HTMLMediaElement> m_element;
};

void HTMLMediaElement::recordAutoplayMetric(AutoplayMetrics metric) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, autoplayHistogram,
                      ("Blink.MediaElement.Autoplay", NumberOfAutoplayMetrics));
  autoplayHistogram.count(metric);
}

WebMimeRegistry::SupportsType HTMLMediaElement::supportsType(
    const ContentType& contentType) {
  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

  String type = contentType.type().lower();
  // The codecs string is not lower-cased because MP4 values are case sensitive
  // per http://tools.ietf.org/html/rfc4281#page-7.
  String typeCodecs = contentType.parameter(codecs);

  if (type.isEmpty())
    return WebMimeRegistry::IsNotSupported;

  // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty
  // string if type is a type that the user agent knows it cannot render or is
  // the type "application/octet-stream"
  if (type == "application/octet-stream")
    return WebMimeRegistry::IsNotSupported;

  return Platform::current()->mimeRegistry()->supportsMediaMIMEType(type,
                                                                    typeCodecs);
}

URLRegistry* HTMLMediaElement::s_mediaStreamRegistry = 0;

void HTMLMediaElement::setMediaStreamRegistry(URLRegistry* registry) {
  DCHECK(!s_mediaStreamRegistry);
  s_mediaStreamRegistry = registry;
}

bool HTMLMediaElement::isMediaStreamURL(const String& url) {
  return s_mediaStreamRegistry ? s_mediaStreamRegistry->contains(url) : false;
}

bool HTMLMediaElement::isHLSURL(const KURL& url) {
  // Keep the same logic as in media_codec_util.h.
  if (url.isNull() || url.isEmpty())
    return false;

  if (!url.isLocalFile() && !url.protocolIs("http") && !url.protocolIs("https"))
    return false;

  return url.getString().contains("m3u8");
}

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName,
                                   Document& document)
    : HTMLElement(tagName, document),
      ActiveScriptWrappable(this),
      ActiveDOMObject(&document),
      m_loadTimer(this, &HTMLMediaElement::loadTimerFired),
      m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired),
      m_playbackProgressTimer(this,
                              &HTMLMediaElement::playbackProgressTimerFired),
      m_audioTracksTimer(this, &HTMLMediaElement::audioTracksTimerFired),
      m_playedTimeRanges(),
      m_asyncEventQueue(GenericEventQueue::create(this)),
      m_playbackRate(1.0f),
      m_defaultPlaybackRate(1.0f),
      m_networkState(kNetworkEmpty),
      m_readyState(kHaveNothing),
      m_readyStateMaximum(kHaveNothing),
      m_volume(1.0f),
      m_lastSeekTime(0),
      m_previousProgressTime(std::numeric_limits<double>::max()),
      m_duration(std::numeric_limits<double>::quiet_NaN()),
      m_lastTimeUpdateEventWallTime(0),
      m_lastTimeUpdateEventMovieTime(0),
      m_defaultPlaybackStartPosition(0),
      m_loadState(WaitingForSource),
      m_deferredLoadState(NotDeferred),
      m_deferredLoadTimer(this, &HTMLMediaElement::deferredLoadTimerFired),
      m_webLayer(nullptr),
      m_displayMode(Unknown),
      m_cachedTime(std::numeric_limits<double>::quiet_NaN()),
      m_fragmentEndTime(std::numeric_limits<double>::quiet_NaN()),
      m_pendingActionFlags(0),
      m_lockedPendingUserGesture(false),
      m_playing(false),
      m_shouldDelayLoadEvent(false),
      m_haveFiredLoadedData(false),
      m_autoplaying(true),
      m_muted(false),
      m_paused(true),
      m_seeking(false),
      m_sentStalledEvent(false),
      m_ignorePreloadNone(false),
      m_textTracksVisible(false),
      m_shouldPerformAutomaticTrackSelection(true),
      m_tracksAreReady(true),
      m_processingPreferenceChange(false),
      m_remoteRoutesAvailable(false),
      m_playingRemotely(false),
      m_inOverlayFullscreenVideo(false),
      m_audioTracks(AudioTrackList::create(*this)),
      m_videoTracks(VideoTrackList::create(*this)),
      m_textTracks(nullptr),
      m_playPromiseResolveTask(CancellableTaskFactory::create(
          this,
          &HTMLMediaElement::resolveScheduledPlayPromises)),
      m_playPromiseRejectTask(CancellableTaskFactory::create(
          this,
          &HTMLMediaElement::rejectScheduledPlayPromises)),
      m_audioSourceNode(nullptr),
      m_autoplayHelperClient(AutoplayHelperClientImpl::create(this)),
      m_autoplayHelper(
          AutoplayExperimentHelper::create(m_autoplayHelperClient.get())),
      m_autoplayUmaHelper(AutoplayUmaHelper::create(this)),
      m_remotePlaybackClient(nullptr),
      m_autoplayVisibilityObserver(nullptr) {
  ThreadState::current()->registerPreFinalizer(this);

  BLINK_MEDIA_LOG << "HTMLMediaElement(" << (void*)this << ")";

  // If any experiment is enabled, then we want to enable a user gesture by
  // default, otherwise the experiment does nothing.
  if ((document.settings() &&
       document.settings()->mediaPlaybackRequiresUserGesture()) ||
      m_autoplayHelper->isExperimentEnabled()) {
    m_lockedPendingUserGesture = true;
  }

  setHasCustomStyleCallbacks();
  addElementToDocumentMap(this, &document);

  UseCounter::count(document, UseCounter::HTMLMediaElement);
}

HTMLMediaElement::~HTMLMediaElement() {
  BLINK_MEDIA_LOG << "~HTMLMediaElement(" << (void*)this << ")";

  // m_audioSourceNode is explicitly cleared by AudioNode::dispose().
  // Since AudioNode::dispose() is guaranteed to be always called before
  // the AudioNode is destructed, m_audioSourceNode is explicitly cleared
  // even if the AudioNode and the HTMLMediaElement die together.
  DCHECK(!m_audioSourceNode);
}

void HTMLMediaElement::dispose() {
  closeMediaSource();

  // Destroying the player may cause a resource load to be canceled,
  // which could result in LocalDOMWindow::dispatchWindowLoadEvent() being
  // called via ResourceFetch::didLoadResource(), then
  // FrameLoader::checkCompleted(). But it's guaranteed that the load event
  // doesn't get dispatched during the object destruction.
  // See Document::isDelayingLoadEvent().
  // Also see http://crbug.com/275223 for more details.
  clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
}

void HTMLMediaElement::didMoveToNewDocument(Document& oldDocument) {
  BLINK_MEDIA_LOG << "didMoveToNewDocument(" << (void*)this << ")";

  m_autoplayUmaHelper->didMoveToNewDocument(oldDocument);
  // If any experiment is enabled, then we want to enable a user gesture by
  // default, otherwise the experiment does nothing.
  bool oldDocumentRequiresUserGesture =
      (oldDocument.settings() &&
       oldDocument.settings()->mediaPlaybackRequiresUserGesture()) ||
      m_autoplayHelper->isExperimentEnabled();
  bool newDocumentRequiresUserGesture =
      (document().settings() &&
       document().settings()->mediaPlaybackRequiresUserGesture()) ||
      m_autoplayHelper->isExperimentEnabled();
  if (newDocumentRequiresUserGesture && !oldDocumentRequiresUserGesture) {
    m_lockedPendingUserGesture = true;
  }

  if (m_shouldDelayLoadEvent) {
    document().incrementLoadEventDelayCount();
    // Note: Keeping the load event delay count increment on oldDocument that
    // was added when m_shouldDelayLoadEvent was set so that destruction of
    // m_webMediaPlayer can not cause load event dispatching in oldDocument.
  } else {
    // Incrementing the load event delay count so that destruction of
    // m_webMediaPlayer can not cause load event dispatching in oldDocument.
    oldDocument.incrementLoadEventDelayCount();
  }

  removeElementFromDocumentMap(this, &oldDocument);
  addElementToDocumentMap(this, &document());

  // FIXME: This is a temporary fix to prevent this object from causing the
  // MediaPlayer to dereference LocalFrame and FrameLoader pointers from the
  // previous document. This restarts the load, as if the src attribute had been
  // set.  A proper fix would provide a mechanism to allow this object to
  // refresh the MediaPlayer's LocalFrame and FrameLoader references on document
  // changes so that playback can be resumed properly.
  m_ignorePreloadNone = false;
  invokeLoadAlgorithm();

  // Decrement the load event delay count on oldDocument now that
  // m_webMediaPlayer has been destroyed and there is no risk of dispatching a
  // load event from within the destructor.
  oldDocument.decrementLoadEventDelayCount();

  ActiveDOMObject::didMoveToNewExecutionContext(&document());
  HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLMediaElement::supportsFocus() const {
  if (ownerDocument()->isMediaDocument())
    return false;

  // If no controls specified, we should still be able to focus the element if
  // it has tabIndex.
  return shouldShowControls() || HTMLElement::supportsFocus();
}

bool HTMLMediaElement::isMouseFocusable() const {
  return false;
}

void HTMLMediaElement::parseAttribute(const QualifiedName& name,
                                      const AtomicString& oldValue,
                                      const AtomicString& value) {
  if (name == srcAttr) {
    BLINK_MEDIA_LOG << "parseAttribute(" << (void*)this
                    << ", srcAttr, old=" << oldValue << ", new=" << value
                    << ")";
    // Trigger a reload, as long as the 'src' attribute is present.
    if (!value.isNull()) {
      m_ignorePreloadNone = false;
      invokeLoadAlgorithm();
    }
  } else if (name == controlsAttr) {
    UseCounter::count(document(),
                      UseCounter::HTMLMediaElementControlsAttribute);
    configureMediaControls();
  } else if (name == preloadAttr) {
    setPlayerPreload();
  } else if (name == disableremoteplaybackAttr) {
    UseCounter::count(document(), UseCounter::DisableRemotePlaybackAttribute);
    if (mediaControls() && (oldValue != value))
      mediaControls()->refreshCastButtonVisibility();
  } else {
    HTMLElement::parseAttribute(name, oldValue, value);
  }
}

void HTMLMediaElement::finishParsingChildren() {
  HTMLElement::finishParsingChildren();

  if (Traversal<HTMLTrackElement>::firstChild(*this))
    scheduleTextTrackResourceLoad();
}

bool HTMLMediaElement::layoutObjectIsNeeded(const ComputedStyle& style) {
  return shouldShowControls() && HTMLElement::layoutObjectIsNeeded(style);
}

LayoutObject* HTMLMediaElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutMedia(this);
}

Node::InsertionNotificationRequest HTMLMediaElement::insertedInto(
    ContainerNode* insertionPoint) {
  BLINK_MEDIA_LOG << "insertedInto(" << (void*)this << ", " << insertionPoint
                  << ")";

  HTMLElement::insertedInto(insertionPoint);
  if (insertionPoint->isConnected()) {
    UseCounter::count(document(), UseCounter::HTMLMediaElementInDocument);
    if ((!getAttribute(srcAttr).isEmpty() || m_srcObject) &&
        m_networkState == kNetworkEmpty) {
      m_ignorePreloadNone = false;
      invokeLoadAlgorithm();
    }
  }

  return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLMediaElement::didNotifySubtreeInsertionsToDocument() {
  configureMediaControls();
}

void HTMLMediaElement::removedFrom(ContainerNode* insertionPoint) {
  BLINK_MEDIA_LOG << "removedFrom(" << (void*)this << ", " << insertionPoint
                  << ")";

  HTMLElement::removedFrom(insertionPoint);
  if (insertionPoint->inActiveDocument()) {
    configureMediaControls();
    if (m_networkState > kNetworkEmpty)
      pauseInternal();
  }
}

void HTMLMediaElement::attachLayoutTree(const AttachContext& context) {
  HTMLElement::attachLayoutTree(context);

  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::didRecalcStyle(StyleRecalcChange) {
  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::scheduleTextTrackResourceLoad() {
  BLINK_MEDIA_LOG << "scheduleTextTrackResourceLoad(" << (void*)this << ")";

  m_pendingActionFlags |= LoadTextTrackResource;

  if (!m_loadTimer.isActive())
    m_loadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::scheduleNextSourceChild() {
  // Schedule the timer to try the next <source> element WITHOUT resetting state
  // ala invokeLoadAlgorithm.
  m_pendingActionFlags |= LoadMediaResource;
  m_loadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName) {
  scheduleEvent(Event::createCancelable(eventName));
}

void HTMLMediaElement::scheduleEvent(Event* event) {
#if LOG_MEDIA_EVENTS
  BLINK_MEDIA_LOG << "scheduleEvent(" << (void*)this << ")"
                  << " - scheduling '" << event->type() << "'";
#endif
  m_asyncEventQueue->enqueueEvent(event);
}

void HTMLMediaElement::loadTimerFired(TimerBase*) {
  if (m_pendingActionFlags & LoadTextTrackResource)
    honorUserPreferencesForAutomaticTextTrackSelection();

  if (m_pendingActionFlags & LoadMediaResource) {
    if (m_loadState == LoadingFromSourceElement)
      loadNextSourceChild();
    else
      loadInternal();
  }

  m_pendingActionFlags = 0;
}

MediaError* HTMLMediaElement::error() const {
  return m_error;
}

void HTMLMediaElement::setSrc(const AtomicString& url) {
  setAttribute(srcAttr, url);
}

void HTMLMediaElement::setSrcObject(MediaStreamDescriptor* srcObject) {
  BLINK_MEDIA_LOG << "setSrcObject(" << (void*)this << ")";
  m_srcObject = srcObject;
  invokeLoadAlgorithm();
}

HTMLMediaElement::NetworkState HTMLMediaElement::getNetworkState() const {
  return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType) const {
  WebMimeRegistry::SupportsType support = supportsType(ContentType(mimeType));
  String canPlay;

  // 4.8.10.3
  switch (support) {
    case WebMimeRegistry::IsNotSupported:
      canPlay = emptyString();
      break;
    case WebMimeRegistry::MayBeSupported:
      canPlay = "maybe";
      break;
    case WebMimeRegistry::IsSupported:
      canPlay = "probably";
      break;
  }

  BLINK_MEDIA_LOG << "canPlayType(" << (void*)this << ", " << mimeType
                  << ") -> " << canPlay;

  return canPlay;
}

void HTMLMediaElement::load() {
  BLINK_MEDIA_LOG << "load(" << (void*)this << ")";

  m_autoplayHelper->loadMethodCalled();

  m_ignorePreloadNone = true;
  invokeLoadAlgorithm();
}

// TODO(srirama.m): Currently m_ignorePreloadNone is reset before calling
// invokeLoadAlgorithm() in all places except load(). Move it inside here
// once microtask is implemented for "Await a stable state" step
// in resource selection algorithm.
void HTMLMediaElement::invokeLoadAlgorithm() {
  BLINK_MEDIA_LOG << "invokeLoadAlgorithm(" << (void*)this << ")";

  // Perform the cleanup required for the resource load algorithm to run.
  stopPeriodicTimers();
  m_loadTimer.stop();
  cancelDeferredLoad();
  // FIXME: Figure out appropriate place to reset LoadTextTrackResource if
  // necessary and set m_pendingActionFlags to 0 here.
  m_pendingActionFlags &= ~LoadMediaResource;
  m_sentStalledEvent = false;
  m_haveFiredLoadedData = false;
  m_displayMode = Unknown;

  // 1 - Abort any already-running instance of the resource selection algorithm
  // for this element.
  m_loadState = WaitingForSource;
  m_currentSourceNode = nullptr;

  // 2 - Let pending tasks be a list of tasks from the media element's media
  // element task source in one of the task queues.
  //
  // 3 - For each task in the pending tasks that would run resolve pending
  // play promises or project pending play prmoises algorithms, immediately
  // resolve or reject those promises in the order the corresponding tasks
  // were queued.
  //
  // TODO(mlamouri): the promises are first resolved then rejected but the
  // order between resolved/rejected promises isn't respected. This could be
  // improved when the same task is used for both cases.
  if (m_playPromiseResolveTask->isPending()) {
    m_playPromiseResolveTask->cancel();
    resolveScheduledPlayPromises();
  }
  if (m_playPromiseRejectTask->isPending()) {
    m_playPromiseRejectTask->cancel();
    rejectScheduledPlayPromises();
  }

  // 4 - Remove each task in pending tasks from its task queue.
  cancelPendingEventsAndCallbacks();

  // 5 - If the media element's networkState is set to NETWORK_LOADING or
  // NETWORK_IDLE, queue a task to fire a simple event named abort at the media
  // element.
  if (m_networkState == kNetworkLoading || m_networkState == kNetworkIdle)
    scheduleEvent(EventTypeNames::abort);

  resetMediaPlayerAndMediaSource();

  // 6 - If the media element's networkState is not set to NETWORK_EMPTY, then
  // run these substeps
  if (m_networkState != kNetworkEmpty) {
    // 4.1 - Queue a task to fire a simple event named emptied at the media
    // element.
    scheduleEvent(EventTypeNames::emptied);

    // 4.2 - If a fetching process is in progress for the media element, the
    // user agent should stop it.
    setNetworkState(kNetworkEmpty);

    // 4.4 - Forget the media element's media-resource-specific tracks.
    forgetResourceSpecificTracks();

    // 4.5 - If readyState is not set to kHaveNothing, then set it to that
    // state.
    m_readyState = kHaveNothing;
    m_readyStateMaximum = kHaveNothing;

    DCHECK(!m_paused || m_playPromiseResolvers.isEmpty());

    // 4.6 - If the paused attribute is false, then run these substeps
    if (!m_paused) {
      // 4.6.1 - Set the paused attribute to true.
      m_paused = true;

      // 4.6.2 - Take pending play promises and reject pending play promises
      // with the result and an "AbortError" DOMException.
      rejectPlayPromises(
          AbortError,
          "The play() request was interrupted by a new load request.");
    }

    // 4.7 - If seeking is true, set it to false.
    m_seeking = false;

    // 4.8 - Set the current playback position to 0.
    //       Set the official playback position to 0.
    //       If this changed the official playback position, then queue a task
    //       to fire a simple event named timeupdate at the media element.
    // FIXME: Add support for firing this event.

    // 4.9 - Set the initial playback position to 0.
    // FIXME: Make this less subtle. The position only becomes 0 because the
    // ready state is HAVE_NOTHING.
    invalidateCachedTime();

    // 4.10 - Set the timeline offset to Not-a-Number (NaN).
    // 4.11 - Update the duration attribute to Not-a-Number (NaN).

    cueTimeline().updateActiveCues(0);
  } else if (!m_paused) {
    // TODO(foolip): There is a proposal to always reset the paused state
    // in the media element load algorithm, to avoid a bogus play() promise
    // rejection: https://github.com/whatwg/html/issues/869
    // This is where that change would have an effect, and it is measured to
    // verify the assumption that it's a very rare situation.
    UseCounter::count(document(),
                      UseCounter::HTMLMediaElementLoadNetworkEmptyNotPaused);
  }

  // 7 - Set the playbackRate attribute to the value of the defaultPlaybackRate
  // attribute.
  setPlaybackRate(defaultPlaybackRate());

  // 8 - Set the error attribute to null and the autoplaying flag to true.
  m_error = nullptr;
  m_autoplaying = true;

  // 9 - Invoke the media element's resource selection algorithm.
  invokeResourceSelectionAlgorithm();

  // 10 - Note: Playback of any previously playing media resource for this
  // element stops.
}

void HTMLMediaElement::invokeResourceSelectionAlgorithm() {
  BLINK_MEDIA_LOG << "invokeResourceSelectionAlgorithm(" << (void*)this << ")";
  // The resource selection algorithm
  // 1 - Set the networkState to NETWORK_NO_SOURCE
  setNetworkState(kNetworkNoSource);

  // 2 - Set the element's show poster flag to true
  // TODO(srirama.m): Introduce show poster flag and update it as per spec

  m_playedTimeRanges = TimeRanges::create();

  // FIXME: Investigate whether these can be moved into m_networkState !=
  // kNetworkEmpty block above
  // so they are closer to the relevant spec steps.
  m_lastSeekTime = 0;
  m_duration = std::numeric_limits<double>::quiet_NaN();

  // 3 - Set the media element's delaying-the-load-event flag to true (this
  // delays the load event)
  setShouldDelayLoadEvent(true);
  if (mediaControls())
    mediaControls()->reset();

  // 4 - Await a stable state, allowing the task that invoked this algorithm to
  // continue
  // TODO(srirama.m): Remove scheduleNextSourceChild() and post a microtask
  // instead.  See http://crbug.com/593289 for more details.
  scheduleNextSourceChild();
}

void HTMLMediaElement::loadInternal() {
  // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose
  // mode was not in the disabled state when the element's resource selection
  // algorithm last started".
  m_textTracksWhenResourceSelectionBegan.clear();
  if (m_textTracks) {
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
      TextTrack* track = m_textTracks->anonymousIndexedGetter(i);
      if (track->mode() != TextTrack::disabledKeyword())
        m_textTracksWhenResourceSelectionBegan.append(track);
    }
  }

  selectMediaResource();
}

void HTMLMediaElement::selectMediaResource() {
  BLINK_MEDIA_LOG << "selectMediaResource(" << (void*)this << ")";

  enum Mode { Object, Attribute, Children, Nothing };
  Mode mode = Nothing;

  // 6 - If the media element has an assigned media provider object, then let
  //     mode be object.
  if (m_srcObject) {
    mode = Object;
  } else if (fastHasAttribute(srcAttr)) {
    // Otherwise, if the media element has no assigned media provider object
    // but has a src attribute, then let mode be attribute.
    mode = Attribute;
  } else if (HTMLSourceElement* element =
                 Traversal<HTMLSourceElement>::firstChild(*this)) {
    // Otherwise, if the media element does not have an assigned media
    // provider object and does not have a src attribute, but does have a
    // source element child, then let mode be children and let candidate be
    // the first such source element child in tree order.
    mode = Children;
    m_nextChildNodeToConsider = element;
    m_currentSourceNode = nullptr;
  } else {
    // Otherwise the media element has no assigned media provider object and
    // has neither a src attribute nor a source element child: set the
    // networkState to kNetworkEmpty, and abort these steps; the synchronous
    // section ends.
    m_loadState = WaitingForSource;
    setShouldDelayLoadEvent(false);
    setNetworkState(kNetworkEmpty);
    updateDisplayState();

    BLINK_MEDIA_LOG << "selectMediaResource(" << (void*)this
                    << "), nothing to load";
    return;
  }

  // 7 - Set the media element's networkState to NETWORK_LOADING.
  setNetworkState(kNetworkLoading);

  // 8 - Queue a task to fire a simple event named loadstart at the media
  // element.
  scheduleEvent(EventTypeNames::loadstart);

  // 9 - Run the appropriate steps...
  switch (mode) {
    case Object:
      loadSourceFromObject();
      BLINK_MEDIA_LOG << "selectMediaResource(" << (void*)this
                      << ", using 'srcObject' attribute";
      break;
    case Attribute:
      loadSourceFromAttribute();
      BLINK_MEDIA_LOG << "selectMediaResource(" << (void*)this
                      << "), using 'src' attribute url";
      break;
    case Children:
      loadNextSourceChild();
      BLINK_MEDIA_LOG << "selectMediaResource(" << (void*)this
                      << "), using source element";
      break;
    default:
      NOTREACHED();
  }
}

void HTMLMediaElement::loadSourceFromObject() {
  DCHECK(m_srcObject);
  m_loadState = LoadingFromSrcObject;

  // No type is available when the resource comes from the 'srcObject'
  // attribute.
  loadResource(WebMediaPlayerSource(WebMediaStream(m_srcObject)),
               ContentType((String())));
}

void HTMLMediaElement::loadSourceFromAttribute() {
  m_loadState = LoadingFromSrcAttr;
  const AtomicString& srcValue = fastGetAttribute(srcAttr);

  // If the src attribute's value is the empty string ... jump down to the
  // failed step below
  if (srcValue.isEmpty()) {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    BLINK_MEDIA_LOG << "loadSourceFromAttribute(" << (void*)this
                    << "), empty 'src'";
    return;
  }

  KURL mediaURL = document().completeURL(srcValue);
  if (!isSafeToLoadURL(mediaURL, Complain)) {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  // No type is available when the url comes from the 'src' attribute so
  // MediaPlayer will have to pick a media engine based on the file extension.
  loadResource(WebMediaPlayerSource(WebURL(mediaURL)), ContentType((String())));
}

void HTMLMediaElement::loadNextSourceChild() {
  ContentType contentType((String()));
  KURL mediaURL = selectNextSourceChild(&contentType, Complain);
  if (!mediaURL.isValid()) {
    waitForSourceChange();
    return;
  }

  // Reset the MediaPlayer and MediaSource if any
  resetMediaPlayerAndMediaSource();

  m_loadState = LoadingFromSourceElement;
  loadResource(WebMediaPlayerSource(WebURL(mediaURL)), contentType);
}

void HTMLMediaElement::loadResource(const WebMediaPlayerSource& source,
                                    const ContentType& contentType) {
  DCHECK(isMainThread());
  KURL url;
  if (source.isURL()) {
    url = source.getAsURL();
    DCHECK(isSafeToLoadURL(url, Complain));
    BLINK_MEDIA_LOG << "loadResource(" << (void*)this << ", "
                    << urlForLoggingMedia(url) << ", " << contentType.raw()
                    << ")";
  }

  LocalFrame* frame = document().frame();
  if (!frame) {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  // The resource fetch algorithm
  setNetworkState(kNetworkLoading);

  m_autoplayHelper->loadingStarted();

  // Set m_currentSrc *before* changing to the cache url, the fact that we are
  // loading from the app cache is an internal detail not exposed through the
  // media element API.
  m_currentSrc = url;

  if (m_audioSourceNode)
    m_audioSourceNode->onCurrentSrcChanged(m_currentSrc);

  BLINK_MEDIA_LOG << "loadResource(" << (void*)this << ") - m_currentSrc -> "
                  << urlForLoggingMedia(m_currentSrc);

  startProgressEventTimer();

  // Reset display mode to force a recalculation of what to show because we are
  // resetting the player.
  setDisplayMode(Unknown);

  setPlayerPreload();

  if (fastHasAttribute(mutedAttr))
    m_muted = true;
  updateVolume();

  DCHECK(!m_mediaSource);

  bool attemptLoad = true;

  m_mediaSource = HTMLMediaSource::lookup(url.getString());
  if (m_mediaSource && !m_mediaSource->attachToElement(this)) {
    // Forget our reference to the MediaSource, so we leave it alone
    // while processing remainder of load failure.
    m_mediaSource = nullptr;
    attemptLoad = false;
  }

  bool canLoadResource = source.isMediaStream() || canLoadURL(url, contentType);
  if (attemptLoad && canLoadResource) {
    DCHECK(!webMediaPlayer());

    // Conditionally defer the load if effective preload is 'none'.
    // Skip this optional deferral for MediaStream sources or any blob URL,
    // including MediaSource blob URLs.
    if (!source.isMediaStream() && !url.protocolIs("blob") &&
        effectivePreloadType() == WebMediaPlayer::PreloadNone) {
      BLINK_MEDIA_LOG << "loadResource(" << (void*)this
                      << ") : Delaying load because preload == 'none'";
      deferLoad();
    } else {
      startPlayerLoad();
    }
  } else {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
  }

  // If there is no poster to display, allow the media engine to render video
  // frames as soon as they are available.
  updateDisplayState();

  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::startPlayerLoad(const KURL& playerProvidedUrl) {
  DCHECK(!m_webMediaPlayer);

  WebMediaPlayerSource source;
  if (m_srcObject) {
    source = WebMediaPlayerSource(WebMediaStream(m_srcObject));
  } else {
    // Filter out user:pass as those two URL components aren't
    // considered for media resource fetches (including for the CORS
    // use-credentials mode.) That behavior aligns with Gecko, with IE
    // being more restrictive and not allowing fetches to such URLs.
    //
    // Spec reference: http://whatwg.org/c/#concept-media-load-resource
    //
    // FIXME: when the HTML spec switches to specifying resource
    // fetches in terms of Fetch (http://fetch.spec.whatwg.org), and
    // along with that potentially also specifying a setting for its
    // 'authentication flag' to control how user:pass embedded in a
    // media resource URL should be treated, then update the handling
    // here to match.
    KURL requestURL =
        playerProvidedUrl.isNull() ? KURL(m_currentSrc) : playerProvidedUrl;
    if (!requestURL.user().isEmpty())
      requestURL.setUser(String());
    if (!requestURL.pass().isEmpty())
      requestURL.setPass(String());

    KURL kurl(ParsedURLString, requestURL);
    source = WebMediaPlayerSource(WebURL(kurl));
  }

  LocalFrame* frame = document().frame();
  // TODO(srirama.m): Figure out how frame can be null when
  // coming from executeDeferredLoad()
  if (!frame) {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  m_webMediaPlayer =
      frame->loader().client()->createWebMediaPlayer(*this, source, this);
  if (!m_webMediaPlayer) {
    mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    return;
  }

  if (layoutObject())
    layoutObject()->setShouldDoFullPaintInvalidation();
  // Make sure if we create/re-create the WebMediaPlayer that we update our
  // wrapper.
  m_audioSourceProvider.wrap(m_webMediaPlayer->getAudioSourceProvider());
  m_webMediaPlayer->setVolume(effectiveMediaVolume());

  m_webMediaPlayer->setPoster(posterImageURL());

  m_webMediaPlayer->setPreload(effectivePreloadType());

  m_webMediaPlayer->load(loadType(), source, corsMode());

  if (isFullscreen()) {
    // This handles any transition to or from fullscreen overlay mode.
    frame->chromeClient().enterFullscreenForElement(this);
  }
}

void HTMLMediaElement::setPlayerPreload() {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setPreload(effectivePreloadType());

  if (loadIsDeferred() && effectivePreloadType() != WebMediaPlayer::PreloadNone)
    startDeferredLoad();
}

bool HTMLMediaElement::loadIsDeferred() const {
  return m_deferredLoadState != NotDeferred;
}

void HTMLMediaElement::deferLoad() {
  // This implements the "optional" step 3 from the resource fetch algorithm.
  DCHECK(!m_deferredLoadTimer.isActive());
  DCHECK_EQ(m_deferredLoadState, NotDeferred);
  // 1. Set the networkState to NETWORK_IDLE.
  // 2. Queue a task to fire a simple event named suspend at the element.
  changeNetworkStateFromLoadingToIdle();
  // 3. Queue a task to set the element's delaying-the-load-event
  // flag to false. This stops delaying the load event.
  m_deferredLoadTimer.startOneShot(0, BLINK_FROM_HERE);
  // 4. Wait for the task to be run.
  m_deferredLoadState = WaitingForStopDelayingLoadEventTask;
  // Continued in executeDeferredLoad().
}

void HTMLMediaElement::cancelDeferredLoad() {
  m_deferredLoadTimer.stop();
  m_deferredLoadState = NotDeferred;
}

void HTMLMediaElement::executeDeferredLoad() {
  DCHECK_GE(m_deferredLoadState, WaitingForTrigger);

  // resource fetch algorithm step 3 - continued from deferLoad().

  // 5. Wait for an implementation-defined event (e.g. the user requesting that
  // the media element begin playback).  This is assumed to be whatever 'event'
  // ended up calling this method.
  cancelDeferredLoad();
  // 6. Set the element's delaying-the-load-event flag back to true (this
  // delays the load event again, in case it hasn't been fired yet).
  setShouldDelayLoadEvent(true);
  // 7. Set the networkState to NETWORK_LOADING.
  setNetworkState(kNetworkLoading);

  startProgressEventTimer();

  startPlayerLoad();
}

void HTMLMediaElement::startDeferredLoad() {
  if (m_deferredLoadState == WaitingForTrigger) {
    executeDeferredLoad();
    return;
  }
  if (m_deferredLoadState == ExecuteOnStopDelayingLoadEventTask)
    return;
  DCHECK_EQ(m_deferredLoadState, WaitingForStopDelayingLoadEventTask);
  m_deferredLoadState = ExecuteOnStopDelayingLoadEventTask;
}

void HTMLMediaElement::deferredLoadTimerFired(TimerBase*) {
  setShouldDelayLoadEvent(false);

  if (m_deferredLoadState == ExecuteOnStopDelayingLoadEventTask) {
    executeDeferredLoad();
    return;
  }
  DCHECK_EQ(m_deferredLoadState, WaitingForStopDelayingLoadEventTask);
  m_deferredLoadState = WaitingForTrigger;
}

WebMediaPlayer::LoadType HTMLMediaElement::loadType() const {
  if (m_mediaSource)
    return WebMediaPlayer::LoadTypeMediaSource;

  if (m_srcObject ||
      (!m_currentSrc.isNull() && isMediaStreamURL(m_currentSrc.getString())))
    return WebMediaPlayer::LoadTypeMediaStream;

  return WebMediaPlayer::LoadTypeURL;
}

bool HTMLMediaElement::textTracksAreReady() const {
  // 4.8.10.12.1 Text track model
  // ...
  // The text tracks of a media element are ready if all the text tracks whose
  // mode was not in the disabled state when the element's resource selection
  // algorithm last started now have a text track readiness state of loaded or
  // failed to load.
  for (unsigned i = 0; i < m_textTracksWhenResourceSelectionBegan.size(); ++i) {
    if (m_textTracksWhenResourceSelectionBegan[i]->getReadinessState() ==
            TextTrack::Loading ||
        m_textTracksWhenResourceSelectionBegan[i]->getReadinessState() ==
            TextTrack::NotLoaded)
      return false;
  }

  return true;
}

void HTMLMediaElement::textTrackReadyStateChanged(TextTrack* track) {
  if (webMediaPlayer() &&
      m_textTracksWhenResourceSelectionBegan.contains(track)) {
    if (track->getReadinessState() != TextTrack::Loading)
      setReadyState(static_cast<ReadyState>(webMediaPlayer()->getReadyState()));
  } else {
    // The track readiness state might have changed as a result of the user
    // clicking the captions button. In this case, a check whether all the
    // resources have failed loading should be done in order to hide the CC
    // button.
    if (mediaControls() &&
        track->getReadinessState() == TextTrack::FailedToLoad)
      mediaControls()->refreshClosedCaptionsButtonVisibility();
  }
}

void HTMLMediaElement::textTrackModeChanged(TextTrack* track) {
  // Mark this track as "configured" so configureTextTracks won't change the
  // mode again.
  if (track->trackType() == TextTrack::TrackElement)
    track->setHasBeenConfigured(true);

  configureTextTrackDisplay();

  DCHECK(textTracks()->contains(track));
  textTracks()->scheduleChangeEvent();
}

void HTMLMediaElement::disableAutomaticTextTrackSelection() {
  m_shouldPerformAutomaticTrackSelection = false;
}

bool HTMLMediaElement::isSafeToLoadURL(const KURL& url,
                                       InvalidURLAction actionIfInvalid) {
  if (!url.isValid()) {
    BLINK_MEDIA_LOG << "isSafeToLoadURL(" << (void*)this << ", "
                    << urlForLoggingMedia(url)
                    << ") -> FALSE because url is invalid";
    return false;
  }

  LocalFrame* frame = document().frame();
  if (!frame || !document().getSecurityOrigin()->canDisplay(url)) {
    if (actionIfInvalid == Complain)
      FrameLoader::reportLocalLoadFailed(frame, url.elidedString());
    BLINK_MEDIA_LOG << "isSafeToLoadURL(" << (void*)this << ", "
                    << urlForLoggingMedia(url)
                    << ") -> FALSE rejected by SecurityOrigin";
    return false;
  }

  if (!document().contentSecurityPolicy()->allowMediaFromSource(url)) {
    BLINK_MEDIA_LOG << "isSafeToLoadURL(" << (void*)this << ", "
                    << urlForLoggingMedia(url)
                    << ") -> rejected by Content Security Policy";
    return false;
  }

  return true;
}

bool HTMLMediaElement::isMediaDataCORSSameOrigin(SecurityOrigin* origin) const {
  // hasSingleSecurityOrigin() tells us whether the origin in the src is
  // the same as the actual request (i.e. after redirect).
  // didPassCORSAccessCheck() means it was a successful CORS-enabled fetch
  // (vs. non-CORS-enabled or failed).
  // taintsCanvas() does checkAccess() on the URL plus allow data sources,
  // to ensure that it is not a URL that requires CORS (basically same
  // origin).
  return hasSingleSecurityOrigin() &&
         ((webMediaPlayer() && webMediaPlayer()->didPassCORSAccessCheck()) ||
          !origin->taintsCanvas(currentSrc()));
}

void HTMLMediaElement::startProgressEventTimer() {
  if (m_progressEventTimer.isActive())
    return;

  m_previousProgressTime = WTF::currentTime();
  // 350ms is not magic, it is in the spec!
  m_progressEventTimer.startRepeating(0.350, BLINK_FROM_HERE);
}

void HTMLMediaElement::waitForSourceChange() {
  BLINK_MEDIA_LOG << "waitForSourceChange(" << (void*)this << ")";

  stopPeriodicTimers();
  m_loadState = WaitingForSource;

  // 6.17 - Waiting: Set the element's networkState attribute to the
  // NETWORK_NO_SOURCE value
  setNetworkState(kNetworkNoSource);

  // 6.18 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  setShouldDelayLoadEvent(false);

  updateDisplayState();

  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::noneSupported() {
  BLINK_MEDIA_LOG << "noneSupported(" << (void*)this << ")";

  stopPeriodicTimers();
  m_loadState = WaitingForSource;
  m_currentSourceNode = nullptr;

  // 4.8.13.5
  // The dedicated media source failure steps are the following steps:

  // 1 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_SRC_NOT_SUPPORTED.
  m_error = MediaError::create(MediaError::kMediaErrSrcNotSupported);

  // 2 - Forget the media element's media-resource-specific text tracks.
  forgetResourceSpecificTracks();

  // 3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE
  // value.
  setNetworkState(kNetworkNoSource);

  // 4 - Set the element's show poster flag to true.
  updateDisplayState();

  // 5 - Fire a simple event named error at the media element.
  scheduleEvent(EventTypeNames::error);

  // 6 - Reject pending play promises with NotSupportedError.
  scheduleRejectPlayPromises(NotSupportedError);

  closeMediaSource();

  // 7 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  setShouldDelayLoadEvent(false);

  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::mediaEngineError(MediaError* err) {
  DCHECK_GE(m_readyState, kHaveMetadata);
  BLINK_MEDIA_LOG << "mediaEngineError(" << (void*)this << ", "
                  << static_cast<int>(err->code()) << ")";

  // 1 - The user agent should cancel the fetching process.
  stopPeriodicTimers();
  m_loadState = WaitingForSource;

  // 2 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
  m_error = err;

  // 3 - Queue a task to fire a simple event named error at the media element.
  scheduleEvent(EventTypeNames::error);

  // 4 - Set the element's networkState attribute to the NETWORK_IDLE value.
  setNetworkState(kNetworkIdle);

  // 5 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  setShouldDelayLoadEvent(false);

  // 6 - Abort the overall resource selection algorithm.
  m_currentSourceNode = nullptr;
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks() {
  BLINK_MEDIA_LOG << "cancelPendingEventsAndCallbacks(" << (void*)this << ")";
  m_asyncEventQueue->cancelAllEvents();

  for (HTMLSourceElement* source =
           Traversal<HTMLSourceElement>::firstChild(*this);
       source; source = Traversal<HTMLSourceElement>::nextSibling(*source))
    source->cancelPendingErrorEvent();
}

void HTMLMediaElement::networkStateChanged() {
  setNetworkState(webMediaPlayer()->getNetworkState());
}

void HTMLMediaElement::mediaLoadingFailed(WebMediaPlayer::NetworkState error) {
  stopPeriodicTimers();

  // If we failed while trying to load a <source> element, the movie was never
  // parsed, and there are more <source> children, schedule the next one
  if (m_readyState < kHaveMetadata && m_loadState == LoadingFromSourceElement) {
    // resource selection algorithm
    // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM
    // manipulation task source, to fire a simple event named error at the
    // candidate element.
    if (m_currentSourceNode)
      m_currentSourceNode->scheduleErrorEvent();
    else
      BLINK_MEDIA_LOG << "mediaLoadingFailed(" << (void*)this
                      << ") - error event not sent, <source> was removed";

    // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous
    // section consists of all the remaining steps of this algorithm until the
    // algorithm says the synchronous section has ended.

    // 9.Otherwise.11 - Forget the media element's media-resource-specific
    // tracks.
    forgetResourceSpecificTracks();

    if (havePotentialSourceChild()) {
      BLINK_MEDIA_LOG << "mediaLoadingFailed(" << (void*)this
                      << ") - scheduling next <source>";
      scheduleNextSourceChild();
    } else {
      BLINK_MEDIA_LOG << "mediaLoadingFailed(" << (void*)this
                      << ") - no more <source> elements, waiting";
      waitForSourceChange();
    }

    return;
  }

  if (error == WebMediaPlayer::NetworkStateNetworkError &&
      m_readyState >= kHaveMetadata)
    mediaEngineError(MediaError::create(MediaError::kMediaErrNetwork));
  else if (error == WebMediaPlayer::NetworkStateDecodeError)
    mediaEngineError(MediaError::create(MediaError::kMediaErrDecode));
  else if ((error == WebMediaPlayer::NetworkStateFormatError ||
            error == WebMediaPlayer::NetworkStateNetworkError) &&
           m_loadState == LoadingFromSrcAttr)
    noneSupported();

  updateDisplayState();
  if (mediaControls())
    mediaControls()->reset();
}

void HTMLMediaElement::setNetworkState(WebMediaPlayer::NetworkState state) {
  BLINK_MEDIA_LOG << "setNetworkState(" << (void*)this << ", "
                  << static_cast<int>(state) << ") - current state is "
                  << static_cast<int>(m_networkState);

  if (state == WebMediaPlayer::NetworkStateEmpty) {
    // Just update the cached state and leave, we can't do anything.
    setNetworkState(kNetworkEmpty);
    return;
  }

  if (state == WebMediaPlayer::NetworkStateFormatError ||
      state == WebMediaPlayer::NetworkStateNetworkError ||
      state == WebMediaPlayer::NetworkStateDecodeError) {
    mediaLoadingFailed(state);
    return;
  }

  if (state == WebMediaPlayer::NetworkStateIdle) {
    if (m_networkState > kNetworkIdle) {
      changeNetworkStateFromLoadingToIdle();
      setShouldDelayLoadEvent(false);
    } else {
      setNetworkState(kNetworkIdle);
    }
  }

  if (state == WebMediaPlayer::NetworkStateLoading) {
    if (m_networkState < kNetworkLoading || m_networkState == kNetworkNoSource)
      startProgressEventTimer();
    setNetworkState(kNetworkLoading);
  }

  if (state == WebMediaPlayer::NetworkStateLoaded) {
    if (m_networkState != kNetworkIdle)
      changeNetworkStateFromLoadingToIdle();
  }
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle() {
  m_progressEventTimer.stop();

  // Schedule one last progress event so we guarantee that at least one is fired
  // for files that load very quickly.
  if (webMediaPlayer() && webMediaPlayer()->didLoadingProgress())
    scheduleEvent(EventTypeNames::progress);
  scheduleEvent(EventTypeNames::suspend);
  setNetworkState(kNetworkIdle);
}

void HTMLMediaElement::readyStateChanged() {
  setReadyState(static_cast<ReadyState>(webMediaPlayer()->getReadyState()));
}

void HTMLMediaElement::setReadyState(ReadyState state) {
  BLINK_MEDIA_LOG << "setReadyState(" << (void*)this << ", "
                  << static_cast<int>(state) << ") - current state is "
                  << static_cast<int>(m_readyState);

  // Set "wasPotentiallyPlaying" BEFORE updating m_readyState,
  // potentiallyPlaying() uses it
  bool wasPotentiallyPlaying = potentiallyPlaying();

  ReadyState oldState = m_readyState;
  ReadyState newState = state;

  bool tracksAreReady = textTracksAreReady();

  if (newState == oldState && m_tracksAreReady == tracksAreReady)
    return;

  m_tracksAreReady = tracksAreReady;

  if (tracksAreReady) {
    m_readyState = newState;
  } else {
    // If a media file has text tracks the readyState may not progress beyond
    // kHaveFutureData until the text tracks are ready, regardless of the state
    // of the media file.
    if (newState <= kHaveMetadata)
      m_readyState = newState;
    else
      m_readyState = kHaveCurrentData;
  }

  if (oldState > m_readyStateMaximum)
    m_readyStateMaximum = oldState;

  if (m_networkState == kNetworkEmpty)
    return;

  if (m_seeking) {
    // 4.8.10.9, step 9 note: If the media element was potentially playing
    // immediately before it started seeking, but seeking caused its readyState
    // attribute to change to a value lower than kHaveFutureData, then a waiting
    // will be fired at the element.
    if (wasPotentiallyPlaying && m_readyState < kHaveFutureData)
      scheduleEvent(EventTypeNames::waiting);

    // 4.8.10.9 steps 12-14
    if (m_readyState >= kHaveCurrentData)
      finishSeek();
  } else {
    if (wasPotentiallyPlaying && m_readyState < kHaveFutureData) {
      // 4.8.10.8
      scheduleTimeupdateEvent(false);
      scheduleEvent(EventTypeNames::waiting);
    }
  }

  if (m_readyState >= kHaveMetadata && oldState < kHaveMetadata) {
    createPlaceholderTracksIfNecessary();

    selectInitialTracksIfNecessary();

    MediaFragmentURIParser fragmentParser(m_currentSrc);
    m_fragmentEndTime = fragmentParser.endTime();

    m_duration = duration();
    scheduleEvent(EventTypeNames::durationchange);

    if (isHTMLVideoElement())
      scheduleEvent(EventTypeNames::resize);
    scheduleEvent(EventTypeNames::loadedmetadata);

    bool jumped = false;
    if (m_defaultPlaybackStartPosition > 0) {
      seek(m_defaultPlaybackStartPosition);
      jumped = true;
    }
    m_defaultPlaybackStartPosition = 0;

    double initialPlaybackPosition = fragmentParser.startTime();
    if (std::isnan(initialPlaybackPosition))
      initialPlaybackPosition = 0;

    if (!jumped && initialPlaybackPosition > 0) {
      UseCounter::count(document(),
                        UseCounter::HTMLMediaElementSeekToFragmentStart);
      seek(initialPlaybackPosition);
      jumped = true;
    }

    if (mediaControls())
      mediaControls()->reset();
    if (layoutObject())
      layoutObject()->updateFromElement();
  }

  bool shouldUpdateDisplayState = false;

  if (m_readyState >= kHaveCurrentData && oldState < kHaveCurrentData &&
      !m_haveFiredLoadedData) {
    m_haveFiredLoadedData = true;
    shouldUpdateDisplayState = true;
    scheduleEvent(EventTypeNames::loadeddata);
    setShouldDelayLoadEvent(false);
  }

  bool isPotentiallyPlaying = potentiallyPlaying();
  if (m_readyState == kHaveFutureData && oldState <= kHaveCurrentData &&
      tracksAreReady) {
    scheduleEvent(EventTypeNames::canplay);
    if (isPotentiallyPlaying)
      scheduleNotifyPlaying();
    shouldUpdateDisplayState = true;
  }

  if (m_readyState == kHaveEnoughData && oldState < kHaveEnoughData &&
      tracksAreReady) {
    if (oldState <= kHaveCurrentData) {
      scheduleEvent(EventTypeNames::canplay);
      if (isPotentiallyPlaying)
        scheduleNotifyPlaying();
    }

    // Check for autoplay, and record metrics about it if needed.
    if (shouldAutoplay(RecordMetricsBehavior::DoRecord)) {
      m_autoplayUmaHelper->onAutoplayInitiated(AutoplaySource::Attribute);

      // If the autoplay experiment says that it's okay to play now,
      // then don't require a user gesture.
      m_autoplayHelper->becameReadyToPlay();

      if (!isGestureNeededForPlayback()) {
        if (isHTMLVideoElement() && muted() &&
            RuntimeEnabledFeatures::autoplayMutedVideosEnabled()) {
          // We might end up in a situation where the previous
          // observer didn't had time to fire yet. We can avoid
          // creating a new one in this case.
          if (!m_autoplayVisibilityObserver) {
            m_autoplayVisibilityObserver = new ElementVisibilityObserver(
                this,
                WTF::bind(&HTMLMediaElement::onVisibilityChangedForAutoplay,
                          wrapWeakPersistent(this)));
            m_autoplayVisibilityObserver->start();
          }
        } else {
          m_paused = false;
          invalidateCachedTime();
          scheduleEvent(EventTypeNames::play);
          scheduleNotifyPlaying();
          m_autoplaying = false;
        }
      }
    }

    scheduleEvent(EventTypeNames::canplaythrough);

    shouldUpdateDisplayState = true;
  }

  if (shouldUpdateDisplayState) {
    updateDisplayState();
    if (mediaControls())
      mediaControls()->refreshClosedCaptionsButtonVisibility();
  }

  updatePlayState();
  cueTimeline().updateActiveCues(currentTime());
}

void HTMLMediaElement::progressEventTimerFired(TimerBase*) {
  if (m_networkState != kNetworkLoading)
    return;

  double time = WTF::currentTime();
  double timedelta = time - m_previousProgressTime;

  if (webMediaPlayer() && webMediaPlayer()->didLoadingProgress()) {
    scheduleEvent(EventTypeNames::progress);
    m_previousProgressTime = time;
    m_sentStalledEvent = false;
    if (layoutObject())
      layoutObject()->updateFromElement();
  } else if (timedelta > 3.0 && !m_sentStalledEvent) {
    scheduleEvent(EventTypeNames::stalled);
    m_sentStalledEvent = true;
    setShouldDelayLoadEvent(false);
  }
}

void HTMLMediaElement::addPlayedRange(double start, double end) {
  BLINK_MEDIA_LOG << "addPlayedRange(" << (void*)this << ", " << start << ", "
                  << end << ")";
  if (!m_playedTimeRanges)
    m_playedTimeRanges = TimeRanges::create();
  m_playedTimeRanges->add(start, end);
}

bool HTMLMediaElement::supportsSave() const {
  return webMediaPlayer() && webMediaPlayer()->supportsSave();
}

void HTMLMediaElement::setIgnorePreloadNone() {
  BLINK_MEDIA_LOG << "setIgnorePreloadNone(" << (void*)this << ")";
  m_ignorePreloadNone = true;
  setPlayerPreload();
}

void HTMLMediaElement::seek(double time) {
  BLINK_MEDIA_LOG << "seek(" << (void*)this << ", " << time << ")";

  // 2 - If the media element's readyState is HAVE_NOTHING, abort these steps.
  // FIXME: remove m_webMediaPlayer check once we figure out how
  // m_webMediaPlayer is going out of sync with readystate.
  // m_webMediaPlayer is cleared but readystate is not set to HAVE_NOTHING.
  if (!m_webMediaPlayer || m_readyState == kHaveNothing)
    return;

  // Ignore preload none and start load if necessary.
  setIgnorePreloadNone();

  // Get the current time before setting m_seeking, m_lastSeekTime is returned
  // once it is set.
  refreshCachedTime();
  // This is needed to avoid getting default playback start position from
  // currentTime().
  double now = m_cachedTime;

  // 3 - If the element's seeking IDL attribute is true, then another instance
  // of this algorithm is already running. Abort that other instance of the
  // algorithm without waiting for the step that it is running to complete.
  // Nothing specific to be done here.

  // 4 - Set the seeking IDL attribute to true.
  // The flag will be cleared when the engine tells us the time has actually
  // changed.
  m_seeking = true;

  // 6 - If the new playback position is later than the end of the media
  // resource, then let it be the end of the media resource instead.
  time = std::min(time, duration());

  // 7 - If the new playback position is less than the earliest possible
  // position, let it be that position instead.
  time = std::max(time, 0.0);

  // Ask the media engine for the time value in the movie's time scale before
  // comparing with current time. This is necessary because if the seek time is
  // not equal to currentTime but the delta is less than the movie's time scale,
  // we will ask the media engine to "seek" to the current movie time, which may
  // be a noop and not generate a timechanged callback. This means m_seeking
  // will never be cleared and we will never fire a 'seeked' event.
  double mediaTime = webMediaPlayer()->mediaTimeForTimeValue(time);
  if (time != mediaTime) {
    BLINK_MEDIA_LOG << "seek(" << (void*)this << ", " << time
                    << ") - media timeline equivalent is " << mediaTime;
    time = mediaTime;
  }

  // 8 - If the (possibly now changed) new playback position is not in one of
  // the ranges given in the seekable attribute, then let it be the position in
  // one of the ranges given in the seekable attribute that is the nearest to
  // the new playback position. ... If there are no ranges given in the seekable
  // attribute then set the seeking IDL attribute to false and abort these
  // steps.
  TimeRanges* seekableRanges = seekable();

  if (!seekableRanges->length()) {
    m_seeking = false;
    return;
  }
  time = seekableRanges->nearest(time, now);

  if (m_playing && m_lastSeekTime < now)
    addPlayedRange(m_lastSeekTime, now);

  m_lastSeekTime = time;

  // 10 - Queue a task to fire a simple event named seeking at the element.
  scheduleEvent(EventTypeNames::seeking);

  // 11 - Set the current playback position to the given new playback position.
  webMediaPlayer()->seek(time);

  // 14-17 are handled, if necessary, when the engine signals a readystate
  // change or otherwise satisfies seek completion and signals a time change.
}

void HTMLMediaElement::finishSeek() {
  BLINK_MEDIA_LOG << "finishSeek(" << (void*)this << ")";

  // 14 - Set the seeking IDL attribute to false.
  m_seeking = false;

  // 16 - Queue a task to fire a simple event named timeupdate at the element.
  scheduleTimeupdateEvent(false);

  // 17 - Queue a task to fire a simple event named seeked at the element.
  scheduleEvent(EventTypeNames::seeked);

  setDisplayMode(Video);
}

HTMLMediaElement::ReadyState HTMLMediaElement::getReadyState() const {
  return m_readyState;
}

bool HTMLMediaElement::hasVideo() const {
  return webMediaPlayer() && webMediaPlayer()->hasVideo();
}

bool HTMLMediaElement::hasAudio() const {
  return webMediaPlayer() && webMediaPlayer()->hasAudio();
}

bool HTMLMediaElement::seeking() const {
  return m_seeking;
}

void HTMLMediaElement::refreshCachedTime() const {
  if (!webMediaPlayer() || m_readyState < kHaveMetadata)
    return;

  m_cachedTime = webMediaPlayer()->currentTime();
}

void HTMLMediaElement::invalidateCachedTime() {
  BLINK_MEDIA_LOG << "invalidateCachedTime(" << (void*)this << ")";
  m_cachedTime = std::numeric_limits<double>::quiet_NaN();
}

// playback state
double HTMLMediaElement::currentTime() const {
  if (m_defaultPlaybackStartPosition)
    return m_defaultPlaybackStartPosition;

  if (m_readyState == kHaveNothing)
    return 0;

  if (m_seeking) {
    BLINK_MEDIA_LOG << "currentTime(" << (void*)this
                    << ") - seeking, returning " << m_lastSeekTime;
    return m_lastSeekTime;
  }

  if (!std::isnan(m_cachedTime) && m_paused) {
#if LOG_CACHED_TIME_WARNINGS
    static const double minCachedDeltaForWarning = 0.01;
    double delta = m_cachedTime - webMediaPlayer()->currentTime();
    if (delta > minCachedDeltaForWarning)
      BLINK_MEDIA_LOG << "currentTime(" << (void*)this
                      << ") - WARNING, cached time is " << delta
                      << "seconds off of media time when paused";
#endif
    return m_cachedTime;
  }

  refreshCachedTime();

  return m_cachedTime;
}

void HTMLMediaElement::setCurrentTime(double time) {
  // If the media element's readyState is kHaveNothing, then set the default
  // playback start position to that time.
  if (m_readyState == kHaveNothing) {
    m_defaultPlaybackStartPosition = time;
    return;
  }

  seek(time);
}

double HTMLMediaElement::duration() const {
  // FIXME: remove m_webMediaPlayer check once we figure out how
  // m_webMediaPlayer is going out of sync with readystate.
  // m_webMediaPlayer is cleared but readystate is not set to kHaveNothing.
  if (!m_webMediaPlayer || m_readyState < kHaveMetadata)
    return std::numeric_limits<double>::quiet_NaN();

  // FIXME: Refactor so m_duration is kept current (in both MSE and
  // non-MSE cases) once we have transitioned from kHaveNothing ->
  // kHaveMetadata. Currently, m_duration may be out of date for at least MSE
  // case because MediaSource and SourceBuffer do not notify the element
  // directly upon duration changes caused by endOfStream, remove, or append
  // operations; rather the notification is triggered by the WebMediaPlayer
  // implementation observing that the underlying engine has updated duration
  // and notifying the element to consult its MediaSource for current
  // duration. See http://crbug.com/266644

  if (m_mediaSource)
    return m_mediaSource->duration();

  return webMediaPlayer()->duration();
}

bool HTMLMediaElement::paused() const {
  return m_paused;
}

double HTMLMediaElement::defaultPlaybackRate() const {
  return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(double rate) {
  if (m_defaultPlaybackRate == rate)
    return;

  m_defaultPlaybackRate = rate;
  scheduleEvent(EventTypeNames::ratechange);
}

double HTMLMediaElement::playbackRate() const {
  return m_playbackRate;
}

void HTMLMediaElement::setPlaybackRate(double rate) {
  BLINK_MEDIA_LOG << "setPlaybackRate(" << (void*)this << ", " << rate << ")";

  if (m_playbackRate != rate) {
    m_playbackRate = rate;
    invalidateCachedTime();
    scheduleEvent(EventTypeNames::ratechange);
  }

  updatePlaybackRate();
}

HTMLMediaElement::DirectionOfPlayback HTMLMediaElement::getDirectionOfPlayback()
    const {
  return m_playbackRate >= 0 ? Forward : Backward;
}

void HTMLMediaElement::updatePlaybackRate() {
  // FIXME: remove m_webMediaPlayer check once we figure out how
  // m_webMediaPlayer is going out of sync with readystate.
  // m_webMediaPlayer is cleared but readystate is not set to kHaveNothing.
  if (m_webMediaPlayer && potentiallyPlaying())
    webMediaPlayer()->setRate(playbackRate());
}

bool HTMLMediaElement::ended() const {
  // 4.8.10.8 Playing the media resource
  // The ended attribute must return true if the media element has ended
  // playback and the direction of playback is forwards, and false otherwise.
  return endedPlayback() && getDirectionOfPlayback() == Forward;
}

bool HTMLMediaElement::autoplay() const {
  return fastHasAttribute(autoplayAttr);
}

bool HTMLMediaElement::shouldAutoplay(
    const RecordMetricsBehavior recordMetrics) {
  if (m_autoplaying && m_paused && autoplay()) {
    if (document().isSandboxed(SandboxAutomaticFeatures)) {
      if (recordMetrics == RecordMetricsBehavior::DoRecord)
        m_autoplayHelper->recordSandboxFailure();
      return false;
    }

    return true;
  }

  return false;
}

String HTMLMediaElement::preload() const {
  return preloadTypeToString(preloadType());
}

void HTMLMediaElement::setPreload(const AtomicString& preload) {
  BLINK_MEDIA_LOG << "setPreload(" << (void*)this << ", " << preload << ")";
  setAttribute(preloadAttr, preload);
}

WebMediaPlayer::Preload HTMLMediaElement::preloadType() const {
  // Force preload to none for cellular connections or when data saver is
  // explicitly set.
  if (networkStateNotifier().isCellularConnectionType() ||
      (document().settings() &&
       (document().settings()->dataSaverEnabled() ||
        document().settings()->forcePreloadNoneForMediaElements()))) {
    UseCounter::count(document(),
                      UseCounter::HTMLMediaElementPreloadForcedNone);
    return WebMediaPlayer::PreloadNone;
  }

  const AtomicString& preload = fastGetAttribute(preloadAttr);
  if (equalIgnoringCase(preload, "none")) {
    UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadNone);
    return WebMediaPlayer::PreloadNone;
  }
  if (equalIgnoringCase(preload, "metadata")) {
    UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadMetadata);
    return WebMediaPlayer::PreloadMetaData;
  }
  if (equalIgnoringCase(preload, "auto")) {
    UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadAuto);
    return WebMediaPlayer::PreloadAuto;
  }

  // "The attribute's missing value default is user-agent defined, though the
  // Metadata state is suggested as a compromise between reducing server load
  // and providing an optimal user experience."

  // The spec does not define an invalid value default:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28950

  // TODO(foolip): Try to make "metadata" the default preload state:
  // https://crbug.com/310450
  UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadDefault);
  return WebMediaPlayer::PreloadAuto;
}

String HTMLMediaElement::effectivePreload() const {
  return preloadTypeToString(effectivePreloadType());
}

WebMediaPlayer::Preload HTMLMediaElement::effectivePreloadType() const {
  if (autoplay() && !isGestureNeededForPlayback())
    return WebMediaPlayer::PreloadAuto;

  WebMediaPlayer::Preload preload = preloadType();
  if (m_ignorePreloadNone && preload == WebMediaPlayer::PreloadNone)
    return WebMediaPlayer::PreloadMetaData;

  return preload;
}

ScriptPromise HTMLMediaElement::playForBindings(ScriptState* scriptState) {
  // We have to share the same logic for internal and external callers. The
  // internal callers do not want to receive a Promise back but when ::play()
  // is called, |m_playPromiseResolvers| needs to be populated. What this code
  // does is to populate |m_playPromiseResolvers| before calling ::play() and
  // remove the Promise if ::play() failed.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  m_playPromiseResolvers.append(resolver);

  Nullable<ExceptionCode> code = play();
  if (!code.isNull()) {
    DCHECK(!m_playPromiseResolvers.isEmpty());
    m_playPromiseResolvers.removeLast();

    String message;
    switch (code.get()) {
      case NotAllowedError:
        message = "play() can only be initiated by a user gesture.";
        break;
      case NotSupportedError:
        message = "The element has no supported sources.";
        break;
      default:
        NOTREACHED();
    }
    resolver->reject(DOMException::create(code.get(), message));
    return promise;
  }

  return promise;
}

Nullable<ExceptionCode> HTMLMediaElement::play() {
  BLINK_MEDIA_LOG << "play(" << (void*)this << ")";

  m_autoplayHelper->playMethodCalled();

  if (!UserGestureIndicator::processingUserGesture() && !document().frame()->isNodeJS()) {
    m_autoplayUmaHelper->onAutoplayInitiated(AutoplaySource::Method);
    if (isGestureNeededForPlayback()) {
      // If playback is deferred, then don't start playback but don't
      // fail yet either.
      if (m_autoplayHelper->isPlaybackDeferred())
        return nullptr;

      // If we're already playing, then this play would do nothing anyway.
      // Call playInternal to handle scheduling the promise resolution.
      if (!m_paused) {
        playInternal();
        return nullptr;
      }

      recordAutoplayMetric(PlayMethodFailed);
      String message = ExceptionMessages::failedToExecute(
          "play", "HTMLMediaElement",
          "API can only be initiated by a user gesture.");
      document().addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, WarningMessageLevel, message));
      return NotAllowedError;
    }
  } else {
    UserGestureIndicator::utilizeUserGesture();
    // We ask the helper to remove the gesture requirement for us, so that
    // it can record the reason.
    m_autoplayHelper->unlockUserGesture(GesturelessPlaybackEnabledByPlayMethod);
  }

  if (m_error && m_error->code() == MediaError::kMediaErrSrcNotSupported)
    return NotSupportedError;

  playInternal();

  return nullptr;
}

void HTMLMediaElement::playInternal() {
  BLINK_MEDIA_LOG << "playInternal(" << (void*)this << ")";

  // Always return the buffering strategy to normal when not paused,
  // regardless of the cause. (In contrast with aggressive buffering which is
  // only enabled by pause(), not pauseInternal().)
  if (webMediaPlayer())
    webMediaPlayer()->setBufferingStrategy(
        WebMediaPlayer::BufferingStrategy::Normal);

  // 4.8.10.9. Playing the media resource
  if (m_networkState == kNetworkEmpty)
    invokeResourceSelectionAlgorithm();

  // Generally "ended" and "looping" are exclusive. Here, the loop attribute
  // is ignored to seek back to start in case loop was set after playback
  // ended. See http://crbug.com/364442
  if (endedPlayback(LoopCondition::Ignored))
    seek(0);

  if (m_paused) {
    m_paused = false;
    invalidateCachedTime();
    scheduleEvent(EventTypeNames::play);

    if (m_readyState <= kHaveCurrentData)
      scheduleEvent(EventTypeNames::waiting);
    else if (m_readyState >= kHaveFutureData)
      scheduleNotifyPlaying();
  } else if (m_readyState >= kHaveFutureData) {
    scheduleResolvePlayPromises();
  }

  m_autoplaying = false;

  setIgnorePreloadNone();
  updatePlayState();
}

void HTMLMediaElement::pause() {
  BLINK_MEDIA_LOG << "pause(" << (void*)this << ")";

  // Only buffer aggressively on a user-initiated pause. Other types of pauses
  // (which go directly to pauseInternal()) should not cause this behavior.
  if (webMediaPlayer() && UserGestureIndicator::utilizeUserGesture())
    webMediaPlayer()->setBufferingStrategy(
        WebMediaPlayer::BufferingStrategy::Aggressive);

  pauseInternal();
}

void HTMLMediaElement::pauseInternal() {
  BLINK_MEDIA_LOG << "pauseInternal(" << (void*)this << ")";

  if (m_networkState == kNetworkEmpty)
    invokeResourceSelectionAlgorithm();

  m_autoplayHelper->pauseMethodCalled();

  m_autoplaying = false;

  if (!m_paused) {
    m_paused = true;
    scheduleTimeupdateEvent(false);
    scheduleEvent(EventTypeNames::pause);
    scheduleRejectPlayPromises(AbortError);
  }

  updatePlayState();
}

void HTMLMediaElement::requestRemotePlayback() {
  if (webMediaPlayer())
    webMediaPlayer()->requestRemotePlayback();
}

void HTMLMediaElement::requestRemotePlaybackControl() {
  if (webMediaPlayer())
    webMediaPlayer()->requestRemotePlaybackControl();
}

void HTMLMediaElement::closeMediaSource() {
  if (!m_mediaSource)
    return;

  m_mediaSource->close();
  m_mediaSource = nullptr;
}

bool HTMLMediaElement::loop() const {
  return fastHasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b) {
  BLINK_MEDIA_LOG << "setLoop(" << (void*)this << ", " << boolString(b) << ")";
  setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::shouldShowControls(
    const RecordMetricsBehavior recordMetrics) const {
  if (fastHasAttribute(controlsAttr)) {
    if (recordMetrics == RecordMetricsBehavior::DoRecord)
      showControlsHistogram().count(MediaControlsShowAttribute);
    return true;
  }

  if (isFullscreen()) {
    if (recordMetrics == RecordMetricsBehavior::DoRecord)
      showControlsHistogram().count(MediaControlsShowFullscreen);
    return true;
  }

  LocalFrame* frame = document().frame();
  if (frame && !frame->script().canExecuteScripts(NotAboutToExecuteScript)) {
    if (recordMetrics == RecordMetricsBehavior::DoRecord)
      showControlsHistogram().count(MediaControlsShowNoScript);
    return true;
  }

  if (recordMetrics == RecordMetricsBehavior::DoRecord)
    showControlsHistogram().count(MediaControlsShowNotShown);
  return false;
}

double HTMLMediaElement::volume() const {
  return m_volume;
}

void HTMLMediaElement::setVolume(double vol, ExceptionState& exceptionState) {
  BLINK_MEDIA_LOG << "setVolume(" << (void*)this << ", " << vol << ")";

  if (m_volume == vol)
    return;

  if (vol < 0.0f || vol > 1.0f) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange(
            "volume", vol, 0.0, ExceptionMessages::InclusiveBound, 1.0,
            ExceptionMessages::InclusiveBound));
    return;
  }

  m_volume = vol;
  updateVolume();
  scheduleEvent(EventTypeNames::volumechange);
}

bool HTMLMediaElement::muted() const {
  return m_muted;
}

void HTMLMediaElement::setMuted(bool muted) {
  BLINK_MEDIA_LOG << "setMuted(" << (void*)this << ", " << boolString(muted)
                  << ")";

  if (m_muted == muted)
    return;

  bool wasAutoplayingMuted = isAutoplayingMuted();
  bool wasPendingAutoplayMuted = m_autoplayVisibilityObserver && paused() &&
                                 m_muted && isLockedPendingUserGesture();

  if (UserGestureIndicator::processingUserGesture())
    unlockUserGesture();

  m_muted = muted;
  m_autoplayHelper->mutedChanged();

  scheduleEvent(EventTypeNames::volumechange);

  // If an element autoplayed while muted, it needs to be unlocked to unmute,
  // otherwise, it will be paused.
  if (wasAutoplayingMuted) {
    if (isGestureNeededForPlayback()) {
      pause();
      m_autoplayUmaHelper->recordAutoplayUnmuteStatus(
          AutoplayUnmuteActionStatus::Failure);
    } else {
      m_autoplayUmaHelper->recordAutoplayUnmuteStatus(
          AutoplayUnmuteActionStatus::Success);
    }
  }

  // This is called after the volumechange event to make sure isAutoplayingMuted
  // returns the right value when webMediaPlayer receives the volume update.
  updateVolume();

  // If an element was a candidate for autoplay muted but not visible, it will
  // have a visibility observer ready to start its playback.
  if (wasPendingAutoplayMuted) {
    m_autoplayVisibilityObserver->stop();
    m_autoplayVisibilityObserver = nullptr;
  }
}

void HTMLMediaElement::updateVolume() {
  if (webMediaPlayer())
    webMediaPlayer()->setVolume(effectiveMediaVolume());

  if (mediaControls())
    mediaControls()->updateVolume();
}

double HTMLMediaElement::effectiveMediaVolume() const {
  if (m_muted)
    return 0;

  return m_volume;
}

// The spec says to fire periodic timeupdate events (those sent while playing)
// every "15 to 250ms", we choose the slowest frequency
static const double maxTimeupdateEventFrequency = 0.25;

void HTMLMediaElement::startPlaybackProgressTimer() {
  if (m_playbackProgressTimer.isActive())
    return;

  m_previousProgressTime = WTF::currentTime();
  m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency,
                                         BLINK_FROM_HERE);
}

void HTMLMediaElement::playbackProgressTimerFired(TimerBase*) {
  if (!std::isnan(m_fragmentEndTime) && currentTime() >= m_fragmentEndTime &&
      getDirectionOfPlayback() == Forward) {
    m_fragmentEndTime = std::numeric_limits<double>::quiet_NaN();
    if (!m_paused) {
      UseCounter::count(document(),
                        UseCounter::HTMLMediaElementPauseAtFragmentEnd);
      // changes paused to true and fires a simple event named pause at the
      // media element.
      pauseInternal();
    }
  }

  if (!m_seeking)
    scheduleTimeupdateEvent(true);

  if (!playbackRate())
    return;

  if (!m_paused && mediaControls())
    mediaControls()->playbackProgressed();

  cueTimeline().updateActiveCues(currentTime());
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent) {
  double now = WTF::currentTime();
  double movieTime = currentTime();

  bool haveNotRecentlyFiredTimeupdate =
      (now - m_lastTimeUpdateEventWallTime) >= maxTimeupdateEventFrequency;
  bool movieTimeHasProgressed = movieTime != m_lastTimeUpdateEventMovieTime;

  // Non-periodic timeupdate events must always fire as mandated by the spec,
  // otherwise we shouldn't fire duplicate periodic timeupdate events when the
  // movie time hasn't changed.
  if (!periodicEvent ||
      (haveNotRecentlyFiredTimeupdate && movieTimeHasProgressed)) {
    scheduleEvent(EventTypeNames::timeupdate);
    m_lastTimeUpdateEventWallTime = now;
    m_lastTimeUpdateEventMovieTime = movieTime;
  }
}

void HTMLMediaElement::togglePlayState() {
  if (paused())
    play();
  else
    pause();
}

AudioTrackList& HTMLMediaElement::audioTracks() {
  DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());
  return *m_audioTracks;
}

void HTMLMediaElement::audioTrackChanged(AudioTrack* track) {
  BLINK_MEDIA_LOG << "audioTrackChanged(" << (void*)this
                  << ") trackId= " << String(track->id())
                  << " enabled=" << boolString(track->enabled());
  DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());

  audioTracks().scheduleChangeEvent();

  if (m_mediaSource)
    m_mediaSource->onTrackChanged(track);

  if (!m_audioTracksTimer.isActive())
    m_audioTracksTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::audioTracksTimerFired(TimerBase*) {
  Vector<WebMediaPlayer::TrackId> enabledTrackIds;
  for (unsigned i = 0; i < audioTracks().length(); ++i) {
    AudioTrack* track = audioTracks().anonymousIndexedGetter(i);
    if (track->enabled())
      enabledTrackIds.append(track->id());
  }

  webMediaPlayer()->enabledAudioTracksChanged(enabledTrackIds);
}

WebMediaPlayer::TrackId HTMLMediaElement::addAudioTrack(
    const WebString& id,
    WebMediaPlayerClient::AudioTrackKind kind,
    const WebString& label,
    const WebString& language,
    bool enabled) {
  AtomicString kindString = AudioKindToString(kind);
  BLINK_MEDIA_LOG << "addAudioTrack(" << (void*)this << ", '" << (String)id
                  << "', ' " << (AtomicString)kindString << "', '"
                  << (String)label << "', '" << (String)language << "', "
                  << boolString(enabled) << ")";

  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return blink::WebMediaPlayer::TrackId();

  AudioTrack* audioTrack =
      AudioTrack::create(id, kindString, label, language, enabled);
  audioTracks().add(audioTrack);

  return audioTrack->id();
}

void HTMLMediaElement::removeAudioTrack(WebMediaPlayer::TrackId trackId) {
  BLINK_MEDIA_LOG << "removeAudioTrack(" << (void*)this << ")";

  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return;

  audioTracks().remove(trackId);
}

VideoTrackList& HTMLMediaElement::videoTracks() {
  DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());
  return *m_videoTracks;
}

void HTMLMediaElement::selectedVideoTrackChanged(VideoTrack* track) {
  BLINK_MEDIA_LOG << "selectedVideoTrackChanged(" << (void*)this
                  << ") selectedTrackId="
                  << (track->selected() ? String(track->id()) : "none");
  DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());

  if (track->selected())
    videoTracks().trackSelected(track->id());

  videoTracks().scheduleChangeEvent();

  if (m_mediaSource)
    m_mediaSource->onTrackChanged(track);

  WebMediaPlayer::TrackId id = track->id();
  webMediaPlayer()->selectedVideoTrackChanged(track->selected() ? &id
                                                                : nullptr);
}

WebMediaPlayer::TrackId HTMLMediaElement::addVideoTrack(
    const WebString& id,
    WebMediaPlayerClient::VideoTrackKind kind,
    const WebString& label,
    const WebString& language,
    bool selected) {
  AtomicString kindString = VideoKindToString(kind);
  BLINK_MEDIA_LOG << "addVideoTrack(" << (void*)this << ", '" << (String)id
                  << "', '" << (AtomicString)kindString << "', '"
                  << (String)label << "', '" << (String)language << "', "
                  << boolString(selected) << ")";

  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return blink::WebMediaPlayer::TrackId();

  // If another track was selected (potentially by the user), leave it selected.
  if (selected && videoTracks().selectedIndex() != -1)
    selected = false;

  VideoTrack* videoTrack =
      VideoTrack::create(id, kindString, label, language, selected);
  videoTracks().add(videoTrack);

  return videoTrack->id();
}

void HTMLMediaElement::removeVideoTrack(WebMediaPlayer::TrackId trackId) {
  BLINK_MEDIA_LOG << "removeVideoTrack(" << (void*)this << ")";

  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return;

  videoTracks().remove(trackId);
}

void HTMLMediaElement::addTextTrack(WebInbandTextTrack* webTrack) {
  // 4.8.10.12.2 Sourcing in-band text tracks
  // 1. Associate the relevant data with a new text track and its corresponding
  // new TextTrack object.
  InbandTextTrack* textTrack = InbandTextTrack::create(webTrack);

  // 2. Set the new text track's kind, label, and language based on the
  // semantics of the relevant data, as defined by the relevant specification.
  // If there is no label in that data, then the label must be set to the empty
  // string.
  // 3. Associate the text track list of cues with the rules for updating the
  // text track rendering appropriate for the format in question.
  // 4. If the new text track's kind is metadata, then set the text track
  // in-band metadata track dispatch type as follows, based on the type of the
  // media resource:
  // 5. Populate the new text track's list of cues with the cues parsed so far,
  // folllowing the guidelines for exposing cues, and begin updating it
  // dynamically as necessary.
  //   - Thess are all done by the media engine.

  // 6. Set the new text track's readiness state to loaded.
  textTrack->setReadinessState(TextTrack::Loaded);

  // 7. Set the new text track's mode to the mode consistent with the user's
  // preferences and the requirements of the relevant specification for the
  // data.
  //  - This will happen in honorUserPreferencesForAutomaticTextTrackSelection()
  scheduleTextTrackResourceLoad();

  // 8. Add the new text track to the media element's list of text tracks.
  // 9. Fire an event with the name addtrack, that does not bubble and is not
  // cancelable, and that uses the TrackEvent interface, with the track
  // attribute initialized to the text track's TextTrack object, at the media
  // element's textTracks attribute's TextTrackList object.
  addTextTrack(textTrack);
}

void HTMLMediaElement::removeTextTrack(WebInbandTextTrack* webTrack) {
  if (!m_textTracks)
    return;

  // This cast is safe because we created the InbandTextTrack with the
  // WebInbandTextTrack passed to mediaPlayerDidAddTextTrack.
  InbandTextTrack* textTrack =
      static_cast<InbandTextTrack*>(webTrack->client());
  if (!textTrack)
    return;

  removeTextTrack(textTrack);
}

void HTMLMediaElement::textTracksChanged() {
  if (mediaControls())
    mediaControls()->refreshClosedCaptionsButtonVisibility();
}

void HTMLMediaElement::addTextTrack(TextTrack* track) {
  textTracks()->append(track);

  textTracksChanged();
}

void HTMLMediaElement::removeTextTrack(TextTrack* track) {
  m_textTracks->remove(track);

  textTracksChanged();
}

void HTMLMediaElement::forgetResourceSpecificTracks() {
  // Implements the "forget the media element's media-resource-specific tracks"
  // algorithm.  The order is explicitly specified as text, then audio, and
  // finally video.  Also 'removetrack' events should not be fired.
  if (m_textTracks) {
    TrackDisplayUpdateScope scope(this->cueTimeline());
    m_textTracks->removeAllInbandTracks();
    textTracksChanged();
  }

  m_audioTracks->removeAll();
  m_videoTracks->removeAll();

  m_audioTracksTimer.stop();
}

TextTrack* HTMLMediaElement::addTextTrack(const AtomicString& kind,
                                          const AtomicString& label,
                                          const AtomicString& language,
                                          ExceptionState& exceptionState) {
  // https://html.spec.whatwg.org/multipage/embedded-content.html#dom-media-addtexttrack

  // The addTextTrack(kind, label, language) method of media elements, when
  // invoked, must run the following steps:

  // 1. Create a new TextTrack object.
  // 2. Create a new text track corresponding to the new object, and set its
  //    text track kind to kind, its text track label to label, its text
  //    track language to language, ..., and its text track list of cues to
  //    an empty list.
  TextTrack* textTrack = TextTrack::create(kind, label, language);
  //    ..., its text track readiness state to the text track loaded state, ...
  textTrack->setReadinessState(TextTrack::Loaded);

  // 3. Add the new text track to the media element's list of text tracks.
  // 4. Queue a task to fire a trusted event with the name addtrack, that
  //    does not bubble and is not cancelable, and that uses the TrackEvent
  //    interface, with the track attribute initialised to the new text
  //    track's TextTrack object, at the media element's textTracks
  //    attribute's TextTrackList object.
  addTextTrack(textTrack);

  // Note: Due to side effects when changing track parameters, we have to
  // first append the track to the text track list.
  // FIXME: Since setMode() will cause a 'change' event to be queued on the
  // same task source as the 'addtrack' event (see above), the order is
  // wrong. (The 'change' event shouldn't be fired at all in this case...)

  // ..., its text track mode to the text track hidden mode, ...
  textTrack->setMode(TextTrack::hiddenKeyword());

  // 5. Return the new TextTrack object.
  return textTrack;
}

TextTrackList* HTMLMediaElement::textTracks() {
  if (!m_textTracks)
    m_textTracks = TextTrackList::create(this);

  return m_textTracks.get();
}

void HTMLMediaElement::didAddTrackElement(HTMLTrackElement* trackElement) {
  // 4.8.10.12.3 Sourcing out-of-band text tracks
  // When a track element's parent element changes and the new parent is a media
  // element, then the user agent must add the track element's corresponding
  // text track to the media element's list of text tracks ... [continues in
  // TextTrackList::append]
  TextTrack* textTrack = trackElement->track();
  if (!textTrack)
    return;

  addTextTrack(textTrack);

  // Do not schedule the track loading until parsing finishes so we don't start
  // before all tracks in the markup have been added.
  if (isFinishedParsingChildren())
    scheduleTextTrackResourceLoad();
}

void HTMLMediaElement::didRemoveTrackElement(HTMLTrackElement* trackElement) {
  KURL url = trackElement->getNonEmptyURLAttribute(srcAttr);
  BLINK_MEDIA_LOG << "didRemoveTrackElement(" << (void*)this << ") - 'src' is "
                  << urlForLoggingMedia(url);

  TextTrack* textTrack = trackElement->track();
  if (!textTrack)
    return;

  textTrack->setHasBeenConfigured(false);

  if (!m_textTracks)
    return;

  // 4.8.10.12.3 Sourcing out-of-band text tracks
  // When a track element's parent element changes and the old parent was a
  // media element, then the user agent must remove the track element's
  // corresponding text track from the media element's list of text tracks.
  removeTextTrack(textTrack);

  size_t index = m_textTracksWhenResourceSelectionBegan.find(textTrack);
  if (index != kNotFound)
    m_textTracksWhenResourceSelectionBegan.remove(index);
}

void HTMLMediaElement::honorUserPreferencesForAutomaticTextTrackSelection() {
  if (!m_textTracks || !m_textTracks->length())
    return;

  if (!m_shouldPerformAutomaticTrackSelection)
    return;

  AutomaticTrackSelection::Configuration configuration;
  if (m_processingPreferenceChange)
    configuration.disableCurrentlyEnabledTracks = true;
  if (m_textTracksVisible)
    configuration.forceEnableSubtitleOrCaptionTrack = true;

  Settings* settings = document().settings();
  if (settings)
    configuration.textTrackKindUserPreference =
        settings->textTrackKindUserPreference();

  AutomaticTrackSelection trackSelection(configuration);
  trackSelection.perform(*m_textTracks);

  textTracksChanged();
}

bool HTMLMediaElement::havePotentialSourceChild() {
  // Stash the current <source> node and next nodes so we can restore them after
  // checking to see there is another potential.
  HTMLSourceElement* currentSourceNode = m_currentSourceNode;
  Node* nextNode = m_nextChildNodeToConsider;

  KURL nextURL = selectNextSourceChild(0, DoNothing);

  m_currentSourceNode = currentSourceNode;
  m_nextChildNodeToConsider = nextNode;

  return nextURL.isValid();
}

KURL HTMLMediaElement::selectNextSourceChild(ContentType* contentType,
                                             InvalidURLAction actionIfInvalid) {
  // Don't log if this was just called to find out if there are any valid
  // <source> elements.
  bool shouldLog = actionIfInvalid != DoNothing;
  if (shouldLog)
    BLINK_MEDIA_LOG << "selectNextSourceChild(" << (void*)this << ")";

  if (!m_nextChildNodeToConsider) {
    if (shouldLog)
      BLINK_MEDIA_LOG << "selectNextSourceChild(" << (void*)this
                      << ") -> 0x0000, \"\"";
    return KURL();
  }

  KURL mediaURL;
  Node* node;
  HTMLSourceElement* source = 0;
  String type;
  bool lookingForStartNode = m_nextChildNodeToConsider;
  bool canUseSourceElement = false;

  NodeVector potentialSourceNodes;
  getChildNodes(*this, potentialSourceNodes);

  for (unsigned i = 0; !canUseSourceElement && i < potentialSourceNodes.size();
       ++i) {
    node = potentialSourceNodes[i].get();
    if (lookingForStartNode && m_nextChildNodeToConsider != node)
      continue;
    lookingForStartNode = false;

    if (!isHTMLSourceElement(*node))
      continue;
    if (node->parentNode() != this)
      continue;

    source = toHTMLSourceElement(node);

    // 2. If candidate does not have a src attribute, or if its src
    // attribute's value is the empty string ... jump down to the failed
    // step below
    const AtomicString& srcValue = source->fastGetAttribute(srcAttr);
    if (shouldLog)
      BLINK_MEDIA_LOG << "selectNextSourceChild(" << (void*)this
                      << ") - 'src' is " << urlForLoggingMedia(mediaURL);
    if (srcValue.isEmpty())
      goto checkAgain;

    // 3. Let urlString be the resulting URL string that would have resulted
    // from parsing the URL specified by candidate's src attribute's value
    // relative to the candidate's node document when the src attribute was
    // last changed.
    mediaURL = source->document().completeURL(srcValue);

    // 4. If urlString was not obtained successfully, then end the
    // synchronous section, and jump down to the failed with elements step
    // below.
    if (!isSafeToLoadURL(mediaURL, actionIfInvalid))
      goto checkAgain;

    // 5. If candidate has a type attribute whose value, when parsed as a
    // MIME type ...
    type = source->type();
    if (type.isEmpty() && mediaURL.protocolIsData())
      type = mimeTypeFromDataURL(mediaURL);
    if (!type.isEmpty()) {
      if (shouldLog)
        BLINK_MEDIA_LOG << "selectNextSourceChild(" << (void*)this
                        << ") - 'type' is '" << type << "'";
      if (!supportsType(ContentType(type)))
        goto checkAgain;
    }

    // Making it this far means the <source> looks reasonable.
    canUseSourceElement = true;

  checkAgain:
    if (!canUseSourceElement && actionIfInvalid == Complain && source)
      source->scheduleErrorEvent();
  }

  if (canUseSourceElement) {
    if (contentType)
      *contentType = ContentType(type);
    m_currentSourceNode = source;
    m_nextChildNodeToConsider = source->nextSibling();
  } else {
    m_currentSourceNode = nullptr;
    m_nextChildNodeToConsider = nullptr;
  }

  if (shouldLog)
    BLINK_MEDIA_LOG << "selectNextSourceChild(" << (void*)this << ") -> "
                    << m_currentSourceNode.get() << ", "
                    << (canUseSourceElement ? urlForLoggingMedia(mediaURL)
                                            : "");
  return canUseSourceElement ? mediaURL : KURL();
}

void HTMLMediaElement::sourceWasAdded(HTMLSourceElement* source) {
  BLINK_MEDIA_LOG << "sourceWasAdded(" << (void*)this << ", " << source << ")";

  KURL url = source->getNonEmptyURLAttribute(srcAttr);
  BLINK_MEDIA_LOG << "sourceWasAdded(" << (void*)this << ") - 'src' is "
                  << urlForLoggingMedia(url);

  // We should only consider a <source> element when there is not src attribute
  // at all.
  if (fastHasAttribute(srcAttr))
    return;

  // 4.8.8 - If a source element is inserted as a child of a media element that
  // has no src attribute and whose networkState has the value NETWORK_EMPTY,
  // the user agent must invoke the media element's resource selection
  // algorithm.
  if (getNetworkState() == HTMLMediaElement::kNetworkEmpty) {
    invokeResourceSelectionAlgorithm();
    // Ignore current |m_nextChildNodeToConsider| and consider |source|.
    m_nextChildNodeToConsider = source;
    return;
  }

  if (m_currentSourceNode && source == m_currentSourceNode->nextSibling()) {
    BLINK_MEDIA_LOG << "sourceWasAdded(" << (void*)this
                    << ") - <source> inserted immediately after current source";
    // Ignore current |m_nextChildNodeToConsider| and consider |source|.
    m_nextChildNodeToConsider = source;
    return;
  }

  // Consider current |m_nextChildNodeToConsider| as it is already in the middle
  // of processing.
  if (m_nextChildNodeToConsider)
    return;

  if (m_loadState != WaitingForSource)
    return;

  // 4.8.9.5, resource selection algorithm, source elements section:
  // 21. Wait until the node after pointer is a node other than the end of the
  // list. (This step might wait forever.)
  // 22. Asynchronously await a stable state...
  // 23. Set the element's delaying-the-load-event flag back to true (this
  // delays the load event again, in case it hasn't been fired yet).
  setShouldDelayLoadEvent(true);

  // 24. Set the networkState back to NETWORK_LOADING.
  setNetworkState(kNetworkLoading);

  // 25. Jump back to the find next candidate step above.
  m_nextChildNodeToConsider = source;
  scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement* source) {
  BLINK_MEDIA_LOG << "sourceWasRemoved(" << (void*)this << ", " << source
                  << ")";

  KURL url = source->getNonEmptyURLAttribute(srcAttr);
  BLINK_MEDIA_LOG << "sourceWasRemoved(" << (void*)this << ") - 'src' is "
                  << urlForLoggingMedia(url);

  if (source != m_currentSourceNode && source != m_nextChildNodeToConsider)
    return;

  if (source == m_nextChildNodeToConsider) {
    if (m_currentSourceNode)
      m_nextChildNodeToConsider = m_currentSourceNode->nextSibling();
    BLINK_MEDIA_LOG << "sourceWasRemoved(" << (void*)this
                    << ") - m_nextChildNodeToConsider set to "
                    << m_nextChildNodeToConsider.get();
  } else if (source == m_currentSourceNode) {
    // Clear the current source node pointer, but don't change the movie as the
    // spec says:
    // 4.8.8 - Dynamically modifying a source element and its attribute when the
    // element is already inserted in a video or audio element will have no
    // effect.
    m_currentSourceNode = nullptr;
    BLINK_MEDIA_LOG << "sourceWasRemoved(" << (void*)this
                    << ") - m_currentSourceNode set to 0";
  }
}

void HTMLMediaElement::timeChanged() {
  BLINK_MEDIA_LOG << "timeChanged(" << (void*)this << ")";

  cueTimeline().updateActiveCues(currentTime());

  invalidateCachedTime();

  // 4.8.10.9 steps 12-14. Needed if no ReadyState change is associated with the
  // seek.
  if (m_seeking && m_readyState >= kHaveCurrentData &&
      !webMediaPlayer()->seeking())
    finishSeek();

  // Always call scheduleTimeupdateEvent when the media engine reports a time
  // discontinuity, it will only queue a 'timeupdate' event if we haven't
  // already posted one at the current movie time.
  scheduleTimeupdateEvent(false);

  double now = currentTime();
  double dur = duration();

  // When the current playback position reaches the end of the media resource
  // when the direction of playback is forwards, then the user agent must follow
  // these steps:
  if (!std::isnan(dur) && dur && now >= dur &&
      getDirectionOfPlayback() == Forward) {
    // If the media element has a loop attribute specified
    if (loop()) {
      //  then seek to the earliest possible position of the media resource and
      //  abort these steps.
      seek(0);
    } else {
      // If the media element has still ended playback, and the direction of
      // playback is still forwards, and paused is false,
      if (!m_paused) {
        // changes paused to true and fires a simple event named pause at the
        // media element.
        m_paused = true;
        scheduleEvent(EventTypeNames::pause);
        scheduleRejectPlayPromises(AbortError);
      }
      // Queue a task to fire a simple event named ended at the media element.
      scheduleEvent(EventTypeNames::ended);
    }
  }
  updatePlayState();
}

void HTMLMediaElement::durationChanged() {
  BLINK_MEDIA_LOG << "durationChanged(" << (void*)this << ")";
  // FIXME: Change WebMediaPlayer to convey the currentTime
  // when the duration change occured. The current WebMediaPlayer
  // implementations always clamp currentTime() to duration()
  // so the requestSeek condition here is always false.
  durationChanged(duration(), currentTime() > duration());
}

void HTMLMediaElement::durationChanged(double duration, bool requestSeek) {
  BLINK_MEDIA_LOG << "durationChanged(" << (void*)this << ", " << duration
                  << ", " << boolString(requestSeek) << ")";

  // Abort if duration unchanged.
  if (m_duration == duration)
    return;

  BLINK_MEDIA_LOG << "durationChanged(" << (void*)this << ") : " << m_duration
                  << " -> " << duration;
  m_duration = duration;
  scheduleEvent(EventTypeNames::durationchange);

  if (mediaControls())
    mediaControls()->reset();
  if (layoutObject())
    layoutObject()->updateFromElement();

  if (requestSeek)
    seek(duration);
}

void HTMLMediaElement::playbackStateChanged() {
  BLINK_MEDIA_LOG << "playbackStateChanged(" << (void*)this << ")";

  if (!webMediaPlayer())
    return;

  if (webMediaPlayer()->paused())
    pauseInternal();
  else
    playInternal();
}

void HTMLMediaElement::requestSeek(double time) {
  // The player is the source of this seek request.
  setCurrentTime(time);
}

void HTMLMediaElement::remoteRouteAvailabilityChanged(bool routesAvailable) {
  m_remoteRoutesAvailable = routesAvailable;
  if (mediaControls())
    mediaControls()->refreshCastButtonVisibility();
  if (remotePlaybackClient())
    remotePlaybackClient()->availabilityChanged(routesAvailable);
}

void HTMLMediaElement::connectedToRemoteDevice() {
  m_playingRemotely = true;
  if (mediaControls())
    mediaControls()->startedCasting();
  if (remotePlaybackClient())
    remotePlaybackClient()->stateChanged(WebRemotePlaybackState::Connected);
}

void HTMLMediaElement::disconnectedFromRemoteDevice() {
  m_playingRemotely = false;
  if (mediaControls())
    mediaControls()->stoppedCasting();
  if (remotePlaybackClient())
    remotePlaybackClient()->stateChanged(WebRemotePlaybackState::Disconnected);
}

void HTMLMediaElement::cancelledRemotePlaybackRequest() {
  if (remotePlaybackClient())
    remotePlaybackClient()->promptCancelled();
}

bool HTMLMediaElement::isAutoplayingMuted() {
  if (!isHTMLVideoElement() ||
      !RuntimeEnabledFeatures::autoplayMutedVideosEnabled()) {
    return false;
  }

  return !paused() && muted() && isLockedPendingUserGesture();
}

void HTMLMediaElement::requestReload(const WebURL& newUrl) {
  DCHECK(webMediaPlayer());
  DCHECK(!m_srcObject);
  DCHECK(newUrl.isValid());
  DCHECK(isSafeToLoadURL(newUrl, Complain));
  resetMediaPlayerAndMediaSource();
  startPlayerLoad(newUrl);
}

// MediaPlayerPresentation methods
void HTMLMediaElement::repaint() {
  if (m_webLayer)
    m_webLayer->invalidate();

  updateDisplayState();
  if (layoutObject())
    layoutObject()->setShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::sizeChanged() {
  BLINK_MEDIA_LOG << "sizeChanged(" << (void*)this << ")";

  DCHECK(hasVideo());  // "resize" makes no sense in absence of video.
  if (m_readyState > kHaveNothing && isHTMLVideoElement())
    scheduleEvent(EventTypeNames::resize);

  if (layoutObject())
    layoutObject()->updateFromElement();
}

TimeRanges* HTMLMediaElement::buffered() const {
  if (m_mediaSource)
    return m_mediaSource->buffered();

  if (!webMediaPlayer())
    return TimeRanges::create();

  return TimeRanges::create(webMediaPlayer()->buffered());
}

TimeRanges* HTMLMediaElement::played() {
  if (m_playing) {
    double time = currentTime();
    if (time > m_lastSeekTime)
      addPlayedRange(m_lastSeekTime, time);
  }

  if (!m_playedTimeRanges)
    m_playedTimeRanges = TimeRanges::create();

  return m_playedTimeRanges->copy();
}

TimeRanges* HTMLMediaElement::seekable() const {
  if (!webMediaPlayer())
    return TimeRanges::create();

  if (m_mediaSource)
    return m_mediaSource->seekable();

  return TimeRanges::create(webMediaPlayer()->seekable());
}

bool HTMLMediaElement::potentiallyPlaying() const {
  // "pausedToBuffer" means the media engine's rate is 0, but only because it
  // had to stop playing when it ran out of buffered data. A movie in this state
  // is "potentially playing", modulo the checks in couldPlayIfEnoughData().
  bool pausedToBuffer =
      m_readyStateMaximum >= kHaveFutureData && m_readyState < kHaveFutureData;
  return (pausedToBuffer || m_readyState >= kHaveFutureData) &&
         couldPlayIfEnoughData();
}

bool HTMLMediaElement::couldPlayIfEnoughData() const {
  return !paused() && !endedPlayback() && !stoppedDueToErrors();
}

bool HTMLMediaElement::endedPlayback(LoopCondition loopCondition) const {
  double dur = duration();
  if (std::isnan(dur))
    return false;

  // 4.8.10.8 Playing the media resource

  // A media element is said to have ended playback when the element's
  // readyState attribute is HAVE_METADATA or greater,
  if (m_readyState < kHaveMetadata)
    return false;

  // and the current playback position is the end of the media resource and the
  // direction of playback is forwards, Either the media element does not have a
  // loop attribute specified,
  double now = currentTime();
  if (getDirectionOfPlayback() == Forward)
    return dur > 0 && now >= dur &&
           (loopCondition == LoopCondition::Ignored || !loop());

  // or the current playback position is the earliest possible position and the
  // direction of playback is backwards
  DCHECK_EQ(getDirectionOfPlayback(), Backward);
  return now <= 0;
}

bool HTMLMediaElement::stoppedDueToErrors() const {
  if (m_readyState >= kHaveMetadata && m_error) {
    TimeRanges* seekableRanges = seekable();
    if (!seekableRanges->contain(currentTime()))
      return true;
  }

  return false;
}

void HTMLMediaElement::updatePlayState() {
  bool isPlaying = webMediaPlayer() && !webMediaPlayer()->paused();
  bool shouldBePlaying = potentiallyPlaying();

  BLINK_MEDIA_LOG << "updatePlayState(" << (void*)this
                  << ") - shouldBePlaying = " << boolString(shouldBePlaying)
                  << ", isPlaying = " << boolString(isPlaying);

  if (shouldBePlaying) {
    setDisplayMode(Video);
    invalidateCachedTime();

    if (!isPlaying) {
      // Set rate, muted before calling play in case they were set before the
      // media engine was setup.  The media engine should just stash the rate
      // and muted values since it isn't already playing.
      webMediaPlayer()->setRate(playbackRate());
      updateVolume();
      webMediaPlayer()->play();
      m_autoplayHelper->playbackStarted();
    }

    if (mediaControls())
      mediaControls()->playbackStarted();
    startPlaybackProgressTimer();
    m_playing = true;

  } else {  // Should not be playing right now
    if (isPlaying) {
      webMediaPlayer()->pause();
      m_autoplayHelper->playbackStopped();
    }

    refreshCachedTime();

    m_playbackProgressTimer.stop();
    m_playing = false;
    double time = currentTime();
    if (time > m_lastSeekTime)
      addPlayedRange(m_lastSeekTime, time);

    if (mediaControls())
      mediaControls()->playbackStopped();
  }

  if (layoutObject())
    layoutObject()->updateFromElement();
}

void HTMLMediaElement::stopPeriodicTimers() {
  m_progressEventTimer.stop();
  m_playbackProgressTimer.stop();
}

void HTMLMediaElement::
    clearMediaPlayerAndAudioSourceProviderClientWithoutLocking() {
  getAudioSourceProvider().setClient(nullptr);
  if (m_webMediaPlayer) {
    m_audioSourceProvider.wrap(nullptr);
    m_webMediaPlayer.reset();
  }
}

void HTMLMediaElement::clearMediaPlayer() {
  forgetResourceSpecificTracks();

  closeMediaSource();

  cancelDeferredLoad();

  {
    AudioSourceProviderClientLockScope scope(*this);
    clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  }

  stopPeriodicTimers();
  m_loadTimer.stop();

  m_pendingActionFlags = 0;
  m_loadState = WaitingForSource;

  // We can't cast if we don't have a media player.
  m_remoteRoutesAvailable = false;
  m_playingRemotely = false;
  if (mediaControls())
    mediaControls()->refreshCastButtonVisibilityWithoutUpdate();

  if (layoutObject())
    layoutObject()->setShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::contextDestroyed() {
  BLINK_MEDIA_LOG << "contextDestroyed(" << (void*)this << ")";

  // Close the async event queue so that no events are enqueued.
  cancelPendingEventsAndCallbacks();
  m_asyncEventQueue->close();

  // Clear everything in the Media Element
  clearMediaPlayer();
  m_readyState = kHaveNothing;
  m_readyStateMaximum = kHaveNothing;
  setNetworkState(kNetworkEmpty);
  setShouldDelayLoadEvent(false);
  m_currentSourceNode = nullptr;
  invalidateCachedTime();
  cueTimeline().updateActiveCues(0);
  m_playing = false;
  m_paused = true;
  m_seeking = false;

  if (layoutObject())
    layoutObject()->updateFromElement();

  stopPeriodicTimers();

  // Ensure that hasPendingActivity() is not preventing garbage collection,
  // since otherwise this media element will simply leak.
  DCHECK(!hasPendingActivity());
}

bool HTMLMediaElement::hasPendingActivity() const {
  // The delaying-the-load-event flag is set by resource selection algorithm
  // when looking for a resource to load, before networkState has reached to
  // kNetworkLoading.
  if (m_shouldDelayLoadEvent)
    return true;

  // When networkState is kNetworkLoading, progress and stalled events may be
  // fired.
  if (m_networkState == kNetworkLoading)
    return true;

  // When playing or if playback may continue, timeupdate events may be fired.
  if (couldPlayIfEnoughData())
    return true;

  // When the seek finishes timeupdate and seeked events will be fired.
  if (m_seeking)
    return true;

  // When connected to a MediaSource, e.g. setting MediaSource.duration will
  // cause a durationchange event to be fired.
  if (m_mediaSource)
    return true;

  // Wait for any pending events to be fired.
  if (m_asyncEventQueue->hasPendingEvents())
    return true;

  return false;
}

bool HTMLMediaElement::isFullscreen() const {
  return Fullscreen::isCurrentFullScreenElement(*this);
}

void HTMLMediaElement::enterFullscreen() {
  BLINK_MEDIA_LOG << "enterFullscreen(" << (void*)this << ")";

  Fullscreen::requestFullscreen(*this, Fullscreen::PrefixedRequest);
}

void HTMLMediaElement::exitFullscreen() {
  BLINK_MEDIA_LOG << "exitFullscreen(" << (void*)this << ")";

  Fullscreen::exitFullscreen(document());
}

void HTMLMediaElement::didBecomeFullscreenElement() {
  if (mediaControls())
    mediaControls()->enteredFullscreen();
  // FIXME: There is no embedder-side handling in layout test mode.
  if (webMediaPlayer() && !LayoutTestSupport::isRunningLayoutTest())
    webMediaPlayer()->enteredFullscreen();
  // Cache this in case the player is destroyed before leaving fullscreen.
  m_inOverlayFullscreenVideo = usesOverlayFullscreenVideo();
  if (m_inOverlayFullscreenVideo)
    document().layoutViewItem().compositor()->setNeedsCompositingUpdate(
        CompositingUpdateRebuildTree);
}

void HTMLMediaElement::willStopBeingFullscreenElement() {
  if (mediaControls())
    mediaControls()->exitedFullscreen();
  if (webMediaPlayer())
    webMediaPlayer()->exitedFullscreen();
  if (m_inOverlayFullscreenVideo)
    document().layoutViewItem().compositor()->setNeedsCompositingUpdate(
        CompositingUpdateRebuildTree);
  m_inOverlayFullscreenVideo = false;
}

WebLayer* HTMLMediaElement::platformLayer() const {
  return m_webLayer;
}

bool HTMLMediaElement::hasClosedCaptions() const {
  if (m_textTracks) {
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
      if (m_textTracks->anonymousIndexedGetter(i)->canBeRendered())
        return true;
    }
  }
  return false;
}

bool HTMLMediaElement::textTracksVisible() const {
  return m_textTracksVisible;
}

static void assertShadowRootChildren(ShadowRoot& shadowRoot) {
#if DCHECK_IS_ON()
  // There can be up to two children, either or both of the text
  // track container and media controls. If both are present, the
  // text track container must be the first child.
  unsigned numberOfChildren = shadowRoot.countChildren();
  DCHECK_LE(numberOfChildren, 2u);
  Node* firstChild = shadowRoot.firstChild();
  Node* lastChild = shadowRoot.lastChild();
  if (numberOfChildren == 1) {
    DCHECK(firstChild->isTextTrackContainer() || firstChild->isMediaControls());
  } else if (numberOfChildren == 2) {
    DCHECK(firstChild->isTextTrackContainer());
    DCHECK(lastChild->isMediaControls());
  }
#endif
}

TextTrackContainer& HTMLMediaElement::ensureTextTrackContainer() {
  ShadowRoot& shadowRoot = ensureUserAgentShadowRoot();
  assertShadowRootChildren(shadowRoot);

  Node* firstChild = shadowRoot.firstChild();
  if (firstChild && firstChild->isTextTrackContainer())
    return toTextTrackContainer(*firstChild);

  TextTrackContainer* textTrackContainer =
      TextTrackContainer::create(document());

  // The text track container should be inserted before the media controls,
  // so that they are rendered behind them.
  shadowRoot.insertBefore(textTrackContainer, firstChild);

  assertShadowRootChildren(shadowRoot);

  return *textTrackContainer;
}

void HTMLMediaElement::updateTextTrackDisplay() {
  BLINK_MEDIA_LOG << "updateTextTrackDisplay(" << (void*)this << ")";

  ensureTextTrackContainer().updateDisplay(
      *this, TextTrackContainer::DidNotStartExposingControls);
}

void HTMLMediaElement::mediaControlsDidBecomeVisible() {
  BLINK_MEDIA_LOG << "mediaControlsDidBecomeVisible(" << (void*)this << ")";

  // When the user agent starts exposing a user interface for a video element,
  // the user agent should run the rules for updating the text track rendering
  // of each of the text tracks in the video element's list of text tracks ...
  if (isHTMLVideoElement() && textTracksVisible())
    ensureTextTrackContainer().updateDisplay(
        *this, TextTrackContainer::DidStartExposingControls);
}

void HTMLMediaElement::setTextTrackKindUserPreferenceForAllMediaElements(
    Document* document) {
  auto it = documentToElementSetMap().find(document);
  if (it == documentToElementSetMap().end())
    return;
  DCHECK(it->value);
  WeakMediaElementSet& elements = *it->value;
  for (const auto& element : elements)
    element->automaticTrackSelectionForUpdatedUserPreference();
}

void HTMLMediaElement::automaticTrackSelectionForUpdatedUserPreference() {
  if (!m_textTracks || !m_textTracks->length())
    return;

  markCaptionAndSubtitleTracksAsUnconfigured();
  m_processingPreferenceChange = true;
  m_textTracksVisible = false;
  honorUserPreferencesForAutomaticTextTrackSelection();
  m_processingPreferenceChange = false;

  // If a track is set to 'showing' post performing automatic track selection,
  // set text tracks state to visible to update the CC button and display the
  // track.
  m_textTracksVisible = m_textTracks->hasShowingTracks();
  updateTextTrackDisplay();
}

void HTMLMediaElement::markCaptionAndSubtitleTracksAsUnconfigured() {
  if (!m_textTracks)
    return;

  // Mark all tracks as not "configured" so that
  // honorUserPreferencesForAutomaticTextTrackSelection() will reconsider
  // which tracks to display in light of new user preferences (e.g. default
  // tracks should not be displayed if the user has turned off captions and
  // non-default tracks should be displayed based on language preferences if
  // the user has turned captions on).
  for (unsigned i = 0; i < m_textTracks->length(); ++i) {
    TextTrack* textTrack = m_textTracks->anonymousIndexedGetter(i);
    if (textTrack->isVisualKind())
      textTrack->setHasBeenConfigured(false);
  }
}

unsigned HTMLMediaElement::webkitAudioDecodedByteCount() const {
  if (!webMediaPlayer())
    return 0;
  return webMediaPlayer()->audioDecodedByteCount();
}

unsigned HTMLMediaElement::webkitVideoDecodedByteCount() const {
  if (!webMediaPlayer())
    return 0;
  return webMediaPlayer()->videoDecodedByteCount();
}

bool HTMLMediaElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMediaElement::setShouldDelayLoadEvent(bool shouldDelay) {
  if (m_shouldDelayLoadEvent == shouldDelay)
    return;

  BLINK_MEDIA_LOG << "setShouldDelayLoadEvent(" << (void*)this << ", "
                  << boolString(shouldDelay) << ")";

  m_shouldDelayLoadEvent = shouldDelay;
  if (shouldDelay)
    document().incrementLoadEventDelayCount();
  else
    document().decrementLoadEventDelayCount();
}

MediaControls* HTMLMediaElement::mediaControls() const {
  if (ShadowRoot* shadowRoot = userAgentShadowRoot()) {
    Node* lastChild = shadowRoot->lastChild();
    if (lastChild && lastChild->isMediaControls())
      return toMediaControls(lastChild);
  }

  return nullptr;
}

void HTMLMediaElement::ensureMediaControls() {
  if (mediaControls())
    return;

  MediaControls* mediaControls = MediaControls::create(*this);

  mediaControls->reset();
  if (isFullscreen())
    mediaControls->enteredFullscreen();

  ShadowRoot& shadowRoot = ensureUserAgentShadowRoot();
  assertShadowRootChildren(shadowRoot);

  // The media controls should be inserted after the text track container,
  // so that they are rendered in front of captions and subtitles.
  shadowRoot.appendChild(mediaControls);

  assertShadowRootChildren(shadowRoot);

  if (!shouldShowControls() || !isConnected())
    mediaControls->hide();
}

void HTMLMediaElement::configureMediaControls() {
  if (!isConnected()) {
    if (mediaControls())
      mediaControls()->hide();
    return;
  }

  ensureMediaControls();
  mediaControls()->reset();

  if (shouldShowControls(RecordMetricsBehavior::DoRecord))
    mediaControls()->show();
  else
    mediaControls()->hide();
}

CueTimeline& HTMLMediaElement::cueTimeline() {
  if (!m_cueTimeline)
    m_cueTimeline = new CueTimeline(*this);
  return *m_cueTimeline;
}

void HTMLMediaElement::configureTextTrackDisplay() {
  DCHECK(m_textTracks);
  BLINK_MEDIA_LOG << "configureTextTrackDisplay(" << (void*)this << ")";

  if (m_processingPreferenceChange)
    return;

  bool haveVisibleTextTrack = m_textTracks->hasShowingTracks();
  m_textTracksVisible = haveVisibleTextTrack;

  if (!haveVisibleTextTrack && !mediaControls())
    return;

  if (mediaControls())
    mediaControls()->changedClosedCaptionsVisibility();

  cueTimeline().updateActiveCues(currentTime());

  // Note: The "time marches on" algorithm (updateActiveCues) runs the "rules
  // for updating the text track rendering" (updateTextTrackDisplay) only for
  // "affected tracks", i.e. tracks where the the active cues have changed.
  // This misses cues in tracks that changed mode between hidden and showing.
  // This appears to be a spec bug, which we work around here:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28236
  updateTextTrackDisplay();
}

EventDispatchHandlingState* HTMLMediaElement::preDispatchEventHandler(
    Event* event) {
  if (event && event->type() == EventTypeNames::webkitfullscreenchange)
    configureMediaControls();

  return nullptr;
}

// TODO(srirama.m): Merge it to resetMediaElement if possible and remove it.
void HTMLMediaElement::resetMediaPlayerAndMediaSource() {
  closeMediaSource();

  {
    AudioSourceProviderClientLockScope scope(*this);
    clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  }

  // We haven't yet found out if any remote routes are available.
  m_remoteRoutesAvailable = false;
  m_playingRemotely = false;

  if (m_audioSourceNode)
    getAudioSourceProvider().setClient(m_audioSourceNode);
}

void HTMLMediaElement::setAudioSourceNode(
    AudioSourceProviderClient* sourceNode) {
  DCHECK(isMainThread());
  m_audioSourceNode = sourceNode;

  AudioSourceProviderClientLockScope scope(*this);
  getAudioSourceProvider().setClient(m_audioSourceNode);
}

void HTMLMediaElement::setAllowHiddenVolumeControls(bool allow) {
  ensureMediaControls();
  mediaControls()->setAllowHiddenVolumeControls(allow);
}

WebMediaPlayer::CORSMode HTMLMediaElement::corsMode() const {
  const AtomicString& crossOriginMode = fastGetAttribute(crossoriginAttr);
  if (crossOriginMode.isNull())
    return WebMediaPlayer::CORSModeUnspecified;
  if (equalIgnoringCase(crossOriginMode, "use-credentials"))
    return WebMediaPlayer::CORSModeUseCredentials;
  return WebMediaPlayer::CORSModeAnonymous;
}

void HTMLMediaElement::setWebLayer(WebLayer* webLayer) {
  if (webLayer == m_webLayer)
    return;

  // If either of the layers is null we need to enable or disable compositing.
  // This is done by triggering a style recalc.
  if (!m_webLayer || !webLayer)
    setNeedsCompositingUpdate();

  if (m_webLayer)
    GraphicsLayer::unregisterContentsLayer(m_webLayer);
  m_webLayer = webLayer;
  if (m_webLayer)
    GraphicsLayer::registerContentsLayer(m_webLayer);
}

void HTMLMediaElement::mediaSourceOpened(WebMediaSource* webMediaSource) {
  setShouldDelayLoadEvent(false);
  m_mediaSource->setWebMediaSourceAndOpen(wrapUnique(webMediaSource));
}

bool HTMLMediaElement::isInteractiveContent() const {
  return fastHasAttribute(controlsAttr);
}

void HTMLMediaElement::defaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::focusin) {
    if (mediaControls())
      mediaControls()->mediaElementFocused();
  }
  HTMLElement::defaultEventHandler(event);
}

DEFINE_TRACE(HTMLMediaElement) {
  visitor->trace(m_playedTimeRanges);
  visitor->trace(m_asyncEventQueue);
  visitor->trace(m_error);
  visitor->trace(m_currentSourceNode);
  visitor->trace(m_nextChildNodeToConsider);
  visitor->trace(m_mediaSource);
  visitor->trace(m_audioTracks);
  visitor->trace(m_videoTracks);
  visitor->trace(m_cueTimeline);
  visitor->trace(m_textTracks);
  visitor->trace(m_textTracksWhenResourceSelectionBegan);
  visitor->trace(m_playPromiseResolvers);
  visitor->trace(m_playPromiseResolveList);
  visitor->trace(m_playPromiseRejectList);
  visitor->trace(m_audioSourceProvider);
  visitor->trace(m_autoplayHelperClient);
  visitor->trace(m_autoplayHelper);
  visitor->trace(m_autoplayUmaHelper);
  visitor->trace(m_srcObject);
  visitor->trace(m_autoplayVisibilityObserver);
  visitor->template registerWeakMembers<HTMLMediaElement,
                                        &HTMLMediaElement::clearWeakMembers>(
      this);
  Supplementable<HTMLMediaElement>::trace(visitor);
  HTMLElement::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLMediaElement) {
  visitor->traceWrappers(m_videoTracks);
  visitor->traceWrappers(m_audioTracks);
  visitor->traceWrappers(m_textTracks);
  HTMLElement::traceWrappers(visitor);
}

void HTMLMediaElement::createPlaceholderTracksIfNecessary() {
  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return;

  // Create a placeholder audio track if the player says it has audio but it
  // didn't explicitly announce the tracks.
  if (hasAudio() && !audioTracks().length())
    addAudioTrack("audio", WebMediaPlayerClient::AudioTrackKindMain,
                  "Audio Track", "", true);

  // Create a placeholder video track if the player says it has video but it
  // didn't explicitly announce the tracks.
  if (hasVideo() && !videoTracks().length())
    addVideoTrack("video", WebMediaPlayerClient::VideoTrackKindMain,
                  "Video Track", "", true);
}

void HTMLMediaElement::selectInitialTracksIfNecessary() {
  if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
    return;

  // Enable the first audio track if an audio track hasn't been enabled yet.
  if (audioTracks().length() > 0 && !audioTracks().hasEnabledTrack())
    audioTracks().anonymousIndexedGetter(0)->setEnabled(true);

  // Select the first video track if a video track hasn't been selected yet.
  if (videoTracks().length() > 0 && videoTracks().selectedIndex() == -1)
    videoTracks().anonymousIndexedGetter(0)->setSelected(true);
}

bool HTMLMediaElement::isLockedPendingUserGesture() const {
  return m_lockedPendingUserGesture;
}

void HTMLMediaElement::unlockUserGesture() {
  m_lockedPendingUserGesture = false;
}

bool HTMLMediaElement::isGestureNeededForPlayback() const {
  if (!m_lockedPendingUserGesture)
    return false;

  if (loadType() == WebMediaPlayer::LoadTypeMediaStream)
    return false;

  // We want to allow muted video to autoplay if:
  // - the flag is enabled;
  // - Data Saver is not enabled;
  // - Autoplay is enabled in settings;
  if (isHTMLVideoElement() && muted() &&
      RuntimeEnabledFeatures::autoplayMutedVideosEnabled() &&
      !(document().settings() && document().settings()->dataSaverEnabled()) &&
      isAutoplayAllowedPerSettings()) {
    return false;
  }

  if (m_autoplayHelper->isGestureRequirementOverridden())
    return false;

  return true;
}

bool HTMLMediaElement::isAutoplayAllowedPerSettings() const {
  LocalFrame* frame = document().frame();
  if (!frame)
    return false;
  FrameLoaderClient* frameLoaderClient = frame->loader().client();
  return frameLoaderClient && frameLoaderClient->allowAutoplay(false);
}

void HTMLMediaElement::setNetworkState(NetworkState state) {
  if (m_networkState != state) {
    m_networkState = state;
    if (MediaControls* controls = mediaControls())
      controls->networkStateChanged();
  }
}

void HTMLMediaElement::notifyPositionMayHaveChanged(
    const IntRect& visibleRect) {
  m_autoplayHelper->positionChanged(visibleRect);
}

void HTMLMediaElement::updatePositionNotificationRegistration() {
  m_autoplayHelper->updatePositionNotificationRegistration();
}

void HTMLMediaElement::setRemotePlaybackClient(
    WebRemotePlaybackClient* client) {
  m_remotePlaybackClient = client;
}

// TODO(liberato): remove once autoplay gesture override experiment concludes.
void HTMLMediaElement::triggerAutoplayViewportCheckForTesting() {
  if (FrameView* view = document().view())
    m_autoplayHelper->positionChanged(
        view->rootFrameToContents(view->computeVisibleArea()));
  m_autoplayHelper->triggerAutoplayViewportCheckForTesting();
}

void HTMLMediaElement::scheduleResolvePlayPromises() {
  // TODO(mlamouri): per spec, we should create a new task but we can't create
  // a new cancellable task without cancelling the previous one. There are two
  // approaches then: cancel the previous task and create a new one with the
  // appended promise list or append the new promise to the current list. The
  // latter approach is preferred because it might be the less observable
  // change.
  DCHECK(m_playPromiseResolveList.isEmpty() ||
         m_playPromiseResolveTask->isPending());
  if (m_playPromiseResolvers.isEmpty())
    return;

  m_playPromiseResolveList.appendVector(m_playPromiseResolvers);
  m_playPromiseResolvers.clear();

  if (m_playPromiseResolveTask->isPending())
    return;

  Platform::current()->currentThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, m_playPromiseResolveTask->cancelAndCreate());
}

void HTMLMediaElement::scheduleRejectPlayPromises(ExceptionCode code) {
  // TODO(mlamouri): per spec, we should create a new task but we can't create
  // a new cancellable task without cancelling the previous one. There are two
  // approaches then: cancel the previous task and create a new one with the
  // appended promise list or append the new promise to the current list. The
  // latter approach is preferred because it might be the less observable
  // change.
  DCHECK(m_playPromiseRejectList.isEmpty() ||
         m_playPromiseRejectTask->isPending());
  if (m_playPromiseResolvers.isEmpty())
    return;

  m_playPromiseRejectList.appendVector(m_playPromiseResolvers);
  m_playPromiseResolvers.clear();

  if (m_playPromiseRejectTask->isPending())
    return;

  // TODO(mlamouri): because cancellable tasks can't take parameters, the
  // error code needs to be saved.
  m_playPromiseErrorCode = code;
  Platform::current()->currentThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, m_playPromiseRejectTask->cancelAndCreate());
}

void HTMLMediaElement::scheduleNotifyPlaying() {
  scheduleEvent(EventTypeNames::playing);
  scheduleResolvePlayPromises();
}

void HTMLMediaElement::resolveScheduledPlayPromises() {
  for (auto& resolver : m_playPromiseResolveList)
    resolver->resolve();

  m_playPromiseResolveList.clear();
}

void HTMLMediaElement::rejectScheduledPlayPromises() {
  // TODO(mlamouri): the message is generated based on the code because
  // arguments can't be passed to a cancellable task. In order to save space
  // used by the object, the string isn't saved.
  DCHECK(m_playPromiseErrorCode == AbortError ||
         m_playPromiseErrorCode == NotSupportedError);
  if (m_playPromiseErrorCode == AbortError)
    rejectPlayPromisesInternal(
        AbortError, "The play() request was interrupted by a call to pause().");
  else
    rejectPlayPromisesInternal(
        NotSupportedError,
        "Failed to load because no supported source was found.");
}

void HTMLMediaElement::rejectPlayPromises(ExceptionCode code,
                                          const String& message) {
  m_playPromiseRejectList.appendVector(m_playPromiseResolvers);
  m_playPromiseResolvers.clear();
  rejectPlayPromisesInternal(code, message);
}

void HTMLMediaElement::rejectPlayPromisesInternal(ExceptionCode code,
                                                  const String& message) {
  DCHECK(code == AbortError || code == NotSupportedError);

  for (auto& resolver : m_playPromiseRejectList)
    resolver->reject(DOMException::create(code, message));

  m_playPromiseRejectList.clear();
}

EnumerationHistogram& HTMLMediaElement::showControlsHistogram() const {
  if (isHTMLVideoElement()) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, histogram,
                        ("Media.Controls.Show.Video", MediaControlsShowMax));
    return histogram;
  }

  DEFINE_STATIC_LOCAL(EnumerationHistogram, histogram,
                      ("Media.Controls.Show.Audio", MediaControlsShowMax));
  return histogram;
}

void HTMLMediaElement::onVisibilityChangedForAutoplay(bool isVisible) {
  if (!isVisible)
    return;

  if (shouldAutoplay()) {
    m_paused = false;
    invalidateCachedTime();
    scheduleEvent(EventTypeNames::play);
    scheduleNotifyPlaying();
    m_autoplaying = false;

    updatePlayState();
  }

  // TODO(zqzhang): There's still flaky leak if onVisibilityChangedForAutoplay()
  // is never called.  The leak comes from either ElementVisibilityObserver or
  // IntersectionObserver. Should keep an eye on it.  See
  // https://crbug.com/627539
  m_autoplayVisibilityObserver->stop();
  m_autoplayVisibilityObserver = nullptr;
}

void HTMLMediaElement::clearWeakMembers(Visitor* visitor) {
  if (!ThreadHeap::isHeapObjectAlive(m_audioSourceNode)) {
    getAudioSourceProvider().setClient(nullptr);
    m_audioSourceNode = nullptr;
  }
}

void HTMLMediaElement::AudioSourceProviderImpl::wrap(
    WebAudioSourceProvider* provider) {
  MutexLocker locker(provideInputLock);

  if (m_webAudioSourceProvider && provider != m_webAudioSourceProvider)
    m_webAudioSourceProvider->setClient(nullptr);

  m_webAudioSourceProvider = provider;
  if (m_webAudioSourceProvider)
    m_webAudioSourceProvider->setClient(m_client.get());
}

void HTMLMediaElement::AudioSourceProviderImpl::setClient(
    AudioSourceProviderClient* client) {
  MutexLocker locker(provideInputLock);

  if (client)
    m_client = new HTMLMediaElement::AudioClientImpl(client);
  else
    m_client.clear();

  if (m_webAudioSourceProvider)
    m_webAudioSourceProvider->setClient(m_client.get());
}

void HTMLMediaElement::AudioSourceProviderImpl::provideInput(
    AudioBus* bus,
    size_t framesToProcess) {
  DCHECK(bus);

  MutexTryLocker tryLocker(provideInputLock);
  if (!tryLocker.locked() || !m_webAudioSourceProvider || !m_client.get()) {
    bus->zero();
    return;
  }

  // Wrap the AudioBus channel data using WebVector.
  size_t n = bus->numberOfChannels();
  WebVector<float*> webAudioData(n);
  for (size_t i = 0; i < n; ++i)
    webAudioData[i] = bus->channel(i)->mutableData();

  m_webAudioSourceProvider->provideInput(webAudioData, framesToProcess);
}

void HTMLMediaElement::AudioClientImpl::setFormat(size_t numberOfChannels,
                                                  float sampleRate) {
  if (m_client)
    m_client->setFormat(numberOfChannels, sampleRate);
}

DEFINE_TRACE(HTMLMediaElement::AudioClientImpl) {
  visitor->trace(m_client);
}

DEFINE_TRACE(HTMLMediaElement::AudioSourceProviderImpl) {
  visitor->trace(m_client);
}

HTMLMediaElement::AutoplayHelperClientImpl::~AutoplayHelperClientImpl() {}

bool HTMLMediaElement::AutoplayHelperClientImpl::isLegacyViewportType() {
  return m_element->document().viewportDescription().isLegacyViewportType();
}

PageVisibilityState
HTMLMediaElement::AutoplayHelperClientImpl::pageVisibilityState() const {
  return m_element->document().pageVisibilityState();
}

String HTMLMediaElement::AutoplayHelperClientImpl::autoplayExperimentMode()
    const {
  String mode;
  if (m_element->document().settings())
    mode = m_element->document().settings()->autoplayExperimentMode();

  return mode;
}

bool HTMLMediaElement::AutoplayHelperClientImpl::isAutoplayAllowedPerSettings()
    const {
  return m_element->isAutoplayAllowedPerSettings();
}

void HTMLMediaElement::AutoplayHelperClientImpl::setRequestPositionUpdates(
    bool request) {
  if (LayoutObject* layoutObject = m_element->layoutObject()) {
    LayoutMediaItem layoutMediaItem =
        LayoutMediaItem(toLayoutMedia(layoutObject));
    layoutMediaItem.setRequestPositionUpdates(request);
  }
}

IntRect HTMLMediaElement::AutoplayHelperClientImpl::absoluteBoundingBoxRect()
    const {
  IntRect result;
  if (LayoutObject* object = m_element->layoutObject())
    result = object->absoluteBoundingBoxRect();
  return result;
}

}  // namespace blink

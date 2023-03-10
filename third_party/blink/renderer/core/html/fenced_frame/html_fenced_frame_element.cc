// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_ad_sizes.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

PhysicalRect ToPhysicalRect(const DOMRectReadOnly& rect) {
  return PhysicalRect(LayoutUnit::FromDoubleRound(rect.x()),
                      LayoutUnit::FromDoubleRound(rect.y()),
                      LayoutUnit::FromDoubleRound(rect.width()),
                      LayoutUnit::FromDoubleRound(rect.height()));
}

mojom::blink::FencedFrameMode GetModeAttributeValue(const String& value) {
  // Keep this in sync with the values in the `FencedFrameMode` enum.
  if (EqualIgnoringASCIICase(value, "opaque-ads"))
    return mojom::blink::FencedFrameMode::kOpaqueAds;
  return mojom::blink::FencedFrameMode::kDefault;
}

String FencedFrameModeToString(mojom::blink::FencedFrameMode mode) {
  switch (mode) {
    case mojom::blink::FencedFrameMode::kDefault:
      return "default";
    case mojom::blink::FencedFrameMode::kOpaqueAds:
      return "opaque-ads";
  }

  NOTREACHED();
  return "";
}

// Helper function that returns whether the mode of the parent tree is different
// than the mode given to the function. Note that this function will return
// false if there is no mode set in the parent tree (i.e. not in a fenced frame
// tree).
bool ParentModeIsDifferent(mojom::blink::FencedFrameMode current_mode,
                           LocalFrame& frame) {
  Page* ancestor_page = frame.GetPage();
  return ancestor_page->IsMainFrameFencedFrameRoot() &&
         ancestor_page->FencedFrameMode() != current_mode;
}

bool HasDifferentModeThanParent(HTMLFencedFrameElement& outer_element) {
  return ParentModeIsDifferent(outer_element.GetMode(),
                               *(outer_element.GetDocument().GetFrame()));
}

// Returns whether `requested_size` is exactly the same size as `allowed_size`.
// `requested_size` and `allowed_size` should both be in CSS pixel units.
bool SizeMatchesExactly(const PhysicalSize& requested_size,
                        const gfx::Size& allowed_size) {
  // The comparison must be performed as a `PhysicalSize`, in order to use
  // its fixed point representation and get exact results.
  return requested_size == PhysicalSize(allowed_size);
}

// Returns a loss score (higher is worse) comparing the fit between
// `requested_size` and `allowed_size`.
// Both sizes should be in CSS pixel units.
double ComputeSizeLossFunction(const PhysicalSize& requested_size,
                               const gfx::Size& allowed_size) {
  const double requested_width = requested_size.width.ToDouble();
  const double requested_height = requested_size.height.ToDouble();

  const double allowed_width = allowed_size.width();
  const double allowed_height = allowed_size.height();

  const double allowed_area = allowed_width * allowed_height;
  const double requested_area = requested_width * requested_height;

  // Calculate the fraction of the outer container that is wasted when the
  // allowed inner frame size is scaled to fit inside of it.
  const double scale_x = allowed_width / requested_width;
  const double scale_y = allowed_height / requested_height;

  const double wasted_area =
      scale_x < scale_y
          ? allowed_width * (allowed_height - (scale_x * requested_height))
          : allowed_height * (allowed_width - (scale_y * requested_width));

  const double wasted_area_fraction = wasted_area / allowed_area;

  // Calculate a penalty to tie-break between allowed sizes with the same
  // aspect ratio in favor of resolutions closer to the requested one.
  const double resolution_penalty =
      std::abs(1 - std::min(requested_area, allowed_area) /
                       std::max(requested_area, allowed_area));

  return wasted_area_fraction + resolution_penalty;
}

}  // namespace

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
  StartResizeObserver();
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
  visitor->Trace(resize_observer_);
  visitor->Trace(config_);
}

void HTMLFencedFrameElement::DisconnectContentFrame() {
  DCHECK(!GetDocument().IsPrerendering());

  // The `frame_delegate_` will not exist if the element was not allowed to
  // create its underlying frame at insertion-time.
  if (frame_delegate_)
    frame_delegate_->Dispose();
  frame_delegate_ = nullptr;

  HTMLFrameOwnerElement::DisconnectContentFrame();
}

ParsedPermissionsPolicy HTMLFencedFrameElement::ConstructContainerPolicy()
    const {
  if (!GetExecutionContext())
    return ParsedPermissionsPolicy();

  scoped_refptr<const SecurityOrigin> src_origin =
      GetOriginForPermissionsPolicy();
  scoped_refptr<const SecurityOrigin> self_origin =
      GetExecutionContext()->GetSecurityOrigin();

  PolicyParserMessageBuffer logger;

  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(allow_, self_origin, src_origin,
                                              logger, GetExecutionContext());

  for (const auto& message : logger.GetMessages()) {
    GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther, message.level,
            message.content),
        /* discard_duplicates */ true);
  }

  return container_policy;
}

void HTMLFencedFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse)
    return;

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());

  // Trigger style recalc to trigger layout tree re-attachment.
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kFrame));
}

void HTMLFencedFrameElement::DidChangeContainerPolicy() {
  // Don't notify about updates if frame_delegate_ is null, for example when
  // the delegate hasn't been created yet.
  if (frame_delegate_)
    frame_delegate_->DidChangeFramePolicy(GetFramePolicy());
}

// START HTMLFencedFrameElement::FencedFrameDelegate.

HTMLFencedFrameElement::FencedFrameDelegate*
HTMLFencedFrameElement::FencedFrameDelegate::Create(
    HTMLFencedFrameElement* outer_element) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(
      outer_element->GetExecutionContext()));

  // If the frame embedding a fenced frame is a detached frame, the execution
  // context will be null. That makes it impossible to check the sandbox flags,
  // so delegate creation is stopped if that is the case.
  if (!outer_element->GetExecutionContext()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame in a detached frame."));
    return nullptr;
  }

  // If the element has been disconnected by the time we attempt to create the
  // delegate (eg, due to deferral while prerendering), we should not create the
  // delegate.
  //
  // NB: this check should remain at the beginning of this function so that the
  // remainder of the function can safely assume the frame is connected.
  if (!outer_element->isConnected()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame when disconnected."));
    return nullptr;
  }

  if (outer_element->GetExecutionContext()->IsSandboxed(
          kFencedFrameMandatoryUnsandboxedFlags)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. A sandboxed document can load fenced "
            "frames only when all of the following permissions are set: "
            "allow-same-origin, allow-forms, allow-scripts, allow-popups, "
            "allow-popups-to-escape-sandbox and "
            "allow-top-navigation-by-user-activation."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kSandboxFlagsNotSet);
    RecordFencedFrameUnsandboxedFlags(
        outer_element->GetExecutionContext()->GetSandboxFlags());
    RecordFencedFrameFailedSandboxLoadInTopLevelFrame(
        outer_element->GetDocument().GetFrame()->IsMainFrame());
    return nullptr;
  }

  if (!SubframeLoadingDisabler::CanLoadFrame(*outer_element)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Subframe loading disabled."));
    return nullptr;
  }

  // The frame limit only needs to be checked on initial creation before
  // attempting to insert it into the DOM. This behavior matches how iframes
  // handles frame limits.
  if (!outer_element->IsCurrentlyWithinFrameLimit()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Frame limit exceeded."));
    return nullptr;
  }

  // We must be connected at this point due to the isConnected check at the top
  // of this function.
  DCHECK(outer_element->GetDocument().GetFrame());

  if (HasDifferentModeThanParent(*outer_element)) {
    mojom::blink::FencedFrameMode parent_mode =
        outer_element->GetDocument().GetPage()->FencedFrameMode();

    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Cannot create a fenced frame with mode '" +
                FencedFrameModeToString(outer_element->GetMode()) +
                "' nested in a fenced frame with mode '" +
                FencedFrameModeToString(parent_mode) + "'."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kIncompatibleMode);
    return nullptr;
  }

  return MakeGarbageCollected<FencedFrameMPArchDelegate>(outer_element);
}

HTMLFencedFrameElement::FencedFrameDelegate::~FencedFrameDelegate() = default;

void HTMLFencedFrameElement::FencedFrameDelegate::Trace(
    Visitor* visitor) const {
  visitor->Trace(outer_element_);
}

// END HTMLFencedFrameElement::FencedFrameDelegate.

HTMLIFrameElement* HTMLFencedFrameElement::InnerIFrameElement() const {
  if (const ShadowRoot* root = UserAgentShadowRoot())
    return To<HTMLIFrameElement>(root->lastChild());
  return nullptr;
}

void HTMLFencedFrameElement::setConfig(FencedFrameConfig* config) {
  config_ = config;
  if (config_) {
    NavigateToConfig();
  }
}

// static
bool HTMLFencedFrameElement::canLoadOpaqueURL(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return false;

  LocalFrame* frame_to_check = LocalDOMWindow::From(script_state)->GetFrame();
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(frame_to_check && context);

  ContentSecurityPolicy* csp = context->GetContentSecurityPolicy();
  DCHECK(csp);

  // "A fenced frame tree of one mode cannot contain a child fenced frame of
  // another mode."
  // See: https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md
  // TODO(lbrady) Link to spec once it's written.
  if (ParentModeIsDifferent(mojom::blink::FencedFrameMode::kOpaqueAds,
                            *frame_to_check)) {
    return false;
  }

  if (!context->IsSecureContext())
    return false;

  // Check that the flags specified in kFencedFrameMandatoryUnsandboxedFlags
  // are not set in this context. Fenced frames loaded in a sandboxed document
  // require these flags to remain unsandboxed.
  if (context->IsSandboxed(kFencedFrameMandatoryUnsandboxedFlags))
    return false;

  // Check the results of the browser checks for the current frame.
  // If the embedding frame is an iframe with CSPEE set, or any ancestor
  // iframes has CSPEE set, the fenced frame will not be allowed to load.
  // The renderer has no knowledge of CSPEE up the ancestor chain, so we defer
  // to the browser to determine the existence of CSPEE outside of the scope
  // we can see here.
  if (frame_to_check->AncestorOrSelfHasCSPEE())
    return false;

  // Ensure that if any CSP headers are set that will affect a fenced frame,
  // they allow all https urls to load. Opaque-ads fenced frames do not support
  // allowing/disallowing specific hosts, as that could reveal information to
  // a fenced frame about its embedding page. See design doc for more info:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/interaction_with_content_security_policy.md
  // This is being checked in the renderer because processing of <meta> tags
  // (including CSP) happen in the renderer after navigation commit, so we can't
  // piggy-back off of the ancestor_or_self_has_cspee bit being sent from the
  // browser (which is sent at commit time) since it doesn't know about all the
  // CSP headers yet.
  for (const auto& policy : csp->GetParsedPolicies()) {
    CSPOperativeDirective directive = CSPDirectiveListOperativeDirective(
        *policy, CSPDirectiveName::FencedFrameSrc);
    if (directive.type != CSPDirectiveName::Unknown) {
      // "*" urls will cause the allow_star flag to set
      if (directive.source_list->allow_star) {
        continue;
      }

      // Check for "https:" or "https://*:*"
      bool found_matching_source = false;
      for (const auto& source : directive.source_list->sources) {
        if (source->scheme == url::kHttpsScheme && source->host == "") {
          found_matching_source = true;
          break;
        }
      }
      if (!found_matching_source)
        return false;
    }
  }

  return true;
}

Node::InsertionNotificationRequest HTMLFencedFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLFrameOwnerElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFencedFrameElement::DidNotifySubtreeInsertionsToDocument() {
  CreateDelegateAndNavigate();
}

void HTMLFencedFrameElement::RemovedFrom(ContainerNode& node) {
  // Verify that the underlying frame has already been disconnected via
  // `DisconnectContentFrame()`. This is only relevant for the MPArch
  // implementation.
  DCHECK_EQ(ContentFrame(), nullptr);
  HTMLFrameOwnerElement::RemovedFrom(node);
}

void HTMLFencedFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kModeAttr) {
    mojom::blink::FencedFrameMode new_mode =
        GetModeAttributeValue(params.new_value);
    if (new_mode != mode_ && freeze_mode_attribute_) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Changing the `mode` attribute on a fenced frame has no effect after "
          "it has already been frozen due to the first navigation."));
      return;
    }

    mode_ = new_mode;
  } else if (params.name == html_names::kSrcAttr) {
    if (config_) {
      DCHECK(config_->url());
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Changing the `src` attribute on a fenced frame has no effect after "
          "it has already been installed a config with a specified url."));
      return;
    }

    KURL url = GetNonEmptyURLAttribute(html_names::kSrcAttr);
    Navigate(url);
  } else if (params.name == html_names::kAllowAttr) {
    if (allow_ != params.new_value) {
      allow_ = params.new_value;
      if (!params.new_value.empty()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kFeaturePolicyAllowAttribute);
      }
    }
  } else {
    HTMLFrameOwnerElement::ParseAttribute(params);
  }
}

bool HTMLFencedFrameElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr;
}

bool HTMLFencedFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLFencedFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else {
    HTMLFrameOwnerElement::CollectStyleForPresentationAttribute(name, value,
                                                                style);
  }
}

void HTMLFencedFrameElement::Navigate(const KURL& url) {
  TRACE_EVENT0("navigation", "HTMLFencedFrameElement::Navigate");
  if (!isConnected())
    return;

  // Please see HTMLFencedFrameDelegate::Create for a list of conditions which
  // could result in not having a frame delegate at this point, one of which is
  // prerendering. If this function is called while prerendering we won't have a
  // delegate and will bail early, but this should still be correct since,
  // post-activation, CreateDelegateAndNavigate will be run which will navigate
  // to the most current src.
  if (!frame_delegate_)
    return;

  if (url.IsEmpty())
    return;

  if (!GetExecutionContext()->IsSecureContext()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame was not loaded because the page is not in a secure "
        "context."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kInsecureContext);
    return;
  }

  if (mode_ == mojom::blink::FencedFrameMode::kDefault &&
      !IsValidFencedFrameURL(GURL(url))) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame whose mode is " + FencedFrameModeToString(mode_) +
            " must be navigated to an \"https\" URL, an \"http\" localhost URL,"
            " or \"about:blank\"."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kIncompatibleURLDefault);
    return;
  }

  if (mode_ == mojom::blink::FencedFrameMode::kOpaqueAds &&
      !IsValidUrnUuidURL(GURL(url)) && !IsValidFencedFrameURL(GURL(url))) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame whose mode is " + FencedFrameModeToString(mode_) +
            " must be navigated to an opaque \"urn:uuid\" URL,"
            " an \"https\" URL, an \"http\" localhost URL,"
            " or \"about:blank\"."));
    RecordFencedFrameCreationOutcome(
        FencedFrameCreationOutcome::kIncompatibleURLOpaque);
    return;
  }

  UpdateContainerPolicy();

  frame_delegate_->Navigate(url);

  if (!frozen_frame_size_) {
    FreezeFrameSize();
    RecordFencedFrameCreationOutcome(
        mode_ == mojom::blink::FencedFrameMode::kDefault
            ? FencedFrameCreationOutcome::kSuccessDefault
            : FencedFrameCreationOutcome::kSuccessOpaque);
  }
}

void HTMLFencedFrameElement::NavigateToConfig() {
  CHECK(config_);

  // Prioritize navigating to `config_`'s internal URN if it exists. If so, that
  // means it was created by information from the browser process, and the URN
  // is stored in the `FencedFrameURLMapping`. Otherwise, `config_` was
  // constructed from script and has a user-supplied URL that `this` will
  // navigate to instead.
  if (config_->urn_uuid(PassKey())) {
    KURL url = config_->urn_uuid(PassKey()).value();
    CHECK(IsValidUrnUuidURL(GURL(url)));
    Navigate(url);
  } else {
    CHECK(config_->url());
    Navigate(
        config_
            ->GetValueIgnoringVisibility<FencedFrameConfig::Attribute::kURL>());
  }
}

void HTMLFencedFrameElement::CreateDelegateAndNavigate() {
  TRACE_EVENT0("navigation",
               "HTMLFencedFrameElement::CreateDelegateAndNavigate");
  // We may queue up several calls to CreateDelegateAndNavigate while
  // prerendering, but we should only actually create the delegate once. Note,
  // this will also mean that we skip calling Navigate() again, but the result
  // should still be correct since the first Navigate call will use the
  // up-to-date src.
  if (frame_delegate_)
    return;
  if (GetDocument().IsPrerendering()) {
    GetDocument().AddPostPrerenderingActivationStep(
        WTF::BindOnce(&HTMLFencedFrameElement::CreateDelegateAndNavigate,
                      WrapWeakPersistent(this)));
    return;
  }

  // Freeze the `mode` attribute to its current value even if it has never been
  // explicitly set before, so that it cannot change after insertion.
  freeze_mode_attribute_ = true;

  frame_delegate_ = FencedFrameDelegate::Create(this);

  if (config_) {
    NavigateToConfig();
  } else {
    Navigate(GetNonEmptyURLAttribute(html_names::kSrcAttr));
  }
}

void HTMLFencedFrameElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);
  if (frame_delegate_)
    frame_delegate_->AttachLayoutTree();
}

bool HTMLFencedFrameElement::LayoutObjectIsNeeded(
    const DisplayStyle& style) const {
  return !collapsed_by_client_ &&
         HTMLFrameOwnerElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLFencedFrameElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy_layout) {
  return MakeGarbageCollected<LayoutIFrame>(this);
}

bool HTMLFencedFrameElement::SupportsFocus() const {
  return frame_delegate_ && frame_delegate_->SupportsFocus();
}

PhysicalSize HTMLFencedFrameElement::CoerceFrameSize(
    const PhysicalSize& requested_size) {
  // Only top-level opaque-ads fenced frames are restricted to a list of sizes.
  // TODO(crbug.com/1123606): Later, we will change the size restriction design
  // such that the size is a property bound to opaque URLs, rather than the
  // mode. When that happens, much of this function will need to change.
  // Remember to remove the following includes:
  // #include
  // "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_ad_sizes.h"
  // #include "third_party/blink/renderer/core/frame/local_dom_window.h"
  // #include "third_party/blink/renderer/core/frame/screen.h"
  if (GetMode() != mojom::blink::FencedFrameMode::kOpaqueAds ||
      GetDocument().GetFrame()->IsInFencedFrameTree()) {
    return requested_size;
  }

  // If the requested size is degenerate, return the first allowed ad size.
  if (requested_size.width.ToDouble() <
          std::numeric_limits<double>::epsilon() ||
      requested_size.height.ToDouble() <
          std::numeric_limits<double>::epsilon()) {
    return PhysicalSize(kAllowedAdSizes[0]);
  }

  // If the requested size has an exact match on the allow list, allow it.
  static_assert(kAllowedAdSizes.size() > 0UL);
  for (const gfx::Size& allowed_size : kAllowedAdSizes) {
    if (SizeMatchesExactly(requested_size, allowed_size)) {
      RecordOpaqueFencedFrameSizeCoercion(false);
      return requested_size;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1123606): For now, only allow screen-width ads on Android.
  // We will improve this condition in the future, to account for all cases
  // e.g. split screen, desktop mode, WebView.
  Document& document = GetDocument();
  int width_for_scaling = document.domWindow() && document.domWindow()->screen()
                              ? document.domWindow()->screen()->availWidth()
                              : 0;

  // If scaling based on screen width is allowed, check for exact matches
  // with the list of heights and aspect ratios.
  if (width_for_scaling > 0) {
    static_assert(kAllowedAdHeights.size() > 0UL);
    for (const int allowed_height : kAllowedAdHeights) {
      if (SizeMatchesExactly(requested_size,
                             {width_for_scaling, allowed_height})) {
        return requested_size;
      }
    }

    static_assert(kAllowedAdAspectRatios.size() > 0UL);
    for (const gfx::Size& allowed_aspect_ratio : kAllowedAdAspectRatios) {
      if (SizeMatchesExactly(
              requested_size,
              {width_for_scaling,
               (width_for_scaling * allowed_aspect_ratio.height()) /
                   allowed_aspect_ratio.width()})) {
        return requested_size;
      }
    }
  }
#endif

  // If the requested size isn't allowed, we will freeze the inner frame
  // element with the nearest available size (the best fit according to our
  // size loss function).
  GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "A fenced frame in opaque-ads mode attempted to load with an "
      "unsupported size, and was therefore rounded to the nearest supported "
      "size."));
  RecordOpaqueFencedFrameSizeCoercion(true);

  // The best size so far, and its loss. A lower loss represents
  // a better fit, so we will find the size that minimizes it, i.e.
  // the least bad size.
  gfx::Size best_size = kAllowedAdSizes[0];
  double best_size_loss = std::numeric_limits<double>::infinity();

  for (const gfx::Size& allowed_size : kAllowedAdSizes) {
    double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
    if (size_loss < best_size_loss) {
      best_size_loss = size_loss;
      best_size = allowed_size;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  if (width_for_scaling > 0) {
    for (const int allowed_height : kAllowedAdHeights) {
      const gfx::Size allowed_size = {width_for_scaling, allowed_height};
      double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
      if (size_loss < best_size_loss) {
        best_size_loss = size_loss;
        best_size = allowed_size;
      }
    }

    for (const gfx::Size& allowed_aspect_ratio : kAllowedAdAspectRatios) {
      const gfx::Size allowed_size = {
          width_for_scaling,
          (width_for_scaling * allowed_aspect_ratio.height()) /
              allowed_aspect_ratio.width()};
      double size_loss = ComputeSizeLossFunction(requested_size, allowed_size);
      if (size_loss < best_size_loss) {
        best_size_loss = size_loss;
        best_size = allowed_size;
      }
    }
  }
#endif

  return PhysicalSize(best_size);
}

const absl::optional<PhysicalSize> HTMLFencedFrameElement::FrozenFrameSize()
    const {
  if (!frozen_frame_size_)
    return absl::nullopt;
  const float ratio = GetDocument().DevicePixelRatio();
  return PhysicalSize(
      LayoutUnit::FromFloatRound(frozen_frame_size_->width * ratio),
      LayoutUnit::FromFloatRound(frozen_frame_size_->height * ratio));
}

void HTMLFencedFrameElement::FreezeFrameSize() {
  DCHECK(!frozen_frame_size_);

  // When the parser finds `<fencedframe>` with the `src` attribute, the
  // |Navigate| occurs after |LayoutObject| tree is created and its initial
  // layout was done (|NeedsLayout| is cleared,) but the size of the `<iframe>`
  // is still (0, 0). Wait until a lifecycle completes and the resize observer
  // runs.
  if (!content_rect_) {
    should_freeze_frame_size_on_next_layout_ = true;
    return;
  }

  FreezeFrameSize(content_rect_->size);
}

void HTMLFencedFrameElement::FreezeFrameSize(const PhysicalSize& size) {
  DCHECK(!frozen_frame_size_);
  // TODO(crbug.com/1123606): This will change when we move frame size coercion
  // from here to during FLEDGE/SharedStorage.
  frozen_frame_size_ = CoerceFrameSize(size);

  frame_delegate_->FreezeFrameSize();
}

void HTMLFencedFrameElement::StartResizeObserver() {
  DCHECK(!resize_observer_);
  resize_observer_ =
      ResizeObserver::Create(GetDocument().domWindow(),
                             MakeGarbageCollected<ResizeObserverDelegate>());
  resize_observer_->observe(this);
}

void HTMLFencedFrameElement::StopResizeObserver() {
  if (!resize_observer_)
    return;
  resize_observer_->disconnect();
  resize_observer_ = nullptr;
}

void HTMLFencedFrameElement::ResizeObserverDelegate::OnResize(
    const HeapVector<Member<ResizeObserverEntry>>& entries) {
  if (entries.empty())
    return;
  const Member<ResizeObserverEntry>& entry = entries.back();
  auto* element = To<HTMLFencedFrameElement>(entry->target());
  const DOMRectReadOnly* content_rect = entry->contentRect();
  element->OnResize(ToPhysicalRect(*content_rect));
}

void HTMLFencedFrameElement::OnResize(const PhysicalRect& content_rect) {
  // If we don't have a delegate, then we won't have a frame, so no reason to
  // freeze.
  if (!frame_delegate_)
    return;
  if (frozen_frame_size_.has_value() && !size_set_after_freeze_) {
    // Only log this once per fenced frame.
    RecordFencedFrameResizedAfterSizeFrozen();
    size_set_after_freeze_ = true;
  }
  content_rect_ = content_rect;
  // If the size information at |FreezeFrameSize| is not complete and we
  // needed to postpone freezing until the next resize, do it now. See
  // |FreezeFrameSize| for more.
  if (should_freeze_frame_size_on_next_layout_) {
    should_freeze_frame_size_on_next_layout_ = false;
    DCHECK(!frozen_frame_size_);
    FreezeFrameSize(content_rect_->size);
    return;
  }
}

// TODO(domfarolino): Remove this.
void HTMLFencedFrameElement::UpdateInnerStyleOnFrozenInternalFrame() {
  DCHECK(content_rect_);
  const absl::optional<PhysicalSize> frozen_size = frozen_frame_size_;
  DCHECK(frozen_size);
  const double child_width = frozen_size->width.ToDouble();
  const double child_height = frozen_size->height.ToDouble();
  // TODO(kojii): Theoretically this `transform` is the same as `object-fit:
  // contain`, but `<iframe>` does not support the `object-fit` property today.
  // We can change to use the `object-fit` property and stop the resize-observer
  // once it is supported.
  String css;
  if (child_width <= std::numeric_limits<double>::epsilon() ||
      child_height <= std::numeric_limits<double>::epsilon()) {
    // If the child's width or height is zero, the scale will be infinite. Do
    // not scale in such cases.
    css =
        String::Format("width: %fpx; height: %fpx", child_width, child_height);
  } else {
    const double parent_width = content_rect_->Width().ToDouble();
    const double parent_height = content_rect_->Height().ToDouble();
    const double scale_x = parent_width / child_width;
    const double scale_y = parent_height / child_height;
    const double scale = std::min(scale_x, scale_y);
    const double tx = (parent_width - child_width * scale) / 2;
    const double ty = (parent_height - child_height * scale) / 2;
    css = String::Format(
        "width: %fpx; height: %fpx; transform: translate(%fpx, %fpx) scale(%f)",
        child_width, child_height, tx, ty, scale);
  }
  InnerIFrameElement()->setAttribute(html_names::kStyleAttr, css,
                                     ASSERT_NO_EXCEPTION);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_
#define COMPONENTS_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/web_view_test_client.h"
#include "components/test_runner/web_widget_test_client.h"
#include "components/test_runner/web_widget_test_proxy.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"

namespace blink {
class WebDragData;
class WebFileChooserCompletion;
class WebImage;
class WebLocalFrame;
class WebString;
class WebView;
class WebWidget;
struct WebFileChooserParams;
struct WebPoint;
struct WebWindowFeatures;
}

namespace test_runner {

class AccessibilityController;
class TestInterfaces;
class TestRunnerForSpecificView;
class TextInputController;
class WebTestDelegate;
class WebTestInterfaces;

// WebViewTestProxyBase is the "brain" of WebViewTestProxy in the sense that
// WebViewTestProxy does the bridge between RenderViewImpl and
// WebViewTestProxyBase and when it requires a behavior to be different from the
// usual, it will call WebViewTestProxyBase that implements the expected
// behavior. See WebViewTestProxy class comments for more information.
class TEST_RUNNER_EXPORT WebViewTestProxyBase : public WebWidgetTestProxyBase {
 public:
  blink::WebView* web_view() { return web_view_; }
  void set_web_view(blink::WebView* view) {
    DCHECK(view);
    DCHECK(!web_view_);
    web_view_ = view;
  }

  void set_view_test_client(
      std::unique_ptr<WebViewTestClient> view_test_client) {
    DCHECK(view_test_client);
    DCHECK(!view_test_client_);
    view_test_client_ = std::move(view_test_client);
  }

  WebTestDelegate* delegate() { return delegate_; }
  void set_delegate(WebTestDelegate* delegate) {
    DCHECK(delegate);
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  TestInterfaces* test_interfaces() { return test_interfaces_; }
  void SetInterfaces(WebTestInterfaces* web_test_interfaces);

  AccessibilityController* accessibility_controller() {
    return accessibility_controller_.get();
  }

  TestRunnerForSpecificView* view_test_runner() {
    return view_test_runner_.get();
  }

  void Reset();
  void BindTo(blink::WebLocalFrame* frame);

  void GetScreenOrientationForTesting(blink::WebScreenInfo&);

 protected:
  WebViewTestProxyBase();
  ~WebViewTestProxyBase();

  blink::WebViewClient* view_test_client() { return view_test_client_.get(); }

 private:
  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* web_view_;
  blink::WebWidget* web_widget_;
  std::unique_ptr<WebViewTestClient> view_test_client_;
  std::unique_ptr<AccessibilityController> accessibility_controller_;
  std::unique_ptr<TextInputController> text_input_controller_;
  std::unique_ptr<TestRunnerForSpecificView> view_test_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxyBase);
};

// WebViewTestProxy is used during LayoutTests and always instantiated, at time
// of writing with Base=RenderViewImpl. It does not directly inherit from it for
// layering purposes.
// The intent of that class is to wrap RenderViewImpl for tests purposes in
// order to reduce the amount of test specific code in the production code.
// WebViewTestProxy is only doing the glue between RenderViewImpl and
// WebViewTestProxyBase, that means that there is no logic living in this class
// except deciding which base class should be called (could be both).
//
// Examples of usage:
//  * when a fooClient has a mock implementation, WebViewTestProxy can override
//    the fooClient() call and have WebViewTestProxyBase return the mock
//    implementation.
//  * when a value needs to be overridden by LayoutTests, WebViewTestProxy can
//    override RenderViewImpl's getter and call a getter from
//    WebViewTestProxyBase instead. In addition, WebViewTestProxyBase will have
//    a public setter that could be called from the TestRunner.
#if defined(OS_WIN)
// WebViewTestProxy is a diamond-shaped hierarchy, with WebWidgetClient at the
// root. VS warns when we inherit the WebWidgetClient method implementations
// from RenderWidget. It's safe to ignore that warning.
#pragma warning(disable : 4250)
#endif
template <class Base, typename... Args>
class WebViewTestProxy : public Base, public WebViewTestProxyBase {
 public:
  explicit WebViewTestProxy(Args... args) : Base(args...) {}

  // WebWidgetClient implementation.
  blink::WebScreenInfo screenInfo() override {
    blink::WebScreenInfo info = Base::screenInfo();
    blink::WebScreenInfo test_info = widget_test_client()->screenInfo();
    if (test_info.orientationType != blink::WebScreenOrientationUndefined) {
      info.orientationType = test_info.orientationType;
      info.orientationAngle = test_info.orientationAngle;
    }
    return info;
  }
  void scheduleAnimation() override {
    widget_test_client()->scheduleAnimation();
  }
  bool requestPointerLock() override {
    return widget_test_client()->requestPointerLock();
  }
  void requestPointerUnlock() override {
    widget_test_client()->requestPointerUnlock();
  }
  bool isPointerLocked() override {
    return widget_test_client()->isPointerLocked();
  }
  void didFocus() override {
    view_test_client()->didFocus();
    Base::didFocus();
  }
  void setToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override {
    widget_test_client()->setToolTipText(text, hint);
    Base::setToolTipText(text, hint);
  }

  // WebViewClient implementation.
  void startDragging(blink::WebReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& point) override {
    widget_test_client()->startDragging(policy, data, mask, image, point);
    // Don't forward this call to Base because we don't want to do a real
    // drag-and-drop.
  }
  void didChangeContents() override {
    view_test_client()->didChangeContents();
    Base::didChangeContents();
  }
  blink::WebView* createView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             bool suppress_opener) override {
    if (!view_test_client()->createView(creator, request, features, frame_name,
                                        policy, suppress_opener))
      return nullptr;
    return Base::createView(creator, request, features, frame_name, policy,
                            suppress_opener, nullptr);
  }
  void setStatusText(const blink::WebString& text) override {
    view_test_client()->setStatusText(text);
    Base::setStatusText(text);
  }
  void printPage(blink::WebLocalFrame* frame) override {
    view_test_client()->printPage(frame);
  }
  blink::WebSpeechRecognizer* speechRecognizer() override {
    return view_test_client()->speechRecognizer();
  }
  void showValidationMessage(
      const blink::WebRect& anchor_in_root_view,
      const blink::WebString& main_message,
      blink::WebTextDirection main_message_hint,
      const blink::WebString& sub_message,
      blink::WebTextDirection sub_message_hint) override {
    view_test_client()->showValidationMessage(anchor_in_root_view, main_message,
                                              main_message_hint, sub_message,
                                              sub_message_hint);
  }
  blink::WebString acceptLanguages() override {
    return view_test_client()->acceptLanguages();
  }

 private:
  virtual ~WebViewTestProxy() {}

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxy);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_

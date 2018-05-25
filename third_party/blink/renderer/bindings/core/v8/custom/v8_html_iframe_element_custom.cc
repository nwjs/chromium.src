#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_iframe_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

using namespace HTMLNames;

void V8HTMLIFrameElement::nwUserAgentAttributeSetterCustom(v8::Local<v8::Value> value, const v8::FunctionCallbackInfo<v8::Value>& info)
{
  HTMLIFrameElement* frame = V8HTMLIFrameElement::ToImpl(info.Holder());
  // String agentValue = toCoreStringWithNullCheck(value);
  TOSTRING_VOID(V8StringResource<>, agentValue, value);

  frame->setAttribute(HTMLNames::nwuseragentAttr, agentValue);

  if (frame->ContentFrame()->IsLocalFrame()) {
    LocalFrame* lframe = ToLocalFrame(frame->ContentFrame());
    lframe->Loader().setUserAgentOverride(agentValue);
  }
}

} // namespace blink

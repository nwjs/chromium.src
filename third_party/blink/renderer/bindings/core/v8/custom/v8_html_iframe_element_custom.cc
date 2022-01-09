#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_iframe_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"

#include "third_party/blink/renderer/core/html/html_iframe_element.h"

namespace blink {

using namespace html_names;

void V8HTMLIFrameElement::NwUserAgentAttributeSetterCustom(v8::Local<v8::Value> value, const v8::FunctionCallbackInfo<v8::Value>& info)
{
  HTMLIFrameElement* frame = V8HTMLIFrameElement::ToImpl(info.Holder());
  // String agentValue = toCoreStringWithNullCheck(value);
  TOSTRING_VOID(V8StringResource<>, agentValue, value);

  frame->setAttribute(html_names::kNwuseragentAttr, agentValue);

  if (frame->ContentFrame()->IsLocalFrame()) {
    LocalFrame* lframe = DynamicTo<LocalFrame>(frame->ContentFrame());
    lframe->Loader().setUserAgentOverride(agentValue);
  }
}

} // namespace blink

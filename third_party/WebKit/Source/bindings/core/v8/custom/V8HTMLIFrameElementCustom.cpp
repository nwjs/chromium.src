#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HTMLIFrameElement.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {

using namespace HTMLNames;

void V8HTMLIFrameElement::nwUserAgentAttributeSetterCustom(v8::Local<v8::Value> value, const v8::FunctionCallbackInfo<v8::Value>& info)
{
  HTMLIFrameElement* frame = V8HTMLIFrameElement::toImpl(info.Holder());
  // String agentValue = toCoreStringWithNullCheck(value);
  TOSTRING_VOID(V8StringResource<>, agentValue, value);

  frame->setAttribute(HTMLNames::nwuseragentAttr, agentValue);

  if (frame->contentFrame()->isLocalFrame()) {
    LocalFrame* lframe = toLocalFrame(frame->contentFrame());
    lframe->loader().setUserAgentOverride(agentValue);
  }
}

} // namespace blink

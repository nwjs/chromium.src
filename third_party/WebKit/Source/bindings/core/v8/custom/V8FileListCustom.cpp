#include "bindings/core/v8/V8FileList.h"
#include "bindings/core/v8/V8File.h"
#include "platform/bindings/V8Binding.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {
void V8FileList::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  ExecutionContext* context = CurrentExecutionContext(args.GetIsolate());
  if (context && context->IsDocument()) {
    Document* document = ToDocument(context);
    if (document->GetFrame()->isNwDisabledChildFrame()) {
      V8ThrowException::ThrowTypeError(args.GetIsolate(), "FileList constructor cannot be called in nwdisabled frame.");
      return;
    }
  }

  FileList* impl = FileList::Create();
  V8SetReturnValue(args, impl);
}

} // namespace blink

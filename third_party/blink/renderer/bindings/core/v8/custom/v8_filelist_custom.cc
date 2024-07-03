#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

//#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_return_value_for_core.h"

namespace blink {
void V8FileList::ConstructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  v8::Isolate* isolate = args.GetIsolate();
  ExecutionContext* context = CurrentExecutionContext(isolate);
  if (context && context->IsWindow()) {
    Document* document = To<LocalDOMWindow>(context)->document();
    if (document->GetFrame()->isNwDisabledChildFrame()) {
      V8ThrowException::ThrowTypeError(isolate, "FileList constructor cannot be called in nwdisabled frame.");
      return;
    }
  }
  v8::Local<v8::Object> v8_receiver = args.This();
  FileList* impl = MakeGarbageCollected<FileList>();
  v8::Local<v8::Object> v8_wrapper = impl->AssociateWithWrapper(isolate, V8FileList::GetWrapperTypeInfo(), v8_receiver);
  bindings::V8SetReturnValue(args, v8_wrapper);
}

} // namespace blink

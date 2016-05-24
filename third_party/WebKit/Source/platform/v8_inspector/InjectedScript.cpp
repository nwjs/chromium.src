/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "platform/v8_inspector/InjectedScript.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InjectedScriptNative.h"
#include "platform/v8_inspector/InjectedScriptSource.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8FunctionCall.h"
#include "platform/v8_inspector/V8InjectedScriptHost.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8ToProtocolValue.h"

using blink::protocol::Array;
using blink::protocol::Debugger::CallFrame;
using blink::protocol::Debugger::CollectionEntry;
using blink::protocol::Debugger::FunctionDetails;
using blink::protocol::Debugger::GeneratorObjectDetails;
using blink::protocol::Runtime::PropertyDescriptor;
using blink::protocol::Runtime::InternalPropertyDescriptor;
using blink::protocol::Runtime::RemoteObject;
using blink::protocol::Maybe;

namespace blink {

static bool hasInternalError(ErrorString* errorString, bool hasError)
{
    if (hasError)
        *errorString = "Internal error";
    return hasError;
}

PassOwnPtr<InjectedScript> InjectedScript::create(InspectedContext* inspectedContext, InjectedScriptHost* injectedScriptHost)
{
    v8::Isolate* isolate = inspectedContext->isolate();
    v8::HandleScope handles(isolate);
    v8::Local<v8::Context> context = inspectedContext->context();
    v8::Context::Scope scope(context);

    OwnPtr<InjectedScriptNative> injectedScriptNative = adoptPtr(new InjectedScriptNative(isolate));
    String16 injectedScriptSource(reinterpret_cast<const char*>(InjectedScriptSource_js), sizeof(InjectedScriptSource_js));

    v8::Local<v8::FunctionTemplate> wrapperTemplate = injectedScriptHost->wrapperTemplate(isolate);
    if (wrapperTemplate.IsEmpty()) {
        wrapperTemplate = V8InjectedScriptHost::createWrapperTemplate(isolate);
        injectedScriptHost->setWrapperTemplate(wrapperTemplate, isolate);
    }

    v8::Local<v8::Object> scriptHostWrapper = V8InjectedScriptHost::wrap(wrapperTemplate, context, injectedScriptHost);
    if (scriptHostWrapper.IsEmpty())
        return nullptr;

    injectedScriptNative->setOnInjectedScriptHost(scriptHostWrapper);

    // Inject javascript into the context. The compiled script is supposed to evaluate into
    // a single anonymous function(it's anonymous to avoid cluttering the global object with
    // inspector's stuff) the function is called a few lines below with InjectedScriptHost wrapper,
    // injected script id and explicit reference to the inspected global object. The function is expected
    // to create and configure InjectedScript instance that is going to be used by the inspector.
    v8::Local<v8::Value> value;
    if (!inspectedContext->debugger()->compileAndRunInternalScript(context, toV8String(isolate, injectedScriptSource)).ToLocal(&value))
        return nullptr;
    ASSERT(value->IsFunction());
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
    v8::Local<v8::Object> windowGlobal = context->Global();
    v8::Local<v8::Value> info[] = { scriptHostWrapper, windowGlobal, v8::Number::New(isolate, inspectedContext->contextId()) };
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> injectedScriptValue;
    if (!function->Call(context, windowGlobal, WTF_ARRAY_LENGTH(info), info).ToLocal(&injectedScriptValue))
        return nullptr;
    if (!injectedScriptValue->IsObject())
        return nullptr;
    return adoptPtr(new InjectedScript(inspectedContext, injectedScriptValue.As<v8::Object>(), injectedScriptNative.release()));
}

InjectedScript::InjectedScript(InspectedContext* context, v8::Local<v8::Object> object, PassOwnPtr<InjectedScriptNative> injectedScriptNative)
    : m_context(context)
    , m_value(context->isolate(), object)
    , m_native(injectedScriptNative)
{
}

InjectedScript::~InjectedScript()
{
}

v8::Isolate* InjectedScript::isolate() const
{
    return m_context->isolate();
}

void InjectedScript::getProperties(ErrorString* errorString, v8::Local<v8::Object> object, const String16& groupName, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, OwnPtr<Array<PropertyDescriptor>>* properties, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(isolate());
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "getProperties");
    function.appendArgument(object);
    function.appendArgument(groupName);
    function.appendArgument(ownProperties);
    function.appendArgument(accessorPropertiesOnly);
    function.appendArgument(generatePreview);

    OwnPtr<protocol::Value> result = makeCallWithExceptionDetails(function, exceptionDetails);
    if (exceptionDetails->isJust()) {
        // FIXME: make properties optional
        *properties = Array<PropertyDescriptor>::create();
        return;
    }
    protocol::ErrorSupport errors(errorString);
    *properties = Array<PropertyDescriptor>::parse(result.get(), &errors);
}

void InjectedScript::releaseObject(const String16& objectId)
{
    OwnPtr<protocol::Value> parsedObjectId = protocol::parseJSON(objectId);
    if (!parsedObjectId)
        return;
    protocol::DictionaryValue* object = protocol::DictionaryValue::cast(parsedObjectId.get());
    if (!object)
        return;
    int boundId = 0;
    if (!object->getNumber("id", &boundId))
        return;
    m_native->unbind(boundId);
}

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::wrapObject(ErrorString* errorString, v8::Local<v8::Value> value, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    v8::HandleScope handles(isolate());
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "wrapObject");
    v8::Local<v8::Value> wrappedObject;
    if (!wrapValue(errorString, value, groupName, forceValueType, generatePreview).ToLocal(&wrappedObject))
        return nullptr;
    protocol::ErrorSupport errors;
    OwnPtr<protocol::Runtime::RemoteObject> remoteObject = protocol::Runtime::RemoteObject::parse(toProtocolValue(m_context->context(), wrappedObject).get(), &errors);
    if (!remoteObject)
        *errorString = "Object has too long reference chain";
    return remoteObject.release();
}

bool InjectedScript::wrapObjectProperty(ErrorString* errorString, v8::Local<v8::Object> object, v8::Local<v8::Value> key, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    v8::Local<v8::Value> property;
    if (hasInternalError(errorString, !object->Get(m_context->context(), key).ToLocal(&property)))
        return false;
    v8::Local<v8::Value> wrappedProperty;
    if (!wrapValue(errorString, property, groupName, forceValueType, generatePreview).ToLocal(&wrappedProperty))
        return false;
    v8::Maybe<bool> success = object->Set(m_context->context(), key, wrappedProperty);
    if (hasInternalError(errorString, success.IsNothing() || !success.FromJust()))
        return false;
    return true;
}

bool InjectedScript::wrapPropertyInArray(ErrorString* errorString, v8::Local<v8::Array> array, v8::Local<v8::String> property, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "wrapPropertyInArray");
    function.appendArgument(array);
    function.appendArgument(property);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(forceValueType);
    function.appendArgument(generatePreview);
    bool hadException = false;
    callFunctionWithEvalEnabled(function, hadException);
    return !hasInternalError(errorString, hadException);
}

bool InjectedScript::wrapObjectsInArray(ErrorString* errorString, v8::Local<v8::Array> array, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "wrapObjectsInArray");
    function.appendArgument(array);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(forceValueType);
    function.appendArgument(generatePreview);
    bool hadException = false;
    callFunctionWithEvalEnabled(function, hadException);
    return !hasInternalError(errorString, hadException);
}

v8::MaybeLocal<v8::Value> InjectedScript::wrapValue(ErrorString* errorString, v8::Local<v8::Value> value, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "wrapObject");
    function.appendArgument(value);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(forceValueType);
    function.appendArgument(generatePreview);
    bool hadException = false;
    v8::Local<v8::Value> r = callFunctionWithEvalEnabled(function, hadException);
    if (hasInternalError(errorString, hadException || r.IsEmpty()))
        return v8::MaybeLocal<v8::Value>();
    return r;
}

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const
{
    v8::HandleScope handles(isolate());
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "wrapTable");
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(table);
    if (columns.IsEmpty())
        function.appendArgument(false);
    else
        function.appendArgument(columns);
    bool hadException = false;
    v8::Local<v8::Value>  r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException)
        return nullptr;
    protocol::ErrorSupport errors;
    return protocol::Runtime::RemoteObject::parse(toProtocolValue(m_context->context(), r).get(), &errors);
}

bool InjectedScript::findObject(ErrorString* errorString, const RemoteObjectId& objectId, v8::Local<v8::Value>* outObject) const
{
    *outObject = m_native->objectForId(objectId.id());
    if (outObject->IsEmpty())
        *errorString = "Could not find object with given id";
    return !outObject->IsEmpty();
}

String16 InjectedScript::objectGroupName(const RemoteObjectId& objectId) const
{
    return m_native->groupName(objectId.id());
}

void InjectedScript::releaseObjectGroup(const String16& objectGroup)
{
    v8::HandleScope handles(isolate());
    m_native->releaseObjectGroup(objectGroup);
    if (objectGroup == "console") {
        V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "clearLastEvaluationResult");
        bool hadException = false;
        callFunctionWithEvalEnabled(function, hadException);
        ASSERT(!hadException);
    }
}

void InjectedScript::setCustomObjectFormatterEnabled(bool enabled)
{
    v8::HandleScope handles(isolate());
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "setCustomObjectFormatterEnabled");
    function.appendArgument(enabled);
    makeCall(function);
}

bool InjectedScript::canAccessInspectedWindow() const
{
    v8::Local<v8::Context> callingContext = isolate()->GetCallingContext();
    if (callingContext.IsEmpty())
        return true;
    return m_context->debugger()->client()->callingContextCanAccessContext(callingContext, m_context->context());
}

v8::Local<v8::Value> InjectedScript::v8Value() const
{
    return m_value.Get(isolate());
}

v8::Local<v8::Value> InjectedScript::callFunctionWithEvalEnabled(V8FunctionCall& function, bool& hadException) const
{
    v8::Local<v8::Context> localContext = m_context->context();
    v8::Context::Scope scope(localContext);
    bool evalIsDisabled = !localContext->IsCodeGenerationFromStringsAllowed();
    // Temporarily enable allow evals for inspector.
    if (evalIsDisabled)
        localContext->AllowCodeGenerationFromStrings(true);
    v8::Local<v8::Value> resultValue = function.call(hadException);
    if (evalIsDisabled)
        localContext->AllowCodeGenerationFromStrings(false);
    return resultValue;
}

PassOwnPtr<protocol::Value> InjectedScript::makeCall(V8FunctionCall& function)
{
    OwnPtr<protocol::Value> result;
    if (!canAccessInspectedWindow()) {
        result = protocol::StringValue::create("Can not access given context.");
        return nullptr;
    }

    bool hadException = false;
    v8::Local<v8::Value> resultValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (!hadException) {
        result = toProtocolValue(function.context(), resultValue);
        if (!result)
            result = protocol::StringValue::create("Object has too long reference chain");
    } else {
        result = protocol::StringValue::create("Exception while making a call.");
    }
    return result.release();
}

PassOwnPtr<protocol::Value> InjectedScript::makeCallWithExceptionDetails(V8FunctionCall& function, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    OwnPtr<protocol::Value> result;
    v8::HandleScope handles(isolate());
    v8::Context::Scope scope(m_context->context());
    v8::TryCatch tryCatch(isolate());
    v8::Local<v8::Value> resultValue = function.callWithoutExceptionHandling();
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        String16 text = !message.IsEmpty() ? toProtocolStringWithTypeCheck(message->Get()) : "Internal error";
        *exceptionDetails = protocol::Runtime::ExceptionDetails::create().setText(text).build();
    } else {
        result = toProtocolValue(function.context(), resultValue);
        if (!result)
            result = protocol::StringValue::create("Object has too long reference chain");
    }
    return result.release();
}

bool InjectedScript::setLastEvaluationResult(ErrorString* errorString, v8::Local<v8::Value> value)
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "setLastEvaluationResult");
    function.appendArgument(value);
    bool hadException = false;
    function.call(hadException, false);
    return !hasInternalError(errorString, hadException);
}

v8::MaybeLocal<v8::Value> InjectedScript::resolveCallArgument(ErrorString* errorString, protocol::Runtime::CallArgument* callArgument)
{
    if (callArgument->hasObjectId()) {
        OwnPtr<RemoteObjectId> remoteObjectId = RemoteObjectId::parse(errorString, callArgument->getObjectId(""));
        if (!remoteObjectId)
            return v8::MaybeLocal<v8::Value>();
        if (remoteObjectId->contextId() != m_context->contextId()) {
            *errorString = "Argument should belong to the same JavaScript world as target object";
            return v8::MaybeLocal<v8::Value>();
        }
        v8::Local<v8::Value> object;
        if (!findObject(errorString, *remoteObjectId, &object))
            return v8::MaybeLocal<v8::Value>();
        return object;
    }
    if (callArgument->hasValue()) {
        String16 value = callArgument->getValue(nullptr)->toJSONString();
        if (callArgument->getType(String16()) == "number")
            value = "Number(" + value + ")";
        v8::Local<v8::Value> object;
        if (!m_context->debugger()->compileAndRunInternalScript(m_context->context(), toV8String(isolate(), value)).ToLocal(&object)) {
            *errorString = "Couldn't parse value object in call argument";
            return v8::MaybeLocal<v8::Value>();
        }
        return object;
    }
    return v8::Undefined(isolate());
}

v8::MaybeLocal<v8::Object> InjectedScript::commandLineAPI(ErrorString* errorString) const
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "commandLineAPI");
    return callFunctionReturnObject(errorString, function);
}

v8::MaybeLocal<v8::Object> InjectedScript::remoteObjectAPI(ErrorString* errorString, const String16& groupName) const
{
    V8FunctionCall function(m_context->debugger(), m_context->context(), v8Value(), "remoteObjectAPI");
    function.appendArgument(groupName);
    return callFunctionReturnObject(errorString, function);
}

v8::MaybeLocal<v8::Object> InjectedScript::callFunctionReturnObject(ErrorString* errorString, V8FunctionCall& function) const
{
    bool hadException = false;
    v8::Local<v8::Value> result = function.call(hadException, false);
    v8::Local<v8::Object> resultObject;
    if (hasInternalError(errorString, hadException || result.IsEmpty() || !result->ToObject(m_context->context()).ToLocal(&resultObject)))
        return v8::MaybeLocal<v8::Object>();
    return resultObject;
}

PassOwnPtr<protocol::Runtime::ExceptionDetails> InjectedScript::createExceptionDetails(v8::Local<v8::Message> message)
{
    OwnPtr<protocol::Runtime::ExceptionDetails> exceptionDetailsObject = protocol::Runtime::ExceptionDetails::create().setText(toProtocolString(message->Get())).build();
    if (message.IsEmpty())
        return exceptionDetailsObject.release();
    exceptionDetailsObject->setUrl(toProtocolStringWithTypeCheck(message->GetScriptResourceName()));
    exceptionDetailsObject->setScriptId(String16::number(message->GetScriptOrigin().ScriptID()->Value()));

    v8::Maybe<int> lineNumber = message->GetLineNumber(m_context->context());
    if (lineNumber.IsJust())
        exceptionDetailsObject->setLine(lineNumber.FromJust());
    v8::Maybe<int> columnNumber = message->GetStartColumn(m_context->context());
    if (columnNumber.IsJust())
        exceptionDetailsObject->setColumn(columnNumber.FromJust());

    v8::Local<v8::StackTrace> stackTrace = message->GetStackTrace();
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
        exceptionDetailsObject->setStack(m_context->debugger()->createStackTrace(stackTrace, stackTrace->GetFrameCount())->buildInspectorObject());
    return exceptionDetailsObject.release();
}

void InjectedScript::wrapEvaluateResult(ErrorString* errorString, v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch& tryCatch, const String16& objectGroup, bool returnByValue, bool generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::Local<v8::Value> resultValue;
    if (!tryCatch.HasCaught()) {
        if (hasInternalError(errorString, !maybeResultValue.ToLocal(&resultValue)))
            return;
        OwnPtr<RemoteObject> remoteObject = wrapObject(errorString, resultValue, objectGroup, returnByValue, generatePreview);
        if (!remoteObject)
            return;
        if (objectGroup == "console" && !setLastEvaluationResult(errorString, resultValue))
            return;
        *result = remoteObject.release();
        if (wasThrown)
            *wasThrown = false;
    } else {
        v8::Local<v8::Value> exception = tryCatch.Exception();
        OwnPtr<RemoteObject> remoteObject = wrapObject(errorString, exception, objectGroup, false, generatePreview && !exception->IsNativeError());
        if (!remoteObject)
            return;
        *result = remoteObject.release();
        if (exceptionDetails)
            *exceptionDetails = createExceptionDetails(tryCatch.Message());
        if (wasThrown)
            *wasThrown = true;
    }
}

InjectedScript::ScopedGlobalObjectExtension::ScopedGlobalObjectExtension(InjectedScript* current, v8::MaybeLocal<v8::Object> extension)
    : m_context(current->context()->context())
{
    v8::Local<v8::Object> extensionObject;
    if (!extension.ToLocal(&extensionObject))
        return;

    m_symbol = V8Debugger::scopeExtensionSymbol(current->isolate());
    v8::Local<v8::Object> global = m_context->Global();
    if (global->Set(m_context, m_symbol, extensionObject).FromMaybe(false))
        m_global = global;
}

InjectedScript::ScopedGlobalObjectExtension::~ScopedGlobalObjectExtension()
{
    v8::Local<v8::Object> global;
    if (!m_global.ToLocal(&global))
        return;
    global->Delete(m_context, m_symbol);
}

} // namespace blink

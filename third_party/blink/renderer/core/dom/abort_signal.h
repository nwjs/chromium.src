// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_type.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class AbortSignalCompositionManager;
class ExceptionState;
class ExecutionContext;
class ScriptState;

// Implementation of https://dom.spec.whatwg.org/#interface-AbortSignal
class CORE_EXPORT AbortSignal : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class SignalType {
    // Associated with an AbortController.
    kController,
    // Created by AbortSignal.abort().
    kAborted,
    // Created by AbortSignal.timeout().
    kTimeout,
    // Created by AbortSignal.any().
    kComposite,
    // An internal signal which either is directly aborted or uses the internal
    // `Follow` algorithm.
    //
    // TODO(crbug.com/1323391): Specs that use the internal `Follow` algorithm
    // should be modified to create follow-immutable composite signals.
    kInternal,
  };

  // The base class for "abort algorithm" defined at
  // https://dom.spec.whatwg.org/#abortsignal-abort-algorithms. This is
  // semantically equivalent to base::OnceClosure but is GarbageCollected.
  class Algorithm : public GarbageCollected<Algorithm> {
   public:
    virtual ~Algorithm() = default;

    // Called when the associated signal is aborted. This is called at most
    // once.
    virtual void Run() = 0;

    virtual void Trace(Visitor* visitor) const {}
  };

  // A garbage collected handle representing an abort algorithm. Abort
  // algorithms are no longer runnable after the handle is GCed. Algorithms can
  // be explcitly removed by passing the handle to `RemoveAlgorithm()`.
  class CORE_EXPORT AlgorithmHandle : public GarbageCollected<AlgorithmHandle> {
   public:
    explicit AlgorithmHandle(Algorithm*);
    ~AlgorithmHandle();

    Algorithm* GetAlgorithm() { return algorithm_; }

    void Trace(Visitor* visitor) const;

   private:
    Member<Algorithm> algorithm_;
  };

  // The abort algorithm collection functionality is factored out into this
  // interface so we can have a kill switch for the algorithm handle paths. With
  // the remove feature enabled, handles are stored weakly and algorithms can
  // no longer run once the handle is GCed. With the feature disabled, the
  // algorithms are held with strong references to match the previous behavior.
  //
  // TODO(crbug.com/1296280): Remove along with kAbortSignalHandleBasedRemoval.
  class AbortAlgorithmCollection
      : public GarbageCollected<AbortAlgorithmCollection> {
   public:
    virtual void AddAlgorithm(AlgorithmHandle*) = 0;
    virtual void RemoveAlgorithm(AlgorithmHandle*) = 0;
    virtual void Clear() = 0;
    virtual void Run() = 0;
    virtual void Trace(Visitor*) const {}
  };

  // Constructs a SignalType::kInternal signal. This is only for non web-exposed
  // signals.
  explicit AbortSignal(ExecutionContext*);

  // Constructs a new signal with the given `SignalType`.
  AbortSignal(ExecutionContext*, SignalType);

  // Constructs a composite signal. The signal will be aborted if any of
  // `source_signals` are aborted or become aborted.
  AbortSignal(ScriptState*, HeapVector<Member<AbortSignal>>& source_signals);

  ~AbortSignal() override;

  // abort_signal.idl
  static AbortSignal* abort(ScriptState*);
  static AbortSignal* abort(ScriptState*, ScriptValue reason);
  static AbortSignal* any(ScriptState*,
                          HeapVector<Member<AbortSignal>> signals);
  static AbortSignal* timeout(ScriptState*, uint64_t milliseconds);
  ScriptValue reason(ScriptState*) const;
  bool aborted() const { return !abort_reason_.IsEmpty(); }
  void throwIfAborted(ScriptState*, ExceptionState&) const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Internal API

  // The "add an algorithm" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-add for dependent features to call
  // to be notified when abort has been signalled.
  [[nodiscard]] AlgorithmHandle* AddAlgorithm(Algorithm* algorithm);

  // Same with above but with a base::OnceClosure. Use this only when you're
  // sure the objects attached to the callback don't form a reference cycle.
  [[nodiscard]] AlgorithmHandle* AddAlgorithm(base::OnceClosure algorithm);

  // The "remove an algorithm" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-remove.
  //
  // Removes the algorithm associated with the handle. Algorithms are no longer
  // runnable when their handles are GCed, but this can invoked directly if
  // needed, e.g. to not rely on GC timing.
  void RemoveAlgorithm(AlgorithmHandle*);

  // The "To signal abort" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-add. Run all algorithms that were
  // added by AddAlgorithm(), in order of addition, then fire an "abort"
  // event. Does nothing if called more than once.
  void SignalAbort(ScriptState*);
  void SignalAbort(ScriptState*, ScriptValue reason);

  // The "follow" algorithm from the standard:
  // https://dom.spec.whatwg.org/#abortsignal-follow
  // |this| is the followingSignal described in the standard.
  void Follow(ScriptState*, AbortSignal* parent);

  virtual bool IsTaskSignal() const { return false; }

  void Trace(Visitor*) const override;

  SignalType GetSignalType() const { return signal_type_; }

  bool IsCompositeSignal() const {
    return signal_type_ == AbortSignal::SignalType::kComposite;
  }

  // Returns the composition manager for this signal for the given type.
  // Subclasses are expected to override this to return the composition manager
  // associated with their type.
  virtual AbortSignalCompositionManager* GetCompositionManager(
      AbortSignalCompositionType);

 private:
  // Common constructor initialization separated out to make mutually exclusive
  // constructors more readable.
  void InitializeCommon(ExecutionContext*, SignalType);

  void AbortTimeoutFired(ScriptState*);

  // This ensures abort is propagated to any "following" signals.
  //
  // TODO(crbug.com/1323391): Remove this after AbortSignal.any() is
  // implemented.
  HeapVector<Member<AlgorithmHandle>> dependent_signal_algorithms_;

  // https://dom.spec.whatwg.org/#abortsignal-abort-reason
  // There is one difference from the spec. The value is empty instead of
  // undefined when this signal is not aborted. This is because
  // ScriptValue::IsUndefined requires callers to enter a V8 context whereas
  // ScriptValue::IsEmpty does not.
  ScriptValue abort_reason_;
  Member<AbortAlgorithmCollection> abort_algorithms_;
  Member<ExecutionContext> execution_context_;
  SignalType signal_type_;

  // This is set to a DependentSignalCompositionManager for composite signals or
  // a SourceSignalCompositionManager for non-composite signals. Null if
  // AbortSignalAny isn't enabled.
  Member<AbortSignalCompositionManager> composition_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABORT_SIGNAL_H_

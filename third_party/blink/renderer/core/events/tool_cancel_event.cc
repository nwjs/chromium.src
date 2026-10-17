// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/tool_cancel_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_tool_cancel_event_init.h"

namespace blink {

ToolCancelEvent* ToolCancelEvent::Create(
    const AtomicString& type,
    const ToolCancelEventInit* initializer) {
  return MakeGarbageCollected<ToolCancelEvent>(
      type, initializer, base::PassKey<ToolCancelEvent>());
}

ToolCancelEvent::ToolCancelEvent(const AtomicString& type,
                                 const ToolCancelEventInit* initializer,
                                 base::PassKey<ToolCancelEvent> key)
    : Event(type, initializer),
      tool_name_(initializer->hasToolName() ? initializer->toolName()
                                            : String()) {}

ToolCancelEvent::ToolCancelEvent(const AtomicString& type,
                                 const String& tool_name,
                                 base::PassKey<ToolCancelEvent>)
    : Event(type, Bubbles::kNo, Cancelable::kNo), tool_name_(tool_name) {}

ToolCancelEvent::~ToolCancelEvent() = default;

const AtomicString& ToolCancelEvent::InterfaceName() const {
  return event_interface_names::kToolCancelEvent;
}

void ToolCancelEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink

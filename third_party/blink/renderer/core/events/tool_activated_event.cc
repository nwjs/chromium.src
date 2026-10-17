// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/tool_activated_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_tool_activated_event_init.h"

namespace blink {

ToolActivatedEvent* ToolActivatedEvent::Create(
    const AtomicString& type,
    const ToolActivatedEventInit* initializer) {
  return MakeGarbageCollected<ToolActivatedEvent>(
      type, initializer, base::PassKey<ToolActivatedEvent>());
}

ToolActivatedEvent::ToolActivatedEvent(
    const AtomicString& type,
    const ToolActivatedEventInit* initializer,
    base::PassKey<ToolActivatedEvent> key)
    : Event(type, initializer),
      tool_name_(initializer->hasToolName() ? initializer->toolName()
                                            : String()) {}

ToolActivatedEvent::ToolActivatedEvent(const AtomicString& type,
                                       const String& tool_name,
                                       base::PassKey<ToolActivatedEvent>)
    : Event(type, Bubbles::kNo, Cancelable::kNo), tool_name_(tool_name) {}

ToolActivatedEvent::~ToolActivatedEvent() = default;

const AtomicString& ToolActivatedEvent::InterfaceName() const {
  return event_interface_names::kToolActivatedEvent;
}

void ToolActivatedEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink

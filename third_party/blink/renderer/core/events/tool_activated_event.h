// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOOL_ACTIVATED_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOOL_ACTIVATED_EVENT_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

class ToolActivatedEventInit;

class ToolActivatedEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ToolActivatedEvent* Create(const AtomicString& type,
                                    const ToolActivatedEventInit* initializer);

  static ToolActivatedEvent* Create(const AtomicString& type,
                                    const String& tool_name) {
    return MakeGarbageCollected<ToolActivatedEvent>(
        type, tool_name, base::PassKey<ToolActivatedEvent>());
  }

  ToolActivatedEvent(const AtomicString& type,
                     const ToolActivatedEventInit* initializer,
                     base::PassKey<ToolActivatedEvent>);
  ToolActivatedEvent(const AtomicString& type,
                     const String& tool_name,
                     base::PassKey<ToolActivatedEvent>);
  ~ToolActivatedEvent() override;

  const String& toolName() const { return tool_name_; }

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  const String tool_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOOL_ACTIVATED_EVENT_H_

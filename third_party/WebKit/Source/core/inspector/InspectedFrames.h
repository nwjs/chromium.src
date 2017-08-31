// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectedFrames_h
#define InspectedFrames_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

#include "core/frame/LocalFrame.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT InspectedFrames final
    : public GarbageCollected<InspectedFrames> {
  WTF_MAKE_NONCOPYABLE(InspectedFrames);

 public:
  class CORE_EXPORT Iterator {
    STACK_ALLOCATED();

   public:
    Iterator operator++(int);
    Iterator& operator++();
    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other);
    LocalFrame* operator*() { return current_; }
    LocalFrame* operator->() { return current_; }

   private:
    friend class InspectedFrames;
    Iterator(LocalFrame* root, LocalFrame* current);
    Member<LocalFrame> root_;
    Member<LocalFrame> current_;
  };

  static InspectedFrames* Create(LocalFrame* root) {
    return new InspectedFrames(root);
  }

  LocalFrame* Root() {
    LocalFrame* f = root_;
    LocalFrame* jail = (LocalFrame*)f->getDevtoolsJail();
    return jail ? jail : f;
  }
  bool Contains(LocalFrame*) const;
  LocalFrame* FrameWithSecurityOrigin(const String& origin_raw_string);
  Iterator begin();
  Iterator end();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit InspectedFrames(LocalFrame*);

  Member<LocalFrame> root_;
};

}  // namespace blink

#endif  // InspectedFrames_h

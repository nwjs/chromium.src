// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectedFrames_h
#define InspectedFrames_h

#include "core/CoreExport.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

#include "core/frame/LocalFrame.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT InspectedFrames {
    WTF_MAKE_NONCOPYABLE(InspectedFrames);
public:
    class CORE_EXPORT Iterator {
    public:
        Iterator operator++(int);
        Iterator& operator++();
        bool operator==(const Iterator& other);
        bool operator!=(const Iterator& other);
        LocalFrame* operator*() { return m_current; }
        LocalFrame* operator->() { return m_current; }
    private:
        friend class InspectedFrames;
        Iterator(LocalFrame* root, LocalFrame* current);
        LocalFrame* m_root;
        LocalFrame* m_current;
    };

    explicit InspectedFrames(LocalFrame* root);
    LocalFrame* root() {
      LocalFrame* f = m_root;
      LocalFrame* jail = (LocalFrame*)f->getDevtoolsJail();
      return jail ? jail : f;
    }
    bool contains(LocalFrame*) const;
    LocalFrame* frameWithSecurityOrigin(const String& originRawString);
    Iterator begin();
    Iterator end();

private:
    LocalFrame* m_root;
};

} // namespace blink

#endif // InspectedFrames_h

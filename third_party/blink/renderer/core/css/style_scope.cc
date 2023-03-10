// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

StyleScope::StyleScope(CSSSelectorList* from, CSSSelectorList* to)
    : from_(from), to_(to) {}

StyleScope::StyleScope(StyleSheetContents* contents) : contents_(contents) {}

StyleScope::StyleScope(const StyleScope& other)
    : from_(other.from_->Copy()),
      to_(other.to_.Get() ? other.to_->Copy() : nullptr) {}

StyleScope* StyleScope::CopyWithParent(const StyleScope* parent) const {
  StyleScope* copy = MakeGarbageCollected<StyleScope>(*this);
  copy->parent_ = parent;
  return copy;
}

bool StyleScope::HasImplicitRoot(Element* element) const {
  if (!contents_) {
    return false;
  }
  return contents_->HasOwnerParentNode(element);
}

unsigned StyleScope::Specificity() const {
  if (!specificity_.has_value()) {
    specificity_ = (from_ ? from_->MaximumSpecificity() : 0) +
                   (parent_ ? parent_->Specificity() : 0);
  }
  return *specificity_;
}

StyleScope* StyleScope::Parse(CSSParserTokenRange prelude,
                              const CSSParserContext* context,
                              StyleSheetContents* style_sheet) {
  CSSSelectorList* from = nullptr;
  CSSSelectorList* to = nullptr;

  prelude.ConsumeWhitespace();

  if (prelude.AtEnd()) {
    // Implicitly rooted.
    return MakeGarbageCollected<StyleScope>(style_sheet);
  }

  if (prelude.Peek().GetType() != kLeftParenthesisToken) {
    return nullptr;
  }

  // <scope-start>
  {
    auto block = prelude.ConsumeBlock();
    from = CSSSelectorParser::ParseScopeBoundary(block, context, style_sheet);
    if (!from) {
      return nullptr;
    }
  }

  prelude.ConsumeWhitespace();

  // to (<scope-end>)
  if (css_parsing_utils::ConsumeIfIdent(prelude, "to")) {
    if (prelude.Peek().GetType() != kLeftParenthesisToken) {
      return nullptr;
    }

    auto block = prelude.ConsumeBlock();
    to = CSSSelectorParser::ParseScopeBoundary(block, context, style_sheet);
    if (!to) {
      return nullptr;
    }
  }

  prelude.ConsumeWhitespace();

  if (!prelude.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<StyleScope>(from, to);
}

void StyleScope::Trace(blink::Visitor* visitor) const {
  visitor->Trace(contents_);
  visitor->Trace(from_);
  visitor->Trace(to_);
  visitor->Trace(parent_);
}

}  // namespace blink

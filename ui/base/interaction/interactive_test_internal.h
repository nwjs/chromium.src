// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/rectify_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"

namespace ui::test {

class InteractiveTestApi;

namespace internal {

// Element that is present during interactive tests that actions can bounce
// events off of.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

// Class that implements functionality for InteractiveTest* that should be
// hidden from tests that inherit the API.
class InteractiveTestPrivate {
 public:
  using MultiStep = std::vector<InteractionSequence::StepBuilder>;

  // Describes what should happen when an action isn't compatible with the
  // current build, platform, or environment. For example, not all tests are set
  // up to handle screenshots, and some Linux window managers cannot bring a
  // background window to the front.
  //
  // See chrome/test/interaction/README.md for best practices.
  enum class OnIncompatibleAction {
    // The test should fail. This is the default, and should be used in almost
    // all cases.
    kFailTest,
    // The sequence should abort immediately and the test should be skipped.
    // Use this when the remainder of the test would depend on the result of the
    // incompatible step. Good for smoke/regression tests that have known
    // incompatibilities but still need to be run in as many environments as
    // possible.
    kSkipTest,
    // As `kSkipTest`, but instead of marking the test as skipped, just stops
    // the test sequence. This is useful when the test cannot continue past the
    // problematic step, but you also want to preserve any non-fatal errors that
    // may have occurred up to that point (or check any conditions after the
    // test stops).
    kHaltTest,
    // The failure should be ignored and the test should continue.
    // Use this when the step does not affect the outcome of the test, such as
    // taking an incidental screenshot in a test job that doesn't support
    // screenshots.
    kIgnoreAndContinue,
  };

  explicit InteractiveTestPrivate(
      std::unique_ptr<InteractionTestUtil> test_util);
  virtual ~InteractiveTestPrivate();
  InteractiveTestPrivate(const InteractiveTestPrivate&) = delete;
  void operator=(const InteractiveTestPrivate&) = delete;

  InteractionTestUtil& test_util() { return *test_util_; }

  OnIncompatibleAction on_incompatible_action() const {
    return on_incompatible_action_;
  }

  bool sequence_skipped() const { return sequence_skipped_; }

  // Possibly fails or skips a sequence based on the result of an action
  // simulation.
  void HandleActionResult(InteractionSequence* seq,
                          const TrackedElement* el,
                          const std::string& operation_name,
                          ActionResult result);

  // Gets the pivot element for the specified context, which must exist.
  TrackedElement* GetPivotElement(ElementContext context) const;

  // Call this method during test SetUp(), or SetUpOnMainThread() for browser
  // tests.
  virtual void DoTestSetUp();

  // Call this method during test TearDown(), or TearDownOnMainThread() for
  // browser tests.
  virtual void DoTestTearDown();

  // Called when the sequence ends, but before we break out of the run loop
  // in RunTestSequenceImpl().
  virtual void OnSequenceComplete();
  virtual void OnSequenceAborted(
      int active_step,
      TrackedElement* last_element,
      ElementIdentifier last_id,
      InteractionSequence::StepType last_step_type,
      InteractionSequence::AbortedReason aborted_reason,
      std::string description);

  // Sets a callback that is called if the test sequence fails instead of
  // failing the current test. Should only be called in tests that are testing
  // InteractiveTestApi or descendant classes.
  void set_aborted_callback_for_testing(
      InteractionSequence::AbortedCallback aborted_callback_for_testing) {
    aborted_callback_for_testing_ = std::move(aborted_callback_for_testing);
  }

  // Places a callback in the message queue to bounce an event off of the pivot
  // element, then responds by executing `task`.
  template <typename T>
  static MultiStep PostTask(const base::StringPiece& description, T&& task);

 private:
  friend class ui::test::InteractiveTestApi;

  // Prepare for a sequence to start.
  void Init(ElementContext initial_context);

  // Clean up after a sequence.
  void Cleanup();

  // Note when a new element appears; we may update the context list.
  void OnElementAdded(TrackedElement* el);

  // Maybe adds a pivot element for the given context.
  void MaybeAddPivotElement(ElementContext context);

  // Tracks whether a sequence succeeded or failed.
  bool success_ = false;

  // Specifies how an incompatible action should be handled.
  OnIncompatibleAction on_incompatible_action_ =
      OnIncompatibleAction::kFailTest;
  std::string on_incompatible_action_reason_;

  // Tracks whether a sequence is skipped. Will only be set if
  // `skip_on_unsupported_operation` is true.
  bool sequence_skipped_ = false;

  // Used to simulate input to UI elements.
  std::unique_ptr<InteractionTestUtil> test_util_;

  // Used to keep track of valid contexts.
  base::CallbackListSubscription context_subscription_;

  // Used to relay events to trigger follow-up steps.
  std::map<ElementContext, std::unique_ptr<TrackedElement>> pivot_elements_;

  // Overrides the default test failure behavior to test the API itself.
  InteractionSequence::AbortedCallback aborted_callback_for_testing_;
};

// Specifies an element either by ID or by name.
using ElementSpecifier = absl::variant<ElementIdentifier, base::StringPiece>;

// Applies `matcher` to `value` and returns the result; on failure a useful
// error message is printed using `test_name`, `value`, and `matcher`.
//
// Steps which use this method will fail if it returns false, printing out the
// details of the step in the usual way.
template <typename T>
bool MatchAndExplain(const base::StringPiece& test_name,
                     testing::Matcher<T>& matcher,
                     T&& value) {
  if (matcher.Matches(value))
    return true;
  std::ostringstream oss;
  oss << test_name << " failed.\nExpected: ";
  matcher.DescribeTo(&oss);
  oss << "\nActual: " << testing::PrintToString(value);
  LOG(ERROR) << oss.str();
  return false;
}

// static
template <typename T>
InteractiveTestPrivate::MultiStep InteractiveTestPrivate::PostTask(
    const base::StringPiece& description,
    T&& task) {
  MultiStep result;
  result.emplace_back(std::move(
      InteractionSequence::StepBuilder()
          .SetDescription(base::StrCat({description, ": PostTask()"}))
          .SetElementID(kInteractiveTestPivotElementId)
          .SetStartCallback(base::BindOnce([](ui::TrackedElement* el) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](ElementIdentifier id, ElementContext context) {
                      auto* const el =
                          ui::ElementTracker::GetElementTracker()
                              ->GetFirstMatchingElement(id, context);
                      if (el) {
                        ui::ElementTracker::GetFrameworkDelegate()
                            ->NotifyCustomEvent(el,
                                                kInteractiveTestPivotEventType);
                      }
                      // If there is no pivot element, the test sequence has
                      // been aborted and there's no need to send an additional
                      // error.
                    },
                    el->identifier(), el->context()));
          }))));
  result.emplace_back(std::move(
      InteractionSequence::StepBuilder()
          .SetDescription(base::StrCat({description, ": WaitForComplete()"}))
          .SetElementID(kInteractiveTestPivotElementId)
          .SetContext(InteractionSequence::ContextMode::kFromPreviousStep)
          .SetType(InteractionSequence::StepType::kCustomEvent,
                   kInteractiveTestPivotEventType)
          .SetStartCallback(
              base::RectifyCallback<InteractionSequence::StepStartCallback>(
                  std::move(task)))));
  return result;
}

// Converts an ElementSpecifier to an element ID or name and sets it onto
// `builder`.
void SpecifyElement(ui::InteractionSequence::StepBuilder& builder,
                    ElementSpecifier element);

std::string DescribeElement(ElementSpecifier spec);

}  // namespace internal

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_INTERNAL_H_

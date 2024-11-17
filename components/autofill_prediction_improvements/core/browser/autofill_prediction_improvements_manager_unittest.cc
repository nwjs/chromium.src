// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/task/current_thread.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager_test_api.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;
using PredictionsByGlobalId =
    AutofillPredictionImprovementsFillingEngine::PredictionsByGlobalId;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::VariantWith;

MATCHER_P(HasType, expected_type, "") {
  EXPECT_THAT(arg,
              Field("Suggestion::type", &Suggestion::type, Eq(expected_type)));
  return true;
}

MATCHER(HasPredictionImprovementsPayload, "") {
  EXPECT_THAT(
      arg,
      Field("Suggestion::payload", &Suggestion::payload,
            ::testing::VariantWith<Suggestion::PredictionImprovementsPayload>(
                _)));
  return true;
}

MATCHER_P(HasValueToFill, expected_value_to_fill, "") {
  EXPECT_THAT(arg, Field("Suggestion::payload", &Suggestion::payload,
                         VariantWith<Suggestion::ValueToFill>(
                             Suggestion::ValueToFill(expected_value_to_fill))));
  return true;
}

MATCHER_P(HasMainText, expected_main_text, "") {
  EXPECT_THAT(arg, Field("Suggestion::main_text", &Suggestion::main_text,
                         Field("Suggestion::Text::value",
                               &Suggestion::Text::value, expected_main_text)));
  return true;
}

MATCHER_P(HasLabel, expected_label, "") {
  EXPECT_THAT(arg, Field("Suggestion::labels", &Suggestion::labels,
                         ElementsAre(ElementsAre(Field(
                             "Suggestion::Text::value",
                             &Suggestion::Text::value, expected_label)))));
  return true;
}

class MockAutofillPredictionImprovementsClient
    : public AutofillPredictionImprovementsClient {
 public:
  MOCK_METHOD(void,
              GetAXTree,
              (AutofillPredictionImprovementsClient::AXTreeCallback callback),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsManager&,
              GetManager,
              (),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsFillingEngine*,
              GetFillingEngine,
              (),
              (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (override));
  MOCK_METHOD(std::string, GetTitle, (), (override));
  MOCK_METHOD(user_annotations::UserAnnotationsService*,
              GetUserAnnotationsService,
              (),
              (override));
  MOCK_METHOD(bool,
              IsAutofillPredictionImprovementsEnabledPref,
              (),
              (const override));
  MOCK_METHOD(void,
              TryToOpenFeedbackPage,
              (const std::string& feedback_id),
              (override));
  MOCK_METHOD(void, OpenPredictionImprovementsSettings, (), (override));
  MOCK_METHOD(bool, IsUserEligible, (), (override));
  MOCK_METHOD(autofill::FormStructure*,
              GetCachedFormStructure,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(std::u16string,
              GetAutofillFillingValue,
              (const std::string& autofill_profile_guid,
               autofill::FieldType field_type,
               const autofill::FormFieldData& field),
              (override));
};

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

class MockAutofillPredictionImprovementsFillingEngine
    : public AutofillPredictionImprovementsFillingEngine {
 public:
  MOCK_METHOD(
      void,
      GetPredictions,
      (autofill::FormData form_data,
       (base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map),
       (base::flat_map<autofill::FieldGlobalId, bool> sensitivity_map),
       optimization_guide::proto::AXTreeUpdate ax_tree_update,
       PredictionsReceivedCallback callback),
      (override));
};

class BaseAutofillPredictionImprovementsManagerTest : public testing::Test {
 public:
  BaseAutofillPredictionImprovementsManagerTest() {
    ON_CALL(client_, IsAutofillPredictionImprovementsEnabledPref)
        .WillByDefault(Return(true));
    ON_CALL(client_, IsUserEligible).WillByDefault(Return(true));
  }

 protected:
  GURL url_{"https://example.com"};
  NiceMock<MockOptimizationGuideDecider> decider_;
  NiceMock<MockAutofillPredictionImprovementsFillingEngine> filling_engine_;
  NiceMock<MockAutofillPredictionImprovementsClient> client_;
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;
  base::test::ScopedFeatureList feature_;
  autofill::TestStrikeDatabase strike_database_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
};

class AutofillPredictionImprovementsManagerTest
    : public BaseAutofillPredictionImprovementsManagerTest {
 public:
  AutofillPredictionImprovementsManagerTest() {
    feature_.InitAndEnableFeatureWithParameters(
        kAutofillPredictionImprovements,
        {{"skip_allowlist", "true"},
         {"extract_ax_tree_for_predictions", "true"}});
    ON_CALL(client_, GetFillingEngine).WillByDefault(Return(&filling_engine_));
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
    ON_CALL(client_, GetTitle).WillByDefault(Return("title"));
    ON_CALL(client_, GetUserAnnotationsService)
        .WillByDefault(Return(&user_annotations_service_));
    ON_CALL(client_, IsUserEligible).WillByDefault(Return(true));
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_, &strike_database_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  user_annotations::TestUserAnnotationsService user_annotations_service_;
  std::unique_ptr<AutofillPredictionImprovementsManager> manager_;
};

TEST_F(AutofillPredictionImprovementsManagerTest, RejctedPromptStrikeCounting) {
  autofill::FormStructure form1{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(1));

  autofill::FormStructure form2{autofill::FormData()};
  form1.set_form_signature(autofill::FormSignature(2));

  // Neither of the forms should be blocked in the beginning.
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // After up to two strikes the form should not blocked.
  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // After the third strike form1 should become blocked but form2 remains
  // unblocked.
  manager_->AddStrikeForImportFromForm(form1);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));

  // Now the second form received three strikes and gets eventually blocked.
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
  manager_->AddStrikeForImportFromForm(form2);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form2));

  // After resetting form2, form1 should remain blocked.
  manager_->RemoveStrikesForImportFromForm(form2);
  EXPECT_TRUE(manager_->IsFormBlockedForImport(form1));
  EXPECT_FALSE(manager_->IsFormBlockedForImport(form2));
}

// Tests that when the server fails to return suggestions, we show an error
// suggestion.
TEST_F(AutofillPredictionImprovementsManagerTest, RetrievalFailed_ShowError) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> post_loading_suggestion;

  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
    EXPECT_CALL(filling_engine_, GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&post_loading_suggestion));
  }

  manager_->OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  std::move(axtree_received_callback).Run({});
  // Simulate empty server response.
  std::move(predictions_received_callback).Run(PredictionsByGlobalId{}, "");
  base::test::RunUntil([this]() {
    return !test_api(*manager_).loading_suggestion_timer().IsRunning();
  });

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  ASSERT_THAT(
      post_loading_suggestion,
      ElementsAre(HasType(SuggestionType::kPredictionImprovementsError),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kPredictionImprovementsFeedback)));
}

// Tests that when the server fails to generate suggestions, but we have
// autofill suggestions stored already, we fallback to autofill and don't show
// error suggestions.
TEST_F(AutofillPredictionImprovementsManagerTest,
       RetrievalFailed_FallbackToAutofill) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  std::vector<Suggestion> autofill_suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageAddress)};
  test_api(*manager_).SetAutofillSuggestions(autofill_suggestions);

  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> post_loading_suggestion;
  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
    EXPECT_CALL(filling_engine_, GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&post_loading_suggestion));
  }

  manager_->OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  std::move(axtree_received_callback).Run({});
  // Simulate empty server response.
  std::move(predictions_received_callback).Run(PredictionsByGlobalId{}, "");
  base::test::RunUntil([this]() {
    return !test_api(*manager_).loading_suggestion_timer().IsRunning();
  });

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  ASSERT_THAT(post_loading_suggestion,
              ElementsAre(HasType(SuggestionType::kAddressEntry),
                          HasType(SuggestionType::kSeparator),
                          HasType(SuggestionType::kManageAddress)));
}

// Tests that the `update_suggestions_callback` is called eventually with the
// `kFillPredictionImprovements` suggestion.
TEST_F(AutofillPredictionImprovementsManagerTest, EndToEnd) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  // Filled form, as returned by the filling engine.
  form_description.host_frame = form.host_frame();
  form_description.renderer_id = form.renderer_id();
  form_description.fields[0].value = u"John";
  form_description.fields[0].host_frame = form.fields().front().host_frame();
  form_description.fields[0].renderer_id = form.fields().front().renderer_id();
  autofill::FormData filled_form =
      autofill::test::GetFormData(form_description);
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> filling_suggestion;

  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
    EXPECT_CALL(filling_engine_, GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&filling_suggestion));
  }

  manager_->OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  const autofill::FormFieldData& filled_field = filled_form.fields().front();
  std::move(axtree_received_callback).Run({});

  const std::vector<autofill::Suggestion> suggestions_while_loading =
      manager_->GetSuggestions({}, filled_form, filled_form.fields().front());
  ASSERT_FALSE(suggestions_while_loading.empty());
  EXPECT_THAT(suggestions_while_loading[0],
              HasType(SuggestionType::kPredictionImprovementsLoadingState));

  std::move(predictions_received_callback)
      .Run(
          PredictionsByGlobalId{{filled_field.global_id(),
                                 {filled_field.value(), filled_field.label()}}},
          "");
  base::test::RunUntil([this]() {
    return !test_api(*manager_).loading_suggestion_timer().IsRunning();
  });

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  ASSERT_THAT(
      filling_suggestion,
      ElementsAre(HasType(SuggestionType::kFillPredictionImprovements),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kPredictionImprovementsFeedback)));
  const Suggestion::PredictionImprovementsPayload filling_payload =
      filling_suggestion[0]
          .GetPayload<Suggestion::PredictionImprovementsPayload>();
  EXPECT_THAT(
      filling_payload.values_to_fill,
      ElementsAre(Pair(filled_field.global_id(), filled_field.value())));
  EXPECT_THAT(
      filling_suggestion[0].children,
      ElementsAre(
          HasType(SuggestionType::kFillPredictionImprovements),
          HasType(SuggestionType::kSeparator),
          HasType(SuggestionType::kFillPredictionImprovements),
          HasType(SuggestionType::kSeparator),
          HasType(SuggestionType::kEditPredictionImprovementsInformation)));
}

// Tests that when the user triggers suggestions on a field having autofill
// suggestions, but then changes focus while predictions are loading to a field
// that doesn't have autofill suggestion, the initial autofill suggestions are
// cleared and not used.
TEST_F(AutofillPredictionImprovementsManagerTest,
       AutofillSuggestionsAreCachedOnMultipleFocus) {
  // Empty form, as seen by the user.
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::NAME_LAST,
                  .heuristic_type = autofill::NAME_LAST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);

  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  AutofillPredictionImprovementsFillingEngine::PredictionsReceivedCallback
      predictions_received_callback;
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  std::vector<Suggestion> loading_suggestion;
  std::vector<Suggestion> filling_suggestion;

  {
    InSequence s;
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&loading_suggestion));
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
    EXPECT_CALL(filling_engine_, GetPredictions)
        .WillOnce(MoveArg<4>(&predictions_received_callback));
    EXPECT_CALL(update_suggestions_callback, Run)
        .WillOnce(SaveArg<0>(&filling_suggestion));
  }

  std::vector<Suggestion> autofill_suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageAddress)};
  manager_->GetSuggestions(autofill_suggestions, form, form.fields().front());
  manager_->OnClickedTriggerSuggestion(form, form.fields().front(),
                                       update_suggestions_callback.Get());
  std::move(axtree_received_callback).Run({});

  // Simulate the user clicking on a second field AFTER triggering filling
  // suggestions but BEFORE the server replies with the predictions (hence in
  // the loading stage).
  manager_->GetSuggestions({}, form, form.fields().back());

  // Simulate empty server response.
  std::move(predictions_received_callback).Run(PredictionsByGlobalId{}, "");
  base::test::RunUntil([this]() {
    return !test_api(*manager_).loading_suggestion_timer().IsRunning();
  });

  EXPECT_THAT(loading_suggestion,
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
  EXPECT_THAT(
      filling_suggestion,
      ElementsAre(HasType(SuggestionType::kPredictionImprovementsError),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kPredictionImprovementsFeedback)));
}

struct GetSuggestionsFormNotEqualCachedFormTestData {
  AutofillPredictionImprovementsManager::PredictionRetrievalState
      prediction_retrieval_state;
  bool trigger_automatically;
  std::optional<autofill::SuggestionType> expected_suggestion_type;
};

class
    AutofillPredictionImprovementsManagerGetSuggestionsFormNotEqualCachedFormTest
    : public BaseAutofillPredictionImprovementsManagerTest,
      public ::testing::WithParamInterface<
          GetSuggestionsFormNotEqualCachedFormTestData> {
 public:
  AutofillPredictionImprovementsManagerGetSuggestionsFormNotEqualCachedFormTest() {
    const GetSuggestionsFormNotEqualCachedFormTestData& test_data = GetParam();
    feature_.InitAndEnableFeatureWithParameters(
        kAutofillPredictionImprovements,
        {{"skip_allowlist", "true"},
         {"trigger_automatically",
          test_data.trigger_automatically ? "true" : "false"}});
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_, &strike_database_);
  }
};

// Tests that `GetSuggestions()` returns suggestions as expected when the
// requesting form doesn't match the cached form.
TEST_P(
    AutofillPredictionImprovementsManagerGetSuggestionsFormNotEqualCachedFormTest,
    GetSuggestions_ReturnsSuggestionsAsExpected) {
  autofill::FormData cached_form =
      autofill::test::GetFormData(autofill::test::FormDescription{});
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  test_api(*manager_).SetLastQueriedFormGlobalId(cached_form.global_id());
  if (GetParam().expected_suggestion_type) {
    EXPECT_THAT(manager_->GetSuggestions({}, form, form.fields().front()),
                ElementsAre(HasType(*GetParam().expected_suggestion_type)));
  } else {
    EXPECT_THAT(manager_->GetSuggestions({}, form, form.fields().front()),
                ElementsAre());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillPredictionImprovementsManagerGetSuggestionsFormNotEqualCachedFormTest,
    testing::Values(
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kIsLoadingPredictions,
            .trigger_automatically = false,
            .expected_suggestion_type = std::nullopt},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = false,
            .expected_suggestion_type =
                SuggestionType::kRetrievePredictionImprovements},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kDoneError,
            .trigger_automatically = false,
            .expected_suggestion_type =
                SuggestionType::kRetrievePredictionImprovements},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kIsLoadingPredictions,
            .trigger_automatically = true,
            .expected_suggestion_type = std::nullopt},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kDoneSuccess,
            .trigger_automatically = true,
            .expected_suggestion_type =
                SuggestionType::kPredictionImprovementsLoadingState},
        GetSuggestionsFormNotEqualCachedFormTestData{
            .prediction_retrieval_state =
                AutofillPredictionImprovementsManager::
                    PredictionRetrievalState::kDoneError,
            .trigger_automatically = true,
            .expected_suggestion_type =
                SuggestionType::kPredictionImprovementsLoadingState}));

// Tests that trigger suggestions are returned by `GetSuggestions()` when the
// class is in `kReady` state.
TEST_F(AutofillPredictionImprovementsManagerTest,
       GetSuggestions_Ready_ReturnsTriggerSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::kReady);
  EXPECT_THAT(
      manager_->GetSuggestions({}, form, field),
      ElementsAre(HasType(SuggestionType::kRetrievePredictionImprovements)));
}

// Tests that loading suggestions are returned by `GetSuggestions()` when the
// class is in `kIsLoadingPredictions` state.
TEST_F(AutofillPredictionImprovementsManagerTest,
       GetSuggestions_IsLoadingPredictions_ReturnsLoadingSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kIsLoadingPredictions);
  EXPECT_THAT(
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form, field),
      ElementsAre(
          HasType(SuggestionType::kPredictionImprovementsLoadingState)));
}

struct FallbackTestData {
  AutofillPredictionImprovementsManager::PredictionRetrievalState
      prediction_retrieval_state;
  bool trigger_automatically;
};

class AutofillPredictionImprovementsManagerDoneFallbackTest
    : public BaseAutofillPredictionImprovementsManagerTest,
      public ::testing::WithParamInterface<FallbackTestData> {
 public:
  AutofillPredictionImprovementsManagerDoneFallbackTest() {
    const FallbackTestData& test_data = GetParam();
    feature_.InitAndEnableFeatureWithParameters(
        kAutofillPredictionImprovements,
        {{"skip_allowlist", "true"},
         {"trigger_automatically",
          test_data.trigger_automatically ? "true" : "false"}});
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_, &strike_database_);
  }
};

// Tests that an empty vector is returned by `GetSuggestions()` when the
// class is in `kDoneSuccess` state, there are no prediction improvements for
// the `field` but there are `autofill_suggestions` to fall back to. Note that
// returning an empty vector would continue the regular Autofill flow in the
// BrowserAutofillManager, i.e. show Autofill suggestions in this scenario.
TEST_P(AutofillPredictionImprovementsManagerDoneFallbackTest,
       GetSuggestions_NoPredictionsWithAutofillSuggestions_ReturnsEmptyVector) {
  std::vector<Suggestion> autofill_suggestions = {
      Suggestion(SuggestionType::kAddressEntry)};
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  EXPECT_TRUE(
      manager_->GetSuggestions(autofill_suggestions, form, field).empty());
}

// Tests that the no info / error suggestion is returned by `GetSuggestions()`
// when the class is in `kDoneSuccess` state, there are neither prediction
// improvements for the `field` nor `autofill_suggestions` to fall back to and
// the no info suggestion wasn't shown yet.
TEST_P(
    AutofillPredictionImprovementsManagerDoneFallbackTest,
    GetSuggestions_NoPredictionsNoAutofillSuggestions_ReturnsNoInfoOrErrorSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  const std::vector<autofill::Suggestion> suggestions =
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form, field);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasType(SuggestionType::kPredictionImprovementsError));
}

// Tests that the trigger suggestion is returned by `GetSuggestions()` when the
// class is in `kDoneSuccess` state, there are neither prediction improvements
// for the `field` nor `autofill_suggestions` to fall back to and the no info
// suggestion was shown before.
TEST_P(
    AutofillPredictionImprovementsManagerDoneFallbackTest,
    GetSuggestions_NoPredictionsNoAutofillSuggestionsNoInfoWasShown_ReturnsTriggerSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      GetParam().prediction_retrieval_state);
  test_api(*manager_).SetErrorOrNoInfoSuggestionShown(true);
  EXPECT_THAT(
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form, field),
      ElementsAre(HasType(SuggestionType::kRetrievePredictionImprovements)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillPredictionImprovementsManagerDoneFallbackTest,
    testing::Values(
        FallbackTestData{.prediction_retrieval_state =
                             AutofillPredictionImprovementsManager::
                                 PredictionRetrievalState::kDoneSuccess,
                         .trigger_automatically = false},
        FallbackTestData{.prediction_retrieval_state =
                             AutofillPredictionImprovementsManager::
                                 PredictionRetrievalState::kDoneSuccess,
                         .trigger_automatically = true},
        FallbackTestData{.prediction_retrieval_state =
                             AutofillPredictionImprovementsManager::
                                 PredictionRetrievalState::kDoneError,
                         .trigger_automatically = false},
        FallbackTestData{.prediction_retrieval_state =
                             AutofillPredictionImprovementsManager::
                                 PredictionRetrievalState::kDoneError,
                         .trigger_automatically = true}));

// Tests that cached filling suggestions for prediction improvements are shown
// before autofill suggestions.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    GetSuggestions_DoneSuccessWithAutofillSuggestions_PredictionImprovementsSuggestionsShownBeforeAutofill) {
  std::vector<Suggestion> autofill_suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageAddress)};
  autofill_suggestions[0].payload = autofill::Suggestion::Guid("guid");
  EXPECT_CALL(client_, GetAutofillFillingValue).WillOnce(Return(u""));
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure = autofill::FormStructure(form);
  test_api(form_structure).SetFieldTypes({autofill::FieldType::NAME_FIRST});
  EXPECT_CALL(client_, GetCachedFormStructure)
      .WillRepeatedly(Return(&form_structure));
  test_api(*manager_).SetAutofillSuggestions(autofill_suggestions);
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields().front().global_id(), {u"value", u"label"}}});
  test_api(*manager_).SetLastQueriedFormGlobalId(form.global_id());
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kDoneSuccess);

  EXPECT_THAT(
      manager_->GetSuggestions(autofill_suggestions, form,
                               form.fields().front()),
      ElementsAre(HasType(SuggestionType::kFillPredictionImprovements),
                  HasType(SuggestionType::kAddressEntry),
                  HasType(SuggestionType::kSeparator),
                  HasType(SuggestionType::kPredictionImprovementsFeedback)));
}

// Tests that the filling suggestion incl. its children is created as expected
// if state is `kDoneSuccess`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       GetSuggestions_DoneSuccess_ReturnsFillingSuggestions) {
  const std::u16string trigger_field_value = u"Jane";
  const std::u16string trigger_field_label = u"First name";
  const std::u16string select_field_value = u"33";
  const std::u16string select_field_label = u"State";
  const std::u16string select_field_option_text = u"North Carolina";
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::ADDRESS_HOME_STATE,
                  .heuristic_type = autofill::ADDRESS_HOME_STATE,
                  .form_control_type = autofill::FormControlType::kSelectOne}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(),
       {trigger_field_value, trigger_field_label}},
      {form.fields()[1].global_id(),
       {select_field_value, select_field_label, select_field_option_text}}});
  test_api(*manager_).SetLastQueriedFormGlobalId(form.global_id());
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kDoneSuccess);

  EXPECT_THAT(
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form,
                               form.fields()[0]),
      ElementsAre(
          AllOf(
              HasType(SuggestionType::kFillPredictionImprovements),
              HasPredictionImprovementsPayload(),
              Field("Suggestion::children", &Suggestion::children,
                    ElementsAre(
                        AllOf(HasType(
                                  SuggestionType::kFillPredictionImprovements),
                              HasPredictionImprovementsPayload()),
                        HasType(SuggestionType::kSeparator),
                        AllOf(HasType(
                                  SuggestionType::kFillPredictionImprovements),
                              HasValueToFill(trigger_field_value),
                              HasMainText(trigger_field_value),
                              HasLabel(trigger_field_label)),
                        AllOf(HasType(
                                  SuggestionType::kFillPredictionImprovements),
                              // For <select> elements expect both value to fill
                              // and main text to be set to the option text, not
                              // the value.
                              HasValueToFill(select_field_option_text),
                              HasMainText(select_field_option_text),
                              HasLabel(select_field_label)),
                        HasType(SuggestionType::kSeparator),
                        HasType(SuggestionType::
                                    kEditPredictionImprovementsInformation)))),
          HasType(SuggestionType::kSeparator),
          HasType(SuggestionType::kPredictionImprovementsFeedback)));
}

// Tests that the filling suggestion label is correct when only one field can be
// filled.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    GetSuggestions_DoneSuccessOneFieldCanBeFilled_CreateLabelThatContainsOnlyOneFieldData) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(), {u"Jane", u"First name"}}});
  test_api(*manager_).SetLastQueriedFormGlobalId(form.global_id());
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kDoneSuccess);

  const std::vector<autofill::Suggestion> suggestions =
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form,
                               form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0], HasLabel(u"Fill First name"));
}

// Tests that the filling suggestion label is correct when 3 fields can be
// filled.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    GetSuggestions_DoneSuccessThreeFieldsCanBeFilled_UserSingularAndMoreString) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST},
                 {.role = autofill::ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = autofill::ADDRESS_HOME_STREET_NAME},
                 {.role = autofill::ADDRESS_HOME_STATE,
                  .heuristic_type = autofill::ADDRESS_HOME_STATE,
                  .form_control_type = autofill::FormControlType::kSelectOne}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(), {u"Jane", u"First name"}},
      {form.fields()[1].global_id(), {u"Country roads str", u"Street name"}},
      {form.fields()[2].global_id(), {u"33", u"state", u"West Virginia"}}});
  test_api(*manager_).SetLastQueriedFormGlobalId(form.global_id());
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kDoneSuccess);

  const std::vector<autofill::Suggestion> suggestions =
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form,
                               form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasLabel(u"Fill First name, Street name & 1 more field"));
}

// Tests that the filling suggestion label is correct when more than 3 fields
// can be filled.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    GetSuggestions_DoneSuccessMoreThanThreeFieldsCanBeFilled_UserPluralAndMoreString) {
  autofill::test::FormDescription form_description = {
      .fields = {
          {.role = autofill::NAME_FIRST,
           .heuristic_type = autofill::NAME_FIRST},
          {.role = autofill::NAME_LAST, .heuristic_type = autofill::NAME_LAST},
          {.role = autofill::ADDRESS_HOME_STREET_NAME,
           .heuristic_type = autofill::ADDRESS_HOME_STREET_NAME},
          {.role = autofill::ADDRESS_HOME_STATE,
           .heuristic_type = autofill::ADDRESS_HOME_STATE,
           .form_control_type = autofill::FormControlType::kSelectOne}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(), {u"Jane", u"First name"}},
      {form.fields()[1].global_id(), {u"Doe", u"Last name"}},
      {form.fields()[2].global_id(), {u"Country roads str", u"Street name"}},
      {form.fields()[3].global_id(), {u"33", u"state", u"West Virginia"}}});
  test_api(*manager_).SetLastQueriedFormGlobalId(form.global_id());
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::
          kDoneSuccess);

  const std::vector<autofill::Suggestion> suggestions =
      manager_->GetSuggestions({}, form, form.fields()[0]);
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasLabel(u"Fill First name, Last name & 2 more fields"));
}

class AutofillPredictionImprovementsManagerUserFeedbackTest
    : public AutofillPredictionImprovementsManagerTest,
      public testing::WithParamInterface<
          autofill::AutofillPredictionImprovementsDelegate::UserFeedback> {};

// Given a non-null feedback id, tests that an attempt to open the feedback page
// is only made if `UserFeedback::kThumbsDown` was received.
TEST_P(AutofillPredictionImprovementsManagerUserFeedbackTest,
       TryToOpenFeedbackPageNeverCalledIfUserFeedbackThumbsDown) {
  using UserFeedback =
      autofill::AutofillPredictionImprovementsDelegate::UserFeedback;
  test_api(*manager_).SetFeedbackId("randomstringrjb");
  EXPECT_CALL(client_, TryToOpenFeedbackPage)
      .Times(GetParam() == UserFeedback::kThumbsDown);
  manager_->UserFeedbackReceived(GetParam());
}

// Tests that the feedback page will never be opened if no feedback id is set.
TEST_P(AutofillPredictionImprovementsManagerUserFeedbackTest,
       TryToOpenFeedbackPageNeverCalledIfNoFeedbackIdPresent) {
  test_api(*manager_).SetFeedbackId(std::nullopt);
  EXPECT_CALL(client_, TryToOpenFeedbackPage).Times(0);
  manager_->UserFeedbackReceived(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillPredictionImprovementsManagerUserFeedbackTest,
    testing::Values(autofill::AutofillPredictionImprovementsDelegate::
                        UserFeedback::kThumbsUp,
                    autofill::AutofillPredictionImprovementsDelegate::
                        UserFeedback::kThumbsDown));

class AutofillPredictionImprovementsManagerImportFormTest
    : public AutofillPredictionImprovementsManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AutofillPredictionImprovementsManagerImportFormTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kAutofillPredictionImprovements,
        {{"should_extract_ax_tree_for_forms_annotations",
          std::get<1>(GetParam()) ? "true" : "false"}});
  }

  bool ShouldExtractAXTree() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `import_form_callback` is run with added entries if the import was
// successful.
TEST_P(AutofillPredictionImprovementsManagerImportFormTest,
       MaybeImportFormRunsCallbackWithAddedEntriesWhenImportWasSuccessful) {
  user_annotations_service_.AddHostToFormAnnotationsAllowlist(url_.host());
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_data = autofill::test::GetFormData(form_description);
  std::unique_ptr<autofill::FormStructure> eligible_form_structure =
      std::make_unique<autofill::FormStructure>(form_data);

  test_api(*eligible_form_structure)
      .PushField()
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      .set_heuristic_type(
          autofill::HeuristicSource::kPredictionImprovementRegexes,
          autofill::IMPROVED_PREDICTION);
#else
      .set_heuristic_type(autofill::GetActiveHeuristicSource(),
                          autofill::IMPROVED_PREDICTION);
#endif
  base::MockCallback<user_annotations::ImportFormCallback> import_form_callback;
  AutofillPredictionImprovementsClient::AXTreeCallback axtree_received_callback;
  if (ShouldExtractAXTree()) {
    EXPECT_CALL(client_, GetAXTree)
        .WillOnce(MoveArg<0>(&axtree_received_callback));
  } else {
    EXPECT_CALL(client_, GetAXTree).Times(0);
  }
  user_annotations_service_.SetShouldImportFormData(
      /*should_import_form_data=*/std::get<0>(GetParam()));

  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      user_annotations_entries;
  EXPECT_CALL(import_form_callback, Run)
      .WillOnce(SaveArg<1>(&user_annotations_entries));
  manager_->MaybeImportForm(std::move(eligible_form_structure),
                            import_form_callback.Get());
  if (ShouldExtractAXTree()) {
    std::move(axtree_received_callback).Run({});
  }
  EXPECT_THAT(user_annotations_entries.empty(), !std::get<0>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillPredictionImprovementsManagerImportFormTest,
    testing::Combine(/*should_import_form_data=*/testing::Bool(),
                     /*extract_ax_tree=*/testing::Bool()));

// Tests that if the pref is disabled, `import_form_callback` is run with an
// empty list of entries and nothing is forwarded to the
// `user_annotations_service_`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       FormNotImportedWhenPrefDisabled) {
  user_annotations_service_.AddHostToFormAnnotationsAllowlist(url_.host());
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_data = autofill::test::GetFormData(form_description);
  std::unique_ptr<autofill::FormStructure> eligible_form_structure =
      std::make_unique<autofill::FormStructure>(form_data);

  test_api(*eligible_form_structure)
      .PushField()
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      .set_heuristic_type(
          autofill::HeuristicSource::kPredictionImprovementRegexes,
          autofill::IMPROVED_PREDICTION);
#else
      .set_heuristic_type(autofill::GetActiveHeuristicSource(),
                          autofill::IMPROVED_PREDICTION);
#endif
  base::MockCallback<user_annotations::ImportFormCallback> import_form_callback;
  user_annotations_service_.SetShouldImportFormData(
      /*should_import_form_data=*/true);

  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      user_annotations_entries;
  EXPECT_CALL(import_form_callback, Run)
      .WillOnce(SaveArg<1>(&user_annotations_entries));
  EXPECT_CALL(client_, GetAXTree).Times(0);
  EXPECT_CALL(client_, IsAutofillPredictionImprovementsEnabledPref)
      .WillOnce(Return(false));
  manager_->MaybeImportForm(std::move(eligible_form_structure),
                            import_form_callback.Get());
  EXPECT_TRUE(user_annotations_entries.empty());
}

// Tests that `import_form_callback` is run with an empty list of entries when
// `user_annotations::ShouldAddFormSubmissionForURL()` returns `false`.
TEST_F(AutofillPredictionImprovementsManagerTest,
       MaybeImportFormRunsCallbackWithFalseWhenImportIsNotAttempted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kAutofillPredictionImprovements,
      {{"allowed_hosts_for_form_submissions", "otherhost.com"}});
  base::MockCallback<user_annotations::ImportFormCallback> import_form_callback;

  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      user_annotations_entries;
  EXPECT_CALL(import_form_callback, Run)
      .WillOnce(SaveArg<1>(&user_annotations_entries));
  manager_->MaybeImportForm(
      std::make_unique<autofill::FormStructure>(autofill::FormData()),
      import_form_callback.Get());
  EXPECT_TRUE(user_annotations_entries.empty());
}

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(true)` if there's data stored in the user annotations.
TEST_F(AutofillPredictionImprovementsManagerTest,
       HasDataStoredReturnsTrueIfDataIsStored) {
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::HasDataCallback>
      has_data_callback;
  user_annotations_service_.ReplaceAllEntries(
      {optimization_guide::proto::UserAnnotationsEntry()});
  manager_->HasDataStored(has_data_callback.Get());
  EXPECT_CALL(
      has_data_callback,
      Run(autofill::AutofillPredictionImprovementsDelegate::HasData(true)));
  manager_->HasDataStored(has_data_callback.Get());
}

// Tests that the callback passed to `HasDataStored()` is called with
// `HasData(false)` if there's no data stored in the user annotations.
TEST_F(AutofillPredictionImprovementsManagerTest,
       HasDataStoredReturnsFalseIfDataIsNotStored) {
  base::MockCallback<
      autofill::AutofillPredictionImprovementsDelegate::HasDataCallback>
      has_data_callback;
  user_annotations_service_.ReplaceAllEntries({});
  manager_->HasDataStored(has_data_callback.Get());
  EXPECT_CALL(
      has_data_callback,
      Run(autofill::AutofillPredictionImprovementsDelegate::HasData(false)));
  manager_->HasDataStored(has_data_callback.Get());
}

// Tests that the prediction improvements settings page is opened when the
// manage prediction improvements link is clicked.
TEST_F(AutofillPredictionImprovementsManagerTest,
       OpenSettingsWhenManagePILinkIsClicked) {
  EXPECT_CALL(client_, OpenPredictionImprovementsSettings);
  manager_->UserClickedLearnMore();
}

// Tests that calling `OnLoadingSuggestionShown()` is a no-op if the
// `kTriggerAutomatically` parameter is disabled.
TEST_F(AutofillPredictionImprovementsManagerTest,
       OnLoadingSuggestionShownDoesNothingIfParamNotEnabled) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  EXPECT_CALL(update_suggestions_callback, Run).Times(0);
  EXPECT_CALL(client_, GetAXTree).Times(0);
  manager_->OnSuggestionsShown(
      {autofill::SuggestionType::kPredictionImprovementsLoadingState}, form,
      form.fields().front(), update_suggestions_callback.Get());
}

// Tests that the regular Autofill flow continues if predictions are being
// retrieved for form A, while a field of form B is focused.
TEST_F(AutofillPredictionImprovementsManagerTest,
       GetSuggestionsReturnsEmptyVectorIfRequestedFromNewFormWhileLoading) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_a = autofill::test::GetFormData(form_description);
  manager_->OnClickedTriggerSuggestion(form_a, form_a.fields().front(),
                                       base::DoNothing());

  autofill::FormData form_b = autofill::test::GetFormData(form_description);

  EXPECT_TRUE(manager_
                  ->GetSuggestions(/*autofill_suggestions=*/{}, form_b,
                                   form_b.fields().front())
                  .empty());
}

// Tests that the trigger suggestion is shown if predictions were retrieved for
// form A and now a field of form B is focused.
TEST_F(
    AutofillPredictionImprovementsManagerTest,
    GetSuggestionsReturnsTriggerSuggestionIfRequestedFromNewFormAndNotLoading) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST,
                  .heuristic_type = autofill::NAME_FIRST,
                  .label = u"First Name",
                  .value = u"Jane"}}};
  autofill::FormData form_a = autofill::test::GetFormData(form_description);
  test_api(*manager_).SetLastQueriedFormGlobalId(form_a.global_id());

  autofill::FormData form_b = autofill::test::GetFormData(form_description);

  const std::vector<autofill::Suggestion> suggestions =
      manager_->GetSuggestions(/*autofill_suggestions=*/{}, form_b,
                               form_b.fields().front());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_THAT(suggestions[0],
              HasType(SuggestionType::kRetrievePredictionImprovements));
}

TEST_F(AutofillPredictionImprovementsManagerTest,
       ShouldSkipAutofillSuggestion) {
  autofill::Suggestion autofill_suggestion =
      Suggestion(SuggestionType::kAddressEntry);
  autofill_suggestion.payload = autofill::Suggestion::Guid("guid");
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::FieldType::NAME_FIRST},
                 {.role = autofill::FieldType::NAME_LAST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  autofill::FormStructure form_structure{form};
  test_api(form_structure)
      .SetFieldTypes(
          {autofill::FieldType::NAME_FIRST, autofill::FieldType::NAME_LAST});
  test_api(*manager_).SetCache(PredictionsByGlobalId{
      {form.fields()[0].global_id(), {u"Jane", u"First Name"}},
      {form.fields()[1].global_id(), {u"Doe", u"Last Name"}}});
  EXPECT_CALL(client_, GetCachedFormStructure)
      .WillRepeatedly(Return(&form_structure));
  EXPECT_CALL(client_,
              GetAutofillFillingValue(_, autofill::FieldType::NAME_FIRST, _))
      .WillOnce(Return(u"j ǎ Ņ ë"));
  EXPECT_CALL(client_,
              GetAutofillFillingValue(_, autofill::FieldType::NAME_LAST, _))
      .WillOnce(Return(u"  d o Ê"));
  EXPECT_TRUE(test_api(*manager_).ShouldSkipAutofillSuggestion(
      form, autofill_suggestion));
}

class AutofillPredictionImprovementsManagerTriggerAutomaticallyTest
    : public BaseAutofillPredictionImprovementsManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillPredictionImprovementsManagerTriggerAutomaticallyTest() {
    feature_.InitAndEnableFeatureWithParameters(
        kAutofillPredictionImprovements,
        {{"skip_allowlist", "true"},
         {"trigger_automatically", "true"},
         {"extract_ax_tree_for_predictions", GetParam() ? "true" : "false"}});
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
    ON_CALL(client_, GetFillingEngine).WillByDefault(Return(&filling_engine_));
    manager_ = std::make_unique<AutofillPredictionImprovementsManager>(
        &client_, &decider_, &strike_database_);
  }
};

// Tests that calling `OnLoadingSuggestionShown()` results in retrieving the AX
// tree (implying predictions will be attempted to be retrieved) if the
// `kTriggerAutomatically` parameter is enabled.
TEST_P(AutofillPredictionImprovementsManagerTriggerAutomaticallyTest,
       OnLoadingSuggestionShownGetsAXTreeIfParamEnabled) {
  autofill::test::FormDescription form_description = {
      .fields = {{.role = autofill::NAME_FIRST}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);
  base::MockCallback<autofill::AutofillPredictionImprovementsDelegate::
                         UpdateSuggestionsCallback>
      update_suggestions_callback;
  if (GetParam()) {
    EXPECT_CALL(client_, GetAXTree);
  }
  manager_->OnSuggestionsShown(
      {autofill::SuggestionType::kPredictionImprovementsLoadingState}, form,
      form.fields().front(), update_suggestions_callback.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillPredictionImprovementsManagerTriggerAutomaticallyTest,
    testing::Bool());

// Tests that the loading suggestion is returned by `GetSuggestions()` when the
// class is in `kReady` state.
TEST_P(AutofillPredictionImprovementsManagerTriggerAutomaticallyTest,
       GetSuggestions_Ready_ReturnsLoadingSuggestion) {
  autofill::FormData form;
  autofill::FormFieldData field;
  test_api(*manager_).SetPredictionRetrievalState(
      AutofillPredictionImprovementsManager::PredictionRetrievalState::kReady);
  EXPECT_THAT(manager_->GetSuggestions({}, form, field),
              ElementsAre(HasType(
                  SuggestionType::kPredictionImprovementsLoadingState)));
}

class IsFormAndFieldEligibleAutofillPredictionImprovementsTest
    : public BaseAutofillPredictionImprovementsManagerTest {
 public:
  IsFormAndFieldEligibleAutofillPredictionImprovementsTest() {
    ON_CALL(client_, GetLastCommittedURL).WillByDefault(ReturnRef(url_));
    autofill::test::FormDescription form_description = {
        .fields = {{.role = autofill::NAME_FIRST,
                    .heuristic_type = autofill::NAME_FIRST}}};
    form_ = autofill::test::GetFormData(form_description);
  }

  std::unique_ptr<autofill::FormStructure> CreateEligibleForm(
      const GURL& url = GURL("https://example.com")) {
    autofill::FormData form_data;
    form_data.set_main_frame_origin(url::Origin::Create(url));
    auto form = std::make_unique<autofill::FormStructure>(form_data);
    autofill::AutofillField& prediction_improvement_field =
        test_api(*form).PushField();
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    prediction_improvement_field.set_heuristic_type(
        autofill::HeuristicSource::kPredictionImprovementRegexes,
        autofill::IMPROVED_PREDICTION);
#else
    prediction_improvement_field.set_heuristic_type(
        autofill::HeuristicSource::kLegacyRegexes,
        autofill::IMPROVED_PREDICTION);
#endif
    return form;
  }

 protected:
  autofill::FormData form_;
};

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleIfFlagDisabled) {
  feature_.InitAndDisableFeature(kAutofillPredictionImprovements);
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleIfDeciderIsNull) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, nullptr,
                                                &strike_database_};
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsEligibleIfSkipAllowlistIsTrue) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_TRUE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleIfPrefIsDisabled) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  EXPECT_CALL(client_, IsAutofillPredictionImprovementsEnabledPref)
      .WillOnce(Return(false));

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleIfOptimizationGuideCannotBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsEligibleIfOptimizationGuideCanBeApplied) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};
  ON_CALL(decider_, CanApplyOptimization(_, _, nullptr))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_TRUE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleForNotHttps) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "false"}});
  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  std::unique_ptr<autofill::FormStructure> form =
      CreateEligibleForm(GURL("http://http.com"));
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleOnEmptyForm) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});

  autofill::FormData form_data;
  autofill::FormStructure form(form_data);
  autofill::AutofillField field;

  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(form, field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       PredictionImprovementsEligibility_Eligible) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  EXPECT_TRUE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

TEST_F(IsFormAndFieldEligibleAutofillPredictionImprovementsTest,
       IsNotEligibleForNonEligibleUser) {
  feature_.InitAndEnableFeatureWithParameters(kAutofillPredictionImprovements,
                                              {{"skip_allowlist", "true"}});

  std::unique_ptr<autofill::FormStructure> form = CreateEligibleForm();
  autofill::AutofillField* prediction_improvement_field = form->field(0);

  AutofillPredictionImprovementsManager manager{&client_, &decider_,
                                                &strike_database_};

  ON_CALL(client_, IsUserEligible).WillByDefault(Return(false));
  EXPECT_FALSE(manager.IsPredictionImprovementsEligible(
      *form, *prediction_improvement_field));
}

}  // namespace
}  // namespace autofill_prediction_improvements

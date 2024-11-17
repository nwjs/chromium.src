// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_suggestion_generator.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {
namespace {

using autofill::AutofillSuggestionTriggerSource;
using autofill::EqualsSuggestion;
using autofill::FieldGlobalId;
using autofill::FormData;
using autofill::PasswordFormClassification;
using autofill::Suggestion;
using autofill::SuggestionType;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::SizeIs;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
auto IsCreateInlineSuggestion(
    std::optional<std::u16string> suggested_plus_address) {
  const bool is_loading = !suggested_plus_address.has_value();
  Suggestion::PlusAddressPayload payload(std::move(suggested_plus_address));
  payload.offer_refresh = !is_loading;
  return AllOf(
      EqualsSuggestion(SuggestionType::kCreateNewPlusAddressInline),
      Property(&Suggestion::GetPayload<Suggestion::PlusAddressPayload>,
               std::move(payload)),
      Field(&Suggestion::is_loading, Suggestion::IsLoading(is_loading)));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Returns a sample sign-up form.
FormData CreateSignupForm() {
  FormData form = autofill::test::CreateTestPasswordFormData();
  std::vector<autofill::FormFieldData> fields = form.ExtractFields();
  fields.push_back(autofill::test::CreateTestFormField(
      /*label=*/"Password (confirm)", /*name=*/"password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  form.set_fields(std::move(fields));
  return form;
}

// Returns `form` with a non-null host form id and frame token.
FormData SetGeneratedFrameTokenAndHostFormId(FormData form) {
  // Ensure that the form is not unowned.
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  std::vector<autofill::FormFieldData> fields = form.ExtractFields();
  for (autofill::FormFieldData& field : fields) {
    field.set_host_form_id(form.renderer_id());
  }
  form.set_fields(std::move(fields));

  // Set the same non-zero host frame for all fields.
  return autofill::test::CreateFormDataForFrame(
      std::move(form), autofill::test::MakeLocalFrameToken());
}

class FakePlusAddressAllocator : public PlusAddressAllocator {
 public:
  FakePlusAddressAllocator() = default;

  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override {
    std::move(callback).Run(profile_or_error_);
  }

  std::optional<PlusProfile> AllocatePlusAddressSynchronously(
      const url::Origin& origin,
      AllocationMode mode) override {
    if (!is_next_allocation_synchronous_ || !profile_or_error_.has_value()) {
      return std::nullopt;
    }
    return profile_or_error_.value();
  }

  bool IsRefreshingSupported(const url::Origin& origin) const override {
    return true;
  }

  void RemoveAllocatedPlusAddress(const PlusAddress& plus_address) override {}

  void set_is_next_allocation_synchronous(bool is_next_allocation_synchronous) {
    is_next_allocation_synchronous_ = is_next_allocation_synchronous;
  }

  void set_profile_or_error(PlusProfileOrError profile_or_error) {
    profile_or_error_ = std::move(profile_or_error);
  }

 private:
  bool is_next_allocation_synchronous_ = false;
  PlusProfileOrError profile_or_error_ = test::CreatePlusProfile();
};

class PlusAddressSuggestionGeneratorTest : public ::testing::Test {
 public:
  PlusAddressSuggestionGeneratorTest() = default;

  const std::string kPrimaryEmail = "foo@gmail.com";

 protected:
  FakePlusAddressAllocator& allocator() { return allocator_; }
  FakePlusAddressSettingService& setting_service() { return setting_service_; }

 private:
  base::test::ScopedFeatureList features_{
      features::kPlusAddressUserOnboardingEnabled};
  autofill::test::AutofillUnitTestEnvironment autofill_env_;

  FakePlusAddressAllocator allocator_;
  FakePlusAddressSettingService setting_service_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests that an empty PlusAddressPayload is set if there are no cached plus
// addresses.
TEST_F(PlusAddressSuggestionGeneratorTest,
       InlineGenerationWithoutPreallocatedAddresses) {
  base::test::ScopedFeatureList inline_creation_feature(
      features::kPlusAddressInlineCreation);

  allocator().set_is_next_allocation_synchronous(false);
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);
  FormData form = CreateSignupForm();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(IsCreateInlineSuggestion(
          /*suggested_plus_address=*/std::nullopt)));
}

// Tests that if there are cached plus addresses available, then one is set is
// the PlusAddressPayload.
TEST_F(PlusAddressSuggestionGeneratorTest,
       InlineGenerationWithPreallocatedAddresses) {
  base::test::ScopedFeatureList inline_creation_feature(
      features::kPlusAddressInlineCreation);

  allocator().set_is_next_allocation_synchronous(true);
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);
  FormData form = CreateSignupForm();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(IsCreateInlineSuggestion(
          /*suggested_plus_address=*/base::UTF8ToUTF16(
              *test::CreatePlusProfile().plus_address))));
}

TEST_F(PlusAddressSuggestionGeneratorTest,
       SetSuggestedPlusAddressForSuggestion) {
  const PlusAddress plus_address("plus@foo.com");
  Suggestion suggestion(SuggestionType::kCreateNewPlusAddressInline);
  suggestion.payload = Suggestion::PlusAddressPayload();
  suggestion.is_loading = Suggestion::IsLoading(true);
  PlusAddressSuggestionGenerator::SetSuggestedPlusAddressForSuggestion(
      plus_address, suggestion);

  EXPECT_FALSE(suggestion.is_loading);
  EXPECT_EQ(suggestion.GetPayload<Suggestion::PlusAddressPayload>().address,
            base::UTF8ToUTF16(*plus_address));
}

TEST_F(PlusAddressSuggestionGeneratorTest, GetPlusAddressErrorSuggestion) {
  const Suggestion suggestion(
      PlusAddressSuggestionGenerator::GetPlusAddressErrorSuggestion(
          PlusAddressRequestError::AsNetworkError(net::HTTP_BAD_REQUEST)));
  EXPECT_EQ(suggestion.type, SuggestionType::kPlusAddressError);
  EXPECT_EQ(
      suggestion.main_text.value,
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kError);
  EXPECT_TRUE(
      suggestion.GetPayload<Suggestion::PlusAddressPayload>().offer_refresh);
  EXPECT_THAT(
      suggestion.labels,
      ElementsAre(ElementsAre(Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_RESERVE_GENERIC_ERROR_TEXT)))));
}

TEST_F(PlusAddressSuggestionGeneratorTest,
       GetPlusAddressErrorSuggestionForQuotaError) {
  const auto error =
      PlusAddressRequestError::AsNetworkError(net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_TRUE(error.IsQuotaError());

  const Suggestion suggestion(
      PlusAddressSuggestionGenerator::GetPlusAddressErrorSuggestion(error));
  EXPECT_EQ(suggestion.type, SuggestionType::kPlusAddressError);
  EXPECT_EQ(
      suggestion.main_text.value,
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT));
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kError);
  EXPECT_FALSE(
      suggestion.GetPayload<Suggestion::PlusAddressPayload>().offer_refresh);
  EXPECT_THAT(
      suggestion.labels,
      ElementsAre(ElementsAre(Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_RESERVE_QUOTA_ERROR_TEXT)))));
}

// Tests that suggestions in the `is_loading` state do not have a refresh
// button and is not acceptable.
TEST_F(PlusAddressSuggestionGeneratorTest, LoadingStateProperties) {
  Suggestion inline_suggestion(SuggestionType::kCreateNewPlusAddressInline);
  inline_suggestion.payload = Suggestion::PlusAddressPayload();

  PlusAddressSuggestionGenerator::SetLoadingStateForSuggestion(
      /*is_loading=*/true, inline_suggestion);
  EXPECT_TRUE(inline_suggestion.is_loading);
  EXPECT_FALSE(inline_suggestion.is_acceptable);
  EXPECT_FALSE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                   .offer_refresh);

  PlusAddressSuggestionGenerator::SetSuggestedPlusAddressForSuggestion(
      PlusAddress("foo@moo.com"), inline_suggestion);
  EXPECT_FALSE(inline_suggestion.is_loading);
  EXPECT_TRUE(inline_suggestion.GetPayload<Suggestion::PlusAddressPayload>()
                  .offer_refresh);
  EXPECT_TRUE(inline_suggestion.is_acceptable);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that the creation suggestion contains no labels if the notice has not
// been accepted.
TEST_F(PlusAddressSuggestionGeneratorTest, FirstTimeCreateSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressSuggestionRedesign};
  setting_service().set_has_accepted_notice(false);

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);
  FormData form = CreateSignupForm();
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
          PasswordFormClassification(), form.fields()[0].global_id(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(AllOf(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress),
                        Field(&Suggestion::labels, IsEmpty()))));
}

// Tests that no creation suggestion is shown on a login form.
TEST_F(PlusAddressSuggestionGeneratorTest, NoSuggestionsOnLoginForm) {
  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);
  const FormData login_form = SetGeneratedFrameTokenAndHostFormId(
      autofill::test::CreateTestPasswordFormData());
  ASSERT_THAT(login_form.fields(), SizeIs(2));
  const FieldGlobalId focused_field_id = login_form.fields()[0].global_id();
  const base::flat_map<FieldGlobalId, autofill::FieldTypeGroup>
      form_field_type_groups = {
          {focused_field_id, autofill::FieldTypeGroup::kUsernameField},
          {login_form.fields()[1].global_id(),
           autofill::FieldTypeGroup::kPasswordField}};
  PasswordFormClassification classification;
  classification.type = PasswordFormClassification::Type::kLoginForm;
  classification.username_field = focused_field_id;
  EXPECT_THAT(generator.GetSuggestions(
                  /*affiliated_plus_addresses=*/{},
                  /*is_creation_enabled=*/true, login_form,
                  form_field_type_groups, classification, focused_field_id,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked),
              IsEmpty());
}

// Tests that creation is offered on forms classified by PWM as login forms if
// they have name or address fields included.
TEST_F(PlusAddressSuggestionGeneratorTest,
       SuggestionsOnLoginFormWithNameFields) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressRefinedPasswordFormClassification};

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);
  FormData form = autofill::test::CreateTestPasswordFormData();
  {
    std::vector<autofill::FormFieldData> fields = form.ExtractFields();
    fields.push_back(autofill::test::CreateTestFormField(
        /*label=*/"First name", /*name=*/"first_name",
        /*value=*/"", autofill::FormControlType::kInputText));
    form.set_fields(std::move(fields));
  }
  form = SetGeneratedFrameTokenAndHostFormId(std::move(form));
  ASSERT_THAT(form.fields(), SizeIs(3));
  const FieldGlobalId focused_field_id = form.fields()[0].global_id();
  const base::flat_map<FieldGlobalId, autofill::FieldTypeGroup>
      form_field_type_groups = {
          {focused_field_id, autofill::FieldTypeGroup::kUsernameField},
          {form.fields()[1].global_id(),
           autofill::FieldTypeGroup::kPasswordField},
          {form.fields()[2].global_id(), autofill::FieldTypeGroup::kName}};
  PasswordFormClassification classification;
  classification.type = PasswordFormClassification::Type::kLoginForm;
  classification.username_field = focused_field_id;
  EXPECT_THAT(
      generator.GetSuggestions(
          /*affiliated_plus_addresses=*/{},
          /*is_creation_enabled=*/true, form, form_field_type_groups,
          classification, focused_field_id,
          AutofillSuggestionTriggerSource::kFormControlElementClicked),
      ElementsAre(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress)));
}

// Tests properties of the label for suggestions for 2nd (and subsequent)
// create.
// - On Android, there should be no label.
// - On iOS, the label should not contain the primary email.
// - On Desktop, the label should contain the primary email.
TEST_F(PlusAddressSuggestionGeneratorTest, ProfileInLabel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPlusAddressSuggestionRedesign,
      {{"show-forwarding-email", "true"}});
  setting_service().set_has_accepted_notice(true);

  PlusAddressSuggestionGenerator generator(
      &setting_service(), &allocator(),
      url::Origin::Create(GURL("https://foo.bar")), kPrimaryEmail);

  FormData form = CreateSignupForm();
  std::vector<Suggestion> suggestions = generator.GetSuggestions(
      /*affiliated_plus_addresses=*/{},
      /*is_creation_enabled=*/true, form, /*form_field_type_groups=*/{},
      PasswordFormClassification(), form.fields()[0].global_id(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  ASSERT_EQ(suggestions.size(), 1u);

  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_THAT(suggestions[0].labels, IsEmpty());
    return;
  }

  ASSERT_EQ(suggestions[0].labels.size(), 1u);
  ASSERT_EQ(suggestions[0].labels[0].size(), 1u);

  const bool is_email_in_label =
      suggestions[0].labels[0][0].value.find(
          base::UTF8ToUTF16(kPrimaryEmail)) != std::u16string::npos;
  if constexpr (BUILDFLAG(IS_IOS)) {
    EXPECT_FALSE(is_email_in_label);
  } else {
    EXPECT_TRUE(is_email_in_label);
  }
}

}  // namespace
}  // namespace plus_addresses

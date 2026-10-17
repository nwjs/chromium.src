// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_
#define COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_

#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using PageClassification = metrics::OmniboxEventProto::PageClassification;

// This class implements a special version of AutocompleteMatch::MoreRelevant
// that allows matches of particular types to be demoted in AutocompleteResult.
template <class Match>
class CompareWithDemoteByType {
 public:
  CompareWithDemoteByType(PageClassification page_classification) {
    // This rule demotes URLs as strongly as possible without violating user
    // expectations. In particular, for URL-seeking inputs, if the user would
    // likely expect a URL first (i.e., it would be inline autocompleted), then
    // that URL will still score strongly enough to be first.  This is done
    // using a demotion multiple of 0.61.  If a URL would get a score high
    // enough to be inline autocompleted (1400+), even after demotion it will
    // score above 850 ( 1400 * 0.61 > 850).  850 is the maximum score for
    // queries when the input has been detected as URL-seeking.
#if BUILDFLAG(IS_ANDROID)
    if (page_classification ==
        metrics::OmniboxEventProto::
            SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT) {
      demotions_ = {
          {omnibox::AutocompleteMatchType::kHistoryUrl, 0.61f},
          {omnibox::AutocompleteMatchType::kHistoryTitle, 0.61f},
          {omnibox::AutocompleteMatchType::kHistoryBody, 0.61f},
          {omnibox::AutocompleteMatchType::kHistoryKeyword, 0.61f},
          {omnibox::AutocompleteMatchType::kBookmarkTitle, 0.61f},
          {omnibox::AutocompleteMatchType::kDocumentSuggestion, 0.61f}};
    }
#endif
    omnibox::CheckObsoletePageClass(page_classification);

    if (page_classification == metrics::OmniboxEventProto::NTP_REALBOX) {
      demotions_ = {
          {omnibox::AutocompleteMatchType::kHistoryUrl, 0.1f},
          {omnibox::AutocompleteMatchType::kHistoryTitle, 0.1f},
          {omnibox::AutocompleteMatchType::kHistoryBody, 0.1f},
          {omnibox::AutocompleteMatchType::kHistoryKeyword, 0.1f},
          {omnibox::AutocompleteMatchType::kNavsuggest, 0.1f},
          {omnibox::AutocompleteMatchType::kBookmarkTitle, 0.1f},
          {omnibox::AutocompleteMatchType::kNavsuggestPersonalized, 0.1f},
          {omnibox::AutocompleteMatchType::kDocumentSuggestion, 0.1f},
          {omnibox::AutocompleteMatchType::kStarterPack, 0.0f}};
    }
  }

  // Returns the relevance score of |match| demoted appropriately by
  // |demotions_by_type_|.
  int GetDemotedRelevance(const Match& match) const {
    auto demotion_it = demotions_.find(match.type);

    // Add a check here to ensure we don't demote `NAVSUGGEST` matches created
    // by the `EnterpriseSearchAggregatorProvider`. This allows users to see
    // `EnterpriseSearchAggregatorProvider` suggestions in the realbox.
    // TODO(crbug.com/419303069): If additional requirements for demotion
    //   specific to a provider emerges, we should refactor
    //   `CompareWithDemoteByType` to `CompareWithDemoteByProviderAndType`.
    return (demotion_it == demotions_.end() ||
            (match.type == omnibox::AutocompleteMatchType::kNavsuggest &&
             match.provider->type() ==
                 AutocompleteProvider::Type::kEnterpriseSearchAggregator))
               ? match.relevance
               : (match.relevance * demotion_it->second);
  }

  // Comparison function.
  bool operator()(const Match& elem1, const Match& elem2) {
    // Compute demoted relevance scores for each match.
    const int demoted_relevance1 = GetDemotedRelevance(elem1);
    const int demoted_relevance2 = GetDemotedRelevance(elem2);
    if (demoted_relevance1 != demoted_relevance2) {
      // Greater relevance should come first.
      return demoted_relevance1 > demoted_relevance2;
    }
    // For equal-relevance matches, we sort alphabetically, so that providers
    // who return multiple elements at the same priority get a "stable" sort
    // across multiple updates.
    return elem1.contents < elem2.contents;
  }

 private:
  OmniboxFieldTrial::DemotionMultipliers demotions_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MATCH_COMPARE_H_

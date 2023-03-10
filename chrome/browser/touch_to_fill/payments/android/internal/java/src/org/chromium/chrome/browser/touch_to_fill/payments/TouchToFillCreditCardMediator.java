// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.CreditCardProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.HeaderProperties;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/**
 * Contains the logic for the TouchToFillCreditCard component. It sets the state of the model and
 * reacts to events like clicks.
 */
class TouchToFillCreditCardMediator {
    /**
     * The final outcome that closes the Touch To Fill sheet.
     *
     * Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with TouchToFill.CreditCard.Outcome in enums.xml.
     */
    @IntDef({TouchToFillCreditCardOutcome.CREDIT_CARD, TouchToFillCreditCardOutcome.VIRTUAL_CARD,
            TouchToFillCreditCardOutcome.MANAGE_PAYMENTS,
            TouchToFillCreditCardOutcome.SCAN_NEW_CARD, TouchToFillCreditCardOutcome.DISMISS})
    @Retention(RetentionPolicy.SOURCE)
    @interface TouchToFillCreditCardOutcome {
        int CREDIT_CARD = 0;
        int VIRTUAL_CARD = 1;
        int MANAGE_PAYMENTS = 2;
        int SCAN_NEW_CARD = 3;
        int DISMISS = 4;
        int MAX_VALUE = DISMISS;
    }
    @VisibleForTesting
    static final String TOUCH_TO_FILL_OUTCOME_HISTOGRAM = "Autofill.TouchToFill.CreditCard.Outcome";
    @VisibleForTesting
    static final String TOUCH_TO_FILL_INDEX_SELECTED =
            "Autofill.TouchToFill.CreditCard.SelectedIndex";
    @VisibleForTesting
    static final String TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN =
            "Autofill.TouchToFill.CreditCard.NumberOfCardsShown";

    // TODO(crbug/1383487): Remove the Context from the Mediator.
    private Context mContext;
    private TouchToFillCreditCardComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private List<CreditCard> mCards;

    void initialize(Context context, TouchToFillCreditCardComponent.Delegate delegate,
            PropertyModel model) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mModel = model;
    }

    void showSheet(CreditCard[] cards, boolean shouldShowScanCreditCard) {
        assert cards != null;
        mCards = Arrays.asList(cards);

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (CreditCard card : cards) {
            final PropertyModel model = createCardModel(card);
            sheetItems.add(new ListItem(CREDIT_CARD, model));
        }

        if (cards.length == 1) {
            // Use the credit card model as the property model for the fill button too
            assert sheetItems.get(0).type == CREDIT_CARD;
            sheetItems.add(new ListItem(FILL_BUTTON, sheetItems.get(0).model));
        }

        sheetItems.add(0, buildHeader(hasOnlyLocalCards(cards)));

        mModel.set(VISIBLE, true);
        mModel.set(SHOULD_SHOW_SCAN_CREDIT_CARD, shouldShowScanCreditCard);

        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, cards.length);
    }

    void hideSheet() {
        onDismissed(BottomSheetController.StateChangeReason.NONE);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        boolean dismissedByUser =
                reason == StateChangeReason.SWIPE || reason == StateChangeReason.BACK_PRESS;
        mDelegate.onDismissed(dismissedByUser);
        if (dismissedByUser) {
            RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                    TouchToFillCreditCardOutcome.DISMISS,
                    TouchToFillCreditCardOutcome.MAX_VALUE + 1);
        }
    }

    public void scanCreditCard() {
        mDelegate.scanCreditCard();
        RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                TouchToFillCreditCardOutcome.SCAN_NEW_CARD,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
    }

    public void showCreditCardSettings() {
        mDelegate.showCreditCardSettings();
        RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                TouchToFillCreditCardOutcome.MANAGE_PAYMENTS,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
    }

    public void onSelectedCreditCard(CreditCard card) {
        mDelegate.suggestionSelected(card.getGUID());
        RecordHistogram.recordEnumeratedHistogram(TOUCH_TO_FILL_OUTCOME_HISTOGRAM,
                TouchToFillCreditCardOutcome.CREDIT_CARD,
                TouchToFillCreditCardOutcome.MAX_VALUE + 1);
        RecordHistogram.recordCount100Histogram(TOUCH_TO_FILL_INDEX_SELECTED, mCards.indexOf(card));
    }

    private PropertyModel createCardModel(CreditCard card) {
        return new PropertyModel
                .Builder(TouchToFillCreditCardProperties.CreditCardProperties.ALL_KEYS)
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_ICON_ID,
                        card.getIssuerIconDrawableId())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME,
                        card.getCardNameForAutofillDisplay())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER,
                        card.getObfuscatedLastFourDigits())
                .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_EXPIRATION,
                        mContext.getString(
                                        R.string.autofill_credit_card_two_line_label_from_card_number)
                                .replace("$1", card.getFormattedExpirationDate(mContext)))
                .with(ON_CLICK_ACTION, () -> { this.onSelectedCreditCard(card); })
                .build();
    }

    private ListItem buildHeader(boolean hasOnlyLocalCards) {
        return new ListItem(TouchToFillCreditCardProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(IMAGE_DRAWABLE_ID,
                                hasOnlyLocalCards ? R.drawable.fre_product_logo
                                                  : R.drawable.google_pay)
                        .build());
    }

    private static boolean hasOnlyLocalCards(CreditCard[] cards) {
        for (CreditCard card : cards) {
            if (!card.getIsLocal()) return false;
        }
        return true;
    }
}

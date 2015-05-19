/*
 * Copyright 2015 The Iridium Authors
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "iridium/trkbar.h"

namespace content {

void TrkBar::Create(InfoBarService *s, const GURL &url)
{
	s->AddInfoBar(s->CreateConfirmInfoBar(
		std::unique_ptr<ConfirmInfoBarDelegate>(new TrkBar(url))
	));
}

bool TrkBar::ShouldExpire(const NavigationDetails &) const
{
	return false;
}

TrkBar::TrkBar(const GURL &url) :
	ConfirmInfoBarDelegate(), m_url(url)
{
}

base::string16 TrkBar::GetMessageText(void) const
{
	return base::ASCIIToUTF16("Loading traced URL: " + m_url.spec());
}

int TrkBar::GetButtons(void) const
{
	return BUTTON_NONE;
}

infobars::InfoBarDelegate::InfoBarIdentifier TrkBar::GetIdentifier(void) const
{
	return TRACKING_ALERT_INFOBAR_DELEGATE;
}

}; /* namespace content */

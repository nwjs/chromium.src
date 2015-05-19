/*
 * Copyright 2015 The Iridium Authors.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef IRIDIUM_TRKBAR_H
#define IRIDIUM_TRKBAR_H 1

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {

class TrkBar : public ConfirmInfoBarDelegate {
	public:
	static void Create(InfoBarService *, const GURL &);
	bool ShouldExpire(const NavigationDetails &) const override;

	private:
	TrkBar(const GURL &);
	base::string16 GetMessageText(void) const override;
	int GetButtons(void) const override;
	infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier(void) const override;

	GURL m_url;
	DISALLOW_COPY_AND_ASSIGN(TrkBar);
};

}; /* namespace content */

#endif /* IRIDIUM_TRKBAR_H */

/*
 * Copyright 2015 The Iridium Authors.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <cstdio>
#ifdef __linux__
#	include <unistd.h>
#endif
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "url/url_constants.h"
#include "iridium/trkbar.h"
#include "iridium/trknotify.h"

namespace iridium {

void log_url_request(const std::string &caller, const GURL &url)
{
#ifdef __linux__
	bool tty = isatty(fileno(stderr));
#else
	bool tty = false;
#endif
	const char *xred   = tty ? "\033[1;37;41m" : ""; // ]
	const char *xfruit = tty ? "\033[33m"      : ""; // ]
	const char *xdark  = tty ? "\033[1;30m"    : ""; // ]
	const char *xreset = tty ? "\033[0m"       : ""; // ]

	if (url.scheme() == url::kTraceScheme)
		fprintf(stderr, "%s*** %s(%s)%s\n", xred, caller.c_str(),
		        url.possibly_invalid_spec().c_str(), xreset);
	else
		fprintf(stderr, "%s***%s %s(%s)%s\n", xfruit, xdark,
		        caller.c_str(), url.possibly_invalid_spec().c_str(),
		        xreset);
}

static void __trace_url_request(const std::string &caller, const GURL &url)
{
	auto browser = chrome::FindLastActive();
	if (browser == NULL)
		return;

	auto service = InfoBarService::FromWebContents(browser->tab_strip_model()->GetActiveWebContents());
	content::TrkBar::Create(service, url);
}

void trace_url_request(const std::string &caller, const GURL &url)
{
	content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
		base::Bind(&__trace_url_request, caller, url));
}

}; /* namespace iridium */

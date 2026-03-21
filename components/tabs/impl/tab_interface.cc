// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_interface.h"

#include "content/public/browser/web_contents_user_data.h"

namespace tabs {

TabLookupFromWebContents::TabLookupFromWebContents(
    content::WebContents* contents,
    tabs::TabInterface* tab_interface)
    : content::WebContentsUserData<TabLookupFromWebContents>(*contents),
      tab_interface_(tab_interface) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLookupFromWebContents);

// static
TabInterface* TabInterface::GetFromContents(
    content::WebContents* web_contents) {
  if (TabLookupFromWebContents::FromWebContents(web_contents))
    return TabLookupFromWebContents::FromWebContents(web_contents)->model();
  else
    return nullptr;
}

// static
const TabInterface* TabInterface::GetFromContents(
    const content::WebContents* web_contents) {
  if (TabLookupFromWebContents::FromWebContents(web_contents))
  return TabLookupFromWebContents::FromWebContents(web_contents)->model();
  return nullptr;
}

// static
TabInterface* TabInterface::MaybeGetFromContents(
    content::WebContents* web_contents) {
  TabLookupFromWebContents* lookup =
      TabLookupFromWebContents::FromWebContents(web_contents);
  if (!lookup) {
    return nullptr;
  }
  return lookup->model();
}

}  // namespace tabs

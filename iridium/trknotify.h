/*
 * Copyright 2015 The Iridium Authors.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef IRIDIUM_TRKNOTIFY_H
#define IRIDIUM_TRKNOTIFY_H 1

#include <string>
#include "url/gurl.h"

namespace iridium {

extern void log_url_request(const std::string &, const GURL &);
extern void trace_url_request(const std::string &, const GURL &);

}; /* namespace iridium */

#endif /* IRIDIUM_TRKNOTIFY_H */

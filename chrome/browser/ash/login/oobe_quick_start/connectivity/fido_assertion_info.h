// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_ASSERTION_INFO_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_ASSERTION_INFO_H_

#include <string>
#include <vector>

#include "chromeos/ash/components/quick_start/types.h"

namespace ash::quick_start {

// A `struct` to store the information related to a FIDO assertion. It is used
// to construct a request to Google's authentication server, for obtaining
// users' sign-in credentials.
struct FidoAssertionInfo {
  FidoAssertionInfo();
  ~FidoAssertionInfo();
  FidoAssertionInfo(const FidoAssertionInfo& other);
  FidoAssertionInfo& operator=(const FidoAssertionInfo& other);
  bool operator==(const FidoAssertionInfo& rhs) const;

  // User's email.
  std::string email;

  // Key identifier of the key used.
  Base64UrlString credential_id;

  // The authenticator data returned by the authenticator.
  // https://www.w3.org/TR/webauthn/#dom-authenticatorassertionresponse-authenticatordata
  std::vector<uint8_t> authenticator_data;

  // The client data passed to the authenticator when requesting credentials.
  // https://w3.org/TR/webauthn/#dom-authenticatorresponse-clientdatajson
  std::vector<uint8_t> client_data;

  // The signature generated by the authenticator.
  // https://www.w3.org/TR/webauthn/#dom-authenticatorassertionresponse-signature
  std::vector<uint8_t> signature;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_ASSERTION_INFO_H_

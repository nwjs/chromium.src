// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos::network_config {

// Returns true if |network_type| matches |match_type|, which may include kAll
// or kWireless.
bool NetworkTypeMatchesType(mojom::NetworkType network_type,
                            mojom::NetworkType match_type);

// Calls NetworkTypeMatchesType with |network_type| = |network|->type.
bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type);

// Returns true if |connection_state| is in a connected state, including portal.
bool StateIsConnected(mojom::ConnectionStateType connection_state);

// Returns the signal strength for wireless network types or 0 for other types.
int GetWirelessSignalStrength(const mojom::NetworkStateProperties* network);

// Returns true if the device state InhibitReason property is set to anything
// but kNotInhibited.
bool IsInhibited(const mojom::DeviceStateProperties* device);

// Returns an ONC dictionary for network with guid |network_guid| containing a
// configuration of the network's user APN list.
base::Value::Dict UserApnListToOnc(const std::string& network_guid,
                                   const base::Value::List* user_apn_list);

// Converts a list of APN types in the ONC representation to the Mojo enum
// representation.
std::vector<mojom::ApnType> OncApnTypesToMojo(
    const std::vector<std::string>& apn_types);

// Creates a Mojo APN from a ONC dictionary.
mojom::ApnPropertiesPtr GetApnProperties(const base::Value::Dict& onc_apn,
                                         bool is_apn_revamp_enabled);

}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_

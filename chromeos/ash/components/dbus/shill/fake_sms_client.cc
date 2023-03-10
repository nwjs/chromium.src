// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_sms_client.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"

namespace ash {

FakeSMSClient::FakeSMSClient() = default;

FakeSMSClient::~FakeSMSClient() = default;

void FakeSMSClient::GetAll(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           GetAllCallback callback) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSmsTestMessages)) {
    return;
  }
  // Ownership passed to callback
  base::Value sms(base::Value::Type::DICTIONARY);
  sms.SetStringKey("Number", "000-000-0000");
  sms.SetStringKey("Text",
                   "FakeSMSClient: Test Message: " + object_path.value());
  sms.SetStringKey("Timestamp", "Fri Jun  8 13:26:04 EDT 2012");

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(sms)));
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {

// Creates an instance of UserFmRegistrationTokenUploader for each profile.
class UserFmRegistrationTokenUploaderFactory
    : public ProfileKeyedServiceFactory {
 public:
  static UserFmRegistrationTokenUploaderFactory* GetInstance();

  UserFmRegistrationTokenUploaderFactory(
      const UserFmRegistrationTokenUploaderFactory&) = delete;
  UserFmRegistrationTokenUploaderFactory& operator=(
      const UserFmRegistrationTokenUploaderFactory&) = delete;

 private:
  friend base::NoDestructor<UserFmRegistrationTokenUploaderFactory>;

  UserFmRegistrationTokenUploaderFactory();
  ~UserFmRegistrationTokenUploaderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_FACTORY_H_

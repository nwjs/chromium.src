// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_prefs.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class SyncPrefsTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    SyncPrefs::RegisterUserPrefs(&pref_service_,
                                 pref_service_.registry());
  }

  TestingPrefServiceSyncable pref_service_;

 private:
  MessageLoop loop_;
};

// Returns all types visible from the setup UI.
syncer::ModelTypeSet GetUserVisibleTypes() {
  syncer::ModelTypeSet user_visible_types(syncer::UserTypes());
  user_visible_types.Remove(syncer::APP_NOTIFICATIONS);
  user_visible_types.Remove(syncer::APP_SETTINGS);
  user_visible_types.Remove(syncer::AUTOFILL_PROFILE);
  user_visible_types.Remove(syncer::DICTIONARY);
  user_visible_types.Remove(syncer::EXTENSION_SETTINGS);
  user_visible_types.Remove(syncer::SEARCH_ENGINES);
  return user_visible_types;
}

TEST_F(SyncPrefsTest, Basic) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  sync_prefs.SetSyncSetupCompleted();
  EXPECT_TRUE(sync_prefs.HasSyncSetupCompleted());

  EXPECT_FALSE(sync_prefs.IsStartSuppressed());
  sync_prefs.SetStartSuppressed(true);
  EXPECT_TRUE(sync_prefs.IsStartSuppressed());
  sync_prefs.SetStartSuppressed(false);
  EXPECT_FALSE(sync_prefs.IsStartSuppressed());

  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  const base::Time& now = base::Time::Now();
  sync_prefs.SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs.GetLastSyncedTime());

  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());
  sync_prefs.SetKeepEverythingSynced(false);
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());
  sync_prefs.SetKeepEverythingSynced(true);
  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());

  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
  sync_prefs.SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs.GetEncryptionBootstrapToken());
}

#if defined(OS_ANDROID)
#define MAYBE_DefaultTypes DefaultTypes
#else
#define MAYBE_DefaultTypes DISABLED_DefaultTypes
#endif
TEST_F(SyncPrefsTest, MAYBE_DefaultTypes) {
  SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetKeepEverythingSynced(false);

  // Only bookmarks are enabled by default.
  syncer::ModelTypeSet preferred_types = sync_prefs.GetPreferredDataTypes(
      syncer::UserTypes());
  EXPECT_TRUE(preferred_types.Equals(syncer::ModelTypeSet(syncer::BOOKMARKS)));

  // Simulate an upgrade to passwords and autofill support. None of the
  // new types or their pref group types should be registering, ensuring they
  // don't have pref values.
  syncer::ModelTypeSet registered_types = syncer::UserTypes();
  registered_types.Remove(syncer::PASSWORDS);
  registered_types.Remove(syncer::AUTOFILL);
  registered_types.Remove(syncer::AUTOFILL_PROFILE);

  // Enable all other types.
  sync_prefs.SetPreferredDataTypes(registered_types,
                                   registered_types);
  EXPECT_FALSE(preferred_types.Has(syncer::AUTOFILL));
  EXPECT_FALSE(preferred_types.Has(syncer::AUTOFILL_PROFILE));
  EXPECT_TRUE(preferred_types.Has(syncer::BOOKMARKS));
  EXPECT_FALSE(preferred_types.Has(syncer::PASSWORDS));

  // Now manually enable autofill, which should result in autofill profile also
  // being enabled.
  pref_service_.SetBoolean(prefs::kSyncAutofill, true);
  preferred_types =
      sync_prefs.GetPreferredDataTypes(syncer::UserTypes());
  EXPECT_TRUE(preferred_types.Has(syncer::AUTOFILL));
  EXPECT_TRUE(preferred_types.Has(syncer::AUTOFILL_PROFILE));
  EXPECT_TRUE(preferred_types.Has(syncer::BOOKMARKS));
  EXPECT_FALSE(preferred_types.Has(syncer::PASSWORDS));

  // Now manually enable passwords.
  pref_service_.SetBoolean(prefs::kSyncPasswords, true);
  preferred_types =
      sync_prefs.GetPreferredDataTypes(syncer::UserTypes());
  EXPECT_TRUE(preferred_types.Has(syncer::AUTOFILL));
  EXPECT_TRUE(preferred_types.Has(syncer::AUTOFILL_PROFILE));
  EXPECT_TRUE(preferred_types.Has(syncer::BOOKMARKS));
  EXPECT_TRUE(preferred_types.Has(syncer::PASSWORDS));
}

TEST_F(SyncPrefsTest, PreferredTypesKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());

  const syncer::ModelTypeSet user_types = syncer::UserTypes();
  EXPECT_TRUE(user_types.Equals(
      sync_prefs.GetPreferredDataTypes(user_types)));
  const syncer::ModelTypeSet user_visible_types = GetUserVisibleTypes();
  for (syncer::ModelTypeSet::Iterator it = user_visible_types.First();
       it.Good(); it.Inc()) {
    syncer::ModelTypeSet preferred_types;
    preferred_types.Put(it.Get());
    sync_prefs.SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_TRUE(user_types.Equals(
        sync_prefs.GetPreferredDataTypes(user_types)));
  }
}

TEST_F(SyncPrefsTest, PreferredTypesNotKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  sync_prefs.SetKeepEverythingSynced(false);

  const syncer::ModelTypeSet user_types = syncer::UserTypes();
#if defined(OS_ANDROID)
  EXPECT_FALSE(user_types.Equals(
      sync_prefs.GetPreferredDataTypes(user_types)));
#else
  EXPECT_TRUE(user_types.Equals(
      sync_prefs.GetPreferredDataTypes(user_types)));
#endif
  const syncer::ModelTypeSet user_visible_types = GetUserVisibleTypes();
  for (syncer::ModelTypeSet::Iterator it = user_visible_types.First();
       it.Good(); it.Inc()) {
    syncer::ModelTypeSet preferred_types;
    preferred_types.Put(it.Get());
    syncer::ModelTypeSet expected_preferred_types(preferred_types);
    if (it.Get() == syncer::AUTOFILL) {
      expected_preferred_types.Put(syncer::AUTOFILL_PROFILE);
    }
    if (it.Get() == syncer::PREFERENCES) {
      expected_preferred_types.Put(syncer::DICTIONARY);
      expected_preferred_types.Put(syncer::SEARCH_ENGINES);
    }
    if (it.Get() == syncer::APPS) {
      expected_preferred_types.Put(syncer::APP_NOTIFICATIONS);
      expected_preferred_types.Put(syncer::APP_SETTINGS);
    }
    if (it.Get() == syncer::EXTENSIONS) {
      expected_preferred_types.Put(syncer::EXTENSION_SETTINGS);
    }
    if (it.Get() == syncer::SESSIONS) {
      expected_preferred_types.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    }
    // TODO(akalin): Remove this when history delete directives are
    // registered by default.
    if (it.Get() == syncer::HISTORY_DELETE_DIRECTIVES) {
      expected_preferred_types.Clear();
    }
    sync_prefs.SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_TRUE(expected_preferred_types.Equals(
        sync_prefs.GetPreferredDataTypes(user_types)));
  }
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD1(OnSyncManagedPrefChange, void(bool));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  SyncPrefs sync_prefs(&pref_service_);

  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));

  EXPECT_FALSE(sync_prefs.IsManaged());

  sync_prefs.AddSyncPrefObserver(&mock_sync_pref_observer);

  sync_prefs.SetManagedForTest(true);
  EXPECT_TRUE(sync_prefs.IsManaged());
  sync_prefs.SetManagedForTest(false);
  EXPECT_FALSE(sync_prefs.IsManaged());

  sync_prefs.RemoveSyncPrefObserver(&mock_sync_pref_observer);
}

TEST_F(SyncPrefsTest, AcknowledgeSyncedTypes) {
  SyncPrefs sync_prefs(&pref_service_);

  syncer::ModelTypeSet expected_acknowledge_synced_types =
      sync_prefs.GetAcknowledgeSyncedTypesForTest();
  for (int i = syncer::EXTENSION_SETTINGS; i < syncer::MODEL_TYPE_COUNT; ++i) {
    const syncer::ModelType type = syncer::ModelTypeFromInt(i);
    syncer::ModelTypeSet acknowledge_synced_types(type);
    expected_acknowledge_synced_types.Put(type);
    sync_prefs.AcknowledgeSyncedTypes(acknowledge_synced_types);
    EXPECT_TRUE(expected_acknowledge_synced_types.Equals(
        sync_prefs.GetAcknowledgeSyncedTypesForTest()));
  }
}

TEST_F(SyncPrefsTest, ClearPreferences) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());

  sync_prefs.SetSyncSetupCompleted();
  sync_prefs.SetLastSyncedTime(base::Time::Now());
  sync_prefs.SetEncryptionBootstrapToken("token");

  EXPECT_TRUE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_NE(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_EQ("token", sync_prefs.GetEncryptionBootstrapToken());

  sync_prefs.ClearPreferences();

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
}

TEST_F(SyncPrefsTest, NullPrefService) {
  SyncPrefs sync_prefs(NULL);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_FALSE(sync_prefs.IsStartSuppressed());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());
  const syncer::ModelTypeSet user_types = syncer::UserTypes();
  EXPECT_TRUE(sync_prefs.GetPreferredDataTypes(user_types).Empty());
  EXPECT_FALSE(sync_prefs.IsManaged());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
}

}  // namespace

}  // namespace browser_sync

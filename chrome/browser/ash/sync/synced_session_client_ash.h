// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/gurl.h"

namespace ash {

// Represent a subset of SerializedNavigationEntry data available for Ash via
// crosapi.
struct ForeignSyncedSessionTabAsh {
 public:
  ForeignSyncedSessionTabAsh();
  ForeignSyncedSessionTabAsh(const ForeignSyncedSessionTabAsh&);
  ForeignSyncedSessionTabAsh(ForeignSyncedSessionTabAsh&&);
  ForeignSyncedSessionTabAsh& operator=(const ForeignSyncedSessionTabAsh&);
  ForeignSyncedSessionTabAsh& operator=(ForeignSyncedSessionTabAsh&&);
  ~ForeignSyncedSessionTabAsh();

  GURL current_navigation_url;
  std::u16string current_navigation_title;
  base::Time last_modified_timestamp;
};

// Represent subset of SessionWindow data available for Ash via crosapi.
struct ForeignSyncedSessionWindowAsh {
 public:
  ForeignSyncedSessionWindowAsh();
  ForeignSyncedSessionWindowAsh(const ForeignSyncedSessionWindowAsh&);
  ForeignSyncedSessionWindowAsh(ForeignSyncedSessionWindowAsh&&);
  ForeignSyncedSessionWindowAsh& operator=(
      const ForeignSyncedSessionWindowAsh&);
  ForeignSyncedSessionWindowAsh& operator=(ForeignSyncedSessionWindowAsh&&);
  ~ForeignSyncedSessionWindowAsh();

  std::vector<ForeignSyncedSessionTabAsh> tabs;
};

// Represent subset of SyncedSession data available for Ash via crosapi.
struct ForeignSyncedSessionAsh {
 public:
  ForeignSyncedSessionAsh();
  ForeignSyncedSessionAsh(const ForeignSyncedSessionAsh&);
  ForeignSyncedSessionAsh(ForeignSyncedSessionAsh&&);
  ForeignSyncedSessionAsh& operator=(const ForeignSyncedSessionAsh&);
  ForeignSyncedSessionAsh& operator=(ForeignSyncedSessionAsh&&);
  ~ForeignSyncedSessionAsh();

  std::string session_name;
  base::Time modified_time;
  std::vector<ForeignSyncedSessionWindowAsh> windows;
};

// Implements the SyncedSessionClient mojo interface to receive foreign session
// updates.
class SyncedSessionClientAsh final
    : public crosapi::mojom::SyncedSessionClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // OnForeignSyncedPhoneSessionsUpdated() per observer is called every time
    // we receive an update of foreign synced phone sessions from Lacros via the
    // crosapi.
    virtual void OnForeignSyncedPhoneSessionsUpdated(
        const std::vector<ForeignSyncedSessionAsh>& sessions) {}
  };

  SyncedSessionClientAsh();
  SyncedSessionClientAsh(const SyncedSessionClientAsh&) = delete;
  SyncedSessionClientAsh& operator=(const SyncedSessionClientAsh&) = delete;
  ~SyncedSessionClientAsh() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncedSessionClient> receiver);

  // crosapi::mojom::SyncedSessionClient
  void OnForeignSyncedPhoneSessionsUpdated(
      std::vector<crosapi::mojom::SyncedSessionPtr> sessions) override;

  const std::vector<ForeignSyncedSessionAsh>&
  last_foreign_synced_phone_sessions() const {
    return last_foreign_synced_phone_sessions_;
  }

 private:
  mojo::ReceiverSet<crosapi::mojom::SyncedSessionClient> receivers_;
  base::ObserverList<Observer> observers_;
  std::vector<ForeignSyncedSessionAsh> last_foreign_synced_phone_sessions_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNCED_SESSION_CLIENT_ASH_H_

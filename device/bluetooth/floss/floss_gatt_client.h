// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

// Authentication requirements for GATT.
enum class DEVICE_BLUETOOTH_EXPORT AuthRequired {
  kNoAuth = 0,  // No authentication required.
  kNoMitm,      // Encrypted but not authenticated.
  kReqMitm,     // Encrypted and authenticated.

  // Same as above but signed + encrypted.
  kSignedNoMitm,
  kSignedReqMitm,
};

enum class DEVICE_BLUETOOTH_EXPORT WriteType {
  kInvalid = 0,
  kWriteNoResponse,
  kWrite,
  kWritePrepare,
};

enum class DEVICE_BLUETOOTH_EXPORT LePhy {
  kInvalid = 0,
  kPhy1m = 1,
  kPhy2m = 2,
  kPhyCoded = 3,
};

// Status for many GATT apis. Due to complexity here, only kSuccess should be
// used for comparisons.
enum class DEVICE_BLUETOOTH_EXPORT GattStatus {
  kSuccess = 0,
  kInvalidHandle,
  kReadNotPermitted,
  kWriteNotPermitted,
  kInvalidPdu,
  kInsufficientAuthentication,
  kReqNotSupported,
  kInvalidOffset,
  kInsufficientAuthorization,
  kPrepareQueueFull,
  kNotFound,
  kNotLong,
  kInsufficientKeySize,
  kInvalidAttributeLen,
  kUnlikelyError,
  kInsufficientEncryption,
  kUnsupportedGroupType,
  kInsufficientResources,
  kDatabaseOutOfSync,
  kValueNotAllowed,
  // Big jump here
  kTooShort = 0x7f,
  kNoResources,
  kInternalError,
  kWrongState,
  kDbFull,
  kBusy,
  kError,
  kCommandStarted,
  kIllegalParameter,
  kPending,
  kAuthFailed,
  kMore,
  kInvalidConfig,
  kServiceStarted,
  kEncryptedNoMitm,
  kNotEncrypted,
  kCongested,
  kDupReg,
  kAlreadyOpen,
  kCancel,
  // 0xE0 - 0xFC reserved for future use.
  kCccCfgErr = 0xFD,
  kPrcInProgress = 0xFE,
  kOutOfRange = 0xFF,
};

struct DEVICE_BLUETOOTH_EXPORT GattDescriptor {
  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t permissions;

  GattDescriptor();
  ~GattDescriptor();
};

struct DEVICE_BLUETOOTH_EXPORT GattCharacteristic {
  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t properties;
  int32_t permissions;
  int32_t key_size;
  WriteType write_type;
  std::vector<GattDescriptor> descriptors;

  GattCharacteristic();
  GattCharacteristic(const GattCharacteristic&);
  ~GattCharacteristic();
};

struct DEVICE_BLUETOOTH_EXPORT GattService {
  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t service_type;
  std::vector<GattCharacteristic> characteristics;
  std::vector<GattService> included_services;

  GattService();
  GattService(const GattService&);
  ~GattService();
};

// Callback functions expected to be imported by the GATT client.
//
// This also doubles as an observer class for the GATT client since it will
// really only filter out calls that aren't for this client.
class DEVICE_BLUETOOTH_EXPORT FlossGattClientObserver
    : public base::CheckedObserver {
 public:
  FlossGattClientObserver(const FlossGattClientObserver&) = delete;
  FlossGattClientObserver& operator=(const FlossGattClientObserver&) = delete;

  FlossGattClientObserver() = default;
  ~FlossGattClientObserver() override = default;

  // A client has completed registration for callbacks. Subsequent uses should
  // use this client id.
  virtual void GattClientRegistered(GattStatus status, int32_t client_id) {}

  // A client connection has changed state.
  virtual void GattClientConnectionState(GattStatus status,
                                         int32_t client_id,
                                         bool connected,
                                         std::string address) {}

  // The PHY used for a connection has changed states.
  virtual void GattPhyUpdate(std::string address,
                             LePhy tx,
                             LePhy rx,
                             GattStatus status) {}

  // Result of reading the currently used PHY.
  virtual void GattPhyRead(std::string address,
                           LePhy tx,
                           LePhy rx,
                           GattStatus status) {}

  // Service resolution completed and GATT db available.
  virtual void GattSearchComplete(std::string address,
                                  const std::vector<GattService>& services,
                                  GattStatus status) {}

  // Result of reading a characteristic.
  virtual void GattCharacteristicRead(std::string address,
                                      GattStatus status,
                                      int32_t handle,
                                      const std::vector<uint8_t>& data) {}

  // Result of writing a characteristic.
  virtual void GattCharacteristicWrite(std::string address,
                                       GattStatus status,
                                       int32_t handle) {}

  // Reliable write completed.
  virtual void GattExecuteWrite(std::string address, GattStatus status) {}

  // Result of reading a descriptor.
  virtual void GattDescriptorRead(std::string address,
                                  GattStatus status,
                                  int32_t handle,
                                  const std::vector<uint8_t>& data) {}

  // Result of writing to a descriptor.
  virtual void GattDescriptorWrite(std::string address,
                                   GattStatus status,
                                   int32_t handle) {}

  // Notification or indication of a handle on a remote device.
  virtual void GattNotify(std::string address,
                          int32_t handle,
                          const std::vector<uint8_t>& data) {}

  // Result of reading remote rssi.
  virtual void GattReadRemoteRssi(std::string address,
                                  int32_t rssi,
                                  GattStatus status) {}

  // Result of setting connection mtu.
  virtual void GattConfigureMtu(std::string address,
                                int32_t mtu,
                                GattStatus status) {}

  // Change to connection parameters.
  virtual void GattConnectionUpdated(std::string address,
                                     int32_t interval,
                                     int32_t latency,
                                     int32_t timeout,
                                     GattStatus status) {}

  // Notification when there is an addition/removal/change of a GATT service.
  virtual void GattServiceChanged(std::string address) {}
};

class DEVICE_BLUETOOTH_EXPORT FlossGattClient : public FlossDBusClient,
                                                public FlossGattClientObserver {
 public:
  static const char kExportedCallbacksPath[];

  // Creates the instance.
  static std::unique_ptr<FlossGattClient> Create();

  FlossGattClient();
  ~FlossGattClient() override;

  FlossGattClient(const FlossGattClient&) = delete;
  FlossGattClient& operator=(const FlossGattClient&) = delete;

  // Manage observers.
  void AddObserver(FlossGattClientObserver* observer);
  void RemoveObserver(FlossGattClientObserver* observer);

  // Create a GATT client connection to a remote device on given transport.
  virtual void Connect(ResponseCallback<Void> callback,
                       const std::string& remote_device,
                       const BluetoothTransport& transport);

  // Disconnect GATT for given device.
  virtual void Disconnect(ResponseCallback<Void> callback,
                          const std::string& remote_device);

  // Clears the attribute cache of a device.
  virtual void Refresh(ResponseCallback<Void> callback,
                       const std::string& remote_device);

  // Enumerates all GATT services on an already connected device.
  virtual void DiscoverAllServices(ResponseCallback<Void> callback,
                                   const std::string& remote_device);

  // Search for a GATT service on a connected device with a UUID.
  virtual void DiscoverServiceByUuid(ResponseCallback<Void> callback,
                                     const std::string& remote_device,
                                     const device::BluetoothUUID& uuid);

  // Reads a characteristic on a connected device with given |handle|.
  virtual void ReadCharacteristic(ResponseCallback<Void> callback,
                                  const std::string& remote_device,
                                  const int32_t handle,
                                  const AuthRequired auth_required);

  // Reads a characteristic on a connected device between |start_handle| and
  // |end_handle| that matches the given |uuid|.
  virtual void ReadUsingCharacteristicUuid(ResponseCallback<Void> callback,
                                           const std::string& remote_device,
                                           const device::BluetoothUUID& uuid,
                                           const int32_t start_handle,
                                           const int32_t end_handle,
                                           const AuthRequired auth_required);

  // Writes a characteristic on a connected device with given |handle|.
  virtual void WriteCharacteristic(ResponseCallback<Void> callback,
                                   const std::string& remote_device,
                                   const int32_t handle,
                                   const WriteType write_type,
                                   const AuthRequired auth_required,
                                   const std::vector<uint8_t> data);

  // Reads the descriptor for a given characteristic |handle|.
  virtual void ReadDescriptor(ResponseCallback<Void> callback,
                              const std::string& remote_device,
                              const int32_t handle,
                              const AuthRequired auth_required);

  // Writes a descriptor for a given characteristic |handle|.
  virtual void WriteDescriptor(ResponseCallback<Void> callback,
                               const std::string& remote_device,
                               const int32_t handle,
                               const AuthRequired auth_required,
                               const std::vector<uint8_t> data);

  // Register for updates on a specific characteristic.
  virtual void RegisterForNotification(ResponseCallback<GattStatus> callback,
                                       const std::string& remote_device,
                                       const int32_t handle);

  // Unregister for updates on a specific characteristic.
  virtual void UnregisterNotification(ResponseCallback<GattStatus> callback,
                                      const std::string& remote_device,
                                      const int32_t handle);

  // Request RSSI for the connected device.
  virtual void ReadRemoteRssi(ResponseCallback<Void> callback,
                              const std::string& remote_device);

  // Configures the MTU for a connected device.
  virtual void ConfigureMTU(ResponseCallback<Void> callback,
                            const std::string& remote_device,
                            const int32_t mtu);

  // Update the connection parameters for the given device.
  virtual void UpdateConnectionParameters(ResponseCallback<Void> callback,
                                          const std::string& remote_device,
                                          const int32_t min_interval,
                                          const int32_t max_interval,
                                          const int32_t latency,
                                          const int32_t timeout,
                                          const uint16_t min_ce_len,
                                          const uint16_t max_ce_len);

  // Initialize the gatt client for the given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;

 protected:
  friend class BluetoothGattFlossTest;

  // FlossGattClientObserver overrides
  void GattClientRegistered(GattStatus status, int32_t client_id) override;
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override;
  void GattPhyUpdate(std::string address,
                     LePhy tx,
                     LePhy rx,
                     GattStatus status) override;
  void GattPhyRead(std::string address,
                   LePhy tx,
                   LePhy rx,
                   GattStatus status) override;
  void GattSearchComplete(std::string address,
                          const std::vector<GattService>& services,
                          GattStatus status) override;
  void GattCharacteristicRead(std::string address,
                              GattStatus status,
                              int32_t handle,
                              const std::vector<uint8_t>& data) override;
  void GattCharacteristicWrite(std::string address,
                               GattStatus status,
                               int32_t handle) override;
  void GattExecuteWrite(std::string address, GattStatus status) override;
  void GattDescriptorRead(std::string address,
                          GattStatus status,
                          int32_t handle,
                          const std::vector<uint8_t>& data) override;
  void GattDescriptorWrite(std::string address,
                           GattStatus status,
                           int32_t handle) override;
  void GattNotify(std::string address,
                  int32_t handle,
                  const std::vector<uint8_t>& data) override;
  void GattReadRemoteRssi(std::string address,
                          int32_t rssi,
                          GattStatus status) override;
  void GattConfigureMtu(std::string address,
                        int32_t mtu,
                        GattStatus status) override;
  void GattConnectionUpdated(std::string address,
                             int32_t interval,
                             int32_t latency,
                             int32_t timeout,
                             GattStatus status) override;
  void GattServiceChanged(std::string address) override;

  void OnRegisterNotificationResponse(ResponseCallback<GattStatus> callback,
                                      bool is_registering,
                                      DBusResult<Void> result);

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  base::raw_ptr<dbus::Bus> bus_ = nullptr;

  // Path used for gatt api calls by this class.
  dbus::ObjectPath gatt_adapter_path_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<FlossGattClientObserver> observers_;

  // Service which implements the GattClient interface.
  std::string service_name_;

 private:
  friend class FlossGattClientTest;

  // Register this client to get a client id.
  void RegisterClient();

  template <typename R, typename... Args>
  void CallGattMethod(ResponseCallback<R> callback,
                      const char* member,
                      Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kGattInterface,
               gatt_adapter_path_, member, args...);
  }

  // Id given for registering as a client against Floss. Used in many apis.
  int32_t client_id_ = 0;

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossGattClientObserver> exported_callback_manager_{
      gatt::kCallbackInterface};

  base::WeakPtrFactory<FlossGattClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_CLIENT_H_

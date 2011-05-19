// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketFactory;

class TCPSocketParams : public base::RefCounted<TCPSocketParams> {
 public:
  TCPSocketParams(const HostPortPair& host_port_pair, RequestPriority priority,
                  const GURL& referrer, bool disable_resolver_cache);

  // TODO(willchan): Update all unittests so we don't need this.
  TCPSocketParams(const std::string& host, int port, RequestPriority priority,
                  const GURL& referrer, bool disable_resolver_cache);

  const HostResolver::RequestInfo& destination() const { return destination_; }

 private:
  friend class base::RefCounted<TCPSocketParams>;
  ~TCPSocketParams();

  void Initialize(RequestPriority priority, const GURL& referrer,
                  bool disable_resolver_cache);

  HostResolver::RequestInfo destination_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketParams);
};

// TCPConnectJob handles the host resolution necessary for socket creation
// and the transport (likely TCP) connect. TCPConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTimerInMs) and start a connect() to a IPv4 address if the timer
// fires. Then we race the IPv4 connect() against the IPv6 connect() (which has
// a headstart) and return the one that completes first to the socket pool.
class TCPConnectJob : public ConnectJob {
 public:
  TCPConnectJob(const std::string& group_name,
                const scoped_refptr<TCPSocketParams>& params,
                base::TimeDelta timeout_duration,
                ClientSocketFactory* client_socket_factory,
                HostResolver* host_resolver,
                Delegate* delegate,
                NetLog* net_log);
  virtual ~TCPConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const;

  // Makes |addrlist| start with an IPv4 address if |addrlist| contains any
  // IPv4 address.
  //
  // WARNING: this method should only be used to implement the prefer-IPv4
  // hack.  It is a public method for the unit tests.
  static void MakeAddrListStartWithIPv4(AddressList* addrlist);

  static const int kIPv6FallbackTimerInMs;

 private:
  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_TCP_CONNECT,
    STATE_TCP_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTCPConnect();
  int DoTCPConnectComplete(int result);

  // Not part of the state machine.
  void DoIPv6FallbackTCPConnect();
  void DoIPv6FallbackTCPConnectComplete(int result);

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal();

  scoped_refptr<TCPSocketParams> params_;
  ClientSocketFactory* const client_socket_factory_;
  CompletionCallbackImpl<TCPConnectJob> callback_;
  SingleRequestHostResolver resolver_;
  AddressList addresses_;
  State next_state_;

  // The time Connect() was called.
  base::TimeTicks start_time_;

  // The time the connect was started (after DNS finished).
  base::TimeTicks connect_start_time_;

  scoped_ptr<ClientSocket> transport_socket_;

  scoped_ptr<ClientSocket> fallback_transport_socket_;
  scoped_ptr<AddressList> fallback_addresses_;
  CompletionCallbackImpl<TCPConnectJob> fallback_callback_;
  base::TimeTicks fallback_connect_start_time_;
  base::OneShotTimer<TCPConnectJob> fallback_timer_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnectJob);
};

class TCPClientSocketPool : public ClientSocketPool {
 public:
  TCPClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      HostResolver* host_resolver,
      ClientSocketFactory* client_socket_factory,
      NetLog* net_log);

  virtual ~TCPClientSocketPool();

  // ClientSocketPool methods:

  virtual int RequestSocket(const std::string& group_name,
                            const void* resolve_info,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log);

  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log);

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket,
                             int id);

  virtual void Flush();

  virtual void CloseIdleSockets();

  virtual int IdleSocketCount() const;

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

  virtual DictionaryValue* GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const;

  virtual base::TimeDelta ConnectionTimeout() const;

  virtual ClientSocketPoolHistograms* histograms() const;

 private:
  typedef ClientSocketPoolBase<TCPSocketParams> PoolBase;

  class TCPConnectJobFactory
      : public PoolBase::ConnectJobFactory {
   public:
    TCPConnectJobFactory(ClientSocketFactory* client_socket_factory,
                         HostResolver* host_resolver,
                         NetLog* net_log)
        : client_socket_factory_(client_socket_factory),
          host_resolver_(host_resolver),
          net_log_(net_log) {}

    virtual ~TCPConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const;

    virtual base::TimeDelta ConnectionTimeout() const;

   private:
    ClientSocketFactory* const client_socket_factory_;
    HostResolver* const host_resolver_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(TCPConnectJobFactory);
  };

  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(TCPClientSocketPool, TCPSocketParams);

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_

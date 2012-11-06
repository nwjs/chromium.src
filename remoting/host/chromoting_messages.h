// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines IPC messages used by Chromoting components.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// Daemon to Network messages

// Delivers the host configuration (and updates) to the network process.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_Configuration, std::string)

//-----------------------------------------------------------------------------
// Network to Daemon messages

// Asks the daemon to send Secure Attention Sequence (SAS) to the current
// console session.
IPC_MESSAGE_CONTROL0(ChromotingNetworkDaemonMsg_SendSasToConsole)

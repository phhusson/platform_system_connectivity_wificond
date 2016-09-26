/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WIFICOND_NET_NETLINK_MANAGER_H_
#define WIFICOND_NET_NETLINK_MANAGER_H_

#include <functional>
#include <map>
#include <memory>

#include <android-base/macros.h>
#include <android-base/unique_fd.h>

#include "event_loop.h"

namespace android {
namespace wificond {

class NL80211Packet;

// Encapsulates all the different things we know about a specific message
// type like its name, and its id.
struct MessageType {
   // This constructor is needed by map[key] operation.
   MessageType() {};
   explicit MessageType(uint16_t id) {
     family_id = id;
   };
   uint16_t family_id;
   // Multicast groups supported by the family.  The string and mapping to
   // a group id are extracted from the CTRL_CMD_NEWFAMILY message.
   std::map<std::string, uint32_t> groups;
};

// This describes a type of function handling scan results ready notification.
// |interface_index| is the index of interface which the scan results
// are from.
// |aborted| is a boolean indicating if this scan request was aborted or not.
// |ssids| is a vector of scan ssids associated with the corresponding
// scan request.
// |frequencies| is a vector of scan frequencies associated with the
// corresponding scan request.
typedef std::function<void(
    uint32_t interface_index,
    bool aborted,
    std::vector<std::vector<uint8_t>>& ssids,
    std::vector<uint32_t>& frequencies)> OnScanResultsReadyHandler;

class NetlinkManager {
 public:
  explicit NetlinkManager(EventLoop* event_loop);
  virtual ~NetlinkManager();
  // Initialize netlink manager.
  // This includes setting up socket and requesting nl80211 family id from kernel.
  // Returns true on success.
  virtual bool Start();
  // Returns true if this netlink manager object is started.
  virtual bool IsStarted() const;
  // Returns a sequence number available for use.
  virtual uint32_t GetSequenceNumber();
  // Get NL80211 netlink family id,
  virtual uint16_t GetFamilyId();

  // Send |packet| to kernel.
  // This works in an asynchronous way.
  // |handler| will be run when we receive a valid reply from kernel.
  // Do not use this asynchronous interface to send a dump request.
  // Returns true on success.
  virtual bool RegisterHandlerAndSendMessage(const NL80211Packet& packet,
      std::function<void(std::unique_ptr<const NL80211Packet>)> handler);
  // Synchronous version of |RegisterHandlerAndSendMessage|.
  // Returns true on successfully receiving an valid reply.
  // Reply packets will be stored in |*response|.
  virtual bool SendMessageAndGetResponses(
      const NL80211Packet& packet,
      std::vector<std::unique_ptr<const NL80211Packet>>* response);
  // Wrapper of |SendMessageAndGetResponses| for messages with a single
  // response.
  // Returns true on successfully receiving an valid reply.
  // This will returns false if a NLMSG_ERROR is received.
  // Reply packet will be stored in |*response|.
  virtual bool SendMessageAndGetSingleResponse(
      const NL80211Packet& packet,
      std::unique_ptr<const NL80211Packet>* response);

  // Wrapper of |SendMessageAndGetResponses| for messages with a single
  // response.
  // Returns true on successfully receiving an valid reply.
  // This will returns true if a NLMSG_ERROR is received.
  // This is useful when the caller needs the error code from kernel.
  // Reply packet will be stored in |*response|.
  virtual bool SendMessageAndGetSingleResponseOrError(
      const NL80211Packet& packet,
      std::unique_ptr<const NL80211Packet>* response);

  // Wrapper of |SendMessageAndGetResponses| for messages that trigger
  // only a NLMSG_ERROR response
  // Returns true if the message is successfully sent and a NLMSG_ERROR response
  // comes back, regardless of the error code.
  // Error code will be stored in |*error_code|
  virtual bool SendMessageAndGetAckOrError(const NL80211Packet& packet,
                                           int* error_code);
  // Wrapper of |SendMessageAndGetResponses| that returns true iff the response
  // is an ACK.
  virtual bool SendMessageAndGetAck(const NL80211Packet& packet);

  // Sign up to receive and log multicast events of a specific type.
  // |group| is one of the string NL80211_MULTICAST_GROUP_* in nl80211.h.
  virtual bool SubscribeToEvents(const std::string& group);

  // Sign up to be notified when new scan results are available.
  // |handler| will be called when the kernel signals to wificond that a scan
  // has been completed on the given |interface_index|.  See the declaration of
  // OnScanResultsReadyHandler for documentation on the semantics of this
  // callback.
  virtual void SubscribeScanResultNotification(
      uint32_t interface_index,
      OnScanResultsReadyHandler handler);

  // Cancel the sign-up of receiving new scan result notification from
  // interface with index |interface_index|.
  virtual void UnsubscribeScanResultNotification(uint32_t interface_index);

 private:
  bool SetupSocket(android::base::unique_fd* netlink_fd);
  bool WatchSocket(android::base::unique_fd* netlink_fd);
  void ReceivePacketAndRunHandler(int fd);
  bool DiscoverFamilyId();
  bool SendMessageInternal(const NL80211Packet& packet, int fd);
  void BroadcastHandler(std::unique_ptr<const NL80211Packet> packet);
  void OnScanResultsReady(std::unique_ptr<const NL80211Packet> packet);

  // This handler revceives mapping from NL80211 family name to family id,
  // as well as mapping from group name to group id.
  // These mappings are allocated by kernel.
  void OnNewFamily(std::unique_ptr<const NL80211Packet> packet);

  bool started_;
  // We use different sockets for synchronous and asynchronous interfaces.
  // Kernel will reply error message when we start a new request in the
  // middle of a dump request.
  // Using different sockets help us avoid the complexity of message
  // rescheduling.
  android::base::unique_fd sync_netlink_fd_;
  android::base::unique_fd async_netlink_fd_;
  EventLoop* event_loop_;

  // This is a collection of message handlers, for each sequence number.
  std::map<uint32_t,
      std::function<void(std::unique_ptr<const NL80211Packet>)>> message_handlers_;

  // A mapping from interface index to the handler registered to receive
  // scan results notifications.
  std::map<uint32_t, OnScanResultsReadyHandler> on_scan_result_ready_handler_;

  // Mapping from family name to family id, and group name to group id.
  std::map<std::string, MessageType> message_types_;

  uint32_t sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkManager);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_NET_NETLINK_MANAGER_H_

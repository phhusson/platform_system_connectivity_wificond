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

class NetlinkManager {
 public:
  explicit NetlinkManager(EventLoop* event_loop);
  virtual ~NetlinkManager();
  // Initialize netlink manager.
  // This includes setting up socket and requesting nl80211 family id from kernel.
  bool Start();
  // Returns a sequence number available for use.
  uint32_t GetSequenceNumber();
  // Get NL80211 netlink family id,
  uint16_t GetFamilyId();
  // Send |packet| to kernel.
  // This works in an asynchronous way.
  // |handler| will be run when we receive a valid reply from kernel.
  // Do not use this asynchronous interface to send a dump request.
  // Returns true on success.
  bool RegisterHandlerAndSendMessage(const NL80211Packet& packet,
                                     std::function<void(NL80211Packet)> handler);
  // Synchronous version of |RegisterHandlerAndSendMessage|.
  // Returns true on successfully receiving an valid reply.
  // In this case |handler| will be run before this function returns.
  // |handler| will not be run on failure.
  bool SendMessageAndRunHandler(const NL80211Packet& packet,
                                std::function<void(NL80211Packet)> handler);

  // Get the wiphy index from kernel.
  // |*out_wiphy_index| returns the wiphy index from kernel.
  // Returns true on success.
  virtual bool GetWiphyIndex(uint32_t* out_wiphy_index);

 private:
  bool SetupSocket(android::base::unique_fd* netlink_fd);
  bool WatchSocket(android::base::unique_fd* netlink_fd);
  void ReceivePacketAndRunHandler(int fd);
  bool DiscoverFamilyId();
  bool SendMessageInternal(const NL80211Packet& packet, int fd);
  void OnNewWiphy(NL80211Packet packet, uint32_t* out_wiphy_index);

  // This handler revceives mapping from NL80211 family name to family id,
  // as well as mapping from group name to group id.
  // These mappings are allocated by kernel.
  void OnNewFamily(NL80211Packet packet);

  // We use different sockets for synchronous and asynchronous interfaces.
  // Kernel will reply error message when we start a new request in the
  // middle of a dump request.
  // Using different sockets help us avoid the complexity of message
  // rescheduling.
  android::base::unique_fd sync_netlink_fd_;
  android::base::unique_fd async_netlink_fd_;
  EventLoop* event_loop_;

  // This is a collection of message handlers, for each sequence number.
  std::map<uint32_t, std::function<void(NL80211Packet)>> message_handlers_;

  // Mapping from family name to family id, and group name to group id.
  std::map<std::string, MessageType> message_types_;

  uint32_t sequence_number_;
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_NET_NETLINK_MANAGER_H_

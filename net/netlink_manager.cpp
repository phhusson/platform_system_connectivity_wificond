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

#include "net/netlink_manager.h"

#include <string>
#include <vector>

#include <linux/netlink.h>
#include <linux/nl80211.h>
#include <poll.h>
#include <sys/socket.h>

#include <android-base/logging.h>

#include "net/nl80211_attribute.h"
#include "net/nl80211_packet.h"

using android::base::unique_fd;
using std::placeholders::_1;
using std::string;
using std::vector;

namespace android {
namespace wificond {

namespace {

// netlink.h suggests NLMSG_GOODSIZE to be at most 8192 bytes.
constexpr int kReceiveBufferSize = 8 * 1024;
constexpr uint32_t kBroadcastSequenceNumber = 0;
constexpr int kMaximumNetlinkMessageWaitMilliSeconds = 300;
uint8_t ReceiveBuffer[kReceiveBufferSize];

}

NetlinkManager::NetlinkManager(EventLoop* event_loop)
    : event_loop_(event_loop),
      sequence_number_(0) {
}

uint32_t NetlinkManager::GetSequenceNumber() {
  if (++sequence_number_ == kBroadcastSequenceNumber) {
    ++sequence_number_;
  }
  return sequence_number_;
}

void NetlinkManager::ReceivePacketAndRunHandler(int fd) {
  ssize_t len = read(fd, ReceiveBuffer, kReceiveBufferSize);
  if (len == -1) {
    LOG(ERROR) << "Failed to read packet from buffer";
    return;
  }
  if (len == 0) {
    return;
  }
  NL80211Packet packet(vector<uint8_t>(ReceiveBuffer, ReceiveBuffer + len));
  if (!packet.IsValid()) {
    LOG(ERROR) << "Receive invalid packet";
    return;
  }
  // Some document says message from kernel should have port id equal 0.
  // However in practice this is not always true so we don't check that.

  uint32_t sequence_number = packet.GetMessageSequence();
  // TODO(nywang): Handle control messages.

  // TODO(nywang): Handle multicasts, which have sequence number 0.

  auto itr = message_handlers_.find(sequence_number);
  // There is no handler for this sequence number.
  if (itr == message_handlers_.end()) {
    LOG(WARNING) << "No handler for message: " << sequence_number;
    return;
  }
  // Run the handler.
  itr->second(packet);
  // Remove handler after processing.
  if (!packet.IsMulti()) {
    message_handlers_.erase(itr);
  }
}

void NetlinkManager::OnNewFamily(NL80211Packet packet) {
  if (packet.GetMessageType() != GENL_ID_CTRL) {
    LOG(ERROR) << "Wrong message type for new family message";
    return;
  }
  if (packet.GetCommand() != CTRL_CMD_NEWFAMILY) {
    LOG(ERROR) << "Wrong command for new family message";
    return;
  }
  uint16_t family_id;
  if (!packet.GetAttributeValue(CTRL_ATTR_FAMILY_ID, &family_id)) {
    LOG(ERROR) << "Failed to get family id";
    return;
  }
  string family_name;
  if (!packet.GetAttributeValue(CTRL_ATTR_FAMILY_NAME, &family_name)) {
    LOG(ERROR) << "Failed to get family name";
    return;
  }
  if (family_name != "nl80211") {
    LOG(WARNING) << "Ignoring none nl80211 netlink families";
  }
  MessageType nl80211_type(family_id);
  message_types_[family_name] = nl80211_type;
  // Exract multicast groups.
  NL80211NestedAttr multicast_groups(0);
  if (packet.GetAttribute(CTRL_ATTR_MCAST_GROUPS, &multicast_groups)) {
    NL80211NestedAttr current_group(0);
    for (int i = 1; multicast_groups.GetAttribute(i, &current_group); i++) {
      string group_name;
      uint32_t group_id;
      if (!current_group.GetAttributeValue(CTRL_ATTR_MCAST_GRP_NAME, &group_name)) {
        LOG(ERROR) << "Failed to get group name";
      }
      if (!current_group.GetAttributeValue(CTRL_ATTR_MCAST_GRP_ID, &group_id)) {
        LOG(ERROR) << "Failed to get group id";
      }
      message_types_[family_name].groups[group_name] = group_id;
    }
  }
}

void NetlinkManager::Start() {
  bool setup_rt = SetupSocket(&sync_netlink_fd_);
  CHECK(setup_rt) << "Failed to setup synchronous netlink socket";
  setup_rt = SetupSocket(&async_netlink_fd_);
  CHECK(setup_rt) << "Failed to setup asynchronous netlink socket";
  // Request family id for nl80211 messages.
  CHECK(DiscoverFamilyId());
  // Watch socket.
  CHECK(WatchSocket(&async_netlink_fd_));
}

bool NetlinkManager::RegisterHandlerAndSendMessage(
    const NL80211Packet& packet,
    std::function<void(NL80211Packet)> handler) {
  if (packet.IsDump()) {
    LOG(ERROR) << "Do not use asynchronous interface for dump request !";
    return false;
  }
  if (!SendMessageInternal(packet, async_netlink_fd_.get())) {
    return false;
  }
  message_handlers_[packet.GetMessageSequence()] = handler;
  return true;
}

bool NetlinkManager::SendMessageAndRunHandler(
    const NL80211Packet& packet,
    std::function<void(NL80211Packet)> handler) {
  if (!SendMessageInternal(packet, sync_netlink_fd_.get())) {
    return false;
  }
  // Polling netlink socket, waiting for GetFamily reply.
  struct pollfd netlink_output;
  memset(&netlink_output, 0, sizeof(netlink_output));
  netlink_output.fd = sync_netlink_fd_.get();
  netlink_output.events = POLLIN;

  int poll_return = poll(&netlink_output,
                         1,
                         kMaximumNetlinkMessageWaitMilliSeconds);

  if (poll_return == 0) {
    LOG(ERROR) << "Failed to poll netlink fd: time out ";
    return false;
  } else if (poll_return == -1) {
    LOG(ERROR) << "Failed to poll netlink fd: " << strerror(errno);
    return false;
  }
  message_handlers_[packet.GetMessageSequence()] = handler;
  ReceivePacketAndRunHandler(sync_netlink_fd_.get());
  return true;
}


bool NetlinkManager::SendMessageInternal(const NL80211Packet& packet, int fd) {
  const vector<uint8_t>& data = packet.GetConstData();
  ssize_t bytes_sent =
      TEMP_FAILURE_RETRY(send(fd, data.data(), data.size(), 0));
  if (bytes_sent == -1) {
    LOG(ERROR) << "Failed to send netlink message: " << strerror(errno);
    return false;
  }
  return true;
}

bool NetlinkManager::SetupSocket(unique_fd* netlink_fd) {
  struct sockaddr_nl nladdr;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;

  netlink_fd->reset(
      socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_GENERIC));
  if (netlink_fd->get() < 0) {
    LOG(ERROR) << "Failed to create netlink socket: " << strerror(errno);
    return false;
  }
  // Set maximum receive buffer size.
  // Datagram which is larger than this size will be discarded.
  if (setsockopt(netlink_fd->get(),
                 SOL_SOCKET,
                 SO_RCVBUFFORCE,
                 &kReceiveBufferSize,
                 sizeof(kReceiveBufferSize)) < 0) {
    LOG(ERROR) << "Failed to set uevent socket SO_RCVBUFFORCE option: " << strerror(errno);
    return false;
  }
  if (bind(netlink_fd->get(),
           reinterpret_cast<struct sockaddr*>(&nladdr),
           sizeof(nladdr)) < 0) {
    LOG(ERROR) << "Failed to bind netlink socket: " << strerror(errno);
    return false;
  }
  return true;
}

bool NetlinkManager::WatchSocket(unique_fd* netlink_fd) {
  // Watch socket
  bool watch_fd_rt = event_loop_->WatchFileDescriptor(
      netlink_fd->get(),
      EventLoop::kModeInput,
      std::bind(&NetlinkManager::ReceivePacketAndRunHandler, this, _1));
  if (!watch_fd_rt) {
    LOG(ERROR) << "Failed to watch fd: " << netlink_fd->get();
    return false;
  }
  return true;
}

bool NetlinkManager::DiscoverFamilyId() {
  NL80211Packet get_family_request(GENL_ID_CTRL,
                                   CTRL_CMD_GETFAMILY,
                                   GetSequenceNumber(),
                                   getpid());
  NL80211Attr<string> family_name(CTRL_ATTR_FAMILY_NAME, "nl80211");
  get_family_request.AddAttribute(family_name);
  auto handler = std::bind(&NetlinkManager::OnNewFamily, this, _1);
  if (!SendMessageAndRunHandler(get_family_request, handler)) {
    LOG(ERROR) << "Failed to send GetFamily message for NL80211";
    return false;
  }
  if (!message_types_.count("nl80211")) {
    LOG(ERROR) << "Failed to get NL80211 family id";
    return false;
  }
  return true;
}

}  // namespace wificond
}  // namespace android

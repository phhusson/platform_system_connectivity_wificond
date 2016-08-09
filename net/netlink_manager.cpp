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
#include <utils/Timers.h>

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

void AppendPacket(vector<NL80211Packet>* vec, NL80211Packet packet) {
  vec->push_back(packet);
}

}

NetlinkManager::NetlinkManager(EventLoop* event_loop)
    : started_(false),
      event_loop_(event_loop),
      sequence_number_(0) {
}

NetlinkManager::~NetlinkManager() {
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
  // There might be multiple message in one datagram payload.
  uint8_t* ptr = ReceiveBuffer;
  while (ptr < ReceiveBuffer + len) {
    // peek at the header.
    if (ptr + sizeof(nlmsghdr) > ReceiveBuffer + len) {
      LOG(ERROR) << "payload is broken.";
      return;
    }
    const nlmsghdr* nl_header = reinterpret_cast<const nlmsghdr*>(ptr);
    NL80211Packet packet(vector<uint8_t>(ptr, ptr + nl_header->nlmsg_len));
    ptr += nl_header->nlmsg_len;
    if (!packet.IsValid()) {
      LOG(ERROR) << "Receive invalid packet";
      return;
    }
    // Some document says message from kernel should have port id equal 0.
    // However in practice this is not always true so we don't check that.

    uint32_t sequence_number = packet.GetMessageSequence();

    // Handle multicasts.
    if (sequence_number == kBroadcastSequenceNumber) {
      BroadcastHandler(packet);
      continue;
    }

    auto itr = message_handlers_.find(sequence_number);
    // There is no handler for this sequence number.
    if (itr == message_handlers_.end()) {
      LOG(WARNING) << "No handler for message: " << sequence_number;
      return;
    }
    // A multipart message is terminated by NLMSG_DONE.
    // In this case we don't need to run the handler.
    // NLMSG_NOOP means no operation, message must be discarded.
    uint32_t message_type =  packet.GetMessageType();
    if (message_type == NLMSG_DONE || message_type == NLMSG_NOOP) {
      message_handlers_.erase(itr);
      return;
    }
    if (message_type == NLMSG_OVERRUN) {
      LOG(ERROR) << "Get message overrun notification";
      message_handlers_.erase(itr);
      return;
    }

    // In case we receive a NLMSG_ERROR message:
    // NLMSG_ERROR could be either an error or an ACK.
    // It is an ACK message only when error code field is set to 0.
    // An ACK could be return when we explicitly request that with NLM_F_ACK.
    // An ERROR could be received on NLM_F_ACK or other failure cases.
    // We should still run handler in this case, leaving it for the caller
    // to decide what to do with the packet.

    // Run the handler.
    itr->second(packet);
    // Remove handler after processing.
    if (!packet.IsMulti()) {
      message_handlers_.erase(itr);
    }
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
  if (family_name != NL80211_GENL_NAME) {
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

bool NetlinkManager::Start() {
  if (started_) {
    LOG(DEBUG) << "NetlinkManager is already started";
    return true;
  }
  bool setup_rt = SetupSocket(&sync_netlink_fd_);
  if (!setup_rt) {
    LOG(ERROR) << "Failed to setup synchronous netlink socket";
    return false;
  }

  setup_rt = SetupSocket(&async_netlink_fd_);
  if (!setup_rt) {
    LOG(ERROR) << "Failed to setup asynchronous netlink socket";
    return false;
  }

  // Request family id for nl80211 messages.
  if (!DiscoverFamilyId()) {
    return false;
  }
  // Watch socket.
  if (!WatchSocket(&async_netlink_fd_)) {
    return false;
  }
  if (!SubscribeToEvents(NL80211_MULTICAST_GROUP_SCAN)) {
    return false;
  }

  started_ = true;
  return true;
}

bool NetlinkManager::IsStarted() const {
  return started_;
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

bool NetlinkManager::SendMessageAndGetResponses(
    const NL80211Packet& packet,
    vector<NL80211Packet>* response) {
  if (!SendMessageInternal(packet, sync_netlink_fd_.get())) {
    return false;
  }
  // Polling netlink socket, waiting for GetFamily reply.
  struct pollfd netlink_output;
  memset(&netlink_output, 0, sizeof(netlink_output));
  netlink_output.fd = sync_netlink_fd_.get();
  netlink_output.events = POLLIN;

  uint32_t sequence = packet.GetMessageSequence();

  int time_remaining = kMaximumNetlinkMessageWaitMilliSeconds;
  // Multipart messages may come with seperated datagrams, ending with a
  // NLMSG_DONE message.
  // ReceivePacketAndRunHandler() will remove the handler after receiving a
  // NLMSG_DONE message.
  message_handlers_[sequence] = std::bind(AppendPacket, response, _1);

  while (time_remaining > 0 &&
      message_handlers_.find(sequence) != message_handlers_.end()) {
    nsecs_t interval = systemTime(SYSTEM_TIME_MONOTONIC);
    int poll_return = poll(&netlink_output,
                           1,
                           time_remaining);

    if (poll_return == 0) {
      LOG(ERROR) << "Failed to poll netlink fd: time out ";
      message_handlers_.erase(sequence);
      return false;
    } else if (poll_return == -1) {
      LOG(ERROR) << "Failed to poll netlink fd: " << strerror(errno);
      message_handlers_.erase(sequence);
      return false;
    }
    ReceivePacketAndRunHandler(sync_netlink_fd_.get());
    interval = systemTime(SYSTEM_TIME_MONOTONIC) - interval;
    time_remaining -= static_cast<int>(ns2ms(interval));
  }
  if (time_remaining <= 0) {
    LOG(ERROR) << "Timeout waiting for netlink reply messages";
    message_handlers_.erase(sequence);
    return false;
  }
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

uint16_t NetlinkManager::GetFamilyId() {
  return message_types_[NL80211_GENL_NAME].family_id;
}

bool NetlinkManager::DiscoverFamilyId() {
  NL80211Packet get_family_request(GENL_ID_CTRL,
                                   CTRL_CMD_GETFAMILY,
                                   GetSequenceNumber(),
                                   getpid());
  NL80211Attr<string> family_name(CTRL_ATTR_FAMILY_NAME, NL80211_GENL_NAME);
  get_family_request.AddAttribute(family_name);
  vector<NL80211Packet> response;
  if (!SendMessageAndGetResponses(get_family_request, &response) ||
      response.size() != 1) {
    LOG(ERROR) << "Failed to send and get response for NL80211 GetFamily message";
    return false;
  }
  NL80211Packet& packet = response[0];
  if (packet.GetMessageType() == NLMSG_ERROR) {
      LOG(ERROR) << "Receive ERROR message: "
                 << strerror(packet.GetErrorCode());
      return false;
  }
  OnNewFamily(packet);
  if (message_types_.find(NL80211_GENL_NAME) == message_types_.end()) {
    LOG(ERROR) << "Failed to get NL80211 family id";
    return false;
  }
  return true;
}

bool NetlinkManager::SubscribeToEvents(const string& group) {
  auto groups = message_types_[NL80211_GENL_NAME].groups;
  if (groups.find(group) == groups.end()) {
    LOG(ERROR) << "Failed to subscribe: group " << group << " doesn't exist";
    return false;
  }
  uint32_t group_id = groups[group];
  int err = setsockopt(async_netlink_fd_.get(),
                       SOL_NETLINK,
                       NETLINK_ADD_MEMBERSHIP,
                       &group_id,
                       sizeof(group_id));
  if (err < 0) {
    LOG(ERROR) << "Failed to setsockopt: " << strerror(errno);
    return false;
  }
  return true;
}

void NetlinkManager::BroadcastHandler(NL80211Packet& packet) {
  //TODO(nywang): Handle broadcast packets.
}

}  // namespace wificond
}  // namespace android

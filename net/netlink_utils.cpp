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

#include "wificond/net/netlink_utils.h"

#include <string>
#include <vector>

#include <linux/netlink.h>
#include <linux/nl80211.h>

#include <android-base/logging.h>

#include "wificond/net/netlink_manager.h"
#include "wificond/net/nl80211_packet.h"

using std::string;
using std::vector;

namespace android {
namespace wificond {

NetlinkUtils::NetlinkUtils(NetlinkManager* netlink_manager)
    : netlink_manager_(netlink_manager) {
  if (!netlink_manager_->IsStarted()) {
    netlink_manager_->Start();
  }
}

NetlinkUtils::~NetlinkUtils() {}

bool NetlinkUtils::GetWiphyIndex(uint32_t* out_wiphy_index) {
  NL80211Packet get_wiphy(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_GET_WIPHY,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  get_wiphy.AddFlag(NLM_F_DUMP);
  vector<NL80211Packet> response;
  if (!netlink_manager_->SendMessageAndGetResponses(get_wiphy, &response))  {
    LOG(ERROR) << "Failed to get wiphy index";
    return false;
  }
  if (response.empty()) {
    LOG(ERROR) << "Unexpected empty response from kernel";
    return false;
  }
  for (NL80211Packet& packet : response) {
    if (packet.GetMessageType() == NLMSG_ERROR) {
      LOG(ERROR) << "Receive ERROR message: "
                 << strerror(packet.GetErrorCode());
      return false;
    }
    if (packet.GetMessageType() != netlink_manager_->GetFamilyId()) {
      LOG(ERROR) << "Wrong message type for new interface message: "
                 << packet.GetMessageType();
      return false;
    }
    if (packet.GetCommand() != NL80211_CMD_NEW_WIPHY) {
      LOG(ERROR) << "Wrong command for new wiphy message";
      return false;
    }
    if (!packet.GetAttributeValue(NL80211_ATTR_WIPHY, out_wiphy_index)) {
      LOG(ERROR) << "Failed to get wiphy index from reply message";
      return false;
    }
  }
  return true;
}

bool NetlinkUtils::GetInterfaceName(uint32_t wiphy_index,
                                    string* interface_name) {
  NL80211Packet get_interface(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_GET_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());

  get_interface.AddFlag(NLM_F_DUMP);
  NL80211Attr<uint32_t> wiphy(NL80211_ATTR_WIPHY, wiphy_index);
  get_interface.AddAttribute(wiphy);
  vector<NL80211Packet> response;
  if (!netlink_manager_->SendMessageAndGetResponses(get_interface, &response)) {
    LOG(ERROR) << "Failed to send GetWiphy message";
  }
  if (response.empty()) {
    LOG(ERROR) << "Unexpected empty response from kernel";
    return false;
  }
  for (NL80211Packet& packet : response) {
    if (packet.GetMessageType() == NLMSG_ERROR) {
      LOG(ERROR) << "Receive ERROR message: "
                 << strerror(packet.GetErrorCode());
      return false;
    }
    if (packet.GetMessageType() != netlink_manager_->GetFamilyId()) {
      LOG(ERROR) << "Wrong message type for new interface message: "
                 << packet.GetMessageType();
      return false;
    }
    if (packet.GetCommand() != NL80211_CMD_NEW_INTERFACE) {
      LOG(ERROR) << "Wrong command for new interface message: "
                 << packet.GetCommand();
      return false;
    }

    // Today we don't check NL80211_ATTR_IFTYPE because at this point of time
    // driver always reports that interface is in STATION mode. Even when we
    // are asking interfaces infomation on behalf of tethering, it is still so
    // because hostapd is supposed to set interface to AP mode later.

    string if_name;
    if (!packet.GetAttributeValue(NL80211_ATTR_IFNAME, &if_name)) {
      // In some situations, it has been observed that the kernel tells us
      // about a pseudo-device that does not have a real netdev.  In this
      // case, responses will have a NL80211_ATTR_WDEV, and not the expected
      // IFNAME.
      LOG(DEBUG) << "Failed to get interface name";
      continue;
    }
    if (if_name == "p2p0") {
      LOG(DEBUG) << "Driver may tell a lie that p2p0 is in STATION mode,"
                 <<" we need to blacklist it.";
      continue;
    }
    *interface_name = if_name;
    return true;
  }

  LOG(ERROR) << "Failed to get expected interface info from kernel";
  return false;
}

}  // namespace wificond
}  // namespace android

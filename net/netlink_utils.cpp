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
namespace {

constexpr uint8_t kElemIdSsid = 0;

}  // namespace

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

bool NetlinkUtils::GetInterfaceNameAndIndex(uint32_t wiphy_index,
                                            string* interface_name,
                                            uint32_t* interface_index) {
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

    uint32_t if_index;
    if (!packet.GetAttributeValue(NL80211_ATTR_IFINDEX, &if_index)) {
      LOG(DEBUG) << "Failed to get interface index";
      continue;
    }
    *interface_name = if_name;
    *interface_index = if_index;
    return true;
  }

  LOG(ERROR) << "Failed to get expected interface info from kernel";
  return false;
}

bool NetlinkUtils::GetSSIDFromInfoElement(const vector<uint8_t>& ie,
                                          vector<uint8_t>* ssid) {
  // Information elements are stored in 'TLV' format.
  // Field:  |   Type     |          Length           |      Value      |
  // Length: |     1      |             1             |     variable    |
  // Content:| Element ID | Length of the Value field | Element payload |
  const uint8_t* end = ie.data() + ie.size();
  const uint8_t* ptr = ie.data();
  // +1 means we must have space for the length field.
  while (ptr + 1  < end) {
    uint8_t type = *ptr;
    uint8_t length = *(ptr + 1);
    // Length field is invalid.
    if (ptr + 1 + length >= end) {
      return false;
    }
    // SSID element is found.
    if (type == kElemIdSsid) {
      // SSID is an empty string.
      if (length == 0) {
        *ssid = vector<uint8_t>();
      } else {
        *ssid = vector<uint8_t>(ptr + 2, ptr + length + 2);
      }
      return true;
    }
    ptr += 2 + length;
  }
  return false;
}

bool NetlinkUtils::Scan(uint32_t interface_index,
                        const vector<vector<uint8_t>>& ssids,
                        const vector<uint32_t>& freqs) {
  NL80211Packet trigger_scan(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_TRIGGER_SCAN,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  // If we do not use NLM_F_ACK, we only receive a unicast repsonse
  // when there is an error. If everything is good, scan results notification
  // will only be sent through multicast.
  // If NLM_F_ACK is set, there will always be an unicast repsonse, either an
  // ERROR or an ACK message. The handler will always be called and removed by
  // NetlinkManager.
  trigger_scan.AddFlag(NLM_F_ACK);
  NL80211Attr<uint32_t> if_index_attr(NL80211_ATTR_IFINDEX, interface_index);

  NL80211NestedAttr ssids_attr(NL80211_ATTR_SCAN_SSIDS);
  for (size_t i = 0; i < ssids.size(); i++) {
    ssids_attr.AddAttribute(NL80211Attr<vector<uint8_t>>(i, ssids[i]));
  }
  NL80211NestedAttr freqs_attr(NL80211_ATTR_SCAN_FREQUENCIES);
  for (size_t i = 0; i < freqs.size(); i++) {
    freqs_attr.AddAttribute(NL80211Attr<uint32_t>(i, freqs[i]));
  }

  trigger_scan.AddAttribute(if_index_attr);
  trigger_scan.AddAttribute(ssids_attr);
  // An absence of NL80211_ATTR_SCAN_FREQUENCIES attribue informs kernel to
  // scan all supported frequencies.
  if (!freqs.empty()) {
    trigger_scan.AddAttribute(freqs_attr);
  }

  // We are receiving an ERROR/ACK message instead of the actual
  // scan results here, so it is OK to expect a timely response because
  // kernel is supposed to send the ERROR/ACK back before the scan starts.
  vector<NL80211Packet> response;
  if (!netlink_manager_->SendMessageAndGetResponses(trigger_scan, &response)) {
    LOG(ERROR) << "Failed to send TriggerScan message";
    return false;
  }
  if (response.size() != 1) {
    LOG(ERROR) << "Unexpected trigger scan response size: " <<response.size();
  }
  NL80211Packet& packet = response[0];
  uint16_t type = packet.GetMessageType();
  if (type == NLMSG_ERROR) {
    // It is an ACK message if error code is 0.
    if (packet.GetErrorCode() == 0) {
      return true;
    }
    LOG(ERROR) << "Received error messsage in response to scan request "
               << strerror(packet.GetErrorCode());
  } else {
    LOG(ERROR) << "Receive unexpected message type :"
               << "in response to scan request: " << type;
  }

  return false;
}

}  // namespace wificond
}  // namespace android

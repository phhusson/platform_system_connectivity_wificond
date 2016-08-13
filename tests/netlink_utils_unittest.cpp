/*
 * Copyright (C) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <string>
#include <vector>

#include <linux/netlink.h>
#include <linux/nl80211.h>

#include <gtest/gtest.h>

#include "wificond/net/netlink_utils.h"
#include "wificond/tests/mock_netlink_manager.h"

using std::string;
using std::vector;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace android {
namespace wificond {

namespace {

constexpr uint32_t kFakeSequenceNumber = 162;
constexpr uint16_t kFakeFamilyId = 14;
constexpr uint16_t kFakeWiphyIndex = 8;
constexpr int kFakeErrorCode = EIO;
const char kFakeInterfaceName[] = "testif0";
const uint32_t kFakeInterfaceIndex = 34;

// Currently, control messages are only created by the kernel and sent to us.
// Therefore NL80211Packet doesn't have corresponding constructor.
// For test we manually create control messages using this helper function.
NL80211Packet CreateControlMessageError(int error_code) {
  vector<uint8_t> data;
  data.resize(NLMSG_HDRLEN + NLA_ALIGN(sizeof(int)), 0);
  // Initialize length field.
  nlmsghdr* nl_header = reinterpret_cast<nlmsghdr*>(data.data());
  nl_header->nlmsg_len = data.size();
  nl_header->nlmsg_type = NLMSG_ERROR;
  nl_header->nlmsg_seq = kFakeSequenceNumber;
  nl_header->nlmsg_pid = getpid();
  int* error_field = reinterpret_cast<int*>(data.data() + NLMSG_HDRLEN);
  *error_field = -error_code;

  return NL80211Packet(data);
}

}  // namespace

class NetlinkUtilsTest : public ::testing::Test {
 protected:
  std::unique_ptr<NiceMock<MockNetlinkManager>> netlink_manager_;
  std::unique_ptr<NetlinkUtils> netlink_utils_;

  virtual void SetUp() {
    netlink_manager_.reset(new NiceMock<MockNetlinkManager>());
    netlink_utils_.reset(new NetlinkUtils(netlink_manager_.get()));

    ON_CALL(*netlink_manager_,
            GetSequenceNumber()).WillByDefault(Return(kFakeSequenceNumber));
    ON_CALL(*netlink_manager_,
            GetFamilyId()).WillByDefault(Return(kFakeFamilyId));
  }
};

// This mocks the behavior of SendMessageAndGetResponses(), which returns a
// vector of NL80211Packet using passed in pointer.
ACTION_P(MakeupResponse, response) {
  // arg1 is the second parameter: vector<NL80211Packet>* responses.
  *arg1 = response;
}

TEST_F(NetlinkUtilsTest, CanGetWiphyIndex) {
  NL80211Packet new_wiphy(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_WIPHY,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  // Insert wiphy_index attribute.
  NL80211Attr<uint32_t> wiphy_index_attr(NL80211_ATTR_WIPHY, kFakeWiphyIndex);
  new_wiphy.AddAttribute(wiphy_index_attr);
  // Mock a valid response from kernel.
  vector<NL80211Packet> response = {new_wiphy};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  uint32_t wiphy_index;
  EXPECT_TRUE(netlink_utils_->GetWiphyIndex(&wiphy_index));
  EXPECT_EQ(kFakeWiphyIndex, wiphy_index);
}

TEST_F(NetlinkUtilsTest, CanHandleGetWiphyIndexError) {
  // Mock an error response from kernel.
  vector<NL80211Packet> response = {CreateControlMessageError(kFakeErrorCode)};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  uint32_t wiphy_index;
  EXPECT_FALSE(netlink_utils_->GetWiphyIndex(&wiphy_index));
}

TEST_F(NetlinkUtilsTest, CanGetInterfaceNameAndIndex) {
  NL80211Packet new_interface(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  // Insert interface name attribute.
  NL80211Attr<string> if_name_attr(NL80211_ATTR_IFNAME, string(kFakeInterfaceName));
  new_interface.AddAttribute(if_name_attr);
  // Insert interface index attribute.
  NL80211Attr<uint32_t> if_index_attr(NL80211_ATTR_IFINDEX, kFakeInterfaceIndex);
  new_interface.AddAttribute(if_index_attr);

  // Mock a valid response from kernel.
  vector<NL80211Packet> response = {new_interface};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  string interface_name;
  uint32_t interface_index;
  EXPECT_TRUE(netlink_utils_->GetInterfaceNameAndIndex(kFakeWiphyIndex,
                                                       &interface_name,
                                                       &interface_index));
  EXPECT_EQ(string(kFakeInterfaceName), interface_name);
  EXPECT_EQ(kFakeInterfaceIndex, interface_index);
}

TEST_F(NetlinkUtilsTest, HandlesPseudoDevicesInInterfaceNameAndIndexQuery) {
  // Some kernels will have extra responses ahead of the expected packet.
  NL80211Packet psuedo_interface(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  psuedo_interface.AddAttribute(NL80211Attr<uint64_t>(
      NL80211_ATTR_WDEV, 0));

  // This is the packet we're looking for
  NL80211Packet expected_interface(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  expected_interface.AddAttribute(NL80211Attr<string>(
      NL80211_ATTR_IFNAME, string(kFakeInterfaceName)));
  expected_interface.AddAttribute(NL80211Attr<uint32_t>(
      NL80211_ATTR_IFINDEX, kFakeInterfaceIndex));

  // Kernel can send us the pseduo interface packet first
  vector<NL80211Packet> response = {psuedo_interface, expected_interface};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  string interface_name;
  uint32_t interface_index;
  EXPECT_TRUE(netlink_utils_->GetInterfaceNameAndIndex(kFakeWiphyIndex,
                                                       &interface_name,
                                                       &interface_index));
  EXPECT_EQ(string(kFakeInterfaceName), interface_name);
  EXPECT_EQ(kFakeInterfaceIndex, interface_index);
}

TEST_F(NetlinkUtilsTest, HandleP2p0WhenGetInterfaceNameAndIndex) {
  NL80211Packet new_interface(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  // Insert interface name attribute.
  NL80211Attr<string> if_name_attr(NL80211_ATTR_IFNAME, string(kFakeInterfaceName));
  new_interface.AddAttribute(if_name_attr);
  // Insert interface index attribute.
  NL80211Attr<uint32_t> if_index_attr(NL80211_ATTR_IFINDEX, kFakeInterfaceIndex);
  new_interface.AddAttribute(if_index_attr);

  // Create a new interface packet for p2p0.
  NL80211Packet new_interface_p2p0(
      netlink_manager_->GetFamilyId(),
      NL80211_CMD_NEW_INTERFACE,
      netlink_manager_->GetSequenceNumber(),
      getpid());
  NL80211Attr<string> if_name_attr_p2p0(NL80211_ATTR_IFNAME, "p2p0");
  new_interface_p2p0.AddAttribute(if_name_attr_p2p0);
  // Mock response from kernel, including 2 interfaces.
  vector<NL80211Packet> response = {new_interface_p2p0, new_interface};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  string interface_name;
  uint32_t interface_index;
  EXPECT_TRUE(netlink_utils_->GetInterfaceNameAndIndex(kFakeWiphyIndex,
                                                       &interface_name,
                                                       &interface_index));
  EXPECT_EQ(string(kFakeInterfaceName), interface_name);
  EXPECT_EQ(kFakeInterfaceIndex, interface_index);
}

TEST_F(NetlinkUtilsTest, CanHandleGetInterfaceNameAndIndexError) {
  // Mock an error response from kernel.
  vector<NL80211Packet> response = {CreateControlMessageError(kFakeErrorCode)};

  EXPECT_CALL(*netlink_manager_, SendMessageAndGetResponses(_, _)).
      WillOnce(DoAll(MakeupResponse(response), Return(true)));

  string interface_name;
  uint32_t interface_index;
  EXPECT_FALSE(netlink_utils_->GetInterfaceNameAndIndex(kFakeWiphyIndex,
                                                        &interface_name,
                                                        &interface_index));
}

}  // namespace wificond
}  // namespace android

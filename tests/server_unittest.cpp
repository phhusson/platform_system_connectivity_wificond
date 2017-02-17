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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wifi_hal_test/mock_driver_tool.h>
#include <wifi_system_test/mock_hal_tool.h>
#include <wifi_system_test/mock_hostapd_manager.h>
#include <wifi_system_test/mock_interface_tool.h>
#include <wifi_system_test/mock_supplicant_manager.h>

#include "android/net/wifi/IApInterface.h"
#include "wificond/tests/mock_netlink_manager.h"
#include "wificond/tests/mock_netlink_utils.h"
#include "wificond/tests/mock_scan_utils.h"
#include "wificond/server.h"

using android::net::wifi::IApInterface;
using android::wifi_hal::DriverTool;
using android::wifi_hal::MockDriverTool;
using android::wifi_system::HalTool;
using android::wifi_system::HostapdManager;
using android::wifi_system::InterfaceTool;
using android::wifi_system::MockHalTool;
using android::wifi_system::MockHostapdManager;
using android::wifi_system::MockInterfaceTool;
using android::wifi_system::MockSupplicantManager;
using android::wifi_system::SupplicantManager;
using std::unique_ptr;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::_;

namespace android {
namespace wificond {
namespace {

class ServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(*driver_tool_, LoadDriver()).WillByDefault(Return(true));
    ON_CALL(*driver_tool_, UnloadDriver()).WillByDefault(Return(true));
    ON_CALL(*driver_tool_, ChangeFirmwareMode(_)).WillByDefault(Return(true));
    ON_CALL(*if_tool_, SetWifiUpState(_)).WillByDefault(Return(true));
    ON_CALL(*netlink_utils_, GetWiphyIndex(_)).WillByDefault(Return(true));
    ON_CALL(*netlink_utils_, GetInterfaceInfo(_, _, _, _))
        .WillByDefault(Return(true));
  }

  NiceMock<MockHalTool>* hal_tool_ = new NiceMock<MockHalTool>;
  NiceMock<MockInterfaceTool>* if_tool_ = new NiceMock<MockInterfaceTool>;
  NiceMock<MockDriverTool>* driver_tool_ = new NiceMock<MockDriverTool>;
  NiceMock<MockSupplicantManager>* supplicant_manager_ =
      new NiceMock<MockSupplicantManager>;
  NiceMock<MockHostapdManager>* hostapd_manager_ =
      new NiceMock<MockHostapdManager>;

  unique_ptr<NiceMock<MockNetlinkManager>> netlink_manager_{
      new NiceMock<MockNetlinkManager>()};

  unique_ptr<NiceMock<MockNetlinkUtils>> netlink_utils_{
      new NiceMock<MockNetlinkUtils>(netlink_manager_.get())};
  unique_ptr<NiceMock<MockScanUtils>> scan_utils_{
      new NiceMock<MockScanUtils>(netlink_manager_.get())};


  Server server_{unique_ptr<HalTool>(hal_tool_),
                 unique_ptr<InterfaceTool>(if_tool_),
                 unique_ptr<DriverTool>(driver_tool_),
                 unique_ptr<SupplicantManager>(supplicant_manager_),
                 unique_ptr<HostapdManager>(hostapd_manager_),
                 netlink_utils_.get(),
                 scan_utils_.get()};
};  // class ServerTest

}  // namespace

TEST_F(ServerTest, CanSetUpApInterface) {
  sp<IApInterface> ap_if;
  Sequence sequence;
  EXPECT_CALL(*driver_tool_, LoadDriver())
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*driver_tool_, ChangeFirmwareMode(DriverTool::kFirmwareModeAp))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*netlink_utils_, GetWiphyIndex(_))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*netlink_utils_, SubscribeRegDomainChange(_, _));
  EXPECT_CALL(*netlink_utils_, GetInterfaceInfo(_, _, _, _))
      .InSequence(sequence)
      .WillOnce(Return(true));

  EXPECT_TRUE(server_.createApInterface(&ap_if).isOk());
  EXPECT_NE(nullptr, ap_if.get());
}

TEST_F(ServerTest, DoesNotSupportMultipleInterfaces) {
  sp<IApInterface> ap_if;
  EXPECT_CALL(*netlink_utils_, GetWiphyIndex(_)).Times(1);
  EXPECT_CALL(*netlink_utils_, GetInterfaceInfo(_, _, _, _)).Times(1);

  EXPECT_TRUE(server_.createApInterface(&ap_if).isOk());
  EXPECT_NE(nullptr, ap_if.get());

  sp<IApInterface> second_ap_if;
  // We won't throw on a second interface request.
  EXPECT_TRUE(server_.createApInterface(&second_ap_if).isOk());
  // But this time we won't get an interface back.
  EXPECT_EQ(nullptr, second_ap_if.get());
}

TEST_F(ServerTest, CanDestroyInterfaces) {
  sp<IApInterface> ap_if;
  EXPECT_CALL(*netlink_utils_, GetWiphyIndex(_)).Times(2);
  EXPECT_CALL(*netlink_utils_, GetInterfaceInfo(_, _, _, _)).Times(2);
  EXPECT_CALL(*driver_tool_, UnloadDriver()).Times(0);

  EXPECT_TRUE(server_.createApInterface(&ap_if).isOk());

  // When we tear down the interface, we expect the driver to be unloaded.
  EXPECT_CALL(*driver_tool_, UnloadDriver()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*netlink_utils_, UnsubscribeRegDomainChange(_));
  EXPECT_TRUE(server_.tearDownInterfaces().isOk());
  // After a teardown, we should be able to create another interface.
  EXPECT_TRUE(server_.createApInterface(&ap_if).isOk());
}

}  // namespace wificond
}  // namespace android

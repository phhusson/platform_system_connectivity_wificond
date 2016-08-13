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
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wifi_system_test/mock_hostapd_manager.h>

#include "wificond/ap_interface_impl.h"

using android::wifi_system::HostapdManager;
using android::wifi_system::MockHostapdManager;
using std::unique_ptr;
using std::vector;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::_;

namespace android {
namespace wificond {
namespace {

const char kTestInterfaceName[] = "testwifi0";
const uint32_t kTestInterfaceIndex = 42;

class ApInterfaceImplTest : public ::testing::Test {
 protected:
  NiceMock<MockHostapdManager>* hostapd_manager_ =
      new NiceMock<MockHostapdManager>;
  ApInterfaceImpl ap_interface_{kTestInterfaceName,
                                kTestInterfaceIndex,
                                unique_ptr<HostapdManager>(hostapd_manager_)};
};  // class ApInterfaceImplTest

}  // namespace

TEST_F(ApInterfaceImplTest, ShouldReportStartFailure) {
  EXPECT_CALL(*hostapd_manager_, StartHostapd())
      .WillOnce(Return(false));
  EXPECT_FALSE(ap_interface_.StartHostapd());
}

TEST_F(ApInterfaceImplTest, ShouldReportStartSuccess) {
  EXPECT_CALL(*hostapd_manager_, StartHostapd())
      .WillOnce(Return(true));
  EXPECT_TRUE(ap_interface_.StartHostapd());
}

TEST_F(ApInterfaceImplTest, ShouldReportStopFailure) {
  EXPECT_CALL(*hostapd_manager_, StopHostapd())
      .WillOnce(Return(false));
  EXPECT_FALSE(ap_interface_.StopHostapd());
}

TEST_F(ApInterfaceImplTest, ShouldReportStopSuccess) {
  EXPECT_CALL(*hostapd_manager_, StopHostapd())
      .WillOnce(Return(true));
  EXPECT_TRUE(ap_interface_.StopHostapd());
}

TEST_F(ApInterfaceImplTest, ShouldRejectInvalidConfig) {
  EXPECT_CALL(*hostapd_manager_, CreateHostapdConfig(_, _, _, _, _, _))
      .WillOnce(Return(""));
  EXPECT_CALL(*hostapd_manager_, WriteHostapdConfig(_)).Times(0);
  EXPECT_FALSE(ap_interface_.WriteHostapdConfig(
        vector<uint8_t>(),
        false,
        0,
        HostapdManager::EncryptionType::kWpa2,
        vector<uint8_t>()));
}

}  // namespace wificond
}  // namespace android

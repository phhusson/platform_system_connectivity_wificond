/*
 * Copyright (C) 2017, The Android Open Source Project
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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wifi_system_test/mock_interface_tool.h>
#include <wifi_system_test/mock_supplicant_manager.h>

#include "wificond/scanning/scanner_impl.h"
#include "wificond/tests/mock_client_interface_impl.h"
#include "wificond/tests/mock_netlink_manager.h"
#include "wificond/tests/mock_netlink_utils.h"
#include "wificond/tests/mock_offload_service_utils.h"
#include "wificond/tests/mock_scan_utils.h"

using ::android::binder::Status;
using ::android::wifi_system::MockInterfaceTool;
using ::android::wifi_system::MockSupplicantManager;
using ::com::android::server::wifi::wificond::SingleScanSettings;
using ::com::android::server::wifi::wificond::PnoSettings;
using ::com::android::server::wifi::wificond::NativeScanResult;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

using namespace std::placeholders;

namespace android {
namespace wificond {

namespace {

constexpr uint32_t kFakeInterfaceIndex = 12;
constexpr uint32_t kFakeWiphyIndex = 5;

// This is a helper function to mock the behavior of ScanUtils::Scan()
// when we expect a error code.
// |interface_index_ignored|, |request_random_mac_ignored|, |ssids_ignored|,
// |freqs_ignored|, |error_code| are mapped to existing parameters of ScanUtils::Scan().
// |mock_error_code| is a additional parameter used for specifying expected error code.
bool ReturnErrorCodeForScanRequest(
    int mock_error_code,
    uint32_t interface_index_ignored,
    bool request_random_mac_ignored,
    const std::vector<std::vector<uint8_t>>& ssids_ignored,
    const std::vector<uint32_t>& freqs_ignored,
    int* error_code) {
  *error_code = mock_error_code;
  // Returing false because this helper function is used for failure case.
  return false;
}

}  // namespace

class ScannerTest : public ::testing::Test {
 protected:
  void SetUp() override {
   ON_CALL(*offload_service_utils_, IsOffloadScanSupported()).WillByDefault(
      Return(false));
   netlink_scanner_.reset(new ScannerImpl(
       kFakeWiphyIndex, kFakeInterfaceIndex,
       scan_capabilities_, wiphy_features_,
       &client_interface_impl_,
       &netlink_utils_, &scan_utils_, offload_service_utils_));
  }

  unique_ptr<ScannerImpl> netlink_scanner_;
  NiceMock<MockNetlinkManager> netlink_manager_;
  NiceMock<MockNetlinkUtils> netlink_utils_{&netlink_manager_};
  NiceMock<MockScanUtils> scan_utils_{&netlink_manager_};
  NiceMock<MockInterfaceTool> if_tool_;
  NiceMock<MockSupplicantManager> supplicant_manager_;
  NiceMock<MockClientInterfaceImpl> client_interface_impl_{
      &if_tool_, &supplicant_manager_, &netlink_utils_, &scan_utils_};
  shared_ptr<NiceMock<MockOffloadServiceUtils>> offload_service_utils_{
      new NiceMock<MockOffloadServiceUtils>()};
  ScanCapabilities scan_capabilities_;
  WiphyFeatures wiphy_features_;
};

TEST_F(ScannerTest, TestSingleScan) {
  EXPECT_CALL(scan_utils_, Scan(_, _, _, _, _)).WillOnce(Return(true));
  bool success = false;
  EXPECT_TRUE(netlink_scanner_->scan(SingleScanSettings(), &success).isOk());
  EXPECT_TRUE(success);
}

TEST_F(ScannerTest, TestSingleScanFailure) {
  EXPECT_CALL(
      scan_utils_,
      Scan(_, _, _, _, _)).
          WillOnce(Invoke(bind(
              ReturnErrorCodeForScanRequest, EBUSY, _1, _2, _3, _4, _5)));

  bool success = false;
  EXPECT_TRUE(netlink_scanner_->scan(SingleScanSettings(), &success).isOk());
  EXPECT_FALSE(success);
}

TEST_F(ScannerTest, TestProcessAbortsOnScanReturningNoDeviceError) {
  ON_CALL(
      scan_utils_,
      Scan(_, _, _, _, _)).
          WillByDefault(Invoke(bind(
              ReturnErrorCodeForScanRequest, ENODEV, _1, _2, _3, _4, _5)));

  bool success_ignored;
  EXPECT_DEATH(
      netlink_scanner_->scan(SingleScanSettings(), &success_ignored),
      "Driver is in a bad state*");
}

TEST_F(ScannerTest, TestAbortScan) {
  bool single_scan_success = false;
  EXPECT_CALL(scan_utils_, Scan(_, _, _, _, _)).WillOnce(Return(true));
  EXPECT_TRUE(netlink_scanner_->scan(SingleScanSettings(),
                            &single_scan_success).isOk());
  EXPECT_TRUE(single_scan_success);

  EXPECT_CALL(scan_utils_, AbortScan(_));
  EXPECT_TRUE(netlink_scanner_->abortScan().isOk());
}

TEST_F(ScannerTest, TestAbortScanNotIssuedIfNoOngoingScan) {
  EXPECT_CALL(scan_utils_, AbortScan(_)).Times(0);
  EXPECT_TRUE(netlink_scanner_->abortScan().isOk());
}

TEST_F(ScannerTest, TestGetScanResults) {
  vector<NativeScanResult> scan_results;
  EXPECT_CALL(scan_utils_, GetScanResult(_, _)).WillOnce(Return(true));
  EXPECT_TRUE(netlink_scanner_->getScanResults(&scan_results).isOk());
}

TEST_F(ScannerTest, TestStartPnoScanViaNetlink) {
  bool success = false;
  EXPECT_CALL(scan_utils_, StartScheduledScan(_, _, _, _, _, _, _, _)).
              WillOnce(Return(true));
  EXPECT_TRUE(netlink_scanner_->startPnoScan(PnoSettings(), &success).isOk());
  EXPECT_TRUE(success);
}

TEST_F(ScannerTest, TestStopPnoScanViaNetlink) {
  bool success = false;
  // StopScheduledScan() will be called no matter if there is an ongoing
  // scheduled scan or not. This is for making the system more robust.
  EXPECT_CALL(scan_utils_, StopScheduledScan(_)).WillOnce(Return(true));
  EXPECT_TRUE(netlink_scanner_->stopPnoScan(&success).isOk());
  EXPECT_TRUE(success);
}

}  // namespace wificond
}  // namespace android

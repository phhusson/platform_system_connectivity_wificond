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

#include <vector>

#include <gtest/gtest.h>
#include "wificond/tests/offload_test_utils.h"

#include "wificond/scanning/scan_result.h"
#include "wificond/scanning/offload/offload_scan_utils.h"

using android::hardware::wifi::offload::V1_0::ScanResult;
using ::com::android::server::wifi::wificond::NativeScanResult;

namespace android {
namespace wificond {

class OffloadScanUtilsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    dummy_scan_results_ = OffloadTestUtils::createOffloadScanResults();
  }

  void TearDown() override {
    dummy_scan_results_.clear();
  }

  std::vector<ScanResult> dummy_scan_results_;
};

TEST_F(OffloadScanUtilsTest, verifyConversion) {
  std::vector<NativeScanResult> native_scan_results =
      OffloadScanUtils::convertToNativeScanResults(dummy_scan_results_);
  EXPECT_EQ(native_scan_results.size(), dummy_scan_results_.size());
  for (size_t i = 0; i < native_scan_results.size(); i++) {
    EXPECT_EQ(native_scan_results[i].frequency, dummy_scan_results_[i].frequency);
    EXPECT_EQ(native_scan_results[i].tsf, dummy_scan_results_[i].tsf);
    EXPECT_EQ(native_scan_results[i].signal_mbm, dummy_scan_results_[i].rssi);
    EXPECT_EQ(native_scan_results[i].ssid.size(), dummy_scan_results_[i].networkInfo.ssid.size());
    EXPECT_EQ(native_scan_results[i].bssid.size(), dummy_scan_results_[i].bssid.elementCount());
    EXPECT_EQ(native_scan_results[i].capability, dummy_scan_results_[i].capability);
  }
}

} // namespace wificond
} // namespace android

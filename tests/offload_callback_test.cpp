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

#include <functional>
#include <memory>
#include <vector>
#include <string>

#include <gtest/gtest.h>

#include "wificond/scanning/scan_result.h"
#include "wificond/scanning/offload/offload_callback.h"
#include "wificond/tests/offload_test_utils.h"

using android::hardware::wifi::offload::V1_0::ScanResult;
using android::hardware::hidl_vec;

namespace android {
namespace wificond {

class OffloadCallbackTest: public ::testing::Test {
 protected:
   virtual void SetUp() {
     dummy_scan_results_ = OffloadTestUtils::createOffloadScanResults();
   }

   void TearDown() override {
     dummy_scan_results_.clear();
   }

   std::vector<ScanResult> dummy_scan_results_;
   std::unique_ptr<OffloadCallback> dut_;
};

/**
 * Testing OffloadCallback to invoke the registered callback handler
 * with the scan results when they are available
 */
TEST_F(OffloadCallbackTest, checkScanResultSize) {
  std::vector<ScanResult> scan_result;
  dut_.reset(new OffloadCallback(
    [&scan_result] (std::vector<ScanResult> scanResult) -> void {
      scan_result = scanResult;
    }));
  hidl_vec<ScanResult> offloadScanResult(dummy_scan_results_);
  dut_->onScanResult(offloadScanResult);
  EXPECT_EQ(dummy_scan_results_.size(), scan_result.size());
}

} // namespace wificond
} // namespace android

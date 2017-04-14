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

#include "wificond/tests/offload_test_utils.h"

using android::hardware::wifi::offload::V1_0::ScanResult;

namespace android {
namespace wificond {

std::vector<ScanResult> OffloadTestUtils::createOffloadScanResults() {
  std::vector<ScanResult> scanResults;
  ScanResult scanResult;
  std::vector<uint8_t> ssid(kSsid1, kSsid1 + sizeof(kSsid1));
  scanResult.tsf = kTsf;
  scanResult.rssi = kRssi;
  scanResult.frequency = kFrequency1;
  scanResult.capability = kCapability;
  memcpy(&scanResult.bssid[0], &kBssid[0], kBssidSize);
  scanResult.networkInfo.ssid = ssid;
  scanResult.networkInfo.flags = kNetworkFlags;
  scanResults.push_back(scanResult);
  return scanResults;
}

} // namespace wificond
} // namespace android

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

namespace {
  const uint8_t kSsid[] = { 'G', 'o', 'o', 'g', 'l', 'e' };
  const uint8_t kBssid [6] = { 0x12, 0xef, 0xa1, 0x2c, 0x97, 0x8b };
  const int16_t kRssi = -60;
  const uint32_t kFrequency = 2412;
  const uint8_t kBssidSize = 6;
  const uint64_t kTsf = 0;
  const uint16_t kCapability = 0;
  const uint8_t kNetworkFlags = 0;
} // namespace

std::vector<ScanResult> OffloadTestUtils::createOffloadScanResults() {
  std::vector<ScanResult> scanResults;
  ScanResult scanResult;
  std::vector<uint8_t> ssid(kSsid, kSsid + sizeof(kSsid));
  scanResult.tsf = kTsf;
  scanResult.rssi = kRssi;
  scanResult.frequency = kFrequency;
  scanResult.capability = kCapability;
  memcpy(&scanResult.bssid[0], &kBssid[0], kBssidSize);
  scanResult.networkInfo.ssid = ssid;
  scanResult.networkInfo.flags = kNetworkFlags;
  scanResults.push_back(scanResult);
  return scanResults;
}

} // namespace wificond
} // namespace android

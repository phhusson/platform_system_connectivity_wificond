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

#ifndef WIFICOND_OFFLOAD_TEST_UTILS_H_
#define WIFICOND_OFFLOAD_TEST_UTILS_H_

#include <android/hardware/wifi/offload/1.0/IOffload.h>
#include <vector>

using android::hardware::wifi::offload::V1_0::ScanResult;

namespace android {
namespace wificond {

namespace {
  const uint8_t kSsid1[] = { 'G', 'o', 'o', 'g', 'l', 'e' };
  const uint8_t kSsid2[] = { 'X', 'f', 'i', 'n', 'i', 't', 'y' };
  const uint8_t kBssid [6] = { 0x12, 0xef, 0xa1, 0x2c, 0x97, 0x8b };
  const int16_t kRssi = -60;
  const int16_t kRssiThreshold = -76;
  const uint32_t kFrequency1 = 2412;
  const uint32_t kFrequency2 = 2437;
  const uint8_t kBssidSize = 6;
  const uint64_t kTsf = 0;
  const uint16_t kCapability = 0;
  const uint8_t kNetworkFlags = 0;
  const uint32_t kDisconnectedModeScanIntervalMs = 5000;
} // namespace

class OffloadTestUtils {
 public:
  static std::vector<ScanResult> createOffloadScanResults();
};

} // namespace wificond
} // namespace android

#endif // WIFICOND_OFFLOAD_TEST_UTILS_H

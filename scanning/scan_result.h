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

#ifndef WIFICOND_SCANNING_SCAN_RESULT_H_
#define WIFICOND_SCANNING_SCAN_RESULT_H_

#include <vector>

namespace android {
namespace wificond {

// This is the class to represent a scan result for wificond internal use.
class ScanResult {
 public:
  ScanResult(std::vector<uint8_t>& ssid,
             std::vector<uint8_t>& bssid,
             std::vector<uint8_t>& info_element,
             uint32_t frequency,
             int32_t signal_mbm,
             uint64_t tsf);
  void DebugLog();

  // SSID of the BSS.
  std::vector<uint8_t> ssid;
  // BSSID of the BSS.
  std::vector<uint8_t> bssid;
  // Binary array containing the raw information elements from the probe
  // response/beacon.
  std::vector<uint8_t> info_element;
  // Frequency in MHz.
  uint32_t frequency;
  // Signal strength of probe response/beacon in (100 * dBm).
  int32_t signal_mbm;
  // TSF of the received probe response/beacon.
  uint64_t tsf;
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_SCANNING_SCAN_RESULT_H_

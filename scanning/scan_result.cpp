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

#include "wificond/scanning/scan_result.h"

namespace android {
namespace wificond {

ScanResult::ScanResult(std::vector<uint8_t>& ssid_,
             std::vector<uint8_t>& bssid_,
             std::vector<uint8_t>& info_element_,
             uint32_t frequency_,
             int32_t signal_mbm_,
             uint64_t tsf_)
    : ssid(ssid_),
      bssid(bssid_),
      info_element(info_element_),
      frequency(frequency_),
      signal_mbm(signal_mbm_),
      tsf(tsf_) {
}

}  // namespace wificond
}  // namespace android

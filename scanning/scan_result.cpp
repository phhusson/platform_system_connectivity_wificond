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

#include <iomanip>
#include <sstream>

#include <android-base/logging.h>

using std::string;
using std::stringstream;

namespace android {
namespace wificond {

ScanResult::ScanResult(std::vector<uint8_t>& ssid_,
             std::vector<uint8_t>& bssid_,
             std::vector<uint8_t>& info_element_,
             uint32_t frequency_,
             int32_t signal_mbm_,
             uint64_t tsf_,
             uint16_t capability_,
             bool associated_)
    : ssid(ssid_),
      bssid(bssid_),
      info_element(info_element_),
      frequency(frequency_),
      signal_mbm(signal_mbm_),
      tsf(tsf_),
      capability(capability_),
      associated(associated_) {
}

void ScanResult::DebugLog() {
  LOG(INFO) << "Scan result:";
  // |ssid| might be an encoded array but we just print it as ASCII here.
  string ssid_str(ssid.data(), ssid.data() + ssid.size());
  LOG(INFO) << "SSID: " << ssid_str;

  stringstream ss;
  string bssid_str;
  ss << std::hex << std::setfill('0') << std::setw(2);
  for (uint8_t& b : bssid) {
    ss << static_cast<int>(b);
    if (&b != &bssid.back()) {
      ss << ":";
    }
  }
  bssid_str = ss.str();
  LOG(INFO) << "BSSID: " << bssid_str;
  LOG(INFO) << "FREQUENCY: " << frequency;
  LOG(INFO) << "SIGNAL: " << signal_mbm/100 << "dBm";
  LOG(INFO) << "TSF: " << tsf;
  LOG(INFO) << "CAPABILITY: " << capability;
  LOG(INFO) << "ASSOCIATED: " << associated;

}

}  // namespace wificond
}  // namespace android

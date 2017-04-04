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
#include "wificond/scanning/offload/offload_scan_utils.h"
#include "wificond/scanning/scan_result.h"

using ::com::android::server::wifi::wificond::NativeScanResult;
using android::hardware::wifi::offload::V1_0::ScanResult;

namespace android {
namespace wificond {

std::vector<NativeScanResult> OffloadScanUtils::convertToNativeScanResults(
    const std::vector<ScanResult>& scanResult) {
  std::vector<NativeScanResult> nativeScanResult;
  nativeScanResult.reserve(scanResult.size());
  for (size_t i = 0; i < scanResult.size(); i++) {
    NativeScanResult singleScanResult;
    singleScanResult.ssid = scanResult[i].networkInfo.ssid;
    singleScanResult.bssid.assign(scanResult[i].networkInfo.ssid.begin(),
      scanResult[i].networkInfo.ssid.end());
    singleScanResult.frequency = scanResult[i].frequency;
    singleScanResult.signal_mbm = scanResult[i].rssi;
    singleScanResult.tsf = scanResult[i].tsf;
    singleScanResult.capability = scanResult[i].capability;
    singleScanResult.associated = false;
    nativeScanResult.emplace_back(singleScanResult);
  }
  return nativeScanResult;
}

} // wificond
} // android


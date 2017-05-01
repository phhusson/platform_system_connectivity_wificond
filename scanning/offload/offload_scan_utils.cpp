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

#include <android-base/logging.h>

#include "wificond/scanning/scan_result.h"

using ::com::android::server::wifi::wificond::NativeScanResult;
using android::hardware::wifi::offload::V1_0::ScanResult;
using android::hardware::wifi::offload::V1_0::ScanParam;
using android::hardware::wifi::offload::V1_0::ScanFilter;
using android::hardware::wifi::offload::V1_0::NetworkInfo;
using android::hardware::hidl_vec;
using std::vector;

namespace android {
namespace wificond {

vector<NativeScanResult> OffloadScanUtils::convertToNativeScanResults(
    const vector<ScanResult>& scan_result) {
  vector<NativeScanResult> native_scan_result;
  native_scan_result.reserve(scan_result.size());
  for (size_t i = 0; i < scan_result.size(); i++) {
    NativeScanResult single_scan_result;
    single_scan_result.ssid = scan_result[i].networkInfo.ssid;
    single_scan_result.bssid.assign(scan_result[i].networkInfo.ssid.begin(),
        scan_result[i].networkInfo.ssid.end());
    single_scan_result.frequency = scan_result[i].frequency;
    single_scan_result.signal_mbm = scan_result[i].rssi;
    single_scan_result.tsf = scan_result[i].tsf;
    single_scan_result.capability = scan_result[i].capability;
    single_scan_result.associated = false;
    native_scan_result.emplace_back(single_scan_result);
  }
  return native_scan_result;
}

ScanParam OffloadScanUtils::createScanParam(
    const vector<vector<uint8_t>>& ssid_list,
    const vector<uint32_t>& frequency_list, uint32_t scan_interval_ms) {
  ScanParam scan_param;
  scan_param.disconnectedModeScanIntervalMs = scan_interval_ms;
  scan_param.frequencyList = frequency_list;
  vector<hidl_vec<uint8_t>> ssid_list_tmp;
  for (const auto& ssid : ssid_list) {
    ssid_list_tmp.push_back(ssid);
  }
  scan_param.ssidList = ssid_list_tmp;
  return scan_param;
}

ScanFilter OffloadScanUtils::createScanFilter(
    const vector<vector<uint8_t>>& ssids,
    const vector<uint8_t>& flags, int8_t rssi_threshold) {
  ScanFilter scan_filter;
  vector<NetworkInfo> nw_info_list;
  size_t i = 0;
  scan_filter.rssiThreshold = rssi_threshold;
  // Note that the number of ssids should match the number of security flags
  for (const auto& ssid : ssids) {
      NetworkInfo nw_info;
      nw_info.ssid = ssid;
      if (i < flags.size()) {
        nw_info.flags = flags[i++];
      } else {
        continue;
      }
      nw_info_list.push_back(nw_info);
  }
  scan_filter.preferredNetworkInfoList = nw_info_list;
  return scan_filter;
}

} // namespace wificond
} // namespace android


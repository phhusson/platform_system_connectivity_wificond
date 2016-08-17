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

#ifndef WIFICOND_SCANNING_SCAN_UTILS_H_
#define WIFICOND_SCANNING_SCAN_UTILS_H_

#include <vector>

#include <android-base/macros.h>

namespace android {
namespace wificond {

class NL80211Packet;
class NetlinkManager;
class ScanResult;

// Provides scanning helper functions.
class ScanUtils {
 public:
  explicit ScanUtils(NetlinkManager* netlink_manager);
  virtual ~ScanUtils();

  // Send 'get scan results' request to kernel and get the latest scan results.
  // |interface_index| is the index of interface we want to get scan results
  // from.
  // A vector of ScanResult object will be returned by |*out_scan_results|.
  // Returns true on success.
  virtual bool GetScanResult(uint32_t interface_index,
                             std::vector<ScanResult>* out_scan_results);

  // Send scan request to kernel for interface with index |interface_index|.
  // |ssids| is a vector of ssids we request to scan, which mostly is used
  // for hidden networks.
  // If |ssids| is an empty vector, it will do a passive scan.
  // If |ssids| contains an empty string, it will a scan for all ssids.
  // |freqs| is a vector of frequencies we request to scan.
  // If |freqs| is an empty vector, it will scan all supported frequencies.
  // Returns true on success.
  virtual bool Scan(uint32_t interface_index,
                    const std::vector<std::vector<uint8_t>>& ssids,
                    const std::vector<uint32_t>& freqs);

 private:
  bool GetSSIDFromInfoElement(const std::vector<uint8_t>& ie,
                              std::vector<uint8_t>* ssid);
  // Converts a NL80211_CMD_NEW_SCAN_RESULTS packet to a ScanResult object.
  bool ParseScanResult(const NL80211Packet& packet, ScanResult* scan_result);

  NetlinkManager* netlink_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScanUtils);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_SCANNING_SCAN_UTILS_H_

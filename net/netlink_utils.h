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

#ifndef WIFICOND_NET_NETLINK_UTILS_H_
#define WIFICOND_NET_NETLINK_UTILS_H_

#include <string>
#include <vector>

#include <android-base/macros.h>

namespace android {
namespace wificond {

struct BandInfo {
  BandInfo() = default;
  BandInfo(std::vector<uint32_t>& band_2g_,
           std::vector<uint32_t>& band_5g_,
           std::vector<uint32_t>& band_dfs_)
      : band_2g(band_2g_),
        band_5g(band_5g_),
        band_dfs(band_dfs_) {}
  // Frequencies for 2.4 GHz band.
  std::vector<uint32_t> band_2g;
  // Frequencies for 5 GHz band without DFS.
  std::vector<uint32_t> band_5g;
  // Frequencies for DFS.
  std::vector<uint32_t> band_dfs;
};

struct ScanCapabilities {
  ScanCapabilities() = default;
  ScanCapabilities(uint8_t max_num_scan_ssids_,
                   uint8_t max_num_sched_scan_ssids_,
                   uint8_t max_match_sets_)
      : max_num_scan_ssids(max_num_scan_ssids_),
        max_num_sched_scan_ssids(max_num_sched_scan_ssids_),
        max_match_sets(max_match_sets_) {}
  // Number of SSIDs you can scan with a single scan request.
  uint8_t max_num_scan_ssids;
  // Number of SSIDs you can scan with a single scheduled scan request.
  uint8_t max_num_sched_scan_ssids;
  // Maximum number of sets that can be used with NL80211_ATTR_SCHED_SCAN_MATCH.
  uint8_t max_match_sets;
};

class NetlinkManager;
class NL80211Packet;

// Provides NL80211 helper functions.
class NetlinkUtils {
 public:
  explicit NetlinkUtils(NetlinkManager* netlink_manager);
  virtual ~NetlinkUtils();

  // Get the wiphy index from kernel.
  // |*out_wiphy_index| returns the wiphy index from kernel.
  // Returns true on success.
  virtual bool GetWiphyIndex(uint32_t* out_wiphy_index);

  // Get wifi interface info from kernel.
  // |wiphy_index| is the wiphy index we get using GetWiphyIndex().
  // Returns true on success.
  virtual bool GetInterfaceInfo(uint32_t wiphy_index,
                                std::string* name,
                                uint32_t* index,
                                std::vector<uint8_t>* mac_addr);

  virtual bool GetWiphyInfo(uint32_t wiphy_index,
                            BandInfo* out_band_info,
                            ScanCapabilities* out_scan_capabilities);

 private:
  bool ParseBandInfo(const NL80211Packet* const packet,
                     BandInfo* out_band_info);
  bool ParseScanCapabilities(const NL80211Packet* const packet,
                             ScanCapabilities* out_scan_capabilities);
  NetlinkManager* netlink_manager_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkUtils);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_NET_NETLINK_UTILS_H_

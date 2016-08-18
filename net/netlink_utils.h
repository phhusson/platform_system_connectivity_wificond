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

#include <android-base/macros.h>

namespace android {
namespace wificond {

class NL80211Packet;
class NetlinkManager;

// Provides NL80211 helper functions.
class NetlinkUtils {
 public:
  explicit NetlinkUtils(NetlinkManager* netlink_manager);
  virtual ~NetlinkUtils();

  // Get the wiphy index from kernel.
  // |*out_wiphy_index| returns the wiphy index from kernel.
  // Returns true on success.
  virtual bool GetWiphyIndex(uint32_t* out_wiphy_index);

  // Get wifi interface name from kernel.
  // |wiphy_index| is the wiphy index we get using GetWiphyIndex().
  // Returns true on success.
  virtual bool GetInterfaceNameAndIndex(uint32_t wiphy_index,
                                        std::string* interface_name,
                                        uint32_t* interface_index);

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

  NetlinkManager* netlink_manager_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkUtils);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_NET_NETLINK_UTILS_H_

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

#ifndef WIFICOND_CLIENT_INTERFACE_IMPL_H_
#define WIFICOND_CLIENT_INTERFACE_IMPL_H_

#include <string>

#include <android-base/macros.h>
#include <utils/StrongPointer.h>
#include <wifi_system/interface_tool.h>
#include <wifi_system/supplicant_manager.h>

#include "android/net/wifi/IClientInterface.h"

namespace android {
namespace wificond {

class ClientInterfaceBinder;
class NetlinkUtils;
class ScanUtils;

// Holds the guts of how we control network interfaces capable of connecting to
// access points via wpa_supplicant.
//
// Because remote processes may hold on to the corresponding
// binder object past the lifetime of the local object, we are forced to
// keep this object separate from the binder representation of itself.
class ClientInterfaceImpl {
 public:
  ClientInterfaceImpl(
      const std::string& interface_name,
      uint32_t interface_index,
      const std::vector<uint8_t>& interface_mac_addr,
      android::wifi_system::InterfaceTool* if_tool,
      android::wifi_system::SupplicantManager* supplicant_manager,
      NetlinkUtils* netlink_utils,
      ScanUtils* scan_utils);
  ~ClientInterfaceImpl();

  // Get a pointer to the binder representing this ClientInterfaceImpl.
  android::sp<android::net::wifi::IClientInterface> GetBinder() const;

  bool EnableSupplicant();
  bool DisableSupplicant();
  bool GetPacketCounters(std::vector<int32_t>* out_packet_counters);
  bool SignalPoll(std::vector<int32_t>* out_signal_poll_results);
  const std::vector<uint8_t>& GetMacAddress();
  const std::string& GetInterfaceName() const { return interface_name_; }
  bool requestANQP(
      const ::std::vector<uint8_t>& bssid,
      const ::android::sp<::android::net::wifi::IANQPDoneCallback>& callback);

 private:
  void OnScanResultsReady(uint32_t interface_index,
                          bool aborted,
                          std::vector<std::vector<uint8_t>>& ssids,
                          std::vector<uint32_t>& frequencies);

  const std::string interface_name_;
  const uint32_t interface_index_;
  const std::vector<uint8_t> interface_mac_addr_;
  android::wifi_system::InterfaceTool* const if_tool_;
  android::wifi_system::SupplicantManager* const supplicant_manager_;
  NetlinkUtils* const netlink_utils_;
  ScanUtils* const scan_utils_;
  const android::sp<ClientInterfaceBinder> binder_;

  DISALLOW_COPY_AND_ASSIGN(ClientInterfaceImpl);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_CLIENT_INTERFACE_IMPL_H_

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

#include "wificond/client_interface_impl.h"

#include <vector>

#include <android-base/logging.h>
#include <wifi_system/supplicant_manager.h>
#include <wifi_system/wifi.h>

#include "wificond/client_interface_binder.h"
#include "wificond/net/netlink_utils.h"
#include "wificond/scanning/scan_result.h"
#include "wificond/scanning/scan_utils.h"

using android::net::wifi::IClientInterface;
using android::sp;
using android::wifi_system::SupplicantManager;

using namespace std::placeholders;
using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace wificond {

ClientInterfaceImpl::ClientInterfaceImpl(
    const std::string& interface_name,
    uint32_t interface_index,
    const std::vector<uint8_t>& interface_mac_addr,
    SupplicantManager* supplicant_manager,
    NetlinkUtils* netlink_utils,
    ScanUtils* scan_utils)
    : interface_name_(interface_name),
      interface_index_(interface_index),
      interface_mac_addr_(interface_mac_addr),
      supplicant_manager_(supplicant_manager),
      netlink_utils_(netlink_utils),
      scan_utils_(scan_utils),
      binder_(new ClientInterfaceBinder(this)) {
  scan_utils_->SubscribeScanResultNotification(
      interface_index_,
      std::bind(&ClientInterfaceImpl::OnScanResultsReady,
                this,
                _1, _2, _3, _4));
}

ClientInterfaceImpl::~ClientInterfaceImpl() {
  binder_->NotifyImplDead();
  DisableSupplicant();
  scan_utils_->UnsubscribeScanResultNotification(interface_index_);
}

sp<android::net::wifi::IClientInterface> ClientInterfaceImpl::GetBinder() const {
  return binder_;
}

bool ClientInterfaceImpl::EnableSupplicant() {
  return supplicant_manager_->StartSupplicant();
}

bool ClientInterfaceImpl::DisableSupplicant() {
  return supplicant_manager_->StopSupplicant();
}

bool ClientInterfaceImpl::GetPacketCounters(vector<int32_t>* out_packet_counters) {
  StationInfo station_info;
  if(!netlink_utils_->GetStationInfo(interface_index_,
                                     interface_mac_addr_,
                                     &station_info)) {
    return false;
  }
  out_packet_counters->push_back(station_info.station_tx_packets);
  out_packet_counters->push_back(station_info.station_tx_failed);
  return true;
}

void ClientInterfaceImpl::OnScanResultsReady(
                         uint32_t interface_index,
                         bool aborted,
                         std::vector<std::vector<uint8_t>>& ssids,
                         std::vector<uint32_t>& frequencies) {
  if (aborted) {
    LOG(ERROR) << "Scan aborted";
    return;
  }
  vector<ScanResult> scan_results;
  // TODO(nywang): Find a way to differentiate scan results for
  // internel/external scan request. This is useful when location is
  // scanning using regular NL80211 commands.
  scan_utils_->GetScanResult(interface_index, &scan_results);
  // TODO(nywang): Send these scan results back to java framework.
}

}  // namespace wificond
}  // namespace android

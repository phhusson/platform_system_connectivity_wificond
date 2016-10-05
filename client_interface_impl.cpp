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
#include "wificond/net/mlme_event.h"
#include "wificond/net/netlink_utils.h"
#include "wificond/scanning/scan_result.h"
#include "wificond/scanning/scan_utils.h"

using android::net::wifi::IClientInterface;
using android::sp;
using android::wifi_system::InterfaceTool;
using android::wifi_system::SupplicantManager;

using namespace std::placeholders;
using std::string;
using std::unique_ptr;
using std::vector;

namespace android {
namespace wificond {

MlmeEventHandlerImpl::MlmeEventHandlerImpl(ClientInterfaceImpl* client_interface)
    : client_interface_(client_interface) {
}

MlmeEventHandlerImpl::~MlmeEventHandlerImpl() {
}

void MlmeEventHandlerImpl::OnConnect(unique_ptr<MlmeConnectEvent> event) {
  if (event->GetStatusCode() == 0) {
    client_interface_->RefreshAssociateFreq();
    client_interface_->bssid_ = event->GetBSSID();
  }
}

void MlmeEventHandlerImpl::OnRoam(unique_ptr<MlmeRoamEvent> event) {
  if (event->GetStatusCode() == 0) {
    client_interface_->RefreshAssociateFreq();
    client_interface_->bssid_ = event->GetBSSID();
  }
}

void MlmeEventHandlerImpl::OnAssociate(unique_ptr<MlmeAssociateEvent> event) {
  if (event->GetStatusCode() == 0) {
    client_interface_->RefreshAssociateFreq();
    client_interface_->bssid_ = event->GetBSSID();
  }
}

ClientInterfaceImpl::ClientInterfaceImpl(
    const std::string& interface_name,
    uint32_t interface_index,
    const std::vector<uint8_t>& interface_mac_addr,
    InterfaceTool* if_tool,
    SupplicantManager* supplicant_manager,
    NetlinkUtils* netlink_utils,
    ScanUtils* scan_utils)
    : interface_name_(interface_name),
      interface_index_(interface_index),
      interface_mac_addr_(interface_mac_addr),
      if_tool_(if_tool),
      supplicant_manager_(supplicant_manager),
      netlink_utils_(netlink_utils),
      scan_utils_(scan_utils),
      mlme_event_handler_(new MlmeEventHandlerImpl(this)),
      binder_(new ClientInterfaceBinder(this)) {
  scan_utils_->SubscribeScanResultNotification(
      interface_index_,
      std::bind(&ClientInterfaceImpl::OnScanResultsReady,
                this,
                _1, _2, _3, _4));
  netlink_utils_->SubscribeMlmeEvent(
      interface_index_,
      mlme_event_handler_.get());
}

ClientInterfaceImpl::~ClientInterfaceImpl() {
  binder_->NotifyImplDead();
  DisableSupplicant();
  scan_utils_->UnsubscribeScanResultNotification(interface_index_);
  netlink_utils_->UnsubscribeMlmeEvent(interface_index_);
  if_tool_->SetUpState(interface_name_.c_str(), false);
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
  if (!netlink_utils_->GetStationInfo(interface_index_,
                                      interface_mac_addr_,
                                      &station_info)) {
    return false;
  }
  out_packet_counters->push_back(station_info.station_tx_packets);
  out_packet_counters->push_back(station_info.station_tx_failed);

  return true;
}

bool ClientInterfaceImpl::SignalPoll(vector<int32_t>* out_signal_poll_results) {
  StationInfo station_info;
  if (!netlink_utils_->GetStationInfo(interface_index_,
                                      interface_mac_addr_,
                                      &station_info)) {
    return false;
  }
  out_signal_poll_results->push_back(
      static_cast<int32_t>(station_info.current_rssi));
  // Convert from 100kbit/s to Mbps.
  out_signal_poll_results->push_back(
      static_cast<int32_t>(station_info.station_tx_bitrate/10));

  return true;
}

const vector<uint8_t>& ClientInterfaceImpl::GetMacAddress() {
  return interface_mac_addr_;
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

void ClientInterfaceImpl::OnSchedScanResultsReady(
                         uint32_t interface_index) {
  vector<ScanResult> scan_results;
  scan_utils_->GetScanResult(interface_index, &scan_results);
  // TODO(nywang): Send these scan results back to java framework.
}

bool ClientInterfaceImpl::requestANQP(
      const ::std::vector<uint8_t>& bssid,
      const ::android::sp<::android::net::wifi::IANQPDoneCallback>& callback) {
  // TODO(nywang): query ANQP information from wpa_supplicant.
  return true;
}

bool ClientInterfaceImpl::RefreshAssociateFreq() {
  // wpa_supplicant fetches associate frequency using the latest scan result.
  // We should follow the same method here before we find a better solution.
  std::vector<ScanResult> scan_results;
  if (!scan_utils_->GetScanResult(interface_index_, &scan_results)) {
    return false;
  }
  for (auto& scan_result : scan_results) {
    if (scan_result.associated) {
      associate_freq_ = scan_result.frequency;
    }
  }
  return false;
}

}  // namespace wificond
}  // namespace android

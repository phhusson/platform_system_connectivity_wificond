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

#include "wificond/scanning/scanner_impl.h"

#include <string>
#include <vector>

#include <android-base/logging.h>

#include "wificond/scanning/scan_utils.h"

using android::binder::Status;
using android::net::wifi::IScanEvent;
using android::String16;
using android::sp;
using com::android::server::wifi::wificond::NativeScanResult;
using com::android::server::wifi::wificond::SingleScanSettings;
using std::string;
using std::vector;

using namespace std::placeholders;

namespace android {
namespace wificond {

ScannerImpl::ScannerImpl(uint32_t interface_index,
                         const BandInfo& band_info,
                         const ScanCapabilities& scan_capabilities,
                         const WiphyFeatures& wiphy_features,
                         ScanUtils* scan_utils)
    : valid_(true),
      interface_index_(interface_index),
      band_info_(band_info),
      scan_capabilities_(scan_capabilities),
      wiphy_features_(wiphy_features),
      scan_utils_(scan_utils),
      scan_event_handler_(nullptr) {}

ScannerImpl::~ScannerImpl() {
  if (scan_event_handler_ != nullptr) {
    scan_utils_->UnsubscribeScanResultNotification(interface_index_);
  }
}

bool ScannerImpl::CheckIsValid() {
  if (!valid_) {
    LOG(DEBUG) << "Calling on a invalid scanner object."
               << "Underlying client interface object was destroyed.";
  }
  return valid_;
}

Status ScannerImpl::getAvailable2gChannels(vector<int32_t>* out_frequencies) {
  if (!CheckIsValid()) {
    return Status::ok();
  }
  *out_frequencies = vector<int32_t>(band_info_.band_2g.begin(),
                                     band_info_.band_2g.end());
  return Status::ok();
}

Status ScannerImpl::getAvailable5gNonDFSChannels(
    vector<int32_t>* out_frequencies) {
  if (!CheckIsValid()) {
    return Status::ok();
  }
  *out_frequencies = vector<int32_t>(band_info_.band_5g.begin(),
                                     band_info_.band_5g.end());
  return Status::ok();
}

Status ScannerImpl::getAvailableDFSChannels(vector<int32_t>* out_frequencies) {
  if (!CheckIsValid()) {
    return Status::ok();
  }
  *out_frequencies = vector<int32_t>(band_info_.band_dfs.begin(),
                                     band_info_.band_dfs.end());
  return Status::ok();
}

Status ScannerImpl::getScanResults(vector<NativeScanResult>* out_scan_results) {
  if (!CheckIsValid()) {
    return Status::ok();
  }
  if (!scan_utils_->GetScanResult(interface_index_, out_scan_results)) {
    LOG(ERROR) << "Failed to get scan results via NL80211";
  }
  return Status::ok();
}

Status ScannerImpl::scan(const SingleScanSettings& scan_settings,
                         bool* out_success) {
  if (!CheckIsValid()) {
    return Status::ok();
  }
  if (scan_settings.is_full_scan_) {
    if (!scan_utils_->StartFullScan(interface_index_)) {
      *out_success = false;
      return Status::ok();
    }
  }

  // Initialize it with an empty ssid for a wild card scan.
  vector<vector<uint8_t>> ssids = {{0}};
  for (auto& network : scan_settings.hidden_networks_) {
    ssids.push_back(network.ssid_);
  }
  vector<uint32_t> freqs;
  for (auto& channel : scan_settings.channel_settings_) {
    freqs.push_back(channel.frequency_);
  }

  if (!scan_utils_->Scan(interface_index_, ssids, freqs)) {
    *out_success = false;
    LOG(ERROR) << "Failed to start a scan";
    return Status::ok();
  }
  *out_success = true;
  return Status::ok();
}

Status ScannerImpl::SubscribeScanEvents(const sp<IScanEvent>& handler) {
  if (scan_event_handler_ != nullptr) {
    LOG(ERROR) << "Found existing scan events subscriber."
               << " This subscription request will unsubscribe it";
  }
  scan_event_handler_ = handler;
  scan_utils_->SubscribeScanResultNotification(
      interface_index_,
      std::bind(&ScannerImpl::OnScanResultsReady,
                this,
                _1, _2, _3, _4));

  return Status::ok();
}

Status ScannerImpl::UnsubscribeScanEvents() {
  scan_utils_->UnsubscribeScanResultNotification(interface_index_);
  scan_event_handler_ = nullptr;
  return Status::ok();
}

void ScannerImpl::OnScanResultsReady(
    uint32_t interface_index,
    bool aborted,
    vector<vector<uint8_t>>& ssids,
    vector<uint32_t>& frequencies) {
  if (scan_event_handler_ != nullptr) {
    // TODO: Pass other parameters back once we find framework needs them.
    scan_event_handler_->OnScanResultReady();
  }
}

}  // namespace wificond
}  // namespace android

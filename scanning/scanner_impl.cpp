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

#include <vector>

#include <android-base/logging.h>

#include "wificond/scanning/scan_utils.h"

using android::binder::Status;
using com::android::server::wifi::wificond::NativeScanResult;
using std::vector;

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
      scan_utils_(scan_utils) {
}

ScannerImpl::~ScannerImpl() {
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

}  // namespace wificond
}  // namespace android

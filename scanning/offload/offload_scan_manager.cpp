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
#include "wificond/scanning/offload/offload_scan_manager.h"

#include <vector>

#include <android-base/logging.h>

#include "wificond/scanning/offload/offload_scan_utils.h"
#include "wificond/scanning/offload/offload_service_utils.h"
#include "wificond/scanning/scan_result.h"

using ::android::hardware::hidl_vec;
using android::hardware::wifi::offload::V1_0::IOffload;
using android::hardware::wifi::offload::V1_0::ScanResult;
using android::wificond::OffloadCallback;
using ::com::android::server::wifi::wificond::NativeScanResult;
using android::wificond::OnNativeScanResultsReadyHandler;

namespace android {
namespace wificond {

OffloadScanManager::OffloadScanManager(OffloadServiceUtils *utils,
    OnNativeScanResultsReadyHandler handler)
    : wifi_offload_hal_(nullptr),
      wifi_offload_callback_(nullptr),
      scan_result_handler_(handler) {
  if (utils == nullptr) return;
  if (scan_result_handler_ == nullptr) return;
  wifi_offload_hal_ = utils->GetOffloadService();
  if (wifi_offload_hal_ == nullptr) {
    LOG(WARNING) << "No Offload Service available";
    return;
  }
  wifi_offload_callback_ = utils->GetOffloadCallback(
      std::bind(&OffloadScanManager::ReportScanResults, this,
          std::placeholders::_1));
  wifi_offload_hal_->setEventCallback(wifi_offload_callback_);
}

OffloadScanManager::~OffloadScanManager() {}

void OffloadScanManager::ReportScanResults(
    const std::vector<ScanResult> scanResult) {
  if (scan_result_handler_ != nullptr) {
    scan_result_handler_(OffloadScanUtils::convertToNativeScanResults(scanResult));
  }
}

bool OffloadScanManager::isServiceAvailable() const {
  return wifi_offload_hal_ != nullptr;
}

}  // namespace wificond
}  // namespace android

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
using android::hardware::wifi::offload::V1_0::OffloadStatus;

using android::wificond::OffloadCallback;
using android::wificond::OnNativeScanResultsReadyHandler;
using ::com::android::server::wifi::wificond::NativeScanResult;

using namespace std::placeholders;

namespace android {
namespace wificond {

OffloadCallbackHandlersImpl::OffloadCallbackHandlersImpl(OffloadScanManager* offload_scan_manager)
    : offload_scan_manager_(offload_scan_manager) {
}

OffloadCallbackHandlersImpl::~OffloadCallbackHandlersImpl() {}

void OffloadCallbackHandlersImpl::OnScanResultHandler(const std::vector<ScanResult>& scanResult) {
  if (offload_scan_manager_ != nullptr) {
    offload_scan_manager_->ReportScanResults(scanResult);
  }
}

void OffloadCallbackHandlersImpl::OnErrorHandler(OffloadStatus status) {
  if (offload_scan_manager_ != nullptr) {
    offload_scan_manager_->ReportError(status);
  }
}

OffloadScanManager::OffloadScanManager(OffloadServiceUtils *utils,
    OnNativeScanResultsReadyHandler handler)
    : wifi_offload_hal_(nullptr),
      wifi_offload_callback_(nullptr),
      scan_result_handler_(handler),
      offload_status_(OffloadScanManager::kError),
      offload_callback_handlers_(new OffloadCallbackHandlersImpl(this)) {
  if (utils == nullptr) {
    LOG(ERROR) << "Invalid arguments for Offload ScanManager";
    return;
  }
  if (scan_result_handler_ == nullptr) {
    LOG(ERROR) << "Invalid Offload scan result handler";
    return;
  }
  wifi_offload_hal_ = utils->GetOffloadService();
  if (wifi_offload_hal_ == nullptr) {
    LOG(WARNING) << "No Offload Service available";
    offload_status_ = OffloadScanManager::kNoService;
    return;
  }
  wifi_offload_callback_ = utils->GetOffloadCallback(
      offload_callback_handlers_.get());
  if (wifi_offload_callback_ == nullptr) {
    offload_status_ = OffloadScanManager::kNoService;
    LOG(ERROR) << "Invalid Offload callback object";
    return;
  }
  wifi_offload_hal_->setEventCallback(wifi_offload_callback_);
  offload_status_ = OffloadScanManager::kNoError;
}

OffloadScanManager::~OffloadScanManager() {}

void OffloadScanManager::ReportScanResults(
    const std::vector<ScanResult> scanResult) {
  if (scan_result_handler_ != nullptr) {
    scan_result_handler_(OffloadScanUtils::convertToNativeScanResults(scanResult));
  } else {
    LOG(ERROR) << "No scan result handler for Offload ScanManager";
  }
}

void OffloadScanManager::ReportError(OffloadStatus status) {
  StatusCode status_result = OffloadScanManager::kNoError;
  switch(status) {
    case OffloadStatus::OFFLOAD_STATUS_OK:
      status_result = OffloadScanManager::kNoError;
      break;
    case OffloadStatus::OFFLOAD_STATUS_TIMEOUT:
      status_result = OffloadScanManager::kTimeOut;
      break;
    case OffloadStatus::OFFLOAD_STATUS_NO_CONNECTION:
      status_result = OffloadScanManager::kNotConnected;
      break;
    case OffloadStatus::OFFLOAD_STATUS_ERROR:
      status_result = OffloadScanManager::kError;
      break;
    default:
      LOG(WARNING) << "Invalid Offload Error reported";
      return;
  }
  if (status_result != OffloadScanManager::kNoError) {
    LOG(WARNING) << "Offload Error reported " << status_result;
  }
  offload_status_ = status_result;
}

OffloadScanManager::StatusCode OffloadScanManager::getOffloadStatus() const {
  return offload_status_;
}

}  // namespace wificond
}  // namespace android

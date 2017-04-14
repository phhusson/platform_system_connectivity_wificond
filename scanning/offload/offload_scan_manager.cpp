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
#include "wificond/scanning/offload/scan_stats.h"

using ::android::hardware::hidl_vec;
using android::hardware::wifi::offload::V1_0::IOffload;
using android::hardware::wifi::offload::V1_0::ScanResult;
using android::hardware::wifi::offload::V1_0::ScanFilter;
using android::hardware::wifi::offload::V1_0::ScanParam;
using android::hardware::wifi::offload::V1_0::ScanStats;
using android::hardware::wifi::offload::V1_0::OffloadStatus;

using android::wificond::OffloadCallback;
using android::wificond::OnNativeScanResultsReadyHandler;
using ::com::android::server::wifi::wificond::NativeScanResult;
using ::com::android::server::wifi::wificond::NativeScanStats;
using namespace std::placeholders;
using std::vector;

namespace {
  const uint32_t kSubscriptionDelayMs = 5000;
}

namespace android {
namespace wificond {

OffloadCallbackHandlersImpl::OffloadCallbackHandlersImpl(
    OffloadScanManager* offload_scan_manager)
        : offload_scan_manager_(offload_scan_manager) {
}

OffloadCallbackHandlersImpl::~OffloadCallbackHandlersImpl() {}

void OffloadCallbackHandlersImpl::OnScanResultHandler(
    const vector<ScanResult>& scanResult) {
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
      offload_status_(OffloadScanManager::kError),
      subscription_enabled_(false),
      offload_callback_handlers_(new OffloadCallbackHandlersImpl(this)),
      scan_result_handler_(handler) {
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

bool OffloadScanManager::stopScan(OffloadScanManager::ReasonCode* reason_code) {
  if (!subscription_enabled_) {
    LOG(INFO) << "Scans are not subscribed over Offload HAL";
    *reason_code = OffloadScanManager::kNotSubscribed;
    return false;
  }
  if (offload_status_ != OffloadScanManager::kNoService) {
    wifi_offload_hal_->unsubscribeScanResults();
    subscription_enabled_ = false;
  }
  *reason_code = OffloadScanManager::kNone;
  return true;
}

bool OffloadScanManager::startScan(
    uint32_t interval_ms,
    int32_t rssi_threshold,
    const vector<vector<uint8_t>>& scan_ssids,
    const vector<vector<uint8_t>>& match_ssids,
    const vector<uint8_t>& match_security,
    const vector<uint32_t> &freqs,
    OffloadScanManager::ReasonCode* reason_code) {
  if (offload_status_ == OffloadScanManager::kNoService) {
    *reason_code = OffloadScanManager::kNotSupported;
    LOG(WARNING) << "Offload HAL scans are not supported";
    return false;
  } else if (offload_status_ == OffloadScanManager::kNotConnected) {
    LOG(WARNING) << "Offload HAL scans are not available";
    *reason_code = OffloadScanManager::kNotAvailable;
    return false;
  }

  ScanParam param = OffloadScanUtils::createScanParam(scan_ssids, freqs,
      interval_ms);
  ScanFilter filter = OffloadScanUtils::createScanFilter(match_ssids,
      match_security, rssi_threshold);

  wifi_offload_hal_->configureScans(param, filter);
  if (!subscription_enabled_) {
    wifi_offload_hal_->subscribeScanResults(kSubscriptionDelayMs);
  }
  subscription_enabled_ = true;
  *reason_code = OffloadScanManager::kNone;
  return true;
}

OffloadScanManager::StatusCode OffloadScanManager::getOffloadStatus() const {
  return offload_status_;
}

bool OffloadScanManager::getScanStats(NativeScanStats* native_scan_stats) {
  if (offload_status_ != OffloadScanManager::kNoError) {
    LOG(ERROR) << "Unable to get scan stats due to Wifi Offload HAL error";
    return false;
  }
  wifi_offload_hal_->getScanStats(
      [&native_scan_stats] (ScanStats offload_scan_stats)-> void {
          *native_scan_stats =
              OffloadScanUtils::convertToNativeScanStats(offload_scan_stats);
      });
  return true;
}

OffloadScanManager::~OffloadScanManager() {}

void OffloadScanManager::ReportScanResults(
    const vector<ScanResult> scanResult) {
  if (scan_result_handler_ != nullptr) {
    scan_result_handler_(
        OffloadScanUtils::convertToNativeScanResults(scanResult));
  } else {
    LOG(ERROR) << "No scan result handler for Offload ScanManager";
  }
}

void OffloadScanManager::ReportError(OffloadStatus status) {
  OffloadScanManager::StatusCode status_result = OffloadScanManager::kNoError;
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

}  // namespace wificond
}  // namespace android

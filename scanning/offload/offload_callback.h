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
#ifndef ANDROID_HARDWARE_WIFI_OFFLOAD_V1_0_OFFLOADCALLBACK_H
#define ANDROID_HARDWARE_WIFI_OFFLOAD_V1_0_OFFLOADCALLBACK_H

#include "wificond/scanning/offload/offload_callback_handlers.h"
#include <android/hardware/wifi/offload/1.0/IOffloadCallback.h>
#include <hidl/Status.h>
#include <vector>

namespace android {
namespace wificond {

using ::android::hardware::hidl_vec;
using ::android::hardware::wifi::offload::V1_0::IOffloadCallback;
using ::android::hardware::wifi::offload::V1_0::OffloadStatus;
using ::android::hardware::wifi::offload::V1_0::ScanResult;
using ::android::hardware::Void;

class OffloadCallback : public IOffloadCallback {
 public:
  explicit OffloadCallback(OffloadCallbackHandlers* handlers);
  virtual ~OffloadCallback();

  // Methods from ::android::hardware::wifi::offload::V1_0::IOffloadCallback follow.
  ::android::hardware::Return<void> onScanResult(
      const hidl_vec<ScanResult>& scanResult) override;
  ::android::hardware::Return<void> onError(OffloadStatus status) override;
  // Methods from ::android::hidl::base::V1_0::IBase follow.

 private:
  OffloadCallbackHandlers* handlers_;
};

}  // namespace wificond
}  // namespace android

#endif  // ANDROID_HARDWARE_WIFI_OFFLOAD_V1_0_OFFLOADCALLBACK_H

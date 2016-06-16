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

#include "chip.h"

#include "android-base/logging.h"

using std::vector;
using android::sp;

namespace android {
namespace wificond {

Chip::Chip()
    : client_interface_id_(0) {
}

android::binder::Status Chip::RegisterCallback(const sp<android::net::wifi::IChipCallback>& callback) {
  for (auto& it : chip_callbacks_) {
    if (IInterface::asBinder(callback) == IInterface::asBinder(it)) {
      LOG(WARNING) << "Ignore duplicate chip callback registration";
      return binder::Status::ok();
    }
  }
  LOG(INFO) << "New chip callback registered";
  chip_callbacks_.push_back(callback);
  return binder::Status::ok();
}

android::binder::Status Chip::UnregisterCallback(const sp<android::net::wifi::IChipCallback>& callback) {
  for (auto it = chip_callbacks_.begin(); it != chip_callbacks_.end(); it++) {
    if (IInterface::asBinder(callback) == IInterface::asBinder(*it)) {
      chip_callbacks_.erase(it);
      LOG(INFO) << "Unregister chip callback";
      return binder::Status::ok();
    }
  }
  LOG(WARNING) << "Failed to find registered chip callback to unregister";
  return binder::Status::ok();
}

android::binder::Status Chip::ConfigureClientInterface(int32_t* _aidl_return) {
  // TODO: Configure client interface using HAL.
  android::wificond::ClientInterface* interface =
      new android::wificond::ClientInterface();
  client_interfaces_.push_back(interface);
  *_aidl_return = client_interface_id_++;
  return binder::Status::ok();
}

android::binder::Status Chip::ConfigureApInterface(int32_t* _aidl_return) {
  // TODO: Implement this function
  return binder::Status::ok();
}

android::binder::Status Chip::GetClientInterfaces(vector<sp<android::IBinder>>* _aidl_return) {
  *_aidl_return = client_interfaces_;
  return binder::Status::ok();
}

android::binder::Status Chip::GetApInterfaces(vector<sp<android::IBinder>>* _aidl_return) {
  // TODO: Implement this function
  return binder::Status::ok();
}

}  // namespace wificond
}  // namespace android

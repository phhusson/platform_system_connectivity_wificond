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

#ifndef WIFICOND_CHIP_H_
#define WIFICOND_CHIP_H_

#include <vector>

#include <android-base/macros.h>

#include "android/net/wifi/BnChip.h"
#include "client_interface.h"

namespace android {
namespace wificond {

class Chip : public android::net::wifi::BnChip {
 public:
  explicit Chip();
  ~Chip() override = default;
  android::binder::Status RegisterCallback(
      const android::sp<android::net::wifi::IChipCallback>& callback) override;
  android::binder::Status UnregisterCallback(
      const android::sp<android::net::wifi::IChipCallback>& callback) override;
  android::binder::Status ConfigureClientInterface(
      int32_t* _aidl_return) override;
  android::binder::Status ConfigureApInterface(int32_t* _aidl_return) override;
  android::binder::Status GetClientInterfaces(
      std::vector<android::sp<android::IBinder>>* _aidl_return) override;
  android::binder::Status GetApInterfaces(
      std::vector<android::sp<android::IBinder>>* _aidl_return) override;

 private:
  android::sp<android::net::wifi::IChipCallback> ichip_callback_;
  std::vector<android::sp<android::IBinder>> client_interfaces_;
  int32_t client_interface_id_;

  DISALLOW_COPY_AND_ASSIGN(Chip);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_CHIP_H_

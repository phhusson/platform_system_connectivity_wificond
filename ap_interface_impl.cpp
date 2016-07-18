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

#include "wificond/ap_interface_impl.h"

#include "wificond/ap_interface_binder.h"

using android::net::wifi::IApInterface;
using android::wifi_system::HostapdManager;
using std::string;
using std::unique_ptr;
using std::vector;

using EncryptionType = android::wifi_system::HostapdManager::EncryptionType;

namespace android {
namespace wificond {

ApInterfaceImpl::ApInterfaceImpl(const string& interface_name,
                                 unique_ptr<HostapdManager> hostapd_manager)
    : interface_name_(interface_name),
      hostapd_manager_(std::move(hostapd_manager)) {
  binder_ = new ApInterfaceBinder(this);
}

ApInterfaceImpl::~ApInterfaceImpl() {
  binder_->NotifyImplDead();
}

sp<IApInterface> ApInterfaceImpl::GetBinder() const {
  return binder_;
}

bool ApInterfaceImpl::StartHostapd() {
  return hostapd_manager_->StartHostapd();
}

bool ApInterfaceImpl::StopHostapd() {
  return hostapd_manager_->StopHostapd();
}

bool ApInterfaceImpl::WriteHostapdConfig(const vector<uint8_t>& ssid,
                                         bool is_hidden,
                                         int32_t channel,
                                         EncryptionType encryption_type,
                                         const vector<uint8_t>& passphrase) {
  string config = hostapd_manager_->CreateHostapdConfig(
      interface_name_, ssid, is_hidden, channel, encryption_type, passphrase);

  if (config.empty()) {
    return false;
  }

  return hostapd_manager_->WriteHostapdConfig(config);
}

}  // namespace wificond
}  // namespace android

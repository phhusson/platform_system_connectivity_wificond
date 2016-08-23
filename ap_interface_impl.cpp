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

#include <android-base/logging.h>

#include "wificond/ap_interface_binder.h"

using android::net::wifi::IApInterface;
using android::wifi_system::HostapdManager;
using android::wifi_system::InterfaceTool;
using std::string;
using std::unique_ptr;
using std::vector;

using EncryptionType = android::wifi_system::HostapdManager::EncryptionType;

namespace android {
namespace wificond {

ApInterfaceImpl::ApInterfaceImpl(const string& interface_name,
                                 uint32_t interface_index,
                                 InterfaceTool* if_tool,
                                 unique_ptr<HostapdManager> hostapd_manager)
    : interface_name_(interface_name),
      interface_index_(interface_index),
      if_tool_(if_tool),
      hostapd_manager_(std::move(hostapd_manager)),
      binder_(new ApInterfaceBinder(this)) {
  // This log keeps compiler happy.
  LOG(DEBUG) << "Created ap interface " << interface_name_
             << " with index " << interface_index_;
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
  // Drop SIGKILL on hostapd.
  bool success = hostapd_manager_->StopHostapd();

  // Take down the interface.  This has the pleasant side effect of
  // letting the driver know that we don't want any lingering AP logic
  // running in the driver.
  success = if_tool_->SetUpState(interface_name_.c_str(), false) && success;

  return success;
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

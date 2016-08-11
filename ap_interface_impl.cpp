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
using std::string;
using std::unique_ptr;
using std::vector;

using EncryptionType = android::wifi_system::HostapdManager::EncryptionType;

namespace android {
namespace wificond {

ApInterfaceImpl::ApInterfaceImpl(const string& interface_name,
                                 uint32_t interface_index,
                                 unique_ptr<HostapdManager> hostapd_manager)
    : interface_name_(interface_name),
      interface_index_(interface_index),
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
  pid_t hostapd_pid;
  if (hostapd_manager_->GetHostapdPid(&hostapd_pid)) {
    // We do this because it was found that hostapd would leave the driver in a
    // bad state if init killed it harshly with a SIGKILL. b/30311493
    if (kill(hostapd_pid, SIGTERM) == -1) {
      LOG(ERROR) << "Error delivering signal to hostapd: " << strerror(errno);
    }
    // This should really be asynchronous: b/30465379
    for (int tries = 0; tries < 10; tries++) {
      // Try to give hostapd some time to go down.
      if (!hostapd_manager_->IsHostapdRunning()) {
        break;
      }
      usleep(10 * 1000);  // Don't busy wait.
    }
  }

  // Always drop a SIGKILL on hostapd on the way out, just in case.
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

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

#include "wificond/client_interface_impl.h"

#include "wificond/client_interface_binder.h"

using android::net::wifi::IClientInterface;
using android::sp;
using std::string;

namespace android {
namespace wificond {

ClientInterfaceImpl::ClientInterfaceImpl(const std::string& interface_name)
    : interface_name_(interface_name),
      binder_(new ClientInterfaceBinder(this)) {
}

ClientInterfaceImpl::~ClientInterfaceImpl() {
  binder_->NotifyImplDead();
}

sp<android::net::wifi::IClientInterface> ClientInterfaceImpl::GetBinder() const {
  return binder_;
}

}  // namespace wificond
}  // namespace android

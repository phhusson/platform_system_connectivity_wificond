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

#include "wificond/server.h"

using android::binder::Status;
using android::sp;
using android::IBinder;
using std::vector;
using std::unique_ptr;
using android::net::wifi::IApInterface;
using android::wifi_hal::DriverTool;
using android::wifi_system::HalTool;
using android::wifi_system::InterfaceTool;

namespace android {
namespace wificond {

Server::Server(unique_ptr<HalTool> hal_tool,
               unique_ptr<InterfaceTool> if_tool,
               unique_ptr<DriverTool> driver_tool)
    : hal_tool_(std::move(hal_tool)),
      if_tool_(std::move(if_tool)),
      driver_tool_(std::move(driver_tool)) {
}


Status Server::createApInterface(sp<IApInterface>* created_interface) {
  if (!ap_interfaces_.empty()) {
    // In the future we may support multiple interfaces at once.  However,
    // today, we support just one.
    return Status::fromExceptionCode(Status::EX_UNSUPPORTED_OPERATION);
  }

  if (!SetupInterfaceForMode(DriverTool::kFirmwareModeAp)) {
    return Status::fromExceptionCode(Status::EX_ILLEGAL_STATE);
  }

  unique_ptr<ApInterfaceImpl> ap_interface(new ApInterfaceImpl);
  *created_interface = ap_interface->GetBinder();
  ap_interfaces_.push_back(std::move(ap_interface));
  return Status::ok();
}

Status Server::tearDownInterfaces() {
  if (!ap_interfaces_.empty()) {
    ap_interfaces_.clear();
    if (!driver_tool_->UnloadDriver()) {
      return Status::fromExceptionCode(Status::EX_ILLEGAL_STATE);
    }
  }
  return Status::ok();
}

bool Server::SetupInterfaceForMode(int mode) {
  if (!driver_tool_->LoadDriver() ||
      !driver_tool_->ChangeFirmwareMode(mode)) {
    return false;
  }

  // TODO: Confirm the ap interface is ready for use by checking its
  //       nl80211 published capabilities.

  return true;
}

}  // namespace wificond
}  // namespace android

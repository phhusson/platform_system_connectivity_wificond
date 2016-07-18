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

#include <android-base/logging.h>

using android::binder::Status;
using android::sp;
using android::IBinder;
using std::string;
using std::vector;
using std::unique_ptr;
using android::net::wifi::IApInterface;
using android::wifi_hal::DriverTool;
using android::wifi_system::HalTool;
using android::wifi_system::HostapdManager;
using android::wifi_system::InterfaceTool;

namespace android {
namespace wificond {
namespace {

const char kNetworkInterfaceName[] = "wlan0";

}  // namespace

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
    LOG(ERROR) << "Cannot create AP interface when other interfaces exist";
    return Status::ok();
  }

  string interface_name;
  if (!SetupInterfaceForMode(DriverTool::kFirmwareModeAp, &interface_name)) {
    return Status::ok();  // Logging was done internally
  }

  unique_ptr<ApInterfaceImpl> ap_interface(new ApInterfaceImpl(
      interface_name,
      unique_ptr<HostapdManager>(new HostapdManager)));
  *created_interface = ap_interface->GetBinder();
  ap_interfaces_.push_back(std::move(ap_interface));
  return Status::ok();
}

Status Server::tearDownInterfaces() {
  if (!ap_interfaces_.empty()) {
    ap_interfaces_.clear();
    if (!driver_tool_->UnloadDriver()) {
      LOG(ERROR) << "Failed to unload WiFi driver!";
      return Status::ok();
    }
  }
  return Status::ok();
}

bool Server::SetupInterfaceForMode(int mode, string* interface_name) {
  string result;
  if (!driver_tool_->LoadDriver()) {
    LOG(ERROR) << "Failed to load WiFi driver!";
    return false;
  }
  if (!driver_tool_->ChangeFirmwareMode(mode)) {
    LOG(ERROR) << "Failed to change WiFi firmware mode!";
    return false;
  }

  // TODO: Confirm the ap interface is ready for use by checking its
  //       nl80211 published capabilities.
  *interface_name = kNetworkInterfaceName;

  return true;
}

}  // namespace wificond
}  // namespace android

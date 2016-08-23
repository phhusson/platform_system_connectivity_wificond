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

#include "wificond/net/netlink_utils.h"
#include "wificond/scanning/scan_utils.h"

using android::binder::Status;
using android::sp;
using android::IBinder;
using std::string;
using std::vector;
using std::unique_ptr;
using android::net::wifi::IApInterface;
using android::net::wifi::IClientInterface;
using android::wifi_hal::DriverTool;
using android::wifi_system::HalTool;
using android::wifi_system::HostapdManager;
using android::wifi_system::InterfaceTool;

namespace android {
namespace wificond {

Server::Server(unique_ptr<HalTool> hal_tool,
               unique_ptr<InterfaceTool> if_tool,
               unique_ptr<DriverTool> driver_tool,
               NetlinkUtils* netlink_utils,
               ScanUtils* scan_utils)
    : hal_tool_(std::move(hal_tool)),
      if_tool_(std::move(if_tool)),
      driver_tool_(std::move(driver_tool)),
      netlink_utils_(netlink_utils),
      scan_utils_(scan_utils) {
}

Status Server::createApInterface(sp<IApInterface>* created_interface) {
  string interface_name;
  uint32_t interface_index;
  vector<uint8_t> interface_mac_addr;
  if (!SetupInterfaceForMode(DriverTool::kFirmwareModeAp,
                             &interface_name,
                             &interface_index,
                             &interface_mac_addr)) {
    return Status::ok();  // Logging was done internally
  }

  unique_ptr<ApInterfaceImpl> ap_interface(new ApInterfaceImpl(
      interface_name,
      interface_index,
      if_tool_.get(),
      unique_ptr<HostapdManager>(new HostapdManager)));
  *created_interface = ap_interface->GetBinder();
  ap_interfaces_.push_back(std::move(ap_interface));
  return Status::ok();
}

Status Server::createClientInterface(sp<IClientInterface>* created_interface) {
  string interface_name;
  uint32_t interface_index;
  vector<uint8_t> interface_mac_addr;
  if (!SetupInterfaceForMode(DriverTool::kFirmwareModeSta,
                             &interface_name,
                             &interface_index,
                             &interface_mac_addr)) {
    return Status::ok();  // Logging was done internally
  }

  unique_ptr<ClientInterfaceImpl> client_interface(new ClientInterfaceImpl(
      interface_name,
      interface_index,
      interface_mac_addr,
      scan_utils_));
  *created_interface = client_interface->GetBinder();
  client_interfaces_.push_back(std::move(client_interface));
  return Status::ok();
}

Status Server::tearDownInterfaces() {
  ap_interfaces_.clear();
  client_interfaces_.clear();
  if (!driver_tool_->UnloadDriver()) {
    LOG(ERROR) << "Failed to unload WiFi driver!";
  }
  return Status::ok();
}

bool Server::SetupInterfaceForMode(int mode,
                                   string* interface_name,
                                   uint32_t* interface_index,
                                   vector<uint8_t>* interface_mac_addr) {
  if (!ap_interfaces_.empty() || !client_interfaces_.empty()) {
    // In the future we may support multiple interfaces at once.  However,
    // today, we support just one.
    LOG(ERROR) << "Cannot create AP interface when other interfaces exist";
    return false;
  }

  string result;
  if (!driver_tool_->LoadDriver()) {
    LOG(ERROR) << "Failed to load WiFi driver!";
    return false;
  }
  if (!driver_tool_->ChangeFirmwareMode(mode)) {
    LOG(ERROR) << "Failed to change WiFi firmware mode!";
    return false;
  }

  if (!RefreshWiphyIndex()) {
    return false;
  }

  if (!netlink_utils_->GetInterfaceInfo(wiphy_index_,
                                        interface_name,
                                        interface_index,
                                        interface_mac_addr)) {
    return false;
  }

  return true;
}

bool Server::RefreshWiphyIndex() {
  if (!netlink_utils_->GetWiphyIndex(&wiphy_index_)) {
    LOG(ERROR) << "Failed to get wiphy index";
    return false;
  }
  return true;
}

}  // namespace wificond
}  // namespace android

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

#ifndef WIFICOND_SERVER_H_
#define WIFICOND_SERVER_H_

#include <memory>
#include <string>
#include <vector>

#include <android-base/macros.h>
#include <wifi_hal/driver_tool.h>
#include <wifi_system/hal_tool.h>
#include <wifi_system/interface_tool.h>

#include "android/net/wifi/BnWificond.h"
#include "android/net/wifi/IApInterface.h"
#include "android/net/wifi/IClientInterface.h"

#include "wificond/ap_interface_impl.h"
#include "wificond/client_interface_impl.h"

namespace android {
namespace wificond {

class NL80211Packet;
class NetlinkUtils;
class ScanUtils;

class Server : public android::net::wifi::BnWificond {
 public:
  Server(std::unique_ptr<wifi_system::HalTool> hal_tool,
         std::unique_ptr<wifi_system::InterfaceTool> if_tool,
         std::unique_ptr<wifi_hal::DriverTool> driver_tool,
         NetlinkUtils* netlink_utils,
         ScanUtils* scan_utils);
  ~Server() override = default;

  android::binder::Status createApInterface(
      android::sp<android::net::wifi::IApInterface>*
          created_interface) override;

  android::binder::Status createClientInterface(
      android::sp<android::net::wifi::IClientInterface>*
          created_interface) override;

  android::binder::Status tearDownInterfaces() override;

 private:
  // Does the actual work of setting up an interface for a particular mode.
  //
  // |mode| is one of WIFI_GET_FW_PATH_* defined in hardware_legacy/wifi.h.
  // |interface_name| is a pointer to a string to store the name of Linux
  //     network interface that has been setup.
  //
  // Returns true on success, false otherwise.
  bool SetupInterfaceForMode(int mode,
                             std::string* interface_name,
                             uint32_t* interface_index);
  bool RefreshWiphyIndex();

  const std::unique_ptr<wifi_system::HalTool> hal_tool_;
  const std::unique_ptr<wifi_system::InterfaceTool> if_tool_;
  const std::unique_ptr<wifi_hal::DriverTool> driver_tool_;
  NetlinkUtils* const netlink_utils_;
  ScanUtils* const scan_utils_;

  uint32_t wiphy_index_;
  std::vector<std::unique_ptr<ApInterfaceImpl>> ap_interfaces_;
  std::vector<std::unique_ptr<ClientInterfaceImpl>> client_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_SERVER_H_

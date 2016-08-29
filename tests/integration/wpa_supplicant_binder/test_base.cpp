/*
 * Copyright (C) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test_base.h"

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {

const char WpaSupplicantBinderTestBase::kWlan0IfaceName[] = "wlan0";
const char WpaSupplicantBinderTestBase::kP2p0IfaceName[] = "p2p0";
const char WpaSupplicantBinderTestBase::kIfaceDriver[] = "nl80211";
const char WpaSupplicantBinderTestBase::kIfaceConfigFile[] =
    "/data/misc/wifi/wpa_supplicant.conf";
const char WpaSupplicantBinderTestBase::kNetworkSSID[] = "SSID123";
const char WpaSupplicantBinderTestBase::kNetworkPassphrase[] = "Psk123#$%";
const uint8_t WpaSupplicantBinderTestBase::kNetworkBSSID[] = {0xad, 0x76, 0x34,
                                                              0x87, 0x90, 0x0f};
const int WpaSupplicantBinderTestBase::kCallbackTimeoutMillis = 5;
const int WpaSupplicantBinderTestBase::kConnectTimeoutMillis = 20000;

WpaSupplicantBinderTestBase::WpaSupplicantBinderTestBase()
    : ::testing::Test(),
      binder_dispatcher_(),
      iface_tool_(new InterfaceTool()),
      driver_tool_(new DriverTool()) {}

void WpaSupplicantBinderTestBase::SetUp() {
  RunShellCommand("stop");
  RunShellCommand("stop wificond");
  EXPECT_TRUE(android::wifi_system::wifi_stop_supplicant(true) == 0);
  EXPECT_TRUE(driver_tool_->UnloadDriver());
  EXPECT_TRUE(driver_tool_->LoadDriver());
  EXPECT_TRUE(driver_tool_->ChangeFirmwareMode(DriverTool::kFirmwareModeSta));
  EXPECT_TRUE(iface_tool_->SetWifiUpState(true));
  EXPECT_TRUE(android::wifi_system::wifi_start_supplicant(true) == 0);
  waitForBinderServiceRegistration();
  removeAllInterfaces();
  setDebugLevelToExcessive();
}

void WpaSupplicantBinderTestBase::TearDown() {
  EXPECT_TRUE(android::wifi_system::wifi_stop_supplicant(true) == 0);
  EXPECT_TRUE(driver_tool_->UnloadDriver());
  RunShellCommand("start wificond");
  RunShellCommand("start");
}

android::sp<ISupplicant> WpaSupplicantBinderTestBase::getBinderService() {
  android::sp<android::IBinder> service =
      android::defaultServiceManager()->checkService(android::String16(
          wpa_supplicant_binder::binder_constants::kServiceName));
  return android::interface_cast<ISupplicant>(service);
}

bool WpaSupplicantBinderTestBase::IsBinderServiceRegistered() {
  return getBinderService().get() != nullptr;
}

android::sp<IIface> WpaSupplicantBinderTestBase::CreateInterfaceForTest() {
  ParcelableIfaceParams params;
  params.ifname_ = kWlan0IfaceName;
  params.driver_ = kIfaceDriver;
  params.config_file_ = kIfaceConfigFile;

  android::sp<IIface> iface;
  android::binder::Status status = service_->CreateInterface(params, &iface);
  EXPECT_TRUE((status.isOk()) && (iface.get() != nullptr));
  return iface;
}

void WpaSupplicantBinderTestBase::RemoveInterfaceForTest() {
  android::binder::Status status = service_->RemoveInterface(kWlan0IfaceName);
  EXPECT_TRUE(status.isOk());
}

android::sp<INetwork> WpaSupplicantBinderTestBase::AddNetworkForTest(
    const android::sp<IIface>& iface) {
  android::sp<INetwork> network;
  android::binder::Status status = iface->AddNetwork(&network);
  EXPECT_TRUE((status.isOk()) && (network.get() != nullptr));
  return network;
}

void WpaSupplicantBinderTestBase::RemoveNetworkForTest(
    const android::sp<IIface>& iface, int network_id) {
  android::binder::Status status = iface->RemoveNetwork(network_id);
  EXPECT_TRUE((status.isOk()));
}

void WpaSupplicantBinderTestBase::waitForBinderServiceRegistration() {
  for (int tries = 0; tries < 10; tries++) {
    if (IsBinderServiceRegistered()) {
      break;
    }
    usleep(1000);
  }
  service_ = getBinderService();
  EXPECT_TRUE(service_.get() != nullptr);
}

void WpaSupplicantBinderTestBase::removeAllInterfaces() {
  android::sp<IIface> iface;
  // TODO: Add a service->ListInterfaces() to retrieve all the available
  // interfaces.
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  if (status.isOk() && iface.get() != nullptr) {
    status = service_->RemoveInterface(kWlan0IfaceName);
    EXPECT_TRUE(status.isOk());
  }
  status = service_->GetInterface(kP2p0IfaceName, &iface);
  if (status.isOk() && iface.get() != nullptr) {
    status = service_->RemoveInterface(kP2p0IfaceName);
    EXPECT_TRUE(status.isOk());
  }
}

void WpaSupplicantBinderTestBase::setDebugLevelToExcessive() {
  android::binder::Status status =
      service_->SetDebugParams(ISupplicant::DEBUG_LEVEL_EXCESSIVE, true, true);
  EXPECT_TRUE(status.isOk());
}

}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

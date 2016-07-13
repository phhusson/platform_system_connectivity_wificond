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

#include <android-base/logging.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <gtest/gtest.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>
#include <wifi_hal/driver_tool.h>
#include <wifi_system/interface_tool.h>
#include <wifi_system/wifi.h>
#include <wpa_supplicant_binder/binder_constants.h>
#include <wpa_supplicant_binder/parcelable_iface_params.h>

#include <fi/w1/wpa_supplicant/IIface.h>
#include <fi/w1/wpa_supplicant/INetwork.h>
#include <fi/w1/wpa_supplicant/ISupplicant.h>

#include "tests/shell_utils.h"

using android::wifi_hal::DriverTool;
using android::wifi_system::InterfaceTool;
using android::wificond::tests::integration::RunShellCommand;

using fi::w1::wpa_supplicant::IIface;
using fi::w1::wpa_supplicant::INetwork;
using fi::w1::wpa_supplicant::ISupplicant;
using fi::w1::wpa_supplicant::ParcelableIfaceParams;

namespace wpa_supplicant_binder {
namespace {

// Hardcoded values for test.
const char kWlan0IfaceName[] = "wlan0";
const char kP2p0IfaceName[] = "p2p0";
const char kIfaceDriver[] = "nl80211";
const char kIfaceConfigFile[] = "/data/misc/wifi/wpa_supplicant.conf";

/**
 * Base class for all wpa_supplicant binder interface testing.
 * All the test classes should inherit from this class and invoke this base
 * class |Setup| & |TearDown|.
 *
 * The |SetUp| method prepares the device for wpa_supplicant binder testing by
 * stopping framework, reloading driver, restarting wpa_supplicant, etc.
 */
class WpaSupplicantBinderTest : public ::testing::Test {
 protected:
  WpaSupplicantBinderTest()
      : ::testing::Test(),
        iface_tool_(new InterfaceTool()),
        driver_tool_(new DriverTool()) {}

  /**
   * Steps performed before each test:
   * 1. Stop android framework.
   * 2. Stop |wificond|.
   * 3. Stop |wpa_supplicant|.
   * 4. Unload the driver.
   * 5. Load the driver.
   * 6. Set the firmware in Sta mode. (TODO: Ap mode for some tests).
   * 7. Set the wlan0 interface up.
   * 8. Start wpa_supplicant.
   * 9. Wait for wpa_supplicant binder service to be registered.
   * 10. Remove the |wlan0| & |p2p0| interface from wpa_supplicant.
   * 11. Increase |wpa_supplicant| debug level.
   *
   * TODO: We can't fully nuke the existing wpa_supplicant.conf file because
   * there are some device specific parameters stored there needed for
   * wpa_supplicant to work properly.
   */
  void SetUp() override {
    RunShellCommand("stop");
    RunShellCommand("stop wificond");
    EXPECT_TRUE(android::wifi_system::wifi_stop_supplicant(false) == 0);
    EXPECT_TRUE(driver_tool_->UnloadDriver());
    EXPECT_TRUE(driver_tool_->LoadDriver());
    EXPECT_TRUE(driver_tool_->ChangeFirmwareMode(DriverTool::kFirmwareModeSta));
    EXPECT_TRUE(iface_tool_->SetWifiUpState(true));
    EXPECT_TRUE(android::wifi_system::wifi_start_supplicant(false) == 0);
    waitForBinderServiceRegistration();
    removeAllInterfaces();
    setDebugLevelToExcessive();
  }

  /**
   * Steps performed after each test:
   * 1. Stop |wpa_supplicant|.
   * 2. Unload the driver.
   * 3. Start |wificond|.
   * 4. Start android framework.
   *
   * Assuming that android framework will perform the necessary steps after
   * this to put the device in working state.
   */
  void TearDown() override {
    EXPECT_TRUE(android::wifi_system::wifi_stop_supplicant(false) == 0);
    EXPECT_TRUE(driver_tool_->UnloadDriver());
    RunShellCommand("start wificond");
    RunShellCommand("start");
  }

  /**
   * Retrieve reference to wpa_supplicant's binder service.
   */
  android::sp<ISupplicant> getBinderService() {
    android::sp<android::IBinder> service =
        android::defaultServiceManager()->checkService(
            android::String16(binder_constants::kServiceName));
    return android::interface_cast<ISupplicant>(service);
  }

  /**
   * Checks if wpa_supplicant's binder service is registered and visible.
   */
  bool IsBinderServiceRegistered() {
    return getBinderService().get() != nullptr;
  }

  /**
   * Creates a network interface for test using the hardcoded params:
   * |kWlan0IfaceName|, |kIfaceDriver|, |kIfaceConfigFile|.
   */
  android::sp<IIface> CreateInterfaceForTest() {
    ParcelableIfaceParams params;
    params.ifname_ = kWlan0IfaceName;
    params.driver_ = kIfaceDriver;
    params.config_file_ = kIfaceConfigFile;

    android::sp<IIface> iface;
    android::binder::Status status = service_->CreateInterface(params, &iface);
    EXPECT_TRUE((status.isOk()) && (iface.get() != nullptr));
    return iface;
  }

  /**
   * Removes the network interface created using |CreateInterfaceForTest|.
   */
  void RemoveInterfaceForTest() {
    android::binder::Status status = service_->RemoveInterface(kWlan0IfaceName);
    EXPECT_TRUE(status.isOk());
  }

  /**
   * Add a network to the |iface|.
   */
  android::sp<INetwork> AddNetworkForTest(const android::sp<IIface>& iface) {
    android::sp<INetwork> network;
    android::binder::Status status = iface->AddNetwork(&network);
    EXPECT_TRUE((status.isOk()) && (network.get() != nullptr));
    return network;
  }

  /**
   * Removes a network with provided |network_id| from |iface|.
   */
  void RemoveNetworkForTest(const android::sp<IIface>& iface, int network_id) {
    android::binder::Status status = iface->RemoveNetwork(network_id);
    EXPECT_TRUE((status.isOk()));
  }

  android::sp<ISupplicant> service_;

 private:
  /**
   * Waits in a loop for a maximum of 10 milliseconds for the binder service to
   * be registered.
   */
  void waitForBinderServiceRegistration() {
    for (int tries = 0; tries < 10; tries++) {
      if (IsBinderServiceRegistered()) {
        break;
      }
      usleep(1000);
    }
    service_ = getBinderService();
    // Ensure that the service is available. This is needed for all tests.
    EXPECT_TRUE(service_.get() != nullptr);
  }

  /**
   * Removes all the interfaces (|wlan0| & |p2p0|) controlled by
   * |wpa_supplicant|.
   * wpa_supplicant is started with |wlan0| and |p2p0| assigned in init.rc.
   */
  void removeAllInterfaces() {
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

  /**
   * Increase wpa_supplicant debug level to |DEBUG_LEVEL_EXCESSIVE|.
   */
  void setDebugLevelToExcessive() {
    android::binder::Status status = service_->SetDebugParams(
        ISupplicant::DEBUG_LEVEL_EXCESSIVE, true, true);
    EXPECT_TRUE(status.isOk());
  }

  std::unique_ptr<InterfaceTool> iface_tool_;
  std::unique_ptr<DriverTool> driver_tool_;

};  // class WpaSupplicantBinderTest
}  // namespace

/**
 * Verifies the |ISupplicant.CreateInterface| binder call.
 */
TEST_F(WpaSupplicantBinderTest, CreateInterface) {
  // Create the interface.
  CreateInterfaceForTest();

  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE((status.isOk()) && (iface.get() != nullptr));
}

/**
 * Verifies the |ISupplicant.RemoveInterface| binder call.
 */
TEST_F(WpaSupplicantBinderTest, RemoveInterface) {
  // Create the interface first.
  CreateInterfaceForTest();

  // Now remove the interface.
  RemoveInterfaceForTest();

  // The interface should no longer be present now.
  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              ISupplicant::ERROR_IFACE_UNKNOWN);
}

/**
 * Verifies the |ISupplicant.GetDebugLevel|,
 * |ISupplicant.GetDebugShowTimestamp|, |ISupplicant.GetDebugShowKeys| binder
 * calls.
 */
TEST_F(WpaSupplicantBinderTest, GetDebugParams) {
  int debug_level;
  android::binder::Status status = service_->GetDebugLevel(&debug_level);
  EXPECT_TRUE((status.isOk()) &&
              (debug_level == ISupplicant::DEBUG_LEVEL_EXCESSIVE));

  bool debug_show_timestamp;
  status = service_->GetDebugShowTimestamp(&debug_show_timestamp);
  EXPECT_TRUE((status.isOk()) && (debug_show_timestamp));

  bool debug_show_keys;
  status = service_->GetDebugShowKeys(&debug_show_keys);
  EXPECT_TRUE((status.isOk()) && (debug_show_keys));
}

/**
 * Verifies the |IIface.GetName| binder call.
 */
TEST_F(WpaSupplicantBinderTest, GetNameOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

/**
 * Verifies the |IIface.GetName| binder call on an interface
 * which has been removed.
 */
TEST_F(WpaSupplicantBinderTest, GetNameOnRemovedInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE(ifaceName == kWlan0IfaceName);

  // Now remove the interface.
  RemoveInterfaceForTest();

  // Any method call on the iface object should return failure.
  status = iface->GetName(&ifaceName);
  EXPECT_TRUE(status.serviceSpecificErrorCode() == IIface::ERROR_IFACE_INVALID);
}

/**
 * Verifies the |IIface.AddNetwork| binder call.
 */
TEST_F(WpaSupplicantBinderTest, AddNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  AddNetworkForTest(iface);
}

/**
 * Verifies the |IIface.RemoveNetwork| binder call.
 */
TEST_F(WpaSupplicantBinderTest, RemoveNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  // Retrieve the network ID of the network added.
  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  // Now remove the network.
  RemoveNetworkForTest(iface, network_id);

  // The network should no longer be present now.
  status = iface->GetNetwork(network_id, &network);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              IIface::ERROR_NETWORK_UNKNOWN);
}

/**
 * Verifies the |INetwork.GetId| binder call.
 */
TEST_F(WpaSupplicantBinderTest, GetIdOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));
}

/**
 * Verifies the |INetwork.GetInterfaceName| binder call.
 */
TEST_F(WpaSupplicantBinderTest, GetInterfaceNameOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  std::string ifaceName;
  android::binder::Status status = network->GetInterfaceName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

/**
 * Verifies the |INetwork.GetId| binder call on a network
 * which has been removed.
 */
TEST_F(WpaSupplicantBinderTest, GetIdOnRemovedNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));

  // Now remove the network.
  RemoveNetworkForTest(iface, network_id);

  // Any method call on the network object should return failure.
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              INetwork::ERROR_NETWORK_INVALID);
}
}  // namespace wpa_supplicant_binder

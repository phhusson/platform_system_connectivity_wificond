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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>
#include <wifi_hal/driver_tool.h>
#include <wifi_system/interface_tool.h>
#include <wifi_system/wifi.h>
#include <wpa_supplicant_binder/binder_constants.h>
#include <wpa_supplicant_binder/parcelable_iface_params.h>

#include <fi/w1/wpa_supplicant/BnIfaceCallback.h>
#include <fi/w1/wpa_supplicant/BnSupplicantCallback.h>
#include <fi/w1/wpa_supplicant/IIface.h>
#include <fi/w1/wpa_supplicant/INetwork.h>
#include <fi/w1/wpa_supplicant/ISupplicant.h>

#include "tests/integration/binder_dispatcher.h"
#include "tests/shell_utils.h"

using android::wifi_hal::DriverTool;
using android::wifi_system::InterfaceTool;
using android::wificond::tests::integration::RunShellCommand;
using android::wificond::tests::integration::BinderDispatcher;

using fi::w1::wpa_supplicant::BnIfaceCallback;
using fi::w1::wpa_supplicant::BnSupplicantCallback;
using fi::w1::wpa_supplicant::IIface;
using fi::w1::wpa_supplicant::INetwork;
using fi::w1::wpa_supplicant::ISupplicant;
using fi::w1::wpa_supplicant::ParcelableIfaceParams;

using testing::_;
using testing::DoAll;
using testing::Return;

namespace wpa_supplicant_binder {
namespace {

// Hardcoded values for Android wpa_supplicant testing.
// These interfaces may not exist on non-Android devices!
const char kWlan0IfaceName[] = "wlan0";
const char kP2p0IfaceName[] = "p2p0";
const char kIfaceDriver[] = "nl80211";
const char kIfaceConfigFile[] = "/data/misc/wifi/wpa_supplicant.conf";
const char kNetworkSSID[] = "SSID123";
const char kNetworkPassphrase[] = "Psk123";
const char kNetworkBSSID[] = {0xad, 0x76, 0x34, 0x87, 0x90, 0x0f};
const int kCallbackTimeoutMillis = 5;

// Used to get callbacks defined in |BnSupplicantCallback|.
class MockSupplicantCallback : public BnSupplicantCallback {
 public:
  MOCK_METHOD1(OnInterfaceCreated,
               android::binder::Status(const std::string& ifname));
  MOCK_METHOD1(OnInterfaceRemoved,
               android::binder::Status(const std::string& ifname));
};

// Used to get callbacks defined in |BnIfaceCallback|.
class MockIfaceCallback : public BnIfaceCallback {
 public:
  MOCK_METHOD1(OnNetworkAdded, android::binder::Status(int id));
  MOCK_METHOD1(OnNetworkRemoved, android::binder::Status(int id));
};

// Interrupt the binder callback tracker once the desired callback is invoked.
ACTION_P(InterrupBinderDispatcher, binder_dispatcher) {
  binder_dispatcher->InterruptDispatch();
}

// Base class for all wpa_supplicant binder interface testing.
// All the test classes should inherit from this class and invoke this base
// class |Setup| & |TearDown|.
// The |SetUp| method prepares the device for wpa_supplicant binder testing by
// stopping framework, reloading driver, restarting wpa_supplicant, etc.
class WpaSupplicantBinderTest : public ::testing::Test {
 protected:
  WpaSupplicantBinderTest()
      : ::testing::Test(),
        binder_dispatcher_(),
        iface_tool_(new InterfaceTool()),
        driver_tool_(new DriverTool()) {}

  // Steps performed before each test:
  // 1. Stop android framework.
  // 2. Stop |wificond|.
  // 3. Stop |wpa_supplicant|.
  // 4. Unload the driver.
  // 5. Load the driver.
  // 6. Set the firmware in Sta mode. (TODO: Ap mode for some tests).
  // 7. Set the wlan0 interface up.
  // 8. Start wpa_supplicant.
  // 9. Wait for wpa_supplicant binder service to be registered.
  // 10. Remove the |wlan0| & |p2p0| interface from wpa_supplicant.
  // 11. Increase |wpa_supplicant| debug level.
  // TODO: We can't fully nuke the existing wpa_supplicant.conf file because
  // there are some device specific parameters stored there needed for
  // wpa_supplicant to work properly.
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

  // Steps performed after each test:
  // 1. Stop |wpa_supplicant|.
  // 2. Unload the driver.
  // 3. Start |wificond|.
  // 4. Start android framework.
  // Assuming that android framework will perform the necessary steps after
  // this to put the device in working state.

  void TearDown() override {
    EXPECT_TRUE(android::wifi_system::wifi_stop_supplicant(false) == 0);
    EXPECT_TRUE(driver_tool_->UnloadDriver());
    RunShellCommand("start wificond");
    RunShellCommand("start");
  }

  // Retrieve reference to wpa_supplicant's binder service.
  android::sp<ISupplicant> getBinderService() {
    android::sp<android::IBinder> service =
        android::defaultServiceManager()->checkService(
            android::String16(binder_constants::kServiceName));
    return android::interface_cast<ISupplicant>(service);
  }

  // Checks if wpa_supplicant's binder service is registered and visible.
  bool IsBinderServiceRegistered() {
    return getBinderService().get() != nullptr;
  }

  // Creates a network interface for test using the hardcoded params:
  // |kWlan0IfaceName|, |kIfaceDriver|, |kIfaceConfigFile|.
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

  // Removes the network interface created using |CreateInterfaceForTest|.
  void RemoveInterfaceForTest() {
    android::binder::Status status = service_->RemoveInterface(kWlan0IfaceName);
    EXPECT_TRUE(status.isOk());
  }

  // Add a network to the |iface|.
  android::sp<INetwork> AddNetworkForTest(const android::sp<IIface>& iface) {
    android::sp<INetwork> network;
    android::binder::Status status = iface->AddNetwork(&network);
    EXPECT_TRUE((status.isOk()) && (network.get() != nullptr));
    return network;
  }

  // Removes a network with provided |network_id| from |iface|.
  void RemoveNetworkForTest(const android::sp<IIface>& iface, int network_id) {
    android::binder::Status status = iface->RemoveNetwork(network_id);
    EXPECT_TRUE((status.isOk()));
  }

  android::sp<ISupplicant> service_;
  BinderDispatcher binder_dispatcher_;

 private:
  // Waits in a loop for a maximum of 10 milliseconds for the binder service to
  // be registered.
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

  //
  // Removes all the interfaces (|wlan0| & |p2p0|) controlled by
  // |wpa_supplicant|.
  // wpa_supplicant is started with |wlan0| and |p2p0| assigned in init.rc.

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

  // Increase wpa_supplicant debug level to |DEBUG_LEVEL_EXCESSIVE|.
  void setDebugLevelToExcessive() {
    android::binder::Status status = service_->SetDebugParams(
        ISupplicant::DEBUG_LEVEL_EXCESSIVE, true, true);
    EXPECT_TRUE(status.isOk());
  }

  std::unique_ptr<InterfaceTool> iface_tool_;
  std::unique_ptr<DriverTool> driver_tool_;

};  // class WpaSupplicantBinderTest
}  // namespace

// Verifies the |ISupplicant.CreateInterface| binder call.
TEST_F(WpaSupplicantBinderTest, CreateInterface) {
  // Create the interface.
  CreateInterfaceForTest();

  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE((status.isOk()) && (iface.get() != nullptr));
}

// Verifies the |ISupplicant.RemoveInterface| binder call.
TEST_F(WpaSupplicantBinderTest, RemoveInterface) {
  CreateInterfaceForTest();

  RemoveInterfaceForTest();

  // The interface should no longer be present now.
  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              ISupplicant::ERROR_IFACE_UNKNOWN);
}

// Verifies the |ISupplicant.GetDebugLevel|,
// |ISupplicant.GetDebugShowTimestamp|, |ISupplicant.GetDebugShowKeys| binder
// calls.
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

// Verifies the |IIface.GetName| binder call.
TEST_F(WpaSupplicantBinderTest, GetNameOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

// Verifies the |IIface.GetName| binder call on an interface
// which has been removed.
TEST_F(WpaSupplicantBinderTest, GetNameOnRemovedInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE(ifaceName == kWlan0IfaceName);

  RemoveInterfaceForTest();

  // Any method call on the iface object should return failure.
  status = iface->GetName(&ifaceName);
  EXPECT_TRUE(status.serviceSpecificErrorCode() == IIface::ERROR_IFACE_INVALID);
}

// Verifies the |IIface.AddNetwork| binder call.
TEST_F(WpaSupplicantBinderTest, AddNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  AddNetworkForTest(iface);
}

// Verifies the |IIface.RemoveNetwork| binder call.
TEST_F(WpaSupplicantBinderTest, RemoveNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  RemoveNetworkForTest(iface, network_id);

  // The network should no longer be present now.
  status = iface->GetNetwork(network_id, &network);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              IIface::ERROR_NETWORK_UNKNOWN);
}

// Verifies the |INetwork.GetId| binder call.
TEST_F(WpaSupplicantBinderTest, GetIdOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));
}

// Verifies the |INetwork.GetInterfaceName| binder call.
TEST_F(WpaSupplicantBinderTest, GetInterfaceNameOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  std::string ifaceName;
  android::binder::Status status = network->GetInterfaceName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

// Verifies the |INetwork.GetId| binder call on a network
// which has been removed.
TEST_F(WpaSupplicantBinderTest, GetIdOnRemovedNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));

  RemoveNetworkForTest(iface, network_id);

  // Any method call on the network object should return failure.
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              INetwork::ERROR_NETWORK_INVALID);
}

// Verifies the |ISupplicantCallback.OnInterfaceCreated| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTest, OnInterfaceCreated) {
  // Register for supplicant service level state changes
  // (e.g. interface creation)
  android::sp<MockSupplicantCallback> callback(new MockSupplicantCallback);
  android::binder::Status status = service_->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  CreateInterfaceForTest();

  // All registered |ISupplicantCallback| listeners should be notified
  // of any interface adddition.
  EXPECT_CALL(*callback, OnInterfaceCreated(kWlan0IfaceName))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |ISupplicantCallback.OnInterfaceRemoved| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTest, OnInterfaceRemoved) {
  // Register for supplicant service level state changes
  // (e.g. interface creation)
  android::sp<MockSupplicantCallback> callback(new MockSupplicantCallback);
  android::binder::Status status = service_->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  CreateInterfaceForTest();

  RemoveInterfaceForTest();

  // All registered |ISupplicantCallback| listeners should be notified
  // of any interface adddition followed by interface removal.
  EXPECT_CALL(*callback, OnInterfaceCreated(kWlan0IfaceName))
      .WillOnce(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnInterfaceRemoved(kWlan0IfaceName))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |IIfaceCallback.OnNetworkAdded| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTest, OnNetworkAdded) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // First Register for |IIfaceCallback| using the corresponding |IIface|
  // object.
  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  android::binder::Status status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  // All registered |IIfaceCallback| listeners should be notified
  // of any network adddition.
  EXPECT_CALL(*callback, OnNetworkAdded(network_id))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |IIfaceCallback.OnNetworkRemoved| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTest, OnNetworkRemoved) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // First Register for |IIfaceCallback| using the corresponding |IIface|
  // object.
  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  android::binder::Status status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  RemoveNetworkForTest(iface, network_id);

  // All registered |IIfaceCallback| listeners should be notified
  // of any network adddition folled by the network removal.
  EXPECT_CALL(*callback, OnNetworkAdded(network_id))
      .WillOnce(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnNetworkRemoved(network_id))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |INetwork.SetSSID| & |INetwork.GetSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  // Now set the ssid converting to a hex vector.
  std::vector<uint8_t> set_ssid_vec;
  set_ssid_vec.assign(kNetworkSSID, kNetworkSSID + strlen(kNetworkSSID));
  android::binder::Status status = network->SetSSID(set_ssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::vector<uint8_t> get_ssid_vec;
  status = network->GetSSID(&get_ssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_ssid_vec, get_ssid_vec);
}

// Verifies the |INetwork.SetBSSID| & |INetwork.GetBSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetBSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  // Now set the bssid converting to a hex vector.
  std::vector<uint8_t> set_bssid_vec;
  set_bssid_vec.assign(kNetworkBSSID, kNetworkBSSID + sizeof(kNetworkBSSID));
  android::binder::Status status = network->SetBSSID(set_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::vector<uint8_t> get_bssid_vec;
  status = network->GetBSSID(&get_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_bssid_vec, get_bssid_vec);

  // Clear the bssid now.
  set_bssid_vec.clear();
  status = network->SetBSSID(set_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  status = network->GetBSSID(&get_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're cleared.
  EXPECT_TRUE(get_bssid_vec.empty());
}

// Verifies the |INetwork.SetScanSSID| & |INetwork.GetScanSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetScanSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  bool set_scan_ssid = true;
  android::binder::Status status = network->SetScanSSID(set_scan_ssid);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  bool get_scan_ssid;
  status = network->GetScanSSID(&get_scan_ssid);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_scan_ssid, get_scan_ssid);
}

// Verifies the |INetwork.SetRequirePMF| & |INetwork.GetRequirePMF|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetRequirePMF) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  bool set_require_pmf = true;
  android::binder::Status status = network->SetRequirePMF(set_require_pmf);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  bool get_require_pmf;
  status = network->GetRequirePMF(&get_require_pmf);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_require_pmf, get_require_pmf);
}

// Verifies the |INetwork.SetPskPassphrase| & |INetwork.GetPskPassphrase|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetPskPassphrase) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  android::binder::Status status =
      network->SetPskPassphrase(kNetworkPassphrase);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::string get_psk_passphrase;
  status = network->GetPskPassphrase(&get_psk_passphrase);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(kNetworkPassphrase, get_psk_passphrase);
}

// Verifies the |INetwork.SetWepTxKeyIdx| & |INetwork.GetWepTxKeyIdx|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTest, NetworkSetGetWepTxKeyIdx) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // Add network now.
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_idx = 1;
  android::binder::Status status = network->SetWepTxKeyIdx(set_idx);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_idx;
  status = network->GetWepTxKeyIdx(&get_idx);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_idx, get_idx);
}

// Verifies the |INetwork.SetWepKey| & |INetwork.GetWepKey|
// binder calls on a network.
class NetworkWepKeyTest
    : public WpaSupplicantBinderTest,
      public ::testing::WithParamInterface<std::vector<uint8_t>> {};

TEST_P(NetworkWepKeyTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  std::vector<uint8_t> set_wep_key = GetParam();
  android::binder::Status status;
  for (int i = 0; i < fi::w1::wpa_supplicant::INetwork::WEP_KEYS_MAX_NUM; i++) {
    status = network->SetWepKey(i, set_wep_key);
    EXPECT_TRUE(status.isOk()) << status.toString8();
  }

  std::vector<uint8_t> get_wep_key;
  for (int i = 0; i < fi::w1::wpa_supplicant::INetwork::WEP_KEYS_MAX_NUM; i++) {
    status = network->GetWepKey(i, &get_wep_key);
    EXPECT_TRUE(status.isOk()) << status.toString8();
    // Ensure they're the same.
    EXPECT_EQ(get_wep_key, set_wep_key);
  }
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetWepKey,
    NetworkWepKeyTest,
    ::testing::Values(std::vector<uint8_t>({0x56, 0x67, 0x67, 0xf4, 0x56}),
                      std::vector<uint8_t>({0x56, 0x67, 0x67, 0xf4, 0x56,
                                            0x89, 0xad, 0x67, 0x78, 0x89,
                                            0x97, 0xa5, 0xde})));

// Verifies the |INetwork.SetKeyMgmt| & |INetwork.GetKeyMgmt|
// binder calls on a network.
class NetworkKeyMgmtTest : public WpaSupplicantBinderTest,
                           public ::testing::WithParamInterface<int> {};

TEST_P(NetworkKeyMgmtTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetKeyMgmt(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetKeyMgmt(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetKeyMgmt,
    NetworkKeyMgmtTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_NONE,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_WPA_PSK,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_WPA_PSK,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_IEEE8021X));

// Verifies the |INetwork.SetProto| & |INetwork.GetProto|
// binder calls on a network.
class NetworkProtoTest : public WpaSupplicantBinderTest,
                         public ::testing::WithParamInterface<int> {};

TEST_P(NetworkProtoTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetProto(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetProto(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetProto,
    NetworkProtoTest,
    ::testing::Values(fi::w1::wpa_supplicant::INetwork::PROTO_MASK_WPA,
                      fi::w1::wpa_supplicant::INetwork::PROTO_MASK_RSN,
                      fi::w1::wpa_supplicant::INetwork::PROTO_MASK_OSEN));

// Verifies the |INetwork.SetAuthAlg| & |INetwork.GetAuthAlg|
// binder calls on a network.
class NetworkAuthAlgTest : public WpaSupplicantBinderTest,
                           public ::testing::WithParamInterface<int> {};

TEST_P(NetworkAuthAlgTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetAuthAlg(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetAuthAlg(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetAuthAlg,
    NetworkAuthAlgTest,
    ::testing::Values(fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_OPEN,
                      fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_SHARED,
                      fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_LEAP));

// Verifies the |INetwork.SetGroupCipher| & |INetwork.GetGroupCipher|
// binder calls on a network.
class NetworkGroupCipherTest : public WpaSupplicantBinderTest,
                               public ::testing::WithParamInterface<int> {};

TEST_P(NetworkGroupCipherTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetGroupCipher(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetGroupCipher(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetGroupCipher,
    NetworkGroupCipherTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_WEP40,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_WEP104,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_TKIP,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_CCMP));

// Verifies the |INetwork.SetPairwiseCipher| & |INetwork.GetPairwiseCipher|
// binder calls on a network.
class NetworkPairwiseCipherTest : public WpaSupplicantBinderTest,
                                  public ::testing::WithParamInterface<int> {};

TEST_P(NetworkPairwiseCipherTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // Add network now.
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetPairwiseCipher(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetPairwiseCipher(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetPairwiseCipher,
    NetworkPairwiseCipherTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_NONE,
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_TKIP,
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_CCMP));

}  // namespace wpa_supplicant_binder

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

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {

// Interrupt the binder callback tracker once the desired callback is invoked.
ACTION_P(InterrupBinderDispatcher, binder_dispatcher) {
  binder_dispatcher->InterruptDispatch();
}

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
  MOCK_METHOD4(OnStateChanged, android::binder::Status(
                                   int state, const std::vector<uint8_t>& bssid,
                                   int id, const std::vector<uint8_t>& ssid));
};

// Base class for all wpa_supplicant binder interface testing.
// All the test classes should inherit from this class and invoke this base
// class |Setup| & |TearDown|.
// The |SetUp| method prepares the device for wpa_supplicant binder testing by
// stopping framework, reloading driver, restarting wpa_supplicant, etc.
class WpaSupplicantBinderTestBase : public ::testing::Test {
 public:
  // Hardcoded values for Android wpa_supplicant testing.
  // These interfaces may not exist on non-Android devices!
  static const char kWlan0IfaceName[];
  static const char kP2p0IfaceName[];
  static const char kIfaceDriver[];
  static const char kIfaceConfigFile[];
  static const char kNetworkSSID[];
  static const char kNetworkPassphrase[];
  static const uint8_t kNetworkBSSID[];
  static const int kCallbackTimeoutMillis;
  static const int kConnectTimeoutMillis;

 protected:
  WpaSupplicantBinderTestBase();

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
  void SetUp() override;
  // Steps performed after each test:
  // 1. Stop |wpa_supplicant|.
  // 2. Unload the driver.
  // 3. Start |wificond|.
  // 4. Start android framework.
  // Assuming that android framework will perform the necessary steps after
  // this to put the device in working state.
  void TearDown() override;
  // Retrieve reference to wpa_supplicant's binder service.
  android::sp<ISupplicant> getBinderService();
  // Checks if wpa_supplicant's binder service is registered and visible.
  bool IsBinderServiceRegistered();
  // Creates a network interface for test using the hardcoded params:
  // |kWlan0IfaceName|, |kIfaceDriver|, |kIfaceConfigFile|.
  android::sp<IIface> CreateInterfaceForTest();
  // Removes the network interface created using |CreateInterfaceForTest|.
  void RemoveInterfaceForTest();
  // Add a network to the |iface|.
  android::sp<INetwork> AddNetworkForTest(const android::sp<IIface>& iface);
  // Removes a network with provided |network_id| from |iface|.
  void RemoveNetworkForTest(const android::sp<IIface>& iface, int network_id);

  android::sp<ISupplicant> service_;
  BinderDispatcher binder_dispatcher_;

 private:
  // Waits in a loop for a maximum of 10 milliseconds for the binder service to
  // be registered.
  void waitForBinderServiceRegistration();
  // Removes all the interfaces (|wlan0| & |p2p0|) controlled by
  // |wpa_supplicant|.
  // wpa_supplicant is started with |wlan0| and |p2p0| assigned in init.rc.
  void removeAllInterfaces();
  // Increase wpa_supplicant debug level to |DEBUG_LEVEL_EXCESSIVE|.
  void setDebugLevelToExcessive();

  std::unique_ptr<InterfaceTool> iface_tool_;
  std::unique_ptr<DriverTool> driver_tool_;
};  // class WpaSupplicantBinderTestBase

}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

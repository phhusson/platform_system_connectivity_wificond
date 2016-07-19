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

#include <gtest/gtest.h>

#include <ctime>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#include "android/net/wifi/IApInterface.h"
#include "android/net/wifi/IWificond.h"
#include "wificond/ipc_constants.h"
#include "wificond/tests/shell_utils.h"

using android::String16;
using android::base::Trim;
using android::net::wifi::IApInterface;
using android::net::wifi::IWificond;
using android::wificond::ipc_constants::kDevModePropertyKey;
using android::wificond::ipc_constants::kDevModeServiceName;
using android::wificond::ipc_constants::kServiceName;
using android::wificond::tests::integration::RunShellCommand;

namespace android {
namespace wificond {
namespace {

const uint32_t kWificondDeathTimeoutSeconds = 10;
const uint32_t kWificondStartTimeoutSeconds = 10;

bool WificondIsRunning() {
  std::string output;
  RunShellCommand("pgrep -c ^wificond$", &output);
  output = Trim(output);
  if (output == "0") {
    return false;
  }
  return true;
}

bool WificondIsDead() { return !WificondIsRunning(); }

bool WaitForTrue(std::function<bool()> condition, time_t timeout_seconds) {
  time_t start_time_seconds = time(nullptr);
  do {
    if (condition()) {
      return true;
    }
    usleep(1000);
  } while ((time(nullptr) - start_time_seconds) < timeout_seconds);
  return false;
}

bool IsRegisteredAs(const char* service_name) {
  sp<IBinder> service =
      defaultServiceManager()->checkService(String16(service_name));
  return service.get() != nullptr;
}

bool SetDevMode(bool is_on) {
  return property_set(kDevModePropertyKey, (is_on) ? "1" : "0") == 0;
}

class SandboxedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    RunShellCommand("stop wificond");
    EXPECT_TRUE(SetDevMode(true));
    RunShellCommand("start wificond");
    auto in_dev_mode = std::bind(IsRegisteredAs, kDevModeServiceName);
    EXPECT_TRUE(WaitForTrue(in_dev_mode, kWificondStartTimeoutSeconds));
  }

  void TearDown() override {
    RunShellCommand("stop wificond");
    SetDevMode(false);
    RunShellCommand("start wificond");
  }
};  // class SandboxedTest

}  // namespace


TEST(LifeCycleTest, ProcessStartsUp) {
  // Request that wificond be stopped (regardless of its current state).
  RunShellCommand("stop wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsDead, kWificondDeathTimeoutSeconds));

  // Confirm that the service manager has no binder for wificond.
  EXPECT_FALSE(IsRegisteredAs(kServiceName));
  EXPECT_FALSE(IsRegisteredAs(kDevModeServiceName));
  EXPECT_TRUE(SetDevMode(false));  // Clear dev mode

  // Start wificond.
  RunShellCommand("start wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsRunning, kWificondStartTimeoutSeconds));

  // wificond should eventually register with the service manager.
  EXPECT_TRUE(WaitForTrue(std::bind(IsRegisteredAs, kServiceName),
                          kWificondStartTimeoutSeconds));
}

TEST_F(SandboxedTest, CanCreateApInterfaces) {
  sp<IWificond> service;
  ASSERT_EQ(NO_ERROR, getService(String16(kDevModeServiceName), &service));

  // We should be able to create an AP interface.
  sp<IApInterface> ap_interface;
  EXPECT_TRUE(service->createApInterface(&ap_interface).isOk());
  EXPECT_NE(nullptr, ap_interface.get());

  // We should not be able to create two AP interfaces.
  sp<IApInterface> ap_interface2;
  EXPECT_TRUE(service->createApInterface(&ap_interface2).isOk());
  EXPECT_EQ(nullptr, ap_interface2.get());

  // We can tear down the created interface.
  EXPECT_TRUE(service->tearDownInterfaces().isOk());
}

}  // namespace wificond
}  // namespace android

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

#include <android-base/strings.h>
#include <binder/IServiceManager.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#include "android/net/wifi/IApInterface.h"
#include "android/net/wifi/IWificond.h"
#include "tests/shell_utils.h"

using android::String16;
using android::base::Trim;
using android::net::wifi::IApInterface;
using android::net::wifi::IWificond;
using android::wificond::tests::integration::RunShellCommand;

namespace android {
namespace wificond {
namespace {

const uint32_t kWificondDeathTimeoutSeconds = 10;
const uint32_t kWificondStartTimeoutSeconds = 10;

const char kWificondServiceName[] = "wificond";

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

bool IsRegistered() {
  sp<IBinder> service =
      defaultServiceManager()->checkService(String16(kWificondServiceName));
  return service.get() != nullptr;
}

}  // namespace


TEST(LifeCycleTest, ProcessStartsUp) {
  // Request that wificond be stopped (regardless of its current state).
  RunShellCommand("stop wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsDead, kWificondDeathTimeoutSeconds));

  // Confirm that the service manager has no binder for wificond.
  EXPECT_FALSE(IsRegistered());

  // Start wificond.
  RunShellCommand("start wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsRunning, kWificondStartTimeoutSeconds));

  // wificond should eventually register with the service manager.
  EXPECT_TRUE(WaitForTrue(IsRegistered, kWificondStartTimeoutSeconds));
}

TEST(LifeCycleTest, CanCreateApInterfaces) {
  // Restart wificond and wait for it to come back.
  RunShellCommand("stop wificond");
  RunShellCommand("start wificond");
  EXPECT_TRUE(WaitForTrue(IsRegistered, kWificondStartTimeoutSeconds));

  sp<IWificond> service;
  ASSERT_EQ(getService(String16(kWificondServiceName), &service), NO_ERROR);

  // We should be able to create an AP interface.
  sp<IApInterface> ap_interface;
  ASSERT_TRUE(service->CreateApInterface(&ap_interface).isOk());

  // We should not be able to create two AP interfaces.
  sp<IApInterface> ap_interface2;
  ASSERT_FALSE(service->CreateApInterface(&ap_interface2).isOk());

  // We can tear down the created interface.
  ASSERT_TRUE(service->TearDownInterfaces().isOk());
}

}  // namespace wificond
}  // namespace android

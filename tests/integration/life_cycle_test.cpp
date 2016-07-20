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

#include <utils/StrongPointer.h>

#include "android/net/wifi/IApInterface.h"
#include "android/net/wifi/IWificond.h"
#include "wificond/ipc_constants.h"
#include "wificond/tests/shell_utils.h"
#include "wificond/tests/integration/process_utils.h"

using android::net::wifi::IApInterface;
using android::net::wifi::IWificond;
using android::wificond::ipc_constants::kDevModeServiceName;
using android::wificond::ipc_constants::kServiceName;
using android::wificond::tests::integration::RunShellCommand;
using android::wificond::tests::integration::WaitForTrue;
using android::wificond::tests::integration::IsBinderServiceRegistered;
using android::wificond::tests::integration::ScopedDevModeWificond;
using android::wificond::tests::integration::WificondIsRunning;
using android::wificond::tests::integration::WificondIsDead;
using android::wificond::tests::integration::WificondSetDevMode;

namespace android {
namespace wificond {

TEST(LifeCycleTest, ProcessStartsUp) {
  // Request that wificond be stopped (regardless of its current state).
  RunShellCommand("stop wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsDead,
                          ScopedDevModeWificond::kWificondDeathTimeoutSeconds));

  // Confirm that the service manager has no binder for wificond.
  EXPECT_FALSE(IsBinderServiceRegistered(kServiceName));
  EXPECT_FALSE(IsBinderServiceRegistered(kDevModeServiceName));
  EXPECT_TRUE(WificondSetDevMode(false));  // Clear dev mode

  // Start wificond.
  RunShellCommand("start wificond");
  EXPECT_TRUE(WaitForTrue(WificondIsRunning,
                          ScopedDevModeWificond::kWificondStartTimeoutSeconds));

  // wificond should eventually register with the service manager.
  EXPECT_TRUE(WaitForTrue(std::bind(IsBinderServiceRegistered, kServiceName),
                          ScopedDevModeWificond::kWificondStartTimeoutSeconds));
}

TEST(LifeCycleTest, CanCreateApInterfaces) {
  ScopedDevModeWificond dev_mode;
  sp<IWificond> service = dev_mode.EnterDevModeOrDie();

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

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

#include "tests/shell_utils.h"

using android::base::Trim;
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

}  // namespace


TEST(LifeCycleTest, ProcessStartsUp) {
  // Request that wificond be stopped (regardless of its current state).
  RunShellCommand("stop wificond");

  // Confirm that wificond is stopped.
  ASSERT_TRUE(WaitForTrue([]() { return !WificondIsRunning(); },
                          kWificondDeathTimeoutSeconds));

  // Start wificond.
  RunShellCommand("start wificond");
  ASSERT_TRUE(WaitForTrue(WificondIsRunning, kWificondStartTimeoutSeconds));
}

}  // namespace wificond
}  // namespace android

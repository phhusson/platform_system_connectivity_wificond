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

#include "wificond/tests/integration/process_utils.h"

#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>

#include "wificond/ipc_constants.h"
#include "wificond/tests/shell_utils.h"

using android::String16;
using android::base::Trim;
using android::net::wifi::IWificond;
using android::sp;
using android::wificond::ipc_constants::kDevModePropertyKey;
using android::wificond::ipc_constants::kDevModeServiceName;

namespace android {
namespace wificond {
namespace tests {
namespace integration {

const uint32_t ScopedDevModeWificond::kWificondDeathTimeoutSeconds = 10;
const uint32_t ScopedDevModeWificond::kWificondStartTimeoutSeconds = 10;

ScopedDevModeWificond::~ScopedDevModeWificond() {
  if (in_dev_mode_) {
    ExitDevMode();
  }
}

sp<IWificond> ScopedDevModeWificond::EnterDevModeOrDie() {
  sp<IWificond> service = MaybeEnterDevMode();
  if (service.get() == nullptr) {
    LOG(FATAL) << "Failed to restart wificond in dev mode";
  }
  return service;
}

sp<IWificond> ScopedDevModeWificond::MaybeEnterDevMode() {
  sp<IWificond> service;
  RunShellCommand("stop wificond");
  if (!WificondSetDevMode(true)) {
    return service;
  }
  RunShellCommand("start wificond");
  auto in_dev_mode = std::bind(IsBinderServiceRegistered, kDevModeServiceName);
  in_dev_mode_ = WaitForTrue(in_dev_mode, kWificondStartTimeoutSeconds);
  getService(String16(kDevModeServiceName), &service);
  return service;
}

void ScopedDevModeWificond::ExitDevMode() {
  RunShellCommand("stop wificond");
  WificondSetDevMode(false);
  RunShellCommand("start wificond");
  in_dev_mode_ = false;
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

bool IsBinderServiceRegistered(const char* service_name) {
  sp<IBinder> service =
      defaultServiceManager()->checkService(String16(service_name));
  return service.get() != nullptr;
}

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

bool WificondSetDevMode(bool is_on) {
  return property_set(kDevModePropertyKey, (is_on) ? "1" : "0") == 0;
}

}  // namespace integration
}  // namespace tests
}  // namespace android
}  // namespace wificond

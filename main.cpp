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

#include <unistd.h>

#include <csignal>
#include <memory>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <cutils/properties.h>
#include <utils/String16.h>
#include <wifi_hal/driver_tool.h>
#include <wifi_system/hal_tool.h>
#include <wifi_system/interface_tool.h>

#include "wificond/ipc_constants.h"
#include "wificond/looper_backed_event_loop.h"
#include "wificond/server.h"

using android::net::wifi::IWificond;
using android::wifi_hal::DriverTool;
using android::wifi_system::HalTool;
using android::wifi_system::InterfaceTool;
using android::wificond::ipc_constants::kDevModePropertyKey;
using android::wificond::ipc_constants::kDevModeServiceName;
using android::wificond::ipc_constants::kServiceName;
using std::unique_ptr;

namespace {

class ScopedSignalHandler final {
 public:
  ScopedSignalHandler(android::wificond::LooperBackedEventLoop* event_loop) {
    if (s_event_loop_ != nullptr) {
      LOG(FATAL) << "Only instantiate one signal handler per process!";
    }
    s_event_loop_ = event_loop;
    std::signal(SIGINT, &ScopedSignalHandler::LeaveLoop);
    std::signal(SIGTERM, &ScopedSignalHandler::LeaveLoop);
  }

  ~ScopedSignalHandler() {
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    s_event_loop_ = nullptr;
  }

 private:
  static android::wificond::LooperBackedEventLoop* s_event_loop_;
  static void LeaveLoop(int signal) {
    if (s_event_loop_ != nullptr) {
      s_event_loop_->TriggerExit();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedSignalHandler);
};

android::wificond::LooperBackedEventLoop*
    ScopedSignalHandler::s_event_loop_ = nullptr;


// Setup our interface to the Binder driver or die trying.
int SetupBinderOrCrash() {
  int binder_fd = -1;
  android::ProcessState::self()->setThreadPoolMaxThreadCount(0);
  android::IPCThreadState::self()->disableBackgroundScheduling(true);
  int err = android::IPCThreadState::self()->setupPolling(&binder_fd);
  CHECK_EQ(err, 0) << "Error setting up binder polling: " << strerror(-err);
  CHECK_GE(binder_fd, 0) << "Invalid binder FD: " << binder_fd;
  return binder_fd;
}

void RegisterServiceOrCrash(const android::sp<android::IBinder>& service) {
  android::sp<android::IServiceManager> sm = android::defaultServiceManager();
  CHECK_EQ(sm != NULL, true) << "Could not obtain IServiceManager";

  const int8_t dev_mode_on = property_get_bool(kDevModePropertyKey, 0);
  const char* service_name = (dev_mode_on) ? kDevModeServiceName : kServiceName;
  CHECK_EQ(sm->addService(android::String16(service_name), service),
           android::NO_ERROR);
}

}  // namespace

void OnBinderReadReady(int fd) {
  android::IPCThreadState::self()->handlePolledCommands();
}

int main(int argc, char** argv) {
  android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
  LOG(INFO) << "wificond is starting up...";

  unique_ptr<android::wificond::LooperBackedEventLoop> event_dispatcher(
      new android::wificond::LooperBackedEventLoop());
  ScopedSignalHandler scoped_signal_handler(event_dispatcher.get());

  int binder_fd = SetupBinderOrCrash();
  CHECK(event_dispatcher->WatchFileDescriptor(
      binder_fd,
      android::wificond::EventLoop::kModeInput,
      &OnBinderReadReady)) << "Failed to watch binder FD";


  android::sp<android::IBinder> server = new android::wificond::Server(
      unique_ptr<HalTool>(new HalTool),
      unique_ptr<InterfaceTool>(new InterfaceTool),
      unique_ptr<DriverTool>(new DriverTool));
  RegisterServiceOrCrash(server);

  event_dispatcher->Poll();
  LOG(INFO) << "wificond is about to exit";
  return 0;
}

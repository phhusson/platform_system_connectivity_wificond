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
#include <utils/String16.h>

#include <looper_backed_event_loop.h>
#include <server.h>

using android::net::wifi::IWificond;

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

}  // namespace

void OnBinderReadReady(int fd) {
  android::IPCThreadState::self()->handlePolledCommands();
}

int main(int argc, char** argv) {
  android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
  LOG(INFO) << "wificond is starting up...";
  std::unique_ptr<android::wificond::LooperBackedEventLoop> event_dispatcher_(
      new android::wificond::LooperBackedEventLoop());
  ScopedSignalHandler scoped_signal_handler(event_dispatcher_.get());

  int binder_fd = -1;
  android::ProcessState::self()->setThreadPoolMaxThreadCount(0);
  android::IPCThreadState::self()->disableBackgroundScheduling(true);
  int err = android::IPCThreadState::self()->setupPolling(&binder_fd);
  CHECK_EQ(err, 0) << "Error setting up binder polling: " << err;
  CHECK_GE(binder_fd, 0) << "Invalid binder FD: " << binder_fd;
  if (!event_dispatcher_->WatchFileDescriptor(
      binder_fd,
      android::wificond::EventLoop::kModeInput,
      &OnBinderReadReady)) {
    LOG(FATAL) << "Failed to watch binder FD";
  }
  android::sp<android::IServiceManager> sm = android::defaultServiceManager();
  CHECK_EQ(sm != NULL, true) << "Could not obtain IServiceManager";
  android::sp<android::IBinder> server = new android::wificond::Server();
  sm->addService(android::String16("wificond"), server);

  event_dispatcher_->Poll();
  LOG(INFO) << "Leaving the loop...";
  return 0;
}

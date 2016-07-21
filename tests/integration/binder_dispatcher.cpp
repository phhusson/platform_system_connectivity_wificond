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

#include <ctime>

#include "binder_dispatcher.h"

namespace android {
namespace wificond {
namespace tests {
namespace integration {

BinderDispatcher::BinderDispatcher()
    : event_dispatcher_(new LooperBackedEventLoop()),
      needs_init_(false),
      was_interrupted_(false) {}

bool BinderDispatcher::DispatchFor(int timeout_millis) {
  // Initialize the looper and binder.
  if (!needs_init_) {
    Init();
    needs_init_ = true;
  }

  was_interrupted_ = false;
  // Post a delayed task to stop the looper after |timeout_millis|.
  event_dispatcher_->PostDelayedTask(
      std::bind(&BinderDispatcher::StopDispatcher, this), timeout_millis);
  event_dispatcher_->Poll();
  return was_interrupted_;
}

void BinderDispatcher::InterruptDispatch() {
  was_interrupted_ = true;
  StopDispatcher();
}

void BinderDispatcher::Init() {
  // Initilize the binder fd for polling.
  android::ProcessState::self()->setThreadPoolMaxThreadCount(0);
  android::IPCThreadState::self()->disableBackgroundScheduling(true);
  int binder_fd = -1;
  int err = android::IPCThreadState::self()->setupPolling(&binder_fd);
  CHECK_EQ(err, 0) << "Error setting up binder polling: " << strerror(-err);
  CHECK_GE(binder_fd, 0) << "Invalid binder FD: " << binder_fd;

  auto binder_event_handler =
      std::bind(&BinderDispatcher::OnBinderEvent, this, std::placeholders::_1);
  // Add the binder fd to the looper watch list.
  CHECK(event_dispatcher_->WatchFileDescriptor(
      binder_fd,
      android::wificond::EventLoop::kModeInput,
      binder_event_handler))
      << "Failed to watch binder FD";
}

void BinderDispatcher::OnBinderEvent(int /* fd */) {
  android::IPCThreadState::self()->handlePolledCommands();
}

void BinderDispatcher::StopDispatcher() {
  event_dispatcher_->TriggerExit();
}

}  // namespace integration
}  // namespace tests
}  // namespace wificond
}  // namespace android

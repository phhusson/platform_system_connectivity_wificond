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

#include <looper_backed_event_loop.h>

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

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  LOG(INFO) << "wificond is starting up...";
  std::unique_ptr<android::wificond::LooperBackedEventLoop> event_dispatcher_(
      new android::wificond::LooperBackedEventLoop());
  ScopedSignalHandler scoped_signal_handler(event_dispatcher_.get());
  event_dispatcher_->Poll();
  LOG(INFO) << "Leaving the loop...";
  return 0;
}

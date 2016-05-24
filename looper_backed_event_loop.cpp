//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <looper_backed_event_loop.h>

#include <utils/Looper.h>
#include <utils/Timers.h>

namespace {

class EventLoopCallback : public android::MessageHandler {
 public:
  explicit EventLoopCallback(const std::function<void()>& callback)
      : callback_(callback) {
  }

  ~EventLoopCallback() override = default;

  virtual void handleMessage(const android::Message& message) {
    callback_();
  }

 private:
  const std::function<void()> callback_;

  DISALLOW_COPY_AND_ASSIGN(EventLoopCallback);
};

}  // namespace

namespace android {
namespace wificond {


LooperBackedEventLoop::LooperBackedEventLoop()
    : should_continue_(true) {
  looper_ = android::Looper::prepare(Looper::PREPARE_ALLOW_NON_CALLBACKS);
}

LooperBackedEventLoop::~LooperBackedEventLoop() {
}

void LooperBackedEventLoop::PostTask(const std::function<void()>& callback) {
  sp<android::MessageHandler> event_loop_callback =
      new EventLoopCallback(callback);
  looper_->sendMessage(event_loop_callback, NULL);
}

void LooperBackedEventLoop::PostDelayedTask(
    const std::function<void()>& callback,
    int64_t delay_ms) {
  sp<android::MessageHandler> looper_callback = new EventLoopCallback(callback);
  looper_->sendMessageDelayed(ms2ns(delay_ms), looper_callback, NULL);
}

void LooperBackedEventLoop::Poll() {
  while (should_continue_) {
    looper_->pollOnce(-1);
  }
}

void LooperBackedEventLoop::TriggerExit() {
  PostTask([this](){ should_continue_ = false; });
}

}  // namespace wificond
}  // namespace android

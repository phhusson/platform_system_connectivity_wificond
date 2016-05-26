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

#include <memory>

#include <gtest/gtest.h>
#include <utils/StopWatch.h>

#include <looper_backed_event_loop.h>

namespace {

const int kTimingToleranceMs = 25;

}  // namespace

namespace android {
namespace wificond {

class WificondLooperBackedEventLoopTest : public ::testing::Test {
 protected:
  std::unique_ptr<LooperBackedEventLoop> event_loop_;

  virtual void SetUp() {
    event_loop_.reset(new LooperBackedEventLoop());
  }
};

TEST_F(WificondLooperBackedEventLoopTest, LooperBackedEventLoopPostTaskTest) {
  bool task_executed = false;
  event_loop_->PostTask([this, &task_executed]() mutable {
      task_executed = true; event_loop_->TriggerExit();});
  EXPECT_FALSE(task_executed);
  event_loop_->Poll();
  EXPECT_TRUE(task_executed);
}

TEST_F(WificondLooperBackedEventLoopTest,
       LooperBackedEventLoopPostDelayedTaskTest) {
  bool task_executed = false;
  event_loop_->PostDelayedTask([this, &task_executed]() mutable {
      task_executed = true; event_loop_->TriggerExit();}, 500);
  EXPECT_FALSE(task_executed);
  StopWatch stopWatch("DelayedTask");
  event_loop_->Poll();
  int32_t elapsedMillis = ns2ms(stopWatch.elapsedTime());
  EXPECT_NEAR(500, elapsedMillis, kTimingToleranceMs);
  EXPECT_TRUE(task_executed);
}

}  // namespace wificond
}  // namespace android

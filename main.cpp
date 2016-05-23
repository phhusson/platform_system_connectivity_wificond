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

#include <android-base/logging.h>

namespace {

volatile bool ShouldContinue = true;

}  // namespace

void leave_loop(int signal) {
  ShouldContinue = false;
}

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  LOG(INFO) << "wificond is starting up...";
  std::signal(SIGINT, &leave_loop);
  std::signal(SIGTERM, &leave_loop);
  while (ShouldContinue) {
    sleep(1);
  }
  LOG(INFO) << "Leaving the loop...";
  return 0;
}

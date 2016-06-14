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

#ifndef WIFICOND_CLIENT_INTERFACE_H_
#define WIFICOND_CLIENT_INTERFACE_H_

#include <android-base/macros.h>

#include "android/net/wifi/BnClientInterface.h"

namespace android {
namespace wificond {

class ClientInterface : public android::net::wifi::BnClientInterface {
 public:
  ClientInterface() = default;
  ~ClientInterface() override = default;
  // TODO: Add functions configuring an interface.

 private:
  // TODO: Add fields decribing an interface.

  DISALLOW_COPY_AND_ASSIGN(ClientInterface);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_CLIENT_INTERFACE_H_

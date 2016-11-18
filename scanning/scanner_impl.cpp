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

#include "wificond/scanning/scanner_impl.h"

#include <vector>

#include <android-base/logging.h>

#include "wificond/scanning/scan_utils.h"

namespace android {
namespace wificond {

ScannerImpl::ScannerImpl(uint32_t interface_index,
                         ScanUtils* scan_utils)
    : valid_(true),
      interface_index_(interface_index),
      scan_utils_(scan_utils) {
  // Keep compiler happy.
  // Delete this when implementions are checked in.
  if (scan_utils_ == nullptr) {
    LOG(ERROR) << "Invalid ScanUtils for ScannerImpl on interface: "
               << interface_index_;
  }
}

ScannerImpl::~ScannerImpl() {
}

}  // namespace wificond
}  // namespace android

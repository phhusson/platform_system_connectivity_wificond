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

#ifndef WIFICOND_TEST_MOCK_OFFLOAD_H
#define WIFICOND_TEST_MOCK_OFFLOAD_H

#include <gmock/gmock.h>

#include <android/hardware/wifi/offload/1.0/IOffload.h>

using android::hardware::wifi::offload::V1_0::IOffload;
using android::hardware::wifi::offload::V1_0::ScanFilter;
using android::hardware::wifi::offload::V1_0::ScanParam;
using android::hardware::wifi::offload::V1_0::ScanStats;
using android::hardware::wifi::offload::V1_0::IOffloadCallback;
using android::hardware::Return;
using android::sp;

namespace android {
namespace wificond {

typedef std::function<void(const ScanStats& scanStats)> OnScanStatsCallback;

class MockOffload : public IOffload {
 public:
  MockOffload();
  ~MockOffload() override = default;

  MOCK_METHOD2(configureScans, Return<void>(
      const ScanParam& param, const ScanFilter& Filter));
  MOCK_METHOD1(getScanStats, Return<void>(OnScanStatsCallback cb));
  MOCK_METHOD1(subscribeScanResults, Return<void>(uint32_t delayMs));
  MOCK_METHOD0(unsubscribeScanResults, Return<void>());
  MOCK_METHOD1(setEventCallback, Return<void>(const sp<IOffloadCallback>& cb));

};

} // namespace wificond
} // namespace android

#endif // WIFICOND_TEST_MOCK_OFFLOAD_H
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

#include <linux/nl80211.h>

#include "net/nl80211_attribute.h"

using std::string;

namespace android {
namespace wificond {

namespace {

const uint32_t kScanFrequency1 = 2500;
const uint32_t kScanFrequency2 = 5000;
const uint32_t kRSSIThreshold = 80;
const uint32_t kRSSIHysteresis = 10;

}  // namespace

TEST(NL80211AttributeTest, AttributeScanFrequenciesListTest) {
  NL80211NestedAttr scan_freq(NL80211_ATTR_SCAN_FREQUENCIES);

  // Use 1,2,3 .. for anonymous attributes.
  NL80211Attr<uint32_t> freq1(1, kScanFrequency1);
  NL80211Attr<uint32_t> freq2(2, kScanFrequency2);
  scan_freq.AddAttribute(freq1);
  scan_freq.AddAttribute(freq2);

  EXPECT_EQ(scan_freq.GetAttributeId(), NL80211_ATTR_SCAN_FREQUENCIES);
  EXPECT_TRUE(scan_freq.HasAttribute(1, BaseNL80211Attr::kUInt32));
  EXPECT_TRUE(scan_freq.HasAttribute(2, BaseNL80211Attr::kUInt32));

  NL80211Attr<uint32_t> attr_u32(0, 0);
  EXPECT_TRUE(scan_freq.GetAttribute(1, BaseNL80211Attr::kUInt32, &attr_u32));
  EXPECT_EQ(attr_u32.GetValue(), kScanFrequency1);
  EXPECT_TRUE(scan_freq.GetAttribute(2, BaseNL80211Attr::kUInt32, &attr_u32));
  EXPECT_EQ(attr_u32.GetValue(), kScanFrequency2);
}

TEST(NL80211AttributeTest, AttributeCQMTest) {
  NL80211NestedAttr cqm(NL80211_ATTR_CQM);

  NL80211Attr<uint32_t> rssi_thold(NL80211_ATTR_CQM_RSSI_THOLD,
                                         kRSSIThreshold);
  NL80211Attr<uint32_t> rssi_hyst(NL80211_ATTR_CQM_RSSI_HYST,
                                        kRSSIHysteresis);
  NL80211Attr<uint32_t> rssi_threshold_event(
      NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
      NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW);
  cqm.AddAttribute(rssi_thold);
  cqm.AddAttribute(rssi_hyst);
  cqm.AddAttribute(rssi_threshold_event);

  EXPECT_EQ(cqm.GetAttributeId(), NL80211_ATTR_CQM);
  EXPECT_TRUE(cqm.HasAttribute(NL80211_ATTR_CQM_RSSI_THOLD,
                               BaseNL80211Attr::kUInt32));
  EXPECT_TRUE(cqm.HasAttribute(NL80211_ATTR_CQM_RSSI_HYST,
                               BaseNL80211Attr::kUInt32));
  EXPECT_TRUE(cqm.HasAttribute(NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
                               BaseNL80211Attr::kUInt32));

  NL80211Attr<uint32_t> attr_u32(0, 0);
  EXPECT_TRUE(cqm.GetAttribute(NL80211_ATTR_CQM_RSSI_THOLD,
                               BaseNL80211Attr::kUInt32,
                               &attr_u32));
  EXPECT_EQ(attr_u32.GetValue(), kRSSIThreshold);

  EXPECT_TRUE(cqm.GetAttribute(NL80211_ATTR_CQM_RSSI_HYST,
                               BaseNL80211Attr::kUInt32,
                               &attr_u32));
  EXPECT_EQ(attr_u32.GetValue(), kRSSIHysteresis);

  EXPECT_TRUE(cqm.GetAttribute(NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT,
                               BaseNL80211Attr::kUInt32,
                               &attr_u32));
  EXPECT_EQ(attr_u32.GetValue(), NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW);

}

}  // namespace wificond
}  // namespace android

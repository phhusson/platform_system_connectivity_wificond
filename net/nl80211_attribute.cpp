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

#include "net/nl80211_attribute.h"

#include "android-base/logging.h"

using std::vector;

namespace android {
namespace wificond {

void BaseNL80211Attr::InitAttributeHeader(int attribute_id,
                                          int payload_length) {
  data_.resize(NLA_HDRLEN, 0);
  nlattr* header = reinterpret_cast<nlattr*>(data_.data());
  header->nla_type = attribute_id;
  header->nla_len = NLA_HDRLEN + payload_length;
}

int BaseNL80211Attr::GetAttributeId() const {
  const nlattr* header = reinterpret_cast<const nlattr*>(data_.data());
  return header->nla_type;
}

const vector<uint8_t>& BaseNL80211Attr::GetConstData() const {
  return data_;
}

// For NL80211NestedAttr:
NL80211NestedAttr::NL80211NestedAttr(int id) {
  InitAttributeHeader(id, 0);
}

NL80211NestedAttr::NL80211NestedAttr(const vector<uint8_t>& data) {
  data_ = data;
}

void NL80211NestedAttr::AddAttribute(const BaseNL80211Attr& attribute) {
  const vector<uint8_t>& append_data = attribute.GetConstData();
  // Append the data of |attribute| to |this|.
  data_.insert(data_.end(), append_data.begin(), append_data.end());
  uint8_t* head_ptr = &data_.front();
  nlattr* header = reinterpret_cast<nlattr*>(head_ptr);
  header->nla_len += append_data.size();
}

bool NL80211NestedAttr::HasAttribute(int id, AttributeType type) const {
  return GetAttribute(id, type, nullptr);
}

bool NL80211NestedAttr::GetAttribute(int id,
                                     AttributeType type,
                                     BaseNL80211Attr* attribute) const {
  // Skip the top level attribute header.
  const uint8_t* ptr = data_.data() + NLA_HDRLEN;
  const uint8_t* end_ptr = data_.data() + data_.size();
  while (ptr + NLA_HDRLEN <= end_ptr) {
    const nlattr* header = reinterpret_cast<const nlattr*>(ptr);
    if (header->nla_type == id) {
      if (ptr + header->nla_len > end_ptr) {
        LOG(ERROR) << "Failed to get attribute: broken nl80211 atrribute.";
        return false;
      }
      if (attribute != nullptr) {
        switch(type) {
          case kNested:
            *attribute = NL80211NestedAttr(
                vector<uint8_t>(ptr, ptr + header->nla_len));
            break;
          case kUInt32:
            *attribute = NL80211Attr<uint32_t>(
                vector<uint8_t>(ptr, ptr + header->nla_len));
            break;
          // TODO(nywang): write cases for other types of attributes.
          default:
            return false;
        }
      }
      return true;
    }
    ptr += header->nla_len;
  }
  return false;
}

}  // namespace wificond
}  // namespace android

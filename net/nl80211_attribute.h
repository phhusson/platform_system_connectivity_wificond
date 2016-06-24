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

#ifndef WIFICOND_NL80211_ATTRIBUTE_H_
#define WIFICOND_NL80211_ATTRIBUTE_H_

#include <memory>
#include <vector>

#include <linux/netlink.h>

#include <android-base/macros.h>

namespace android {
namespace wificond {

class BaseNL80211Attr {
 public:
  enum AttributeType {
    kNested,
    kUInt32
  };

  virtual ~BaseNL80211Attr() = default;

  const std::vector<uint8_t>& GetConstData() const;
  int GetAttributeId() const;

 protected:
  BaseNL80211Attr() = default;
  void InitAttributeHeader(int attribute_id, int payload_length);

  std::vector<uint8_t> data_;
};

template <typename T>
class NL80211Attr : public BaseNL80211Attr {
 public:
  explicit NL80211Attr(int id, T value) {
    InitAttributeHeader(id, sizeof(T));
    data_.resize(data_.size() + sizeof(T));
    T* storage = reinterpret_cast<T*>(data_.data() + data_.size() - sizeof(T));
    *storage = value;
  }
  // Caller is a responsible for ensuring that |data| is:
  //   1) Is at least NLA_HDRLEN long
  //   2) That *data when interpreted as a nlattr is internally consistent
  // (e.g. data.size() == NLA_HDLLEN + nlattr.nla_len)
  explicit NL80211Attr(const std::vector<uint8_t>& data) {
    data_ = data;
  }

  ~NL80211Attr() override = default;

  T GetValue() const {
    return *reinterpret_cast<const T*>(data_.data() + NLA_HDRLEN);
  }
};  // class NL80211Attr

class NL80211NestedAttr : public BaseNL80211Attr {
 public:
  explicit NL80211NestedAttr(int id);
  explicit NL80211NestedAttr(const std::vector<uint8_t>& data);
  ~NL80211NestedAttr() override = default;

  void AddAttribute(const BaseNL80211Attr& attribute);
  bool HasAttribute(int id, AttributeType type) const;
  // Access an attribute nested within |this|.
  // The result is returned by writing the attribute object to |*attribute|.
  // Deeper nested attributes are not included. This means if A is nested within
  // |this|, and B is nested within A, this function can't be used to access B.
  // The reason is that we may have multiple attributes having the same
  // attribute id, nested within different level of |this|.
  bool GetAttribute(int id,
                    AttributeType type,
                    BaseNL80211Attr* attribute) const;
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_NL80211_ATTRIBUTE_H_

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

#include <json/json.h>

#include <android-base/logging.h>
#include <fi/w1/wpa_supplicant/INetwork.h>

#include "network_params.h"

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {

const char NetworkParams::kJsonHeader[] = "NetworkParams";
const char NetworkParams::kJsonKeySSID[] = "ssid";
const char NetworkParams::kJsonKeyKeyMgmt[] = "key_mgmt";
const char NetworkParams::kJsonKeyPskPassphrase[] = "psk_passphrase";

// List of default values assigned if not present in args.
const int NetworkParams::kDefaultKeyMgmt =
    fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_NONE;
const int NetworkParams::kDefaultProto =
    fi::w1::wpa_supplicant::INetwork::PROTO_MASK_WPA |
    fi::w1::wpa_supplicant::INetwork::PROTO_MASK_RSN;
const int NetworkParams::kDefaultAuthAlg =
    fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_OPEN;
const int NetworkParams::kDefaultGroupCipher =
    fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_TKIP |
    fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_CCMP;
const int NetworkParams::kDefaultPairwiseCipher =
    fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_TKIP |
    fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_CCMP;

std::unique_ptr<NetworkParams> NetworkParams::instance_;

bool NetworkParams::ParseFromJsonString(const std::string &json_string) {
  // Check if the provided string contain a json formatted string with the
  // expected header |kJsonHeader|.
  Json::Value empty_json(Json::objectValue);
  Json::Value json_root;
  Json::Reader reader;
  if (!reader.parse(json_string, json_root)) {
    return false;
  }
  Json::Value json_value = json_root.get(kJsonHeader, empty_json);
  if (json_value == empty_json) {
    return false;
  }
  LOG(INFO) << "Network Params Json: " << json_value;
  std::string ssid = json_value.get(kJsonKeySSID, empty_json).asString();
  if (ssid.empty()) {
    return false;
  }

  std::vector<uint8_t> ssid_vec(ssid.begin(), ssid.end());
  instance_.reset(new NetworkParams(ssid_vec));
  if (!instance_.get()) {
    return false;
  }

  if (json_value.get(kJsonKeyKeyMgmt, empty_json) != empty_json) {
    instance_->key_mgmt_mask_ =
        json_value.get(kJsonKeyKeyMgmt, empty_json).asInt();
  }
  if (json_value.get(kJsonKeyPskPassphrase, empty_json) != empty_json) {
    instance_->psk_passphrase_ =
        json_value.get(kJsonKeyPskPassphrase, empty_json).asString();
  }

  // TODO: Add other params parsing as needed.
  return true;
}

NetworkParams *NetworkParams::GetNetworkParamsForTest() {
  return instance_.get();
}

NetworkParams::NetworkParams(const std::vector<uint8_t> &ssid)
    : ssid_(ssid),
      key_mgmt_mask_(kDefaultKeyMgmt),
      proto_mask_(kDefaultProto),
      auth_alg_mask_(kDefaultAuthAlg),
      group_cipher_mask_(kDefaultGroupCipher),
      pairwise_cipher_mask_(kDefaultPairwiseCipher),
      wep_tx_key_idx_(0) {}

}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

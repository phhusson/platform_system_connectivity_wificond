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
#ifndef WIFICOND_TESTS_INTEGRATION_WPA_SUPPLICANT_BINDER_NETWORK_PARAMS_H
#define WIFICOND_TESTS_INTEGRATION_WPA_SUPPLICANT_BINDER_NETWORK_PARAMS_H

#include <json/json.h>

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {
// Network params to be passed in for connection tests. This is passed in via
// test command line args as json strings. This should atleast contain the
// |ssid| key, all the other keys are optional and takes default values
// if not specified.
// {
//   "NetworkParams" : {
//     "ssid" : "blah",
//     "psk_passphrase" : "blah123",
//     ...
//   }
// }
class NetworkParams {
 public:
  // Parse a new instance of |NetworkParams| class from the json string
  // passed in via command line args.
  // Returns true if successfully parsed, false otherwise.
  static bool ParseFromJsonString(const std::string &json_string);
  // Retrieve the raw pointer of the instance of network params passed in
  // for this test run.
  static NetworkParams *GetNetworkParamsForTest();

  // List of fields that have parsed/default values.
  std::vector<uint8_t> ssid_;
  int key_mgmt_mask_;
  int proto_mask_;
  int auth_alg_mask_;
  int group_cipher_mask_;
  int pairwise_cipher_mask_;
  std::string psk_passphrase_;
  int wep_tx_key_idx_;
  std::vector<uint8_t> wep_key0_;
  std::vector<uint8_t> wep_key1_;
  std::vector<uint8_t> wep_key2_;
  std::vector<uint8_t> wep_key3_;

 private:
  // List of supported json keys.
  static const char kJsonHeader[];
  static const char kJsonKeySSID[];
  static const char kJsonKeyKeyMgmt[];
  static const char kJsonKeyPskPassphrase[];

  // List of default values assigned if not present in args.
  static const int kDefaultKeyMgmt;
  static const int kDefaultProto;
  static const int kDefaultAuthAlg;
  static const int kDefaultGroupCipher;
  static const int kDefaultPairwiseCipher;

  // The instance of network params for this test run.
  static std::unique_ptr<NetworkParams> instance_;

  NetworkParams(const std::vector<uint8_t> &ssid);

};  // class NetworkParams
}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_TESTS_INTEGRATION_WPA_SUPPLICANT_BINDER_NETWORK_PARAMS_H

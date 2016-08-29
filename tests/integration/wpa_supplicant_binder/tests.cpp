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

#include "test_base.h"

using testing::_;
using testing::DoAll;
using testing::Return;

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {

// Verifies the |ISupplicant.CreateInterface| binder call.
TEST_F(WpaSupplicantBinderTestBase, CreateInterface) {
  // Create the interface.
  CreateInterfaceForTest();

  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE((status.isOk()) && (iface.get() != nullptr));
}

// Verifies the |ISupplicant.RemoveInterface| binder call.
TEST_F(WpaSupplicantBinderTestBase, RemoveInterface) {
  CreateInterfaceForTest();

  RemoveInterfaceForTest();

  // The interface should no longer be present now.
  android::sp<IIface> iface;
  android::binder::Status status =
      service_->GetInterface(kWlan0IfaceName, &iface);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              ISupplicant::ERROR_IFACE_UNKNOWN);
}

// Verifies the |ISupplicant.GetDebugLevel|,
// |ISupplicant.GetDebugShowTimestamp|, |ISupplicant.GetDebugShowKeys| binder
// calls.
TEST_F(WpaSupplicantBinderTestBase, GetDebugParams) {
  int debug_level;
  android::binder::Status status = service_->GetDebugLevel(&debug_level);
  EXPECT_TRUE((status.isOk()) &&
              (debug_level == ISupplicant::DEBUG_LEVEL_EXCESSIVE));

  bool debug_show_timestamp;
  status = service_->GetDebugShowTimestamp(&debug_show_timestamp);
  EXPECT_TRUE((status.isOk()) && (debug_show_timestamp));

  bool debug_show_keys;
  status = service_->GetDebugShowKeys(&debug_show_keys);
  EXPECT_TRUE((status.isOk()) && (debug_show_keys));
}

// Verifies the |IIface.GetName| binder call.
TEST_F(WpaSupplicantBinderTestBase, GetNameOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

// Verifies the |IIface.GetName| binder call on an interface
// which has been removed.
TEST_F(WpaSupplicantBinderTestBase, GetNameOnRemovedInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  std::string ifaceName;
  android::binder::Status status = iface->GetName(&ifaceName);
  EXPECT_TRUE(ifaceName == kWlan0IfaceName);

  RemoveInterfaceForTest();

  // Any method call on the iface object should return failure.
  status = iface->GetName(&ifaceName);
  EXPECT_TRUE(status.serviceSpecificErrorCode() == IIface::ERROR_IFACE_INVALID);
}

// Verifies the |IIface.AddNetwork| binder call.
TEST_F(WpaSupplicantBinderTestBase, AddNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  AddNetworkForTest(iface);
}

// Verifies the |IIface.RemoveNetwork| binder call.
TEST_F(WpaSupplicantBinderTestBase, RemoveNetworkOnInterface) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  RemoveNetworkForTest(iface, network_id);

  // The network should no longer be present now.
  status = iface->GetNetwork(network_id, &network);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              IIface::ERROR_NETWORK_UNKNOWN);
}

// Verifies the |INetwork.GetId| binder call.
TEST_F(WpaSupplicantBinderTestBase, GetIdOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));
}

// Verifies the |INetwork.GetInterfaceName| binder call.
TEST_F(WpaSupplicantBinderTestBase, GetInterfaceNameOnNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  std::string ifaceName;
  android::binder::Status status = network->GetInterfaceName(&ifaceName);
  EXPECT_TRUE((status.isOk()) && (ifaceName == kWlan0IfaceName));
}

// Verifies the |INetwork.GetId| binder call on a network
// which has been removed.
TEST_F(WpaSupplicantBinderTestBase, GetIdOnRemovedNetwork) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE((status.isOk()) && (network_id == 0));

  RemoveNetworkForTest(iface, network_id);

  // Any method call on the network object should return failure.
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.serviceSpecificErrorCode() ==
              INetwork::ERROR_NETWORK_INVALID);
}

// Verifies the |ISupplicantCallback.OnInterfaceCreated| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTestBase, OnInterfaceCreated) {
  // Register for supplicant service level state changes
  // (e.g. interface creation)
  android::sp<MockSupplicantCallback> callback(new MockSupplicantCallback);
  android::binder::Status status = service_->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  CreateInterfaceForTest();

  // All registered |ISupplicantCallback| listeners should be notified
  // of any interface adddition.
  EXPECT_CALL(*callback, OnInterfaceCreated(kWlan0IfaceName))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |ISupplicantCallback.OnInterfaceRemoved| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTestBase, OnInterfaceRemoved) {
  // Register for supplicant service level state changes
  // (e.g. interface creation)
  android::sp<MockSupplicantCallback> callback(new MockSupplicantCallback);
  android::binder::Status status = service_->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  CreateInterfaceForTest();

  RemoveInterfaceForTest();

  // All registered |ISupplicantCallback| listeners should be notified
  // of any interface adddition followed by interface removal.
  EXPECT_CALL(*callback, OnInterfaceCreated(kWlan0IfaceName))
      .WillOnce(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnInterfaceRemoved(kWlan0IfaceName))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |IIfaceCallback.OnNetworkAdded| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTestBase, OnNetworkAdded) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // First Register for |IIfaceCallback| using the corresponding |IIface|
  // object.
  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  android::binder::Status status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  // All registered |IIfaceCallback| listeners should be notified
  // of any network adddition.
  EXPECT_CALL(*callback, OnNetworkAdded(network_id))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));

  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |IIfaceCallback.OnNetworkRemoved| callback is
// invoked by wpa_supplicant.
TEST_F(WpaSupplicantBinderTestBase, OnNetworkRemoved) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // First Register for |IIfaceCallback| using the corresponding |IIface|
  // object.
  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  android::binder::Status status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk());

  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk());

  RemoveNetworkForTest(iface, network_id);

  // All registered |IIfaceCallback| listeners should be notified
  // of any network adddition folled by the network removal.
  EXPECT_CALL(*callback, OnNetworkAdded(network_id))
      .WillOnce(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnNetworkRemoved(network_id))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  // Wait for callback.
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kCallbackTimeoutMillis));
}

// Verifies the |INetwork.SetSSID| & |INetwork.GetSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  // Now set the ssid converting to a hex vector.
  std::vector<uint8_t> set_ssid_vec;
  set_ssid_vec.assign(kNetworkSSID, kNetworkSSID + strlen(kNetworkSSID));
  android::binder::Status status = network->SetSSID(set_ssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::vector<uint8_t> get_ssid_vec;
  status = network->GetSSID(&get_ssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_ssid_vec, get_ssid_vec);
}

// Verifies the |INetwork.SetBSSID| & |INetwork.GetBSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetBSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  // Now set the bssid converting to a hex vector.
  std::vector<uint8_t> set_bssid_vec;
  set_bssid_vec.assign(kNetworkBSSID, kNetworkBSSID + INetwork::BSSID_LEN);
  android::binder::Status status = network->SetBSSID(set_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::vector<uint8_t> get_bssid_vec;
  status = network->GetBSSID(&get_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_bssid_vec, get_bssid_vec);

  // Clear the bssid now.
  set_bssid_vec.clear();
  status = network->SetBSSID(set_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  status = network->GetBSSID(&get_bssid_vec);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're cleared.
  EXPECT_TRUE(get_bssid_vec.empty());
}

// Verifies the |INetwork.SetScanSSID| & |INetwork.GetScanSSID|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetScanSSID) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  bool set_scan_ssid = true;
  android::binder::Status status = network->SetScanSSID(set_scan_ssid);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  bool get_scan_ssid;
  status = network->GetScanSSID(&get_scan_ssid);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_scan_ssid, get_scan_ssid);
}

// Verifies the |INetwork.SetRequirePMF| & |INetwork.GetRequirePMF|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetRequirePMF) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  bool set_require_pmf = true;
  android::binder::Status status = network->SetRequirePMF(set_require_pmf);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  bool get_require_pmf;
  status = network->GetRequirePMF(&get_require_pmf);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_require_pmf, get_require_pmf);
}

// Verifies the |INetwork.SetPskPassphrase| & |INetwork.GetPskPassphrase|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetPskPassphrase) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  android::binder::Status status =
      network->SetPskPassphrase(kNetworkPassphrase);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  std::string get_psk_passphrase;
  status = network->GetPskPassphrase(&get_psk_passphrase);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(kNetworkPassphrase, get_psk_passphrase);
}

// Verifies the |INetwork.SetWepTxKeyIdx| & |INetwork.GetWepTxKeyIdx|
// binder calls on a network.
TEST_F(WpaSupplicantBinderTestBase, NetworkSetGetWepTxKeyIdx) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // Add network now.
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_idx = 1;
  android::binder::Status status = network->SetWepTxKeyIdx(set_idx);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_idx;
  status = network->GetWepTxKeyIdx(&get_idx);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(set_idx, get_idx);
}

// Verifies the |INetwork.SetWepKey| & |INetwork.GetWepKey|
// binder calls on a network.
class NetworkWepKeyTest
    : public WpaSupplicantBinderTestBase,
      public ::testing::WithParamInterface<std::vector<uint8_t>> {};

TEST_P(NetworkWepKeyTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  std::vector<uint8_t> set_wep_key = GetParam();
  android::binder::Status status;
  for (int i = 0; i < fi::w1::wpa_supplicant::INetwork::WEP_KEYS_MAX_NUM; i++) {
    status = network->SetWepKey(i, set_wep_key);
    EXPECT_TRUE(status.isOk()) << status.toString8();
  }

  std::vector<uint8_t> get_wep_key;
  for (int i = 0; i < fi::w1::wpa_supplicant::INetwork::WEP_KEYS_MAX_NUM; i++) {
    status = network->GetWepKey(i, &get_wep_key);
    EXPECT_TRUE(status.isOk()) << status.toString8();
    // Ensure they're the same.
    EXPECT_EQ(get_wep_key, set_wep_key);
  }
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetWepKey, NetworkWepKeyTest,
    ::testing::Values(std::vector<uint8_t>({0x56, 0x67, 0x67, 0xf4, 0x56}),
                      std::vector<uint8_t>({0x56, 0x67, 0x67, 0xf4, 0x56, 0x89,
                                            0xad, 0x67, 0x78, 0x89, 0x97, 0xa5,
                                            0xde})));

// Verifies the |INetwork.SetKeyMgmt| & |INetwork.GetKeyMgmt|
// binder calls on a network.
class NetworkKeyMgmtTest : public WpaSupplicantBinderTestBase,
                           public ::testing::WithParamInterface<int> {};

TEST_P(NetworkKeyMgmtTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetKeyMgmt(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetKeyMgmt(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetKeyMgmt, NetworkKeyMgmtTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_NONE,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_WPA_PSK,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_WPA_PSK,
        fi::w1::wpa_supplicant::INetwork::KEY_MGMT_MASK_IEEE8021X));

// Verifies the |INetwork.SetProto| & |INetwork.GetProto|
// binder calls on a network.
class NetworkProtoTest : public WpaSupplicantBinderTestBase,
                         public ::testing::WithParamInterface<int> {};

TEST_P(NetworkProtoTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetProto(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetProto(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetProto, NetworkProtoTest,
    ::testing::Values(fi::w1::wpa_supplicant::INetwork::PROTO_MASK_WPA,
                      fi::w1::wpa_supplicant::INetwork::PROTO_MASK_RSN,
                      fi::w1::wpa_supplicant::INetwork::PROTO_MASK_OSEN));

// Verifies the |INetwork.SetAuthAlg| & |INetwork.GetAuthAlg|
// binder calls on a network.
class NetworkAuthAlgTest : public WpaSupplicantBinderTestBase,
                           public ::testing::WithParamInterface<int> {};

TEST_P(NetworkAuthAlgTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetAuthAlg(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetAuthAlg(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetAuthAlg, NetworkAuthAlgTest,
    ::testing::Values(fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_OPEN,
                      fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_SHARED,
                      fi::w1::wpa_supplicant::INetwork::AUTH_ALG_MASK_LEAP));

// Verifies the |INetwork.SetGroupCipher| & |INetwork.GetGroupCipher|
// binder calls on a network.
class NetworkGroupCipherTest : public WpaSupplicantBinderTestBase,
                               public ::testing::WithParamInterface<int> {};

TEST_P(NetworkGroupCipherTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetGroupCipher(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetGroupCipher(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetGroupCipher, NetworkGroupCipherTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_WEP40,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_WEP104,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_TKIP,
        fi::w1::wpa_supplicant::INetwork::GROUP_CIPHER_MASK_CCMP));

// Verifies the |INetwork.SetPairwiseCipher| & |INetwork.GetPairwiseCipher|
// binder calls on a network.
class NetworkPairwiseCipherTest : public WpaSupplicantBinderTestBase,
                                  public ::testing::WithParamInterface<int> {};

TEST_P(NetworkPairwiseCipherTest, SetGet) {
  android::sp<IIface> iface = CreateInterfaceForTest();

  // Add network now.
  android::sp<INetwork> network = AddNetworkForTest(iface);

  int set_mask = GetParam();
  android::binder::Status status = network->SetPairwiseCipher(set_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  int get_mask;
  status = network->GetPairwiseCipher(&get_mask);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Ensure they're the same.
  EXPECT_EQ(get_mask, set_mask);
}

INSTANTIATE_TEST_CASE_P(
    NetworkSetGetPairwiseCipher, NetworkPairwiseCipherTest,
    ::testing::Values(
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_NONE,
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_TKIP,
        fi::w1::wpa_supplicant::INetwork::PAIRWISE_CIPHER_MASK_CCMP));

}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

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

#include "network_params.h"
#include "test_base.h"

using testing::_;
using testing::DoAll;
using testing::Return;

namespace android {
namespace wificond {
namespace wpa_supplicant_binder_test {

// Base class for all connection related tests.
class ConnectTestBase : public WpaSupplicantBinderTestBase {
 protected:
  // Set the provided networks params |params| on the provided INetwork binder
  // object |network|.
  void SetNetworkParams(const android::sp<INetwork> &network,
                        const NetworkParams *network_params) {
    android::binder::Status status;
    status = network->SetSSID(network_params->ssid_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    status = network->SetKeyMgmt(network_params->key_mgmt_mask_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    status = network->SetProto(network_params->proto_mask_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    status = network->SetAuthAlg(network_params->auth_alg_mask_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    status = network->SetGroupCipher(network_params->group_cipher_mask_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    status = network->SetPairwiseCipher(network_params->pairwise_cipher_mask_);
    EXPECT_TRUE(status.isOk()) << status.toString8();

    if (!network_params->psk_passphrase_.empty()) {
      status = network->SetPskPassphrase(network_params->psk_passphrase_);
      EXPECT_TRUE(status.isOk()) << status.toString8();
    }
    if (!network_params->wep_key0_.empty()) {
      status = network->SetWepKey(0, network_params->wep_key0_);
      EXPECT_TRUE(status.isOk()) << status.toString8();
    }
    if (!network_params->wep_key1_.empty()) {
      status = network->SetWepKey(1, network_params->wep_key1_);
      EXPECT_TRUE(status.isOk()) << status.toString8();
    }
    if (!network_params->wep_key2_.empty()) {
      status = network->SetWepKey(2, network_params->wep_key2_);
      EXPECT_TRUE(status.isOk()) << status.toString8();
    }
    if (!network_params->wep_key3_.empty()) {
      status = network->SetWepKey(3, network_params->wep_key3_);
      EXPECT_TRUE(status.isOk()) << status.toString8();
    }
    status = network->SetWepTxKeyIdx(network_params->wep_tx_key_idx_);
    EXPECT_TRUE(status.isOk()) << status.toString8();
  }
};

/**
 * Test Scenario:
 * 1. Creates the iface wlan0.
 * 2. Adds the specified network.
 * 3. Selects the network for connection.
 * 4. Waits for connection to the network.
 * 5. Disables the network.
 * 6. Waits for disconnection from the network.
 *
 * This verifies the |ISupplicant.CreateInterface|, |IIface.AddNetwork|,
 * |INetwork.SetSSID|, |INetwork.SetKeyMgmt|, |INetwork.Select|,
 * |INetwork.Disable|, |IIfaceCallback.OnStateChanged| binder methods.
 */
TEST_F(ConnectTestBase, SimpleConnectDisconnect) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  NetworkParams *network_params = NetworkParams::GetNetworkParamsForTest();
  ASSERT_TRUE(network_params)
      << "Unable to parse network params from command line";
  SetNetworkParams(network, network_params);

  // Initiate connection to the network by selecting it.
  status = network->Select();
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Wait for the wpa_supplicant connection to complete.
  EXPECT_CALL(*callback, OnStateChanged(_, _, _, _))
      .WillRepeatedly(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnStateChanged(MockIfaceCallback::STATE_COMPLETED, _,
                                        network_id, network_params->ssid_))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kConnectTimeoutMillis));

  // Now disable the network to trigger disconnection.
  status = network->Disable();
  EXPECT_TRUE(status.isOk()) << status.toString8();
  EXPECT_CALL(*callback,
              OnStateChanged(MockIfaceCallback::STATE_INACTIVE, _, _, _))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kConnectTimeoutMillis));
}

/**
 * Test Scenario:
 * 1. Creates the iface wlan0.
 * 2. Adds the specified network.
 * 3. Selects the network for connection.
 * 4. Waits for connection to the network.
 * 5. Disconnects from the network.
 * 6. Waits for disconnection from the network.
 * 7. Issues a reconnect to connect back.
 *
 * This verifies the |ISupplicant.CreateInterface|, |IIface.AddNetwork|,
 * |IIface.Disconnect|, |IIface.Reconnect|, |INetwork.SetSSID|,
 * |INetwork.SetKeyMgmt|, |INetwork.Select|,
 * |INetwork.Disable|, |IIfaceCallback.OnStateChanged| binder methods.
 */
TEST_F(ConnectTestBase, SimpleReconnect) {
  android::sp<IIface> iface = CreateInterfaceForTest();
  android::sp<INetwork> network = AddNetworkForTest(iface);
  int network_id;
  android::binder::Status status = network->GetId(&network_id);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  android::sp<MockIfaceCallback> callback(new MockIfaceCallback);
  status = iface->RegisterCallback(callback);
  EXPECT_TRUE(status.isOk()) << status.toString8();

  NetworkParams *network_params = NetworkParams::GetNetworkParamsForTest();
  ASSERT_TRUE(network_params)
      << "Unable to parse network params from command line";
  SetNetworkParams(network, network_params);

  // Initiate connection to the network by selecting it.
  status = network->Select();
  EXPECT_TRUE(status.isOk()) << status.toString8();

  // Wait for the wpa_supplicant connection to complete.
  EXPECT_CALL(*callback, OnStateChanged(_, _, _, _))
      .WillRepeatedly(Return(android::binder::Status::ok()));
  EXPECT_CALL(*callback, OnStateChanged(MockIfaceCallback::STATE_COMPLETED, _,
                                        network_id, network_params->ssid_))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kConnectTimeoutMillis));

  // Now disconnect the network.
  status = iface->Disconnect();
  EXPECT_TRUE(status.isOk()) << status.toString8();
  EXPECT_CALL(*callback,
              OnStateChanged(MockIfaceCallback::STATE_DISCONNECTED, _, _, _))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kConnectTimeoutMillis));

  // Issue reconnect again since we explicitly disconnected above.
  status = iface->Reconnect();
  EXPECT_TRUE(status.isOk()) << status.toString8();
  EXPECT_CALL(*callback, OnStateChanged(MockIfaceCallback::STATE_COMPLETED, _,
                                        network_id, network_params->ssid_))
      .WillOnce(DoAll(InterrupBinderDispatcher(&binder_dispatcher_),
                      Return(android::binder::Status::ok())));
  EXPECT_TRUE(binder_dispatcher_.DispatchFor(kConnectTimeoutMillis));
}
}  // namespace wpa_supplicant_binder_test
}  // namespace wificond
}  // namespace android

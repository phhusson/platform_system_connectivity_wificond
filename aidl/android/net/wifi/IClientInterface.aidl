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

package android.net.wifi;

// IClientInterface represents a network interface that can be used to connect
// to access points and obtain internet connectivity.
interface IClientInterface {

  // Enable a wpa_supplicant instance running against this interface.
  // Returns true if supplicant was successfully enabled, or is already enabled.
  boolean enableSupplicant();

  // Remove this interface from wpa_supplicant's control.
  // Returns true if removal was successful.
  boolean disableSupplicant();

  // Get packet counters for this interface.
  // First element in array is the number of successfully transmitted packets.
  // Second element in array is the number of tramsmission failure.
  int[] getPacketCounters();

  // Get the MAC address of this interface.
  byte[] getMacAddress();
}

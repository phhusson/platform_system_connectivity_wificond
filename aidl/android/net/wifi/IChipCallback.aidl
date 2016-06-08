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

import android.net.wifi.IApInterface;
import android.net.wifi.IClientInterface;

// A callback for receiving events related to this chip.
interface IChipCallback {

  // Signals that the provided interface is ready for future commands.
  oneway void OnClientInterfaceReady(
      int request_id,
      IClientInterface network_interface);
  oneway void OnApInterfaceReady(
      int request_id,
      IApInterface network_interface);

  // Signals that an interface torn down or that tear down has started.
  //
  // If |is_torn_down| is false, tear down has begun, but has not completed.
  // If |is_torn_down| is true, no future callbacks will be delivered
  // via this callback, and the callback is automatically unregistered.
  //
  // @param is_torn_down true if the interface is completely gone.
  oneway void OnClientTeardownEvent(
      IClientInterface network_interface,
      boolean is_torn_down);
  oneway void OnApTeardownEvent(
      IClientInterface network_interface,
      boolean is_torn_down);

  const int CONFIGURE_ERROR_UNSUPPORTED_MODE = -1;
  const int CONFIGURE_ERROR_UNSUPPORTED_CONFIGURATION = -2;
  const int CONFIGURE_ERROR_CONFLICTING_REQUEST = -3;

  // Signals that an IChip.ConfigureClientInterface() call failed.
  //
  // This event indicates that the configuration request failed and no
  // new interface was created.
  //
  // @param error_code one of CONFIGURE_ERROR_* defined above.
  // @param message a helpful message suitable for logging.
  oneway void OnInvalidConfigureRequest(
      int request_id,
      int error_code,
      String message);

}

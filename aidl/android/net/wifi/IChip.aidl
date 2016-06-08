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
import android.net.wifi.IChipCallback;
import android.net.wifi.IClientInterface;

// IChip represents a particular chip attached to the host.  Generally,
// chips have a set of capabilities which determine the ways they can
// be configured.  To use the functionality of a chip, request an interface
// be configured via IChip.Configure*Interface().  Chips may support being
// configured with multiple interfaces at once.
interface IChip {

  // Register an object to receive chip status updates.
  //
  // Multiple callbacks can be registered simultaneously.
  // Duplicate registrations of the same callback will be ignored.
  //
  // @param callback object to add to the set of registered callbacks.
  oneway void RegisterCallback(IChipCallback callback);

  // Remove a callback from the set of registered callbacks.
  //
  // This must be the same instance as previously registered.
  // Requests to remove unknown callbacks will be ignored.
  //
  // @param callback object to remove from the set of registered callbacks.
  oneway void UnregisterCallback(IChipCallback callback);

  // Request that the chip be configured to expose a new client interface.
  //
  // The success or failure of this request will be indicated to registered
  // IChipCallback objects.
  // The returned request id is unique over the lifetime of this instance
  // of wificond.  Note that a wificond restart may recycle old ids.
  //
  // @return request id to be used to match interface requests to creation
  //         events.
  int ConfigureClientInterface();

  // Request that the chip be configured to expose a new AP interface.
  //
  // The success or failure of this request will be indicated to registered
  // IChipCallback objects.
  // The returned request id is unique over the lifetime of this instance
  // of wificond.  Note that a wificond restart may recycle old ids.
  //
  // @return unique request id to be used to match interface requests to
  //         creation events.
  int ConfigureApInterface();

  // @return list of the currently configured IClientInterface instances.
  List<IBinder> GetClientInterfaces();

  // @return list of the currently configured IApInterface instances.
  List<IBinder> GetApInterfaces();

}

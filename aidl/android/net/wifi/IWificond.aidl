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

// Service interface that exposes primitives for controlling the WiFi
// subsystems of a device.
interface IWificond {

    // Create a network interface suitable for use as an AP.
    @nullable IApInterface createApInterface();

    // Create a network interface suitable for use as a WiFi client.
    @nullable IClientInterface createClientInterface();

    // Tear down all existing interfaces.  This should enable clients to create
    // future interfaces immediately after this method returns.
    void tearDownInterfaces();

}

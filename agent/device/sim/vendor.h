/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __VENDOR_H__
#define __VENDOR_H__
#define VENDOR_ID "ONF"
#define SIM
#define MODEL_ID  "sim"
#define MAX_SUPPORTED_PON 16
#define ONU_BIT_TRANSMISSION_DELAY 0.1004823/1000 /* unit: ns to us */
#define MINIMUM_ONU_RESPONSE_RANGING_TIME 1572135 /* hardcore: this is ranging time for the shortest distance, typically 35us */

// DeviceInfo definitions

#define ONU_ID_START 1
#define ONU_ID_END 32
#define MAX_ONUS_PER_PON (ONU_ID_END - ONU_ID_START + 1)

#define MAX_ALLOC_ID_PER_ONU 4
#define ALLOC_ID_START 1024
#define ALLOC_ID_END (ALLOC_ID_START + MAX_ONUS_PER_PON * MAX_ALLOC_ID_PER_ONU)

#define GEM_PORT_ID_PER_ALLOC_ID 8
#define GEM_PORT_ID_START 1024
#define GEM_PORT_ID_END (GEM_PORT_ID_START + MAX_ONUS_PER_PON * MAX_ALLOC_ID_PER_ONU * GEM_PORT_ID_PER_ALLOC_ID)

#define FLOW_ID_START 1
#define FLOW_ID_END 65535
#define MAX_FLOW_ID FLOW_ID_END
#define INVALID_FLOW_ID 0

#define MAC_DEVICE_ACTIVATION_DELAY 200000 // in microseconds

#endif

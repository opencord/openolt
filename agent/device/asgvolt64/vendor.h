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
#define VENDOR_ID "EdgeCore"
#define ASGVOLT64
#define MODEL_ID  "asgvolt64"
#define MAX_SUPPORTED_PON 64
#define ONU_BIT_TRANSMISSION_DELAY 0.8038585/1000 /* unit: ns to us */
#define MINIMUM_ONU_RESPONSE_RANGING_TIME 198075 /* hardcore: this is ranging time for the shortest distance, typically 35us */

// DeviceInfo definitions

#define ONU_ID_START 1
#define ONU_ID_END 32
#define MAX_ONUS_PER_PON ONU_ID_END

#define ALLOC_ID_START 256
#define ALLOC_ID_END 767

#define GEM_PORT_ID_START 256
#define GEM_PORT_ID_END 4095

#define FLOW_ID_START 1
#define FLOW_ID_END 65535
#define MAX_FLOW_ID FLOW_ID_END
#define INVALID_FLOW_ID 0

#endif

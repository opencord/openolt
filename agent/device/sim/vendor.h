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

#include "device.h"

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

#define DEFAULT_MAC_SYSTEM_MODE BCMOLT_SYSTEM_MODE_GPON__16_X
//#define DEFAULT_MAC_SYSTEM_MODE BCMOLT_SYSTEM_MODE_XGS__2_X
#define DEFAULT_PON_MODE BCMOLT_PON_TYPE_GPON
// #define DEFAULT_PON_MODE BCMOLT_PON_TYPE_XGPON


///////////////////////////////////////////////////////
// Constants relevant for decoding PON Trx EEPROM Data

#define DYNAMIC_PON_TRX_SUPPORT

#define TOTAL_PON_TRX_PORTS 16 // total PON transceiver ports
#define TOTAL_PON_PORTS 16 // total PON ports (we could have up to 2 PON ports on the OLT MAC mapped to the external PON Trx)
const int trx_port_to_pon_port_map[TOTAL_PON_TRX_PORTS][TOTAL_PON_PORTS/TOTAL_PON_TRX_PORTS]={{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},
{11},{12},{13},{14},{15}};
#define PONS_PER_TRX (TOTAL_PON_TRX_PORTS/TOTAL_PON_PORTS) // if there are more than one OLT MAC device,
                                                           // they all have to be of the same type for this to work.

const int bus_index[TOTAL_PON_TRX_PORTS] = {
   47,  48,  37,  38,  35,  36,  33,  34,
   39,  40,  41,  42,  43,  44,  45,  46
};

#define PORT_ADDRESS 50

#define NAME_EEPROM "sfp_eeprom"

#define EEPROM_VENDOR_NAME_START_IDX 148
#define EEPROM_VENDOR_NAME_LENGTH 16

#define EEPROM_VENDOR_OUI_START_IDX 165
#define EEPROM_VENDOR_OUI_LENGTH 3

#define EEPROM_VENDOR_PART_NUMBER_START_IDX 168
#define EEPROM_VENDOR_PART_NUMBER_LENGTH 16

#define EEPROM_VENDOR_REVISION_START_IDX 184
#define EEPROM_VENDOR_REVISION_LENGTH 2

#define EEPROM_DOWNSTREAM_WAVELENGTH_START_IDX 186
#define EEPROM_DOWNSTREAM_WAVELENGTH_LENGTH 2
#define EEPROM_WAVELENGTH_RESOLUTION 0.05

// Define valid values below in case of Combo PON Trx is supported
// #define EEPROM_DOWNSTREAM_SECONDARY_WAVELENGTH_START_IDX 120
// #define EEPROM_DOWNSTREAM_SECONDARY_WAVELENGTH_LENGTH 2

///////////////////////////////////////////////////////


class PonTrx: public PonTrxBase {
   public:
      // override the base member functions if you need a different implementation
      PonTrx(): PonTrxBase("sfp_eeprom", 50, "%d-00%d/%s") {

      }

};

#endif

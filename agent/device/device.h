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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <new>
#include <set>
#include <map>

extern "C"
{
#include <bcmolt_api.h>
#include <bcmolt_api_model_supporting_enums.h>
}

using namespace std;

/////////////////////////////////////////////////////

// Constant definitions

#define EEPROM_DATA_READ_SIZE 256 //bytes
#define EEPROM_READ_PATH_SIZE 200
#define PON_TRX_BUS_FORMAT_SIZE 64
#define MAX_PONS_PER_TRX 2

#define GPON_DOWNSTREAM_WAVELENGTH_NM 1490 // ITU-T G984.2 PMD specification
#define XGSPON_DOWNSTREAM_WAVELENGTH_NM 1577 // ITU-T G9807.1 PMD specification


/////////////////////////////////////////////////////

// Structure definitions

struct pon_data {
    bool valid; // set to true if the below fields are set after reading eeprom data
    uint32_t wavelength;
    bcmolt_pon_type pon_type;
    // add more fields as needed...
};

struct trx_data {
    int sfp_index;
    string vendor_name;
    uint8_t vendor_oui[3];
    string vendor_part_no;
    string vendor_rev;
    bcmolt_pon_type trx_type;
    pon_data p_data[MAX_PONS_PER_TRX];
};

/////////////////////////////////////////////////////

// Class Definitions

// PonTrx: This class defines the member functions and attributes of the Pon Transceiver object
class PonTrxBase {
    public:

        // Location of I2C file systems is pretty standard for Linux systems, and it is /sys/bus/i2c/devices
        // Check here https://www.kernel.org/doc/html/latest/i2c/i2c-sysfs.html#location-of-i2c-sysfs for documentation on Linux I2C
        // The OLTs we work with are based on Linux, however if it is something different this can be overridden.
        PonTrxBase(string eeprom_file_name="sfp_eeprom",
                   int port_addr=50,
                   string eeprom_file_format_name="/sys/bus/i2c/devices/%d-00%d/%s",
                   string sfp_presence_command="/lib/platform-config/current/onl/bin/onlpdump -p");

        // Reads, updates and returns the _sfp_presence_array from the OLT device
        const set<int> read_sfp_presence_data();

        // Returns the _sfp_presence_array
        const set<int> get_sfp_presence_data();

        // Reads the EEPROM data. The sfp_index is the bus index corresponding to the PON Trx
        // returns true if success
        bool read_eeprom_data_for_sfp(int sfp_index);

        // Decodes the EEPROM data. The sfp_index is the bus index corresponding to the PON Trx
        // returns true if success
        bool decode_eeprom_data(int sfp_index);

        // Get Trx Type based on SFP ID provided
        bcmolt_pon_type get_sfp_mode(int sfp_index);

        // Get PON Type based on optical wavelength specified
        bcmolt_pon_type get_pon_type_from_wavelength(int wavelength);

        // Get MAC System mode based on the olt mac id and the set of SFP IDs provided
        pair<bcmolt_system_mode, bool> get_mac_system_mode(int, set<int>);

        // Get Trx data
        trx_data* get_trx_data(int sfp_index);

        ~PonTrxBase();

    protected:
        set<int> _sfp_presence_data;
        set<trx_data*> _t_data;
        map<int, array<char, EEPROM_READ_PATH_SIZE>> _eeprom_read_path;
        map<int, array<unsigned char, EEPROM_DATA_READ_SIZE>> _eeprom_data;
        // Location of I2C file systems is pretty standard for Linux systems, and it is /sys/bus/i2c/devices
        // Check here https://www.kernel.org/doc/html/latest/i2c/i2c-sysfs.html#location-of-i2c-sysfs for documentation on Linux I2C
        // The OLTs we work with are based on Linux, however if it is something different this can be overridden during objection creation.
        string _eeprom_file_path_format = "/sys/bus/i2c/devices/%d-00%d/%s";
        string _eeprom_file_name;
        string _sfp_presence_command = "/lib/platform-config/current/onl/bin/onlpdump -p";
        int _port_addr;

};

/////////////////////////////////////////////////////

// Extern definitions
extern void vendor_init();

#endif

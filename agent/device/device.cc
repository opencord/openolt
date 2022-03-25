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

#include "device.h"
#include "vendor.h"
#include "core_utils.h"

using namespace std;

// PonTrxBase class member function definitions
// Note: 'cout' logger has been used in this file instead of OPENOLT_LOG
// because the logger task may not be initialized in the openolt core module
// before PonTrx module is initialized in the main handler. In that case,
// any OPENOLT_LOG logger (which internally uses BCM_LOG) will not be logged.

PonTrxBase::PonTrxBase(string eeprom_file_name,
                       int port_addr,
                       string eeprom_file_path_name,
                       string sfp_presence_command) :
        _eeprom_file_name{eeprom_file_name},
        _port_addr{port_addr},
        _eeprom_file_path_format{eeprom_file_path_name},
        _sfp_presence_command{sfp_presence_command}
{
}

PonTrxBase::~PonTrxBase() {
    for (auto it : _t_data) {
        delete it;
    }
}

// returns the _sfp_presence_data
const set<int> PonTrxBase::get_sfp_presence_data() {
    return _sfp_presence_data;
}

// reads, updates and returns the _sfp_presence_data
const set<int> PonTrxBase::read_sfp_presence_data() {
    set<int> sp;
    // command to read the sfp presence array
    // The sample output of the command looks like below
    // root@localhost:~# onlpdump -p
    // Presence: 0 1 16
    const string& command = _sfp_presence_command;
    int status_code = system((command + " > temp.txt").c_str());
    if (status_code != 0) {
        perror("error getting sfp presence array from ONL\n");
        // return the current _sfp_presence_data
        return _sfp_presence_data;
    }
    ifstream ifs("temp.txt");
    string res = {istreambuf_iterator<char>(ifs), istreambuf_iterator<char>()};
    ifs.close(); // must close the inout stream so the file can be cleaned up
    // If there is issue reading the sfp presence array, print error and return existing data
    if (remove("temp.txt") != 0) {
        perror("Error deleting temporary file");
        // return the current sfp presence array
        return _sfp_presence_data;
    }

    // parse the output of sfp presence command and read which PON Trx are connected/disconnected now
    stringstream ss(res);
    string s;
    while (getline(ss, s, ' ')) {
        try {
            int i = stoi(s);
            // Set the Trx port index corresponding the PON port to true if it appears on the sfp presence command output
            if (i < TOTAL_PON_TRX_PORTS) {
                sp.insert(i);
            }

        } catch(invalid_argument) {
            // do nothing
        } catch(out_of_range) {
            // do nothing
        }
    }

    // If the current _sfp_presence_data is not same the newly read data,
    // update local copy and also raise an event.
    if (sp != _sfp_presence_data) {
        cout << "sfp presence data has been updated\n";

        set<int> sfp_ports_up, sfp_ports_down;

        set_difference(sp.begin(), sp.end(), _sfp_presence_data.begin(), _sfp_presence_data.end(), inserter(sfp_ports_up, sfp_ports_up.begin()));
        // New SFPs are detected
        if (sfp_ports_up.size() > 0) {
            cout << "following pon sfp ports are up: ";
            for (auto up_it : sfp_ports_up) {
                cout << up_it << " ";
            }
            cout << "\n";
        }
        set_difference(_sfp_presence_data.begin(), _sfp_presence_data.end(), sp.begin(), sp.end(), inserter(sfp_ports_down, sfp_ports_down.begin()));
        // Some SFPs are down
        if (sfp_ports_down.size() > 0) {
            cout << "following pon sfp ports are down: \n";
            for (auto down_it : sfp_ports_down) {
                cout << down_it << " ";
            }
            cout << "\n";
        }

        // FIXME: raise an event for all the PON Trx ports that have been updated
        _sfp_presence_data = sp;
    }

    return _sfp_presence_data;
}

// reads the EEPROM data. The sfp_index is the bus index corresponding to the PON Trx
// returns true if success
bool PonTrxBase::read_eeprom_data_for_sfp(int sfp_index) {
    ifstream is;
    int len;
    array<char, EEPROM_READ_PATH_SIZE> path{};
    array<unsigned char, EEPROM_DATA_READ_SIZE> data{};

    if (sfp_index >= TOTAL_PON_TRX_PORTS) {
        perror("invalid pon trx index\n");
        return false;
    }

    if (_eeprom_read_path.count(sfp_index) == 0) {
        _eeprom_read_path[sfp_index] = path;
        char file_name[PON_TRX_BUS_FORMAT_SIZE];
        char *tmp_place_holder = new char[PON_TRX_BUS_FORMAT_SIZE];
        // 4 is max length of _port_addr
        if ((_eeprom_file_path_format.length() + _eeprom_file_name.length() + 4) > PON_TRX_BUS_FORMAT_SIZE) {
            cerr << "resultant eeprom file length too long\n";
            delete []tmp_place_holder;
            return false;
        }
        strcpy(tmp_place_holder, _eeprom_file_path_format.c_str());
        sprintf(file_name, tmp_place_holder, bus_index[sfp_index], _port_addr, _eeprom_file_name.c_str());
        memcpy(&_eeprom_read_path[sfp_index][0], file_name, PON_TRX_BUS_FORMAT_SIZE);
        delete []tmp_place_holder;
    }
    cout << "eeprom file to read is: " << _eeprom_read_path[sfp_index].data() << "for sfp " << sfp_index << endl;
    FILE* fp = fopen(_eeprom_read_path[sfp_index].data(), "rb");
    if (!fp) {
        perror("failed to open file");
        _eeprom_read_path.erase(sfp_index);
        return false;
    }
    _eeprom_data[sfp_index] = data;
    int val, cnt = 0;

    // Populate EEPROM data
    while ((val = fgetc(fp)) != EOF && cnt < EEPROM_DATA_READ_SIZE) {
        _eeprom_data[sfp_index][cnt] = (unsigned char)val;
        cnt++;
    }
    fclose(fp);
    if (cnt != EEPROM_DATA_READ_SIZE) {
        cerr << "invalid length of data read:" << cnt << endl;
        _eeprom_read_path.erase(sfp_index);
        _eeprom_data.erase(sfp_index);
        return false;
    }

    return true;
}

// decodes the EEPROM data. The sfp_index is the bus index corresponding to the PON Trx
// returns true if success
bool PonTrxBase::decode_eeprom_data(int sfp_index) {
    /*
    try {
    */

    trx_data tmp, *store=NULL;
    tmp.sfp_index = sfp_index;

    // decode vendor name
    array<unsigned char, EEPROM_VENDOR_NAME_LENGTH> vn{};
    copy(&_eeprom_data[sfp_index][EEPROM_VENDOR_NAME_START_IDX],
         &_eeprom_data[sfp_index][EEPROM_VENDOR_NAME_START_IDX] + EEPROM_VENDOR_NAME_LENGTH,
         &vn[0]);
    pair<string, bool> res_str = hex_to_ascii_string(vn.data(), vn.max_size());
    if (!res_str.second) {
        perror("error decoding vendor name\n");
        return false;
    }
    tmp.vendor_name = res_str.first;
    cout << "[" << sfp_index << "]" << " vendor name: " << tmp.vendor_name << endl;


    // decode vendor oui
    array<unsigned char, EEPROM_VENDOR_OUI_LENGTH> oui{};
    copy(&_eeprom_data[sfp_index][EEPROM_VENDOR_OUI_START_IDX],
         &_eeprom_data[sfp_index][EEPROM_VENDOR_OUI_START_IDX] + EEPROM_VENDOR_OUI_LENGTH,
         &oui[0]);
    res_str = hex_to_ascii_string(oui.data(), oui.max_size());
    if (!res_str.second) {
        perror("error decoding vendor oui\n");
        return false;
    }
    if (res_str.first.length() == 3) { // vendor_oui length is 3
        memcpy(tmp.vendor_oui, res_str.first.c_str(), 3);
    }
    cout << "[" << sfp_index << "]" << " vendor oui: " << res_str.first << endl;

    // decode vendor part number
    array<unsigned char, EEPROM_VENDOR_PART_NUMBER_LENGTH> pn{};
    copy(&_eeprom_data[sfp_index][EEPROM_VENDOR_PART_NUMBER_START_IDX],
         &_eeprom_data[sfp_index][EEPROM_VENDOR_PART_NUMBER_START_IDX] + EEPROM_VENDOR_PART_NUMBER_LENGTH,
         &pn[0]);
    res_str = hex_to_ascii_string(pn.data(), pn.max_size());
    if (!res_str.second) {
        perror("error decoding vendor part number\n");
        return false;
    }
    tmp.vendor_part_no = res_str.first;
    cout << "[" << sfp_index << "]" << " vendor pn: " << tmp.vendor_part_no << endl;

    // decode vendor revision
    array<unsigned char, EEPROM_VENDOR_REVISION_LENGTH> rev{};
    copy(&_eeprom_data[sfp_index][EEPROM_VENDOR_REVISION_START_IDX],
         &_eeprom_data[sfp_index][EEPROM_VENDOR_REVISION_START_IDX] + EEPROM_VENDOR_REVISION_LENGTH,
         &rev[0]);
    res_str = hex_to_ascii_string(rev.data(), rev.max_size());
    if (!res_str.second) {
        perror("error decoding vendor revision\n");
        return false;
    }
    tmp.vendor_rev = res_str.first;
    cout << "[" << sfp_index << "]" << " vendor rev: " << tmp.vendor_rev << endl;

    // decode pon wavelength(s)
    array<unsigned char, EEPROM_DOWNSTREAM_WAVELENGTH_LENGTH> wv{};
    copy(&_eeprom_data[sfp_index][EEPROM_DOWNSTREAM_WAVELENGTH_START_IDX],
         &_eeprom_data[sfp_index][EEPROM_DOWNSTREAM_WAVELENGTH_START_IDX] + EEPROM_DOWNSTREAM_WAVELENGTH_LENGTH,
         &wv[0]);
    pair<uint32_t, bool> res_uint = hex_to_uinteger(wv.data(), wv.max_size());
    if (!res_uint.second) {
        perror("error decoding primary downstream wavelength\n");
        return false;
    }
    tmp.p_data[0].valid = true;
    tmp.p_data[0].wavelength = res_uint.first * EEPROM_WAVELENGTH_RESOLUTION;
    tmp.p_data[0].pon_type = get_pon_type_from_wavelength(tmp.p_data[0].wavelength);
    cout << "[" << sfp_index << "]" << " pon wavelength: " << tmp.p_data[0].wavelength << endl;
    // TODO: also fill the field pon_type

    // TODO: Let's ignore secondary wavelength decode for now as we do not know how the secondary wavelength is represent on the Combo PON
    tmp.p_data[1].valid = false;

    store = new(nothrow) trx_data;
    if (store == NULL) {
        perror("could not allocate memory for trx_data");
        return false;
    }

    // Copy data
    store->sfp_index = sfp_index;
    store->vendor_name = tmp.vendor_name;
    memcpy(store->vendor_oui, tmp.vendor_oui, 3);
    store->vendor_part_no = tmp.vendor_part_no;
    store->vendor_rev = tmp.vendor_rev;
    store->trx_type = tmp.trx_type;
    for (int i = 0; i < MAX_PONS_PER_TRX; i++) {
        store->p_data[i].valid = tmp.p_data[i].valid;
        store->p_data[i].wavelength = tmp.p_data[i].wavelength;
        store->p_data[i].pon_type = tmp.p_data[i].pon_type;
    }

    _t_data.insert(store);
    /*
    } catch(...) { // FIXME: Put specific exceptions here
        perror("caught exception\n");
        return false;
    }
    */
    return true;
}

bcmolt_pon_type PonTrxBase::get_pon_type_from_wavelength(int wavelength) {
    bcmolt_pon_type pon_type = BCMOLT_PON_TYPE_UNKNOWN;
    switch (wavelength) {
        case GPON_DOWNSTREAM_WAVELENGTH_NM:
            pon_type = BCMOLT_PON_TYPE_GPON;
        case XGSPON_DOWNSTREAM_WAVELENGTH_NM:
            pon_type = BCMOLT_PON_TYPE_XGPON;
        // Add more in the future as needed.
    }
    return pon_type;
}

bcmolt_pon_type PonTrxBase::get_sfp_mode(int sfp_index) {
    bcmolt_pon_type pon0_type = BCMOLT_PON_TYPE_UNKNOWN;
    bcmolt_pon_type pon1_type = BCMOLT_PON_TYPE_UNKNOWN;
    trx_data *td = NULL;
    set<trx_data*>::iterator it;
    // Iterate the trx data set and break if we find a trx data with mathcing sfp index
    for (it = _t_data.begin(); it != _t_data.end(); it++) {
        if ((*it)->sfp_index == sfp_index) {
            td = *it;
            break;
        }
    }
    if (it == _t_data.end()) {
        perror("end of iterator, pon type not detected");
        return pon0_type;
    }
    if (td == NULL) {
        cerr << "trx data is null\n";
        return pon0_type;
    }

    // Find the PON type/mode
    if (td->p_data[0].valid) {
        pon0_type = td->p_data[0].pon_type;
        if (pon0_type == BCMOLT_PON_TYPE_UNKNOWN) {
            return pon0_type;
        }
        if (td->p_data[1].valid) {
            pon1_type = td->p_data[1].pon_type;
            if (pon1_type == BCMOLT_PON_TYPE_UNKNOWN) {
                return pon1_type;
            }

            if  (pon0_type == pon1_type) {
                return pon0_type;
            } else {
                return BCMOLT_PON_TYPE_XGPON_GPON_WDMA; // Combo SFP!
            }
        }
        return pon0_type;
    }

    return pon0_type;
}

// Returns the MAC System Mode based on the set of SFP IDs provided.
// The derived class will most likely need to override this method to provide a
// different implementation for that particular OLT platform.
pair<bcmolt_system_mode, bool> PonTrxBase::get_mac_system_mode(int olt_mac_id, set<int> sfp_ids) {
    bool ret = true;
    bcmolt_system_mode sm = BCMOLT_SYSTEM_MODE_GPON__16_X;

    return {sm, ret};
}

// Get trx data for sfp
trx_data* PonTrxBase::get_trx_data(int sfp_index) {
    for (auto it : _t_data) {
        if (it->sfp_index == sfp_index) {
            cout << "[" << sfp_index << "]" << " found trx data\n";
            return (it);
        }
    }
    return NULL;
}

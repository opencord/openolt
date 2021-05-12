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

#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <stdexcept>

#include "trx_eeprom_reader.h"

// g++ -std=c++11 -DRXTX_POWER_EXE_MODE trx_eeprom_reader.cc -o rssi

TrxEepromReader::TrxEepromReader(device_type dev_type, const power_type read_type, const int port)
    : _read_type{read_type},
      _port{port} {

    if (dev_type == DEVICE_EITHER) {
        std::string device = TrxEepromReader::get_board_name();
        if (device == "ASGvOLT64") {
            dev_type = TrxEepromReader::DEVICE_GPON;
        } else if (device == "ASXvOLT16") {
            dev_type = TrxEepromReader::DEVICE_XGSPON;
        } else { // improbable
            std::cerr << "ERROR - unknown device: '" << device << "'\n";
            dev_type = TrxEepromReader::DEVICE_XGSPON;
        }
    }

    switch (dev_type) {
        case DEVICE_GPON:
            _dev_name = "ASGvOLT64";
            _buf_size = 600;
            _read_offset = 360,
            _read_num_bytes = 2;
            _port_addr = 50;
            _name_eeprom = "eeprom";
            _bus_index = _gpon_bus_index;
            _max_ports = sizeof(_gpon_bus_index) / sizeof(_gpon_bus_index[0]);
            break;

        case DEVICE_XGSPON:
            _dev_name = "ASXvOLT16";
            _buf_size = 256;
            _read_offset = 104,
            _read_num_bytes = 2;
            _port_addr = 50;
            _name_eeprom = "sfp_eeprom";
            _bus_index = _xgspon_bus_index;
            _max_ports = sizeof(_xgspon_bus_index) / sizeof(_xgspon_bus_index[0]);
            break;
    }

    // trick: convert the first string specifier to integer specifier
    sprintf(_node_format, _master_format, "%d", _port_addr, _name_eeprom.c_str());
}

std::pair<std::string, bool>  TrxEepromReader::read_txt_file(const std::string& file_name) {
    std::ifstream in_file(file_name);

    if (!in_file.is_open()) {
        std::cerr << "error while opening file '" << file_name << "'\n";
        return {"", false};
    }

    std::stringstream buffer;
    buffer << in_file.rdbuf();

    return {buffer.str(), in_file.good()};
}

std::string TrxEepromReader::get_board_name() {
    const std::string board_path = "/sys/devices/virtual/dmi/id/board_name";
    auto res = TrxEepromReader::read_txt_file(board_path);

    // read failure is improbable
    if (!res.second) {
        std::cerr << "ERROR - file " << board_path << "cannot be opened\n";
    }

    if (res.first.find_last_of("\n") != std::string::npos) {
        res.first.pop_back();
    }

    return res.first;
}

int TrxEepromReader::read_binary_file(char* buffer) {
    std::ifstream is;
    int len;

    if (buffer == NULL || _buf_size < 0) {
        return -1;
    }

#ifdef TEST_MODE
    is.open("./eeprom.bin", std::ios_base::in | std::ios_base::binary);
#else
    is.open(_node_path, std::ios_base::in | std::ios_base::binary);
#endif

    if (!is.is_open()) {
        return -2;
    }

    is.read(buffer, _buf_size);
    len = is.gcount();
    is.close();

    if (len > _buf_size || len < (_read_offset + _read_num_bytes)) {
        return -4;
    }

    return len;
}

bool TrxEepromReader::is_valid_port() const {
    return (_port >= 0 && (size_t)_port < _max_ports);
}

void TrxEepromReader::set_port_path() {
    sprintf(_node_path, _node_format, _bus_index[_port]);
}

unsigned long TrxEepromReader::get_value_from_pointer_u(unsigned char *ptr, int size) {
    unsigned long sum = 0;
    unsigned char i;

    if (size > 4) {
        return (sum);
    }

    for (i = 0; i < size; ++i) {
        sum = sum * 256 + (*(ptr + i));
    }

    return (sum);
}

double TrxEepromReader::raw_rx_to_mw(int raw) {
    return raw * 0.0001;
}

double TrxEepromReader::raw_tx_to_mw(int raw) {
    return raw * 0.0002;
}

double TrxEepromReader::mw_to_dbm(double mw) {
    return 10 * log10(mw);
}

std::pair<std::pair<int, int>, bool> TrxEepromReader::read_power_raw() {
    if (is_valid_port()) {
        set_port_path();

        char* eeprom_data = new (std::nothrow) char[_buf_size];

        if (!eeprom_data) {
            std::cerr << "ERROR - memory allocation for eeprom_data failed\n";
            return {{0, 0}, false};
        }

        int ret_file = read_binary_file(eeprom_data);

        if (ret_file < 0) {
            std::cerr << "ERROR - eeprom_data file cannot be opened\n";
            return {{0, 0}, false};
        }

        // le = Little Endian, be = Big Endian
        unsigned char rx_power_le[_read_num_bytes] = {0};
        unsigned char tx_power_le[_read_num_bytes] = {0};
        unsigned long rx_power_be = 0;
        unsigned long tx_power_be = 0;

        if (_read_type == RX_POWER || _read_type == RX_AND_TX_POWER) {
            memcpy(&rx_power_le, eeprom_data + _read_offset, _read_num_bytes);
            rx_power_be = get_value_from_pointer_u(rx_power_le, _read_num_bytes);
        }

        if (_read_type == TX_POWER || _read_type == RX_AND_TX_POWER) {
            memcpy(&tx_power_le, eeprom_data + _read_offset - _read_num_bytes, _read_num_bytes);
            tx_power_be = get_value_from_pointer_u(tx_power_le, _read_num_bytes);
        }

        if (_read_type == RX_POWER) {
            tx_power_be = 0;
        }

        if (_read_type == TX_POWER) {
            rx_power_be = 0;
        }

        delete[] eeprom_data;

        return {{(int)rx_power_be, (int)tx_power_be}, true};
    } else {
        std::cerr << "ERROR - invalid port: " << _port << '\n';
        return {{0, 0}, false};
    }
}

std::pair<std::pair<double, double>, bool> TrxEepromReader::read_power_mean_dbm() {
    auto power_raw = read_power_raw();
    if (power_raw.second) {
        return {{mw_to_dbm(raw_rx_to_mw(power_raw.first.first)), mw_to_dbm(raw_tx_to_mw(power_raw.first.second))}, true};
    } else {
        return {{0.0, 0.0}, false};
    }
}

std::string TrxEepromReader::dump_data() {
    std::ostringstream dump;

    dump << "\tdevice:       " << _dev_name << '\n'
         << "\tbuffer size:  " << _buf_size << '\n'
         << "\tread offset:  " << _read_offset << '\n'
         << "\tnum bytes:    " << _read_num_bytes << '\n'
         << "\tmax ports:    " << _max_ports << '\n'
         << "\tport:         " << _port << '\n';

    //dump << std::showpos;
    dump << std::fixed;
    dump << std::setprecision(3);

    auto rxtx_power_raw = this->read_power_raw();

    dump << "\tnode path:    " << _node_path << '\n';

    if (rxtx_power_raw.second) {
        if (_read_type == RX_POWER || _read_type == RX_AND_TX_POWER) {
            dump << "\tRx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.first
                 << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.first
                 << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << raw_rx_to_mw(rxtx_power_raw.first.first) << " mW, "
                 << std::dec << std::setw(9) << mw_to_dbm(raw_rx_to_mw(rxtx_power_raw.first.first)) << " dBm\n";
        }
        if (_read_type == TX_POWER || _read_type == RX_AND_TX_POWER) {
            dump << "\tTx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.second
                 << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.second
                 << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << raw_tx_to_mw(rxtx_power_raw.first.second) << " mW, "
                 << std::dec << std::setw(9) << mw_to_dbm(raw_tx_to_mw(rxtx_power_raw.first.second)) << " dBm\n";
        }
    }

    return dump.str();
}

int TrxEepromReader::get_buf_size() const {
    return _buf_size;
}

int TrxEepromReader::get_read_offset() const {
    return _read_offset;
}

int TrxEepromReader::get_read_num_bytes() const {
    return _read_num_bytes;
}

int TrxEepromReader::get_max_ports() const {
    return _max_ports;
}

const char* TrxEepromReader::get_node_path() const {
    return _node_path;
}

#ifdef RXTX_POWER_EXE_MODE
int main(int argc, char *argv[]) {
    int port = 0;

    if (argc > 1) {
        std::string help{argv[1]};

        // trim leading '-'s
        do {
            if (help.find_first_of("-") == std::string::npos) {
                break;
            } else {
                help.erase(0, 1);
            }
        } while (true);

        if (help == "h" || help == "help") {
            std::cout << "usage:\n\t" << argv[0] << " " << port << '\n';
            std::cout << "\t#1 port no\n";
            return 0;
        }

        try {
            port = std::stoi(argv[1]);
        } catch(std::invalid_argument eia) {
            std::cerr << "ERROR - invalid argument exception: '" << argv[1] << "'\n";
            return 1;
        } catch(std::out_of_range eoor) {
            std::cerr << "ERROR - out of range exception: '" << argv[1] << "'\n";
            return 1;
        }
    }

    TrxEepromReader trx_eeprom_reader{TrxEepromReader::DEVICE_EITHER, TrxEepromReader::RX_AND_TX_POWER, port};

    std::cout << trx_eeprom_reader.dump_data() << '\n';

    return 0;
}
#endif

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

#ifndef TRX_EEPROM_READER_H
#define TRX_EEPROM_READER_H

/**
 * RSSI - Received Signal Strength Indication
 * see:
 *      https://en.wikipedia.org/wiki/Received_signal_strength_indication
 *
 * per EC Ticket#7825
 * see:
 *      https://support.edge-core.com/hc/en-us/requests/7825
 */

class TrxEepromReader {
    public:
        enum device_type {
            DEVICE_EITHER,
            DEVICE_GPON,
            DEVICE_XGSPON
        };

        enum power_type {
            RX_POWER,
            TX_POWER,
            RX_AND_TX_POWER
        };

        TrxEepromReader(device_type dev_type, const power_type read_type, const int port);
        TrxEepromReader() = delete;

        static std::pair<std::string, bool> read_txt_file(const std::string& file_name);
        static std::string get_board_name();

        int read_binary_file(char* buffer);
        bool is_valid_port() const;
        void set_port_path();
        unsigned long get_value_from_pointer_u(unsigned char *ptr, int size);
        double raw_rx_to_mw(int raw);
        double raw_tx_to_mw(int raw);
        double mw_to_dbm(double mw);
        std::pair<std::pair<int, int>, bool> read_power_raw();
        std::pair<std::pair<double, double>, bool> read_power_mean_dbm();
        std::string dump_data();
        int get_buf_size() const;
        int get_read_offset() const;
        int get_read_num_bytes() const;
        int get_max_ports() const;
        const char* get_node_path() const;

    private:
        int _port;
        power_type _read_type;
        std::string _dev_name;
        int _buf_size;
        int _read_offset;
        int _read_num_bytes;
        int _port_addr;
        std::string _name_eeprom;
        const int* _bus_index;
        size_t _max_ports;

        char _node_format[64] = {0};
        char _node_path[64] = {0};

        const char* _master_format = "/sys/bus/i2c/devices/%s-00%d/%s";
        const int _gpon_bus_index[74] = {
            41,  42,  56,  55,  43,  44,  54,  53,
            45,  46,  52,  51,  47,  48,  50,  49,
            57,  58,  72,  71,  59,  60,  70,  69,
            61,  62,  68,  67,  63,  64,  66,  65,
            73,  74,  88,  87,  75,  76,  86,  85,
            77,  78,  84,  83,  79,  80,  82,  81,
            89,  90, 104, 103,  91,  92, 102, 101,
            93,  94, 100,  99,  95,  96,  98,  97,
            20,  21,  25,  26,  27,  28,  29,  30,
            31,  32
        };
        const int _xgspon_bus_index[20] = {
            47,  48,  37,  38,  35,  36,  33,  34,
            39,  40,  41,  42,  43,  44,  45,  46,
            49,  50,  51,  52
        };
};

#endif

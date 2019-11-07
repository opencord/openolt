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
#include <string.h>
#include "utils.h"

std::string serial_number_to_str(bcmolt_serial_number* serial_number) {
#define SERIAL_NUMBER_SIZE 12
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%c%c%c%c%1X%1X%1X%1X%1X%1X%1X%1X",
            serial_number->vendor_id.arr[0],
            serial_number->vendor_id.arr[1],
            serial_number->vendor_id.arr[2],
            serial_number->vendor_id.arr[3],
            serial_number->vendor_specific.arr[0]>>4 & 0x0f,
            serial_number->vendor_specific.arr[0] & 0x0f,
            serial_number->vendor_specific.arr[1]>>4 & 0x0f,
            serial_number->vendor_specific.arr[1] & 0x0f,
            serial_number->vendor_specific.arr[2]>>4 & 0x0f,
            serial_number->vendor_specific.arr[2] & 0x0f,
            serial_number->vendor_specific.arr[3]>>4 & 0x0f,
            serial_number->vendor_specific.arr[3] & 0x0f);

    return buff;
}

std::string vendor_specific_to_str(char const * const vendor_specific) {
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%1X%1X%1X%1X%1X%1X%1X%1X",
            vendor_specific[0]>>4 & 0x0f,
            vendor_specific[0] & 0x0f,
            vendor_specific[1]>>4 & 0x0f,
            vendor_specific[1] & 0x0f,
            vendor_specific[2]>>4 & 0x0f,
            vendor_specific[2] & 0x0f,
            vendor_specific[3]>>4 & 0x0f,
            vendor_specific[3] & 0x0f);

    return buff;
}


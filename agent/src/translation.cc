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
#include "translation.h"

int interface_key_to_port_no(bcmolt_interface_id intf_id, 
        bcmolt_interface_type intf_type) {
    if (intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
        return (0x1 << 24) + intf_id;
    }
    if (intf_type == BCMOLT_INTERFACE_TYPE_PON) {
        return (0x2 << 28) + intf_id;
    }
    return intf_id;
}

std::string alarm_status_to_string(bcmolt_status status) {
    switch (status) {
        case BCMOLT_STATUS_OFF:
            return "off";
        case BCMOLT_STATUS_ON:
            return "on";
        case BCMOLT_STATUS_NO_CHANGE:
            return "no_change";
    }
    return "unknown";
}

std::string bcmolt_result_to_string(bcmolt_result result) {
    switch (result) {
        case BCMOLT_RESULT_SUCCESS:
            return "success";
        case BCMOLT_RESULT_FAIL:
            return "fail";
        default:
            return "unknown";
    }
}


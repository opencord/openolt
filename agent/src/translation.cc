/*
    Copyright (C) 2018 Open Networking Foundation

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "translation.h"

int interface_key_to_port_no(bcmolt_interface_id intf_id, 
        bcmolt_interface_type intf_type) {
    if (intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
        return (0x1 << 16) + intf_id;
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


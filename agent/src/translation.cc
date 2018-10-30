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

int interface_key_to_port_no(bcmbal_interface_key key) {
    if (key.intf_type == BCMBAL_INTF_TYPE_NNI) {
        return (0x1 << 16) + key.intf_id;
    }
    if (key.intf_type == BCMBAL_INTF_TYPE_PON) {
        return (0x2 << 28) + key.intf_id;
    }
    return key.intf_id;
}

std::string alarm_status_to_string(bcmbal_alarm_status status) {
    switch (status) {
        case BCMBAL_ALARM_STATUS_OFF:
            return "off";
        case BCMBAL_ALARM_STATUS_ON:
            return "on";
        case BCMBAL_ALARM_STATUS_NO__CHANGE:
            return "no_change";
    }
    return "unknown";
}


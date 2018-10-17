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

#ifndef OPENOLT_TRANSLATION_H_
#define OPENOLT_TRANSLATION_H_

#include <string>
extern "C"
{
#include <bal_model_types.h>
}

int interface_key_to_port_no(bcmbal_interface_key key);
std::string alarm_status_to_string(bcmbal_alarm_status status);



#endif

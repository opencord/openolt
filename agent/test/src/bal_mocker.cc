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



#include "bal_mocker.h"
extern "C" {
CMOCK_MOCK_FUNCTION1(BalMocker, bcmolt_host_init, bcmos_errno(bcmolt_host_init_parms*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_cfg_get, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_oper_submit, bcmos_errno(bcmolt_oltid, bcmolt_oper*));
}

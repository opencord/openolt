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

#ifndef __BAL_MOCKER_H__
#define __BAL_MOCKER_H__
#include <cmock/cmock.h>
#include <cstdlib>
#include "bcmos_errno.h"
#include "bcmolt_host_api.h"
#include "bcmolt_system_types_typedefs.h"
#include "bcmolt_msg.h"

class BalMocker : public CMockMocker<BalMocker>
{
public:
    MOCK_METHOD1(bcmolt_host_init, bcmos_errno(bcmolt_host_init_parms*));
    MOCK_METHOD2(bcmolt_cfg_get, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
    MOCK_METHOD2(bcmolt_oper_submit, bcmos_errno(bcmolt_oltid, bcmolt_oper*));
};
#endif

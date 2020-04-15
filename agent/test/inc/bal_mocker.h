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
#ifndef __BAL_MOCKER_H__
#define __BAL_MOCKER_H__
#include <cmock/cmock.h>
#include <gmock-global/gmock-global.h>
#include <cstdlib>
#include <grpc++/grpc++.h>
using grpc::Status;
extern "C" {
#include <bcmos_system.h>
#include <bcmolt_api.h>
#include <bcm_dev_log.h>
#include "bcmos_errno.h"
#include "bcmolt_system_types_typedefs.h"
#include "bcmolt_msg.h"

/** Host subsystem initialization parameters */
typedef struct bcmolt_host_init_parms
{
    int dummy;
} bcmolt_host_init_parms;

}

class BalMocker : public CMockMocker<BalMocker>
{
public:
    MOCK_METHOD1(bcmolt_host_init, bcmos_errno(bcmolt_host_init_parms*));
    MOCK_METHOD2(bcmolt_cfg_get, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
    MOCK_METHOD2(bcmolt_oper_submit, bcmos_errno(bcmolt_oltid, bcmolt_oper*));
    MOCK_METHOD2(bcmolt_cfg_set, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
    MOCK_METHOD2(bcmolt_cfg_clear, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
    MOCK_METHOD2(bcmolt_stat_cfg_set, bcmos_errno(bcmolt_oltid, bcmolt_stat_cfg*));
  // Add more here
};

#endif

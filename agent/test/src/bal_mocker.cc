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

#include "bal_mocker.h"
extern "C" {
CMOCK_MOCK_FUNCTION1(BalMocker, bcmolt_host_init, bcmos_errno(bcmolt_host_init_parms*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_cfg_get, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_oper_submit, bcmos_errno(bcmolt_oltid, bcmolt_oper*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_cfg_set, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
CMOCK_MOCK_FUNCTION2(BalMocker, bcmolt_cfg_clear, bcmos_errno(bcmolt_oltid, bcmolt_cfg*));
}


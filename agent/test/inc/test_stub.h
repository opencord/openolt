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
#ifndef __TEST_STUB_H__
#define __TEST_STUB_H__

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#include "bcmolt_conn_mgr.h"
#include "bcmcli_session.h"
#include "bcmos_errno.h"

// defines some stub definitions that are not available to openolt application
// when compoling in TEST_MODE
//#define BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT 0
#define MAX_SUPPORTED_PON 16


void bcmos_usleep(uint32_t us);
void bcmos_fastlock_init(bcmos_fastlock *lock, uint32_t flags);
long bcmos_fastlock_lock(bcmos_fastlock *lock);
void bcmos_fastlock_unlock(bcmos_fastlock *lock, long flags);

#endif //__TEST_STUB_H__


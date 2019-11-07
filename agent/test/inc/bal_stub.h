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
#ifndef _BAL_STUB_H_
#define _BAl_STUB_H_
extern "C" {
#include <test_stub.h>


void bcmos_usleep(uint32_t us);
void bcmos_fastlock_init(bcmos_fastlock *lock, uint32_t flags);
long bcmos_fastlock_lock(bcmos_fastlock *lock);
void bcmos_fastlock_unlock(bcmos_fastlock *lock, long flags);
}
#endif

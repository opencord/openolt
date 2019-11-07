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
#ifndef __BCMOLT_CONN_MGR_H__
#define __BCMOLT_CONN_MGR_H__
// This is stub header file for unit tets case only
//
extern "C" {
typedef char bcmolt_cm_addr[100];

typedef int bcmolt_goid;

typedef struct bcmos_fastlock {
    pthread_mutex_t lock;
} bcmos_fastlock;

}
#endif

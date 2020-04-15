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

#ifndef BCMOS_SYSTEM_TEST_H_
#define BCMOS_SYSTEM_TEST_H_

#include <pthread.h>

/**
This header file provides missing BAL API definitions required for unit test compilation.
*/

#define BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT 0
struct bcmos_mutex { pthread_mutex_t m; };

#endif


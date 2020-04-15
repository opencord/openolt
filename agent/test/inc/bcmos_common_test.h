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

#ifndef BCMOS_COMMON_TEST_H_
#define BCMOS_COMMON_TEST_H_

/**
This header file provides missing BAL API definitions required for unit test compilation.
*/

extern void* bcmos_calloc(uint32_t size);
#define BCMOLT_INTERFACE_TYPE_EPON_1_G 3
#define BCMOLT_INTERFACE_TYPE_EPON_10_G 4

#endif // BCMOS_COMMON_TEST_H_

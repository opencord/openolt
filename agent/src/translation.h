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
#ifndef OPENOLT_TRANSLATION_H_
#define OPENOLT_TRANSLATION_H_

#include <string>
extern "C"
{
#include <bcmolt_api_model_api_structs.h>
}

int interface_key_to_port_no(bcmolt_interface_id intf_id, 
        bcmolt_interface_type intf_type);
std::string alarm_status_to_string(bcmolt_status status);
std::string bcmolt_result_to_string(bcmolt_result result);

#endif

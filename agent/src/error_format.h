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

#ifndef OPENOLT_ERROR_FORMAT_H_
#define OPENOLT_ERROR_FORMAT_H_

#include <string>
#include <grpc++/grpc++.h>

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
}

grpc::Status bcm_to_grpc_err(bcmos_errno bcm_err, std::string message);
std::string grpc_status_code_to_string(grpc::StatusCode status_code);


#endif

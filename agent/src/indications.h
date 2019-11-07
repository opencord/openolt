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
#ifndef OPENOLT_INDICATIONS_H_
#define OPENOLT_INDICATIONS_H_

#include <grpc++/grpc++.h>
#include <voltha_protos/openolt.grpc.pb.h>
#include "Queue.h"

extern "C" {
    #include <bcm_dev_log_task.h>
}

extern Queue<openolt::Indication> oltIndQ;
extern grpc::Status SubscribeIndication();
extern dev_log_id openolt_log_id;
extern dev_log_id omci_log_id;
extern uint32_t nni_intf_id;

#endif

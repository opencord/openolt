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

#ifndef OPENOLT_STATS_COLLECTION_H_
#define OPENOLT_STATS_COLLECTION_H_

#include <voltha_protos/openolt.grpc.pb.h>

extern "C"
{
#include <bcmolt_api_model_supporting_structs.h>
}

void init_stats();
void stop_collecting_statistics();
openolt::PortStatistics* get_default_port_statistics();
openolt::PortStatistics* collectPortStatistics(bcmolt_interface_id intf_id, bcmolt_interface_type intf_type);
#if 0
openolt::FlowStatistics* get_default_flow_statistics();
openolt::FlowStatistics* collectFlowStatistics(bcmbal_flow_id flow_id, bcmbal_flow_type flow_type);
void register_new_flow(bcmbal_flow_key key);
#endif


#endif

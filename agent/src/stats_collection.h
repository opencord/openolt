/*
    Copyright (C) 2018 Open Networking Foundation

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPENOLT_STATS_COLLECTION_H_
#define OPENOLT_STATS_COLLECTION_H_

#include <openolt.grpc.pb.h>

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

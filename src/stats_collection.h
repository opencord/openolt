#ifndef OPENOLT_STATS_COLLECTION_H_
#define OPENOLT_STATS_COLLECTION_H_

#include <openolt.grpc.pb.h>

extern "C"
{
#include <bal_model_types.h>
}

void start_collecting_statistics();
void stop_collecting_statistics();
openolt::PortStatistics* get_default_port_statistics();
openolt::PortStatistics* collectPortStatistics(int intf_id, bcmbal_intf_type intf_type);
openolt::FlowStatistics* get_default_flow_statistics();
openolt::FlowStatistics* collectFlowStatistics(bcmbal_flow_id flow_id, bcmbal_flow_type flow_type);
void* stats_collection(void* x);
void register_new_flow(bcmbal_flow_key key);


#endif

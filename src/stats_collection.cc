#include "stats_collection.h"

#include <unistd.h>

#include <openolt.grpc.pb.h>
#include "indications.h"
#include "core.h"

extern "C"
{
#include <bcmos_system.h>
#include <bal_api.h>
#include <bal_api_end.h>
#include <flow_fsm.h>
}

//FIXME
#define FLOWS_COUNT 100

bcmbal_flow_key* flows_keys = new bcmbal_flow_key[FLOWS_COUNT];

void init_stats() {
    memset(flows_keys, 0, FLOWS_COUNT * sizeof(bcmbal_flow_key));
}

openolt::PortStatistics* get_default_port_statistics() {
    openolt::PortStatistics* port_stats = new openolt::PortStatistics;
    port_stats->set_intf_id(-1);
    port_stats->set_rx_bytes(-1);
    port_stats->set_rx_packets(-1);
    port_stats->set_rx_ucast_packets(-1);
    port_stats->set_rx_mcast_packets(-1);
    port_stats->set_rx_bcast_packets(-1);
    port_stats->set_rx_error_packets(-1);
    port_stats->set_tx_bytes(-1);
    port_stats->set_tx_packets(-1);
    port_stats->set_tx_ucast_packets(-1);
    port_stats->set_tx_mcast_packets(-1);
    port_stats->set_tx_bcast_packets(-1);
    port_stats->set_tx_error_packets(-1);
    port_stats->set_rx_crc_errors(-1);
    port_stats->set_bip_errors(-1);

    return port_stats;
}

#if 0
openolt::FlowStatistics* get_default_flow_statistics() {
    openolt::FlowStatistics* flow_stats = new openolt::FlowStatistics;
    flow_stats->set_flow_id(-1);
    flow_stats->set_rx_bytes(-1);
    flow_stats->set_rx_packets(-1);
    flow_stats->set_tx_bytes(-1);
    flow_stats->set_tx_packets(-1);

    return flow_stats;
}
#endif

openolt::PortStatistics* collectPortStatistics(int intf_id, bcmbal_intf_type intf_type) {

    bcmos_errno err;
    bcmbal_interface_stat stat;     /**< declare main API struct */
    bcmbal_interface_key key = { }; /**< declare key */
    bcmos_bool clear_on_read = false;

    openolt::PortStatistics* port_stats = get_default_port_statistics();
    // build key
    key.intf_id = (bcmbal_intf_id) intf_id;
    key.intf_type = intf_type;

    /* init the API struct */
    BCMBAL_STAT_INIT(&stat, interface, key);
    BCMBAL_STAT_PROP_GET(&stat, interface, all_properties);

    /* call API */
    err = bcmbal_stat_get(DEFAULT_ATERM_ID, &stat.hdr, clear_on_read);
    if (err == BCM_ERR_OK)
    {
        //std::cout << "Interface statistics retrieved"
        //          << " intf_id:" << intf_id << std::endl;

        port_stats->set_rx_bytes(stat.data.rx_bytes);
        port_stats->set_rx_packets(stat.data.rx_packets);
        port_stats->set_rx_ucast_packets(stat.data.rx_ucast_packets);
        port_stats->set_rx_mcast_packets(stat.data.rx_mcast_packets);
        port_stats->set_rx_bcast_packets(stat.data.rx_bcast_packets);
        port_stats->set_rx_error_packets(stat.data.rx_error_packets);
        port_stats->set_tx_bytes(stat.data.tx_bytes);
        port_stats->set_tx_packets(stat.data.tx_packets);
        port_stats->set_tx_ucast_packets(stat.data.tx_ucast_packets);
        port_stats->set_tx_mcast_packets(stat.data.tx_mcast_packets);
        port_stats->set_tx_bcast_packets(stat.data.tx_bcast_packets);
        port_stats->set_tx_error_packets(stat.data.tx_error_packets);
        port_stats->set_rx_crc_errors(stat.data.rx_crc_errors);
        port_stats->set_bip_errors(stat.data.bip_errors);

    } else {
        std::cout << "ERROR: Failed to retrieve port statistics"
                  << " intf_id:" << intf_id
                  << " intf_type:" << intf_type << std::endl;
    }

    return port_stats;

}

#if 0
openolt::FlowStatistics* collectFlowStatistics(bcmbal_flow_id flow_id, bcmbal_flow_type flow_type) {

    bcmos_errno err;
    bcmbal_flow_stat stat;     /**< declare main API struct */
    bcmbal_flow_key key = { }; /**< declare key */
    bcmos_bool clear_on_read = false;

    openolt::FlowStatistics* flow_stats = get_default_flow_statistics();
    //Key
    key.flow_id = flow_id;
    key.flow_type = flow_type;

    /* init the API struct */
    BCMBAL_STAT_INIT(&stat, flow, key);
    BCMBAL_STAT_PROP_GET(&stat, flow, all_properties);

    err = bcmbal_stat_get(DEFAULT_ATERM_ID, &stat.hdr, clear_on_read);

    if (err == BCM_ERR_OK)
    {
        std::cout << "Flow statistics retrieved"
                  << " flow_id:" << flow_id
                  << " flow_type:" << flow_type << std::endl;

        flow_stats->set_rx_bytes(stat.data.rx_bytes);
        flow_stats->set_rx_packets(stat.data.rx_packets);
        flow_stats->set_tx_bytes(stat.data.tx_bytes);
        flow_stats->set_tx_packets(stat.data.tx_packets);

    } else {
        std::cout   << "ERROR: Failed to retrieve flow statistics"
                    << " flow_id:" << flow_id
                    << " flow_type:" << flow_type << std::endl;
    }

    return flow_stats;
}
#endif


void* stats_collection() {

    if (!state.is_connected()) {
        std::cout << "Voltha is not connected, do not collect stats" << std::endl;
        return;
    }
    if (!state.is_activated()) {
        std::cout << "The OLT is not up, do not collect stats" << std::endl;
        return;
    }

    time_t now;

    std::cout << "Collecting statistics" << std::endl;

    //Ports statistics

    //Uplink ports
    for (int i = 0; i < 4; i++) {
        openolt::PortStatistics* port_stats = collectPortStatistics(i, BCMBAL_INTF_TYPE_NNI);
        //FIXME Use clean port translation
        port_stats->set_intf_id(128 + i);
        time(&now);
        port_stats->set_timestamp((int)now);
        openolt::Indication ind;
        ind.set_allocated_port_stats(port_stats);
        oltIndQ.push(ind);
    }
    //Pon ports
    for (int i = 0; i < 16; i++) {
        openolt::PortStatistics* port_stats = collectPortStatistics(i, BCMBAL_INTF_TYPE_PON);
        //FIXME Use clean port translation
        port_stats->set_intf_id((0x2 << 28) + i);
        time(&now);
        port_stats->set_timestamp((int)now);
        openolt::Indication ind;
        ind.set_allocated_port_stats(port_stats);
        oltIndQ.push(ind);
    }

    //Flows statistics
    // flow_inst *current_entry = NULL;
    //
    // TAILQ_FOREACH(current_entry,
    //               &FLOW_FSM_FLOW_LIST_CTX_PTR->active_flow_list,
    //               flow_inst_next) {
    // int flows_measurements = 0;
    //
    // for (int i = 0; i < FLOWS_COUNT; i++) {
    //
    //     // bcmbal_flow_id flow_id = current_entry->api_req_flow_info.key.flow_id;
    //     // bcmbal_flow_type flow_type = current_entry->api_req_flow_info.key.flow_type;
    //
    //     if (flows_keys[i].flow_id != 0) {
    //         openolt::FlowStatistics* flow_stats = collectFlowStatistics(flows_keys[i].flow_id, flows_keys[i].flow_type);
    //         if (flow_stats->rx_packets() == -1) {
    //             //It Failed
    //             flows_keys[i].flow_id = 0;
    //         } else {
    //             flow_stats->set_flow_id(flows_keys[i].flow_id);
    //             time(&now);
    //             flow_stats->set_timestamp((int)now);
    //             openolt::Indication ind;
    //             ind.set_allocated_flow_stats(flow_stats);
    //             oltIndQ.push(ind);
    //             flows_measurements ++;
    //         }
    //     }
    //
    // }
    // std::cout << "Stats of " << flows_measurements << " flows retrieved" << std::endl;

}

/* Storing flow keys, temporary */
void register_new_flow(bcmbal_flow_key key) {
    for (int i = 0; i < FLOWS_COUNT; i++) {
        if (flows_keys[i].flow_id == 0) {
            flows_keys[i] = key;
            break;
        }
    }
}

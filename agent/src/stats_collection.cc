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

#include "stats_collection.h"

#include <unistd.h>

#include <openolt.grpc.pb.h>
#include "indications.h"
#include "core.h"
#include "translation.h"

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
#include <bcmolt_api_model_api_structs.h>
}

//FIXME
#define FLOWS_COUNT 100

bcmolt_flow_key* flows_keys = new bcmolt_flow_key[FLOWS_COUNT];
bcmolt_odid device_id = 0;    

void init_stats() {
    memset(flows_keys, 0, FLOWS_COUNT * sizeof(bcmolt_flow_key));
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

openolt::PortStatistics* collectPortStatistics(bcmolt_intf_ref intf_ref) {

    bcmos_errno err;
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
    bcmolt_nni_interface_stats nni_stats;
    bcmolt_onu_itu_pon_stats pon_stats;
    bcmolt_pon_interface_itu_pon_stats itu_pon_stats;

    openolt::PortStatistics* port_stats = get_default_port_statistics();

    switch (intf_ref.intf_type) {
        case BCMOLT_INTERFACE_TYPE_NNI:
        {
            bcmolt_nni_interface_key nni_intf_key;
            nni_intf_key.id = intf_ref.intf_id;
            /* init the API struct */
            BCMOLT_STAT_INIT(&nni_stats, nni_interface, stats, nni_intf_key);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_bytes);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_ucast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_mcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_bcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_error_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bytes);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_ucast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_mcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_error_packets);

            /* call API */
            err = bcmolt_stat_get((bcmolt_oltid)device_id, &nni_stats.hdr, clear_on_read);
            if (err == BCM_ERR_OK)
            {
                //std::cout << "Interface statistics retrieved"
                //          << " intf_id:" << intf_id << std::endl;
            
                port_stats->set_rx_bytes(nni_stats.data.rx_bytes);
                port_stats->set_rx_packets(nni_stats.data.rx_packets);
                port_stats->set_rx_ucast_packets(nni_stats.data.rx_ucast_packets);
                port_stats->set_rx_mcast_packets(nni_stats.data.rx_mcast_packets);
                port_stats->set_rx_bcast_packets(nni_stats.data.rx_bcast_packets);
                port_stats->set_rx_error_packets(nni_stats.data.rx_error_packets);
                port_stats->set_tx_bytes(nni_stats.data.tx_bytes);
                port_stats->set_tx_packets(nni_stats.data.tx_packets);
                port_stats->set_tx_ucast_packets(nni_stats.data.tx_ucast_packets);
                port_stats->set_tx_mcast_packets(nni_stats.data.tx_mcast_packets);
                port_stats->set_tx_bcast_packets(nni_stats.data.tx_bcast_packets);
                port_stats->set_tx_error_packets(nni_stats.data.tx_error_packets);
            
            } else {
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type);
            }
            break;
        }
        case BCMOLT_INTERFACE_TYPE_PON:
        {
            bcmolt_pon_interface_key key;
            key.pon_ni = (bcmolt_interface)intf_ref.intf_id;
            BCMOLT_STAT_INIT(&itu_pon_stats, pon_interface, itu_pon_stats, key);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_packets);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, bip_errors);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_crc_error);

            /* call API */
            err = bcmolt_stat_get((bcmolt_oltid)device_id, &itu_pon_stats.hdr, clear_on_read);
            if (err == BCM_ERR_OK) {
                port_stats->set_tx_packets(itu_pon_stats.data.tx_packets);
                port_stats->set_bip_errors(itu_pon_stats.data.bip_errors);
                port_stats->set_rx_crc_errors(itu_pon_stats.data.rx_crc_error);
            } else {
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err %d\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type, err);
            }
            {
                bcmolt_onu_key key;
                key.pon_ni = (bcmolt_interface)intf_ref.intf_id;
                BCMOLT_STAT_INIT(&pon_stats, onu, itu_pon_stats, key);
                BCMOLT_MSG_FIELD_GET(&pon_stats, rx_bytes);
                BCMOLT_MSG_FIELD_GET(&pon_stats, rx_packets);
                BCMOLT_MSG_FIELD_GET(&pon_stats, tx_bytes);

                /* call API */
                err = bcmolt_stat_get((bcmolt_oltid)device_id, &pon_stats.hdr, clear_on_read);
                if (err == BCM_ERR_OK) {
                    port_stats->set_rx_bytes(pon_stats.data.rx_bytes);
                    port_stats->set_rx_packets(pon_stats.data.rx_packets);
                    port_stats->set_tx_bytes(pon_stats.data.tx_bytes);
                } else {
                    OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err %d\n",
                        (int)intf_ref.intf_id, (int)intf_ref.intf_type, err);
                }
            }
            break;
        }
    } 

    port_stats->set_intf_id(interface_key_to_port_no((bcmolt_interface_id)intf_ref.intf_id, (bcmolt_interface_type)intf_ref.intf_type));
    time_t now;
    time(&now);
    port_stats->set_timestamp((int)now);

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


void stats_collection() {

    if (!state.is_connected()) {
        OPENOLT_LOG(INFO, openolt_log_id, "Voltha is not connected, do not collect stats\n");
        return;
    }
    if (!state.is_activated()) {
        OPENOLT_LOG(INFO, openolt_log_id, "The OLT is not up, do not collect stats\n");
        return;
    }


    OPENOLT_LOG(DEBUG, openolt_log_id, "Collecting statistics\n");

    //Ports statistics

    //Uplink ports
    for (int i = 0; i < NumNniIf_(); i++) {
        bcmolt_intf_ref intf_ref;
        intf_ref.intf_type = BCMOLT_INTERFACE_TYPE_NNI;
        intf_ref.intf_id = i;

        openolt::PortStatistics* port_stats =
            collectPortStatistics(intf_ref);

        openolt::Indication ind;
        ind.set_allocated_port_stats(port_stats);
        oltIndQ.push(ind);
    }
    //Pon ports
    for (int i = 0; i < NumPonIf_(); i++) {
        bcmolt_intf_ref intf_ref;
        intf_ref.intf_type = BCMOLT_INTERFACE_TYPE_PON;
        intf_ref.intf_id = i;

        openolt::PortStatistics* port_stats = 
            collectPortStatistics(intf_ref);

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
// void register_new_flow(bcmbal_flow_key key) {
//     for (int i = 0; i < FLOWS_COUNT; i++) {
//         if (flows_keys[i].flow_id == 0) {
//             flows_keys[i] = key;
//             break;
//         }
//     }
// }

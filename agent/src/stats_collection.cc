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

#include "stats_collection.h"

#include <unistd.h>

#include "indications.h"
#include "core.h"
#include "core_data.h"
#include "translation.h"

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
#include <bcmolt_api_model_api_structs.h>
}

//FIXME
#define FLOWS_COUNT 100
#define ALLOC_STATS_GET_INTERVAL 10

bcmolt_flow_key* flows_keys = new bcmolt_flow_key[FLOWS_COUNT];
bcmolt_odid device_id = 0;

void init_stats() {
    memset(flows_keys, 0, FLOWS_COUNT * sizeof(bcmolt_flow_key));
}

common::PortStatistics* get_default_port_statistics() {
    common::PortStatistics* port_stats = new common::PortStatistics;
    port_stats->set_intf_id(-1);
    port_stats->set_rx_bytes(-1);
    port_stats->set_rx_packets(-1);
    port_stats->set_rx_ucast_packets(-1);
    port_stats->set_rx_mcast_packets(-1);
    port_stats->set_rx_bcast_packets(-1);
    port_stats->set_rx_error_packets(-1);
    port_stats->set_rx_crc_errors(-1);
    port_stats->set_rx_frames(-1);
    port_stats->set_rx_frames_64(-1);
    port_stats->set_rx_frames_65_127(-1);
    port_stats->set_rx_frames_128_255(-1);
    port_stats->set_rx_frames_256_511(-1);
    port_stats->set_rx_frames_512_1023(-1);
    port_stats->set_rx_frames_1024_1518(-1);
    port_stats->set_rx_frames_1519_2047(-1);
    port_stats->set_rx_frames_2048_4095(-1);
    port_stats->set_rx_frames_4096_9216(-1);
    port_stats->set_rx_frames_9217_16383(-1);
    port_stats->set_tx_bytes(-1);
    port_stats->set_tx_packets(-1);
    port_stats->set_tx_ucast_packets(-1);
    port_stats->set_tx_mcast_packets(-1);
    port_stats->set_tx_bcast_packets(-1);
    port_stats->set_tx_error_packets(-1);
    port_stats->set_tx_frames(-1);
    port_stats->set_tx_frames_64(-1);
    port_stats->set_tx_frames_65_127(-1);
    port_stats->set_tx_frames_128_255(-1);
    port_stats->set_tx_frames_256_511(-1);
    port_stats->set_tx_frames_512_1023(-1);
    port_stats->set_tx_frames_1024_1518(-1);
    port_stats->set_tx_frames_1519_2047(-1);
    port_stats->set_tx_frames_2048_4095(-1);
    port_stats->set_tx_frames_4096_9216(-1);
    port_stats->set_tx_frames_9217_16383(-1);
    port_stats->set_bip_errors(-1);
    port_stats->set_bip_units(-1);
    port_stats->set_rxgem(-1);
    port_stats->set_rxgemdropped(-1);
    port_stats->set_rxgemidle(-1);
    port_stats->set_rxgemcorrected(-1);
    port_stats->set_rxfragmenterror(-1);
    port_stats->set_rxpacketsdropped(-1);
    port_stats->set_rxcpuomcipacketsdropped(-1);
    port_stats->set_rxcpu(-1);
    port_stats->set_rxomci(-1);
    port_stats->set_rxomcipacketscrcerror(-1);
    port_stats->set_rxgemillegal(-1);
    port_stats->set_txgem(-1);
    port_stats->set_txcpu(-1);
    port_stats->set_txomci(-1);
    port_stats->set_txdroppedillegallength(-1);
    port_stats->set_txdroppedtpidmiss(-1);
    port_stats->set_txdroppedvidmiss(-1);
    port_stats->set_txdroppedtotal(-1);
    port_stats->set_rxfcserrorpackets(-1);
    port_stats->set_rxundersizepackets(-1);
    port_stats->set_rxoversizepackets(-1);
    port_stats->set_txundersizepackets(-1);
    port_stats->set_txoversizepackets(-1);

    return port_stats;
}

openolt::OnuStatistics get_default_onu_statistics() {
    openolt::OnuStatistics onu_stats;

    onu_stats.set_positive_drift(-1);
    onu_stats.set_negative_drift(-1);
    onu_stats.set_delimiter_miss_detection(-1);
    onu_stats.set_bip_errors(-1);
    onu_stats.set_bip_units(-1);
    onu_stats.set_fec_corrected_symbols(-1);
    onu_stats.set_fec_codewords_corrected(-1);
    onu_stats.set_fec_codewords_uncorrectable(-1);
    onu_stats.set_fec_codewords(-1);
    onu_stats.set_fec_corrected_units(-1);
    onu_stats.set_xgem_key_errors(-1);
    onu_stats.set_xgem_loss(-1);
    onu_stats.set_rx_ploams_error(-1);
    onu_stats.set_rx_ploams_non_idle(-1);
    onu_stats.set_rx_omci(-1);
    onu_stats.set_rx_omci_packets_crc_error(-1);
    onu_stats.set_rx_bytes(-1);
    onu_stats.set_rx_packets(-1);
    onu_stats.set_tx_bytes(-1);
    onu_stats.set_tx_packets(-1);
    onu_stats.set_ber_reported(-1);
    onu_stats.set_lcdg_errors(-1);
    onu_stats.set_rdi_errors(-1);

    return onu_stats;
}

openolt::GemPortStatistics get_default_gemport_statistics() {
    openolt::GemPortStatistics gemport_stats;

    gemport_stats.set_intf_id(-1);
    gemport_stats.set_gemport_id(-1);
    gemport_stats.set_rx_packets(-1);
    gemport_stats.set_rx_bytes(-1);
    gemport_stats.set_tx_packets(-1);
    gemport_stats.set_tx_bytes(-1);

    return gemport_stats;
}

openolt::OnuAllocIdStatistics get_default_alloc_statistics() {
    openolt::OnuAllocIdStatistics alloc_stats;

    alloc_stats.set_intfid(-1);
    alloc_stats.set_allocid(-1);
    alloc_stats.set_rxbytes(-1);

    return alloc_stats;
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

common::PortStatistics* collectPortStatistics(bcmolt_intf_ref intf_ref) {

    common::PortStatistics* port_stats = get_default_port_statistics();
#ifndef TEST_MODE
    bcmos_errno err;
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
    bcmolt_nni_interface_stats nni_stats;
    bcmolt_onu_itu_pon_stats pon_stats;
    bcmolt_pon_interface_itu_pon_stats itu_pon_stats;
    bcmolt_internal_nni_enet_stats enet_stat;

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
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_64);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_65_127);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_128_255);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_256_511);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_512_1023);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_1024_1518);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_1519_2047);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_2048_4095);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_4096_9216);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_9217_16383);

            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bytes);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_ucast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_mcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_error_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_64);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_65_127);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_128_255);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_256_511);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_512_1023);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_1024_1518);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_1519_2047);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_2048_4095);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_4096_9216);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_9217_16383);

            /* call API */
            err = bcmolt_stat_get((bcmolt_oltid)device_id, &nni_stats.hdr, clear_on_read);
            if (err == BCM_ERR_OK)
            {
                //std::cout << "Interface statistics retrieved"
                //          << " intf_id:" << intf_id << std::endl;
                port_stats->set_rx_bytes(nni_stats.data.rx_bytes);
                port_stats->set_rx_frames(nni_stats.data.rx_packets);
                port_stats->set_rx_ucast_frames(nni_stats.data.rx_ucast_packets);
                port_stats->set_rx_mcast_frames(nni_stats.data.rx_mcast_packets);
                port_stats->set_rx_bcast_frames(nni_stats.data.rx_bcast_packets);
                port_stats->set_rx_error_frames(nni_stats.data.rx_error_packets);
                port_stats->set_tx_bytes(nni_stats.data.tx_bytes);
                port_stats->set_tx_frames(nni_stats.data.tx_packets);
                port_stats->set_tx_ucast_frames(nni_stats.data.tx_ucast_packets);
                port_stats->set_tx_mcast_frames(nni_stats.data.tx_mcast_packets);
                port_stats->set_tx_bcast_frames(nni_stats.data.tx_bcast_packets);
                port_stats->set_tx_error_frames(nni_stats.data.tx_error_packets);

            } else {
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
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
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
            }
#if 1 // Shall be fixed as part of VOL-3691. When fixed, the #else code block should be enabled.
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
                    OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                        (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
                }
            }
#else

            {
                bcmolt_internal_nni_key key = {};
                key.pon_ni = (bcmolt_interface)intf_ref.intf_id;
                BCMOLT_STAT_INIT(&enet_stat, internal_nni, enet_stats, key);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_bytes);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_64);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_65_127);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_128_255);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_256_511);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_512_1023);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_1024_1518);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_1519_2047);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_2048_4095);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_4096_9216);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, rx_frames_9217_16383);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_bytes);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_64);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_65_127);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_128_255);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_256_511);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_512_1023);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_1024_1518);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_1519_2047);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_2048_4095);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_4096_9216);
                BCMOLT_FIELD_SET_PRESENT(&enet_stat.data, internal_nni_enet_stats_data, tx_frames_9217_16383);

                /* call API */
                err = bcmolt_stat_get((bcmolt_oltid)device_id, &enet_stat.hdr, clear_on_read);
                if (err == BCM_ERR_OK) {
                    port_stats->set_rx_bytes(enet_stat.data.rx_bytes);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_64);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_65_127);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_128_255);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_256_511);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_512_1023);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_1024_1518);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_1519_2047);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_2048_4095);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_4096_9216);
                    port_stats->set_rx_packets(enet_stat.data.rx_frames_9217_16383);

                    port_stats->set_tx_bytes(enet_stat.data.tx_bytes);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_64);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_65_127);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_128_255);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_256_511);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_512_1023);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_1024_1518);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_1519_2047);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_2048_4095);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_4096_9216);
                    port_stats->set_rx_packets(enet_stat.data.tx_frames_9217_16383);
                } else {
                    OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                        (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
                }
            }
#endif
            break;
        }
    }

    port_stats->set_intf_id(interface_key_to_port_no((bcmolt_interface_id)intf_ref.intf_id, (bcmolt_interface_type)intf_ref.intf_type));
    time_t now;
    time(&now);
    port_stats->set_timestamp((int)now);
#endif
    return port_stats;

}

bcmos_errno get_port_statistics(bcmolt_intf_ref intf_ref, common::PortStatistics* port_stats) {
    bcmos_errno err = BCM_ERR_OK;
    common::PortStatistics* port_stats_temp = get_default_port_statistics();
    memcpy(port_stats, port_stats_temp, sizeof(common::PortStatistics));
    delete port_stats_temp;
    
#ifndef TEST_MODE
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
    bcmolt_nni_interface_stats nni_stats;
    bcmolt_pon_interface_itu_pon_stats itu_pon_stats;

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
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_fcs_error_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_undersize_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_oversize_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_64);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_65_127);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_128_255);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_256_511);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_512_1023);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_1024_1518);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_1519_2047);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_2048_4095);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_4096_9216);
            BCMOLT_MSG_FIELD_GET(&nni_stats, rx_frames_9217_16383);

            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bytes);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_ucast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_mcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_bcast_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_error_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_undersize_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_oversize_packets);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_64);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_65_127);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_128_255);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_256_511);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_512_1023);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_1024_1518);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_1519_2047);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_2048_4095);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_4096_9216);
            BCMOLT_MSG_FIELD_GET(&nni_stats, tx_frames_9217_16383);

            /* call API */
            err = bcmolt_stat_get((bcmolt_oltid)device_id, &nni_stats.hdr, clear_on_read);
            if (err == BCM_ERR_OK)
            {
                port_stats->set_rx_bytes(nni_stats.data.rx_bytes);
                port_stats->set_rx_frames(nni_stats.data.rx_packets);
                port_stats->set_rx_ucast_frames(nni_stats.data.rx_ucast_packets);
                port_stats->set_rx_mcast_frames(nni_stats.data.rx_mcast_packets);
                port_stats->set_rx_bcast_frames(nni_stats.data.rx_bcast_packets);
                port_stats->set_rx_error_frames(nni_stats.data.rx_error_packets);
                port_stats->set_rxfcserrorpackets(nni_stats.data.rx_fcs_error_packets);
                port_stats->set_rxundersizepackets(nni_stats.data.rx_undersize_packets);
                port_stats->set_rxoversizepackets(nni_stats.data.rx_oversize_packets);
                port_stats->set_tx_bytes(nni_stats.data.tx_bytes);
                port_stats->set_tx_frames(nni_stats.data.tx_packets);
                port_stats->set_tx_ucast_frames(nni_stats.data.tx_ucast_packets);
                port_stats->set_tx_mcast_frames(nni_stats.data.tx_mcast_packets);
                port_stats->set_tx_bcast_frames(nni_stats.data.tx_bcast_packets);
                port_stats->set_tx_error_frames(nni_stats.data.tx_error_packets);
                port_stats->set_txundersizepackets(nni_stats.data.tx_undersize_packets);
                port_stats->set_txoversizepackets(nni_stats.data.tx_oversize_packets);

            } else {
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
                return err;
            }
            break;
        }
        case BCMOLT_INTERFACE_TYPE_PON:
        {
            bcmolt_pon_interface_key key;
            key.pon_ni = (bcmolt_interface)intf_ref.intf_id;
            BCMOLT_STAT_INIT(&itu_pon_stats, pon_interface, itu_pon_stats, key);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, bip_units);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, bip_errors);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_packets);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_gem);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_gem_dropped);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_gem_idle);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_gem_corrected);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_crc_error);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_fragment_error);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_packets_dropped);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_cpu_omci_packets_dropped);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_cpu);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_omci);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_omci_packets_crc_error);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, rx_gem_illegal);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_packets);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_gem);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_cpu);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_omci);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_dropped_illegal_length);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_dropped_tpid_miss);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_dropped_vid_miss);
            BCMOLT_MSG_FIELD_GET(&itu_pon_stats, tx_dropped_total);
            
            /* call API */
            err = bcmolt_stat_get((bcmolt_oltid)device_id, &itu_pon_stats.hdr, clear_on_read);
            if (err == BCM_ERR_OK) {
                port_stats->set_bip_units(itu_pon_stats.data.bip_units);
                port_stats->set_bip_errors(itu_pon_stats.data.bip_errors);
                port_stats->set_rx_packets(itu_pon_stats.data.rx_packets);
                port_stats->set_rxgem(itu_pon_stats.data.rx_gem);
                port_stats->set_rxgemdropped(itu_pon_stats.data.rx_gem_dropped);
                port_stats->set_rxgemidle(itu_pon_stats.data.rx_gem_idle);
                port_stats->set_rxgemcorrected(itu_pon_stats.data.rx_gem_corrected);
                port_stats->set_rx_crc_errors(itu_pon_stats.data.rx_crc_error);
                port_stats->set_rxfragmenterror(itu_pon_stats.data.rx_fragment_error);
                port_stats->set_rxpacketsdropped(itu_pon_stats.data.rx_packets_dropped);
                port_stats->set_rxcpuomcipacketsdropped(itu_pon_stats.data.rx_cpu_omci_packets_dropped);
                port_stats->set_rxcpu(itu_pon_stats.data.rx_cpu);
                port_stats->set_rxomci(itu_pon_stats.data.rx_omci);
                port_stats->set_rxomcipacketscrcerror(itu_pon_stats.data.rx_omci_packets_crc_error);
                port_stats->set_rxgemillegal(itu_pon_stats.data.rx_gem_illegal);
                port_stats->set_tx_packets(itu_pon_stats.data.tx_packets);
                port_stats->set_txgem(itu_pon_stats.data.tx_gem);
                port_stats->set_txcpu(itu_pon_stats.data.tx_cpu);
                port_stats->set_txomci(itu_pon_stats.data.tx_omci);
                port_stats->set_txdroppedillegallength(itu_pon_stats.data.tx_dropped_illegal_length);
                port_stats->set_txdroppedtpidmiss(itu_pon_stats.data.tx_dropped_tpid_miss);
                port_stats->set_txdroppedvidmiss(itu_pon_stats.data.tx_dropped_vid_miss);
                port_stats->set_txdroppedtotal(itu_pon_stats.data.tx_dropped_total);
            } else {
                OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve port statistics, intf_id %d, intf_type %d, err = %s\n",
                    (int)intf_ref.intf_id, (int)intf_ref.intf_type, bcmos_strerror(err));
                return err;
            }
            break;
        }
    }

    port_stats->set_intf_id(interface_key_to_port_no((bcmolt_interface_id)intf_ref.intf_id, (bcmolt_interface_type)intf_ref.intf_type));
    time_t now;
    time(&now);
    port_stats->set_timestamp((int)now);
#endif
    return err;
}

bcmos_errno get_onu_statistics(bcmolt_interface_id intf_id, bcmolt_onu_id onu_id, openolt::OnuStatistics* onu_stats) {
    bcmos_errno err = BCM_ERR_OK;

#ifndef TEST_MODE
    *onu_stats = get_default_onu_statistics();
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
    bcmolt_onu_itu_pon_stats itu_onu_stats;

    {
        bcmolt_onu_key key;
        key.pon_ni = intf_id;
        key.onu_id = onu_id;
        BCMOLT_STAT_INIT(&itu_onu_stats, onu, itu_pon_stats, key);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, positive_drift);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, negative_drift);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, delimiter_miss_detection);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, bip_errors);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, bip_units);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, fec_corrected_symbols);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, fec_codewords_corrected);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, fec_codewords_uncorrectable);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, fec_codewords);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, fec_corrected_units);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, xgem_key_errors);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, xgem_loss);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_ploams_error);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_ploams_non_idle);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_omci);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_omci_packets_crc_error);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_bytes);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rx_packets);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, tx_bytes);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, tx_packets);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, ber_reported);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, lcdg_errors);
        BCMOLT_MSG_FIELD_GET(&itu_onu_stats, rdi_errors);

        /* call API */
        err = bcmolt_stat_get((bcmolt_oltid)device_id, &itu_onu_stats.hdr, clear_on_read);
        if (err == BCM_ERR_OK) {
            onu_stats->set_positive_drift(itu_onu_stats.data.positive_drift);
            onu_stats->set_negative_drift(itu_onu_stats.data.negative_drift);
            onu_stats->set_delimiter_miss_detection(itu_onu_stats.data.delimiter_miss_detection);
            onu_stats->set_bip_errors(itu_onu_stats.data.bip_errors);
            onu_stats->set_bip_units(itu_onu_stats.data.bip_units);
            onu_stats->set_fec_corrected_symbols(itu_onu_stats.data.fec_corrected_symbols);
            onu_stats->set_fec_codewords_corrected(itu_onu_stats.data.fec_codewords_corrected);
            onu_stats->set_fec_codewords_uncorrectable(itu_onu_stats.data.fec_codewords_uncorrectable);
            onu_stats->set_fec_codewords(itu_onu_stats.data.fec_codewords);
            onu_stats->set_fec_corrected_units(itu_onu_stats.data.fec_corrected_units);
            onu_stats->set_xgem_key_errors(itu_onu_stats.data.xgem_key_errors);
            onu_stats->set_xgem_loss(itu_onu_stats.data.xgem_loss);
            onu_stats->set_rx_ploams_error(itu_onu_stats.data.rx_ploams_error);
            onu_stats->set_rx_ploams_non_idle(itu_onu_stats.data.rx_ploams_non_idle);
            onu_stats->set_rx_omci(itu_onu_stats.data.rx_omci);
            onu_stats->set_rx_omci_packets_crc_error(itu_onu_stats.data.rx_omci_packets_crc_error);
            onu_stats->set_rx_bytes(itu_onu_stats.data.rx_bytes);
            onu_stats->set_rx_packets(itu_onu_stats.data.rx_packets);
            onu_stats->set_tx_bytes(itu_onu_stats.data.tx_bytes);
            onu_stats->set_tx_packets(itu_onu_stats.data.tx_packets);
            onu_stats->set_ber_reported(itu_onu_stats.data.ber_reported);
            onu_stats->set_lcdg_errors(itu_onu_stats.data.lcdg_errors);
            onu_stats->set_rdi_errors(itu_onu_stats.data.rdi_errors);
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve ONU statistics, intf_id %d, onu_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)onu_id, err, bcmos_strerror(err));
            return err;
        }
    }

    onu_stats->set_intf_id(intf_id);
    onu_stats->set_onu_id(onu_id);
    time_t now;
    time(&now);
    onu_stats->set_timestamp((int)now);
#endif

    return err;
}

bcmos_errno get_gemport_statistics(bcmolt_interface_id intf_id, bcmolt_gem_port_id gemport_id, openolt::GemPortStatistics* gemport_stats) {
    bcmos_errno err = BCM_ERR_OK;

#ifndef TEST_MODE
    *gemport_stats = get_default_gemport_statistics();
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
    bcmolt_itupon_gem_stats gem_stats;

    {
        bcmolt_itupon_gem_key key;
        key.pon_ni = intf_id;
        key.gem_port_id = gemport_id;

        BCMOLT_STAT_INIT(&gem_stats, itupon_gem, stats, key);
        BCMOLT_MSG_FIELD_GET(&gem_stats, rx_packets);
        BCMOLT_MSG_FIELD_GET(&gem_stats, rx_bytes);
        BCMOLT_MSG_FIELD_GET(&gem_stats, tx_packets);
        BCMOLT_MSG_FIELD_GET(&gem_stats, tx_bytes);

        /* call API */
        err = bcmolt_stat_get((bcmolt_oltid)device_id, &gem_stats.hdr, clear_on_read);
        if (err == BCM_ERR_OK) {
            gemport_stats->set_rx_packets(gem_stats.data.rx_packets);
            gemport_stats->set_rx_bytes(gem_stats.data.rx_bytes);
            gemport_stats->set_tx_packets(gem_stats.data.tx_packets);
            gemport_stats->set_tx_bytes(gem_stats.data.tx_bytes);
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve GEMPORT statistics, intf_id %d, gemport_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)gemport_id, err, bcmos_strerror(err));
            return err;
        }
    }

    gemport_stats->set_intf_id(intf_id);
    gemport_stats->set_gemport_id(gemport_id);
    time_t now;
    time(&now);
    gemport_stats->set_timestamp((int)now);
#endif

    return err;
}

bcmos_errno set_collect_alloc_stats(bcmolt_interface_id intf_id, bcmolt_alloc_id alloc_id, bcmos_bool collect_stats) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_itupon_alloc_cfg cfg;
    bcmolt_itupon_alloc_key key;
    key.pon_ni = intf_id;
    key.alloc_id = alloc_id;

    BCMOLT_CFG_INIT(&cfg, itupon_alloc, key);
    BCMOLT_FIELD_SET(&cfg.data, itupon_alloc_cfg_data, collect_stats, collect_stats);
    err = bcmolt_cfg_set((bcmolt_oltid)device_id, &cfg.hdr);
    return err;
}

bcmos_errno get_alloc_statistics(bcmolt_interface_id intf_id, bcmolt_alloc_id alloc_id, openolt::OnuAllocIdStatistics* allocid_stats) {
    bcmos_errno err = BCM_ERR_OK;

#ifndef TEST_MODE

    bcmos_errno err1 = BCM_ERR_OK;
    err1 = set_collect_alloc_stats(intf_id, alloc_id, BCMOS_TRUE);
    if (err1 == BCM_ERR_OK) {
        *allocid_stats = get_default_alloc_statistics();
        bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE;
        bcmolt_itupon_alloc_stats alloc_stats;
        bcmolt_itupon_alloc_key key;
        key.pon_ni = intf_id;
        key.alloc_id = alloc_id;

        // Wait to collect alloc stats after enabling it
        sleep(ALLOC_STATS_GET_INTERVAL);

        BCMOLT_STAT_INIT(&alloc_stats, itupon_alloc, stats, key);
        BCMOLT_MSG_FIELD_GET(&alloc_stats, rx_bytes);

        /* call API */
        err = bcmolt_stat_get((bcmolt_oltid)device_id, &alloc_stats.hdr, clear_on_read);
        if (err == BCM_ERR_OK) {
            allocid_stats->set_rxbytes(alloc_stats.data.rx_bytes);
            allocid_stats->set_intfid(intf_id);
            allocid_stats->set_allocid(alloc_id);
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to retrieve ALLOC_ID statistics, intf_id %d, alloc_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)alloc_id, err, bcmos_strerror(err));
            err1 = set_collect_alloc_stats(intf_id, alloc_id, BCMOS_FALSE);
            if (err1 != BCM_ERR_OK) {
            OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to disable collect_stats for ALLOC_ID, intf_id %d, alloc_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)alloc_id, err, bcmos_strerror(err));
            }
            return err;
        }
    } else {
        OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to enable collect_stats for ALLOC_ID, intf_id %d, alloc_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)alloc_id, err, bcmos_strerror(err));
        return err1;
    }
    
    err1 = set_collect_alloc_stats(intf_id, alloc_id, BCMOS_FALSE);
    if (err1 != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id,  "Failed to disable collect_stats for ALLOC_ID, intf_id %d, alloc_id %d, err no: %d - %s\n",
                        (int)intf_id, (int)alloc_id, err, bcmos_strerror(err));
        return err1;
    }

#endif

    return err;
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

        common::PortStatistics* port_stats =
            collectPortStatistics(intf_ref);

        ::openolt::Indication ind;
        ind.set_allocated_port_stats(port_stats);
        oltIndQ.push(ind);
    }
    //Pon ports
    for (int i = 0; i < NumPonIf_(); i++) {
        bcmolt_intf_ref intf_ref;
        intf_ref.intf_type = BCMOLT_INTERFACE_TYPE_PON;
        intf_ref.intf_id = i;

        common::PortStatistics* port_stats =
            collectPortStatistics(intf_ref);

        ::openolt::Indication ind;
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

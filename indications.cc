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

#include "indications.h"
#include "core.h"
#include "core_data.h"
#include "core_utils.h"
#include "stats_collection.h"
#include "translation.h"
#include "state.h"
#include "trx_eeprom_reader.h"

#include <string>

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
#include <bcmolt_host_api.h>
#include <bcmolt_api_model_api_structs.h>
}

using grpc::Status;

bool subscribed = false;
#define current_device 0

static void OmciIndication(bcmolt_devid olt, bcmolt_msg *msg);

#define INTERFACE_STATE_IF_DOWN(state) \
       ((state == BCMOLT_INTERFACE_STATE_INACTIVE || \
         state == BCMOLT_INTERFACE_STATE_PROCESSING || \
         state == BCMOLT_INTERFACE_STATE_ACTIVE_STANDBY) ? BCMOS_TRUE : BCMOS_FALSE)
#define INTERFACE_STATE_IF_UP(state) \
       ((state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING) ? BCMOS_TRUE : BCMOS_FALSE)
#define ONU_STATE_IF_DOWN(state) \
       ((state == BCMOLT_ONU_OPERATION_INACTIVE || \
         state == BCMOLT_ONU_OPERATION_DISABLE || \
         state == BCMOLT_ONU_OPERATION_ACTIVE_STANDBY) ? BCMOS_TRUE : BCMOS_FALSE)
#define ONU_STATE_IF_UP(state) \
       ((state == BCMOLT_ONU_OPERATION_ACTIVE) ? BCMOS_TRUE : BCMOS_FALSE)
#define ONU_ACTIVATION_COMPLETED_SUCCESS(state) \
       ((state == BCMOLT_RESULT_SUCCESS) ? BCMOS_TRUE : BCMOS_FALSE)
#define ONU_ACTIVATION_COMPLETED_FAIL(state) \
       ((state != BCMOLT_RESULT_SUCCESS) ? BCMOS_TRUE : BCMOS_FALSE)
#define SET_OPER_STATE(indication,state) \
       (INTERFACE_STATE_IF_UP(state)) ? indication->set_oper_state("up") : \
       indication->set_oper_state("down")
#define GET_FLOW_TYPE(type) \
       (type == BCMOLT_FLOW_TYPE_UPSTREAM) ? "upstream" : \
       (type == BCMOLT_FLOW_TYPE_DOWNSTREAM) ? "downstream" : \
       (type == BCMOLT_FLOW_TYPE_MULTICAST) ? "multicast" : "unknown"

std::string bcmolt_to_grpc_intf_type(bcmolt_interface_type intf_type)
{
    if (intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
        return "nni";
    } else if (intf_type == BCMOLT_INTERFACE_TYPE_PON) {
        return "pon";
    } else if (intf_type == BCMOLT_INTERFACE_TYPE_HOST) {
        return "host";
    }
    return "unknown";
}

std::string bcmolt_to_grpc_interface_rf__intf_type(bcmolt_interface_type intf_type)
{
    if (intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
        return "nni";
    } else if (intf_type == BCMOLT_INTERFACE_TYPE_PON) {
        return "pon";
    } else if (intf_type == BCMOLT_INTERFACE_TYPE_HOST) {
        return "host";
    }
    return "unknown";
}

inline uint64_t get_pon_stats_alarms_data(bcmolt_interface pon_ni, bcmolt_onu_id onu_id, bcmolt_onu_itu_pon_stats_data_id stat) {
    bcmos_errno err;
    bcmolt_onu_itu_pon_stats itu_pon_stat; /* declare main API struct */
    bcmolt_onu_key key = {}; /* declare key */
    bcmolt_stat_flags clear_on_read = BCMOLT_STAT_FLAGS_NONE; /* declare 'clear on read' flag */

    key.pon_ni = pon_ni;
    key.onu_id = onu_id;

    /* Initialize the API struct. */
    BCMOLT_STAT_INIT(&itu_pon_stat, onu, itu_pon_stats, key);
    switch (stat) {
        case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_RDI_ERRORS:
            BCMOLT_FIELD_SET_PRESENT(&itu_pon_stat.data, onu_itu_pon_stats_data, rdi_errors);
            err = bcmolt_stat_get(dev_id, &itu_pon_stat.hdr, clear_on_read ? BCMOLT_STAT_FLAGS_CLEAR_ON_READ : BCMOLT_STAT_FLAGS_NONE);
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get rdi_errors, err = %s\n", bcmos_strerror(err));
                return err;
            }
            return itu_pon_stat.data.rdi_errors;
        /* It is a further requirement
        case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_BIP_ERRORS:
            BCMOLT_FIELD_SET_PRESENT(&itu_pon_stat.data, onu_itu_pon_stats_data, bip_errors);
            err = bcmolt_stat_get(dev_id, &itu_pon_stat.hdr, clear_on_read ? BCMOLT_STAT_FLAGS_CLEAR_ON_READ : BCMOLT_STAT_FLAGS_NONE);
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get bip_errors, err = %s\n", bcmos_strerror(err));
                return err;
            }
            return itu_pon_stat.data.bip_errors;
        */
        default:
            return BCM_ERR_INTERNAL;
    }

    return err;
}

/*std::string getOnuRegistrationId(uint32_t intf_id, uint32_t onu_id){
    bcmbal_subscriber_terminal_key key;
    bcmbal_subscriber_terminal_cfg sub_term_obj = {};
    uint8_t  *list_mem;// to fetch config details for ONU
    bcmos_errno err = BCM_ERR_OK;

    key.sub_term_id =onu_id;
    key.intf_id = intf_id;
    BCM_LOG(INFO, openolt_log_id,"Processing subscriber terminal cfg get for: %d",key.intf_id);

    BCMBAL_CFG_INIT(&sub_term_obj, subscriber_terminal, key);

    BCMBAL_CFG_PROP_GET(&sub_term_obj, subscriber_terminal, all_properties);
    char *reg_id = (char*)malloc(sizeof(char)*MAX_REGID_LENGTH);
    memset(reg_id, '\0', MAX_REGID_LENGTH);

    //set memory to use for variable-sized lists
    list_mem = (uint8_t*)malloc(BAL_DYNAMIC_LIST_BUFFER_SIZE);

    if (list_mem == NULL)
    {
       BCM_LOG(ERROR,openolt_log_id,"Memory allocation failed while handling subscriber terminal \
               cfg get subscriber_terminal_id(%d), Interface ID(%d)",key.sub_term_id, key.intf_id);
       return reg_id;
    }

    memset(list_mem, 0, BAL_DYNAMIC_LIST_BUFFER_SIZE);
    BCMBAL_CFG_LIST_BUF_SET(&sub_term_obj, subscriber_terminal, list_mem, BAL_DYNAMIC_LIST_BUFFER_SIZE);

    //call API
    err = bcmbal_cfg_get(DEFAULT_ATERM_ID, &sub_term_obj.hdr);

    if (err != BCM_ERR_OK)
    {
        BCM_LOG(ERROR,openolt_log_id, "Failed to get information from BAL subscriber_terminal_id(%d), Interface ID(%d)",
                key.sub_term_id, key.intf_id);
        free(list_mem);
        return reg_id;
    }

    BCM_LOG(INFO,openolt_log_id, "Get Subscriber cfg sent to OLT for Subscriber Id(%d) on Interface(%d)",
            key.sub_term_id, key.intf_id);

    for (int i=0; i<MAX_REGID_LENGTH ; i++){
        reg_id[i]=sub_term_obj.data.registration_id.arr[i];
    }

    free(list_mem);
    return reg_id;
}*/

static void OltOperIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OltIndication* olt_ind = new openolt::OltIndication;
    std::string admin_state;

    switch (msg->subgroup) {
        case BCMOLT_DEVICE_AUTO_SUBGROUP_CONNECTION_COMPLETE:
            admin_state = "up";
            olt_ind->set_oper_state("up");
            break;
        case BCMOLT_DEVICE_AUTO_SUBGROUP_DISCONNECTION_COMPLETE:
             admin_state = "down";
             olt_ind->set_oper_state("down");
            break;
        case BCMOLT_DEVICE_AUTO_SUBGROUP_CONNECTION_FAILURE:
             admin_state = "failure";
             olt_ind->set_oper_state("failure");
            break;
    }
    ind.set_allocated_olt_ind(olt_ind);

    if (msg->subgroup == BCMOLT_DEVICE_AUTO_SUBGROUP_CONNECTION_COMPLETE) {
        /* register for omci indication */
        {
            bcmolt_rx_cfg rx_cfg = {};
            rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
            rx_cfg.rx_cb = OmciIndication;
            rx_cfg.subgroup = bcmolt_onu_auto_subgroup_omci_packet;
            rx_cfg.module = BCMOS_MODULE_ID_OMCI_TRANSPORT;
            bcmolt_ind_subscribe(current_device, &rx_cfg);
        }
        state.activate();
    }
    else {
        state.deactivate();
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void LosIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::LosIndication* los_ind = new openolt::LosIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_PON_INTERFACE:
            switch (msg->subgroup) {
                 case BCMOLT_PON_INTERFACE_AUTO_SUBGROUP_LOS:
                 {
                     bcmolt_pon_interface_los* bcm_los_ind = (bcmolt_pon_interface_los *) msg;
                     int intf_id = interface_key_to_port_no(bcm_los_ind->key.pon_ni,
                         BCMOLT_INTERFACE_TYPE_PON);
                     std::string status = alarm_status_to_string(bcm_los_ind->data.status);

                     OPENOLT_LOG(INFO, openolt_log_id, "LOS indication : intf_id: %d port: %d status %s\n",
                             bcm_los_ind->key.pon_ni, intf_id, status.c_str());

                     los_ind->set_intf_id(intf_id);
                     los_ind->set_status(status);

                     alarm_ind->set_allocated_los_ind(los_ind);
                     ind.set_allocated_alarm_ind(alarm_ind);
                     break;
                 }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void IfIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::IntfIndication* intf_ind = new openolt::IntfIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_PON_INTERFACE:
            switch (msg->subgroup) {
                case BCMOLT_PON_INTERFACE_AUTO_SUBGROUP_STATE_CHANGE_COMPLETED:
                {
                    bcmolt_pon_interface_key *key =
                        &((bcmolt_pon_interface_state_change_completed*)msg)->key;
                    bcmolt_pon_interface_state_change_completed_data *data =
                        &((bcmolt_pon_interface_state_change_completed*)msg)->data;

                    intf_ind->set_intf_id(key->pon_ni);
                    SET_OPER_STATE(intf_ind, data->new_state);

                    OPENOLT_LOG(INFO, openolt_log_id, "intf indication, intf_type %s, intf_id %d, oper_state %s\n",
                        bcmolt_to_grpc_intf_type(BCMOLT_INTERFACE_TYPE_PON).c_str(), key->pon_ni, intf_ind->oper_state().c_str());

                    ind.set_allocated_intf_ind(intf_ind);
                    break;
                }
            }
            break;
        case BCMOLT_OBJ_ID_NNI_INTERFACE:
            switch (msg->subgroup) {
                case BCMOLT_NNI_INTERFACE_AUTO_SUBGROUP_STATE_CHANGE:
                {
                    OPENOLT_LOG(INFO, openolt_log_id, "intf indication, intf_id: %d\n",
                        ((bcmolt_nni_interface_state_change *)msg)->key.id);
                    bcmolt_nni_interface_key *key =
                        &((bcmolt_nni_interface_state_change *)msg)->key;
                    bcmolt_nni_interface_state_change_data *data =
                        &((bcmolt_nni_interface_state_change *)msg)->data;

                    intf_ind->set_intf_id(key->id);
                    SET_OPER_STATE(intf_ind, data->new_state);
                    ind.set_allocated_intf_ind(intf_ind);
                    break;
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void IfOperIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::IntfOperIndication* intf_oper_ind = new openolt::IntfOperIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_PON_INTERFACE:
            switch (msg->subgroup) {
                case BCMOLT_PON_INTERFACE_AUTO_SUBGROUP_STATE_CHANGE_COMPLETED:
                {
                    bcmolt_pon_interface_key *key = &((bcmolt_pon_interface_state_change_completed*)msg)->key;
                    bcmolt_pon_interface_state_change_completed_data *data = &((bcmolt_pon_interface_state_change_completed*)msg)->data;
                    intf_oper_ind->set_intf_id(key->pon_ni);
                    intf_oper_ind->set_type(bcmolt_to_grpc_intf_type(BCMOLT_INTERFACE_TYPE_PON));
                    SET_OPER_STATE(intf_oper_ind, data->new_state);
                    OPENOLT_LOG(INFO, openolt_log_id, "intf oper state indication, intf_type %s, intf_id %d, oper_state %s\n",
                        intf_oper_ind->type().c_str(), key->pon_ni, intf_oper_ind->oper_state().c_str());
                    ind.set_allocated_intf_oper_ind(intf_oper_ind);
                    break;
                }
            }
        case BCMOLT_OBJ_ID_NNI_INTERFACE:
            switch (msg->subgroup) {
                case BCMOLT_NNI_INTERFACE_AUTO_SUBGROUP_STATE_CHANGE:
                {
                    bcmolt_nni_interface_key *key = &((bcmolt_nni_interface_state_change *)msg)->key;
                    bcmolt_nni_interface_state_change_data *data = &((bcmolt_nni_interface_state_change *)msg)->data;
                    bcmolt_interface intf_id = key->id;
                    bcmolt_interface_type intf_type = BCMOLT_INTERFACE_TYPE_NNI;
                    intf_oper_ind->set_intf_id(key->id);
                    intf_oper_ind->set_type(bcmolt_to_grpc_intf_type(BCMOLT_INTERFACE_TYPE_NNI));
                    SET_OPER_STATE(intf_oper_ind, data->new_state);
                    OPENOLT_LOG(INFO, openolt_log_id, "intf oper state indication, intf_type %s, intf_id %d, oper_state %s\n",
                        intf_oper_ind->type().c_str(), key->id, intf_oper_ind->oper_state().c_str());
                    ind.set_allocated_intf_oper_ind(intf_oper_ind);
                    break;
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuAlarmIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuAlarmIndication* onu_alarm_ind = new openolt::OnuAlarmIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_XGPON_ALARM:
                {
                    bcmolt_onu_xgpon_alarm_data *data = &((bcmolt_onu_xgpon_alarm *)msg)->data;
                    bcmolt_onu_key *key = &((bcmolt_onu_xgpon_alarm *)msg)->key;

                    int port_no = interface_key_to_port_no(key->pon_ni, BCMOLT_INTERFACE_TYPE_PON);

                    OPENOLT_LOG(INFO, openolt_log_id, "onu alarm indication, pon_ni %d, onu_id %d, port_no %d, los_status %s, lob_status %s, lopc_miss_status %s, lopc_mic_error_status %s\n", key->pon_ni, key->onu_id, port_no, alarm_status_to_string(data->xgpon_onu_alarm.losi).c_str(), alarm_status_to_string(data->xgpon_onu_alarm.lobi).c_str(), alarm_status_to_string(data->xgpon_onu_alarm.lopci_miss).c_str(), alarm_status_to_string(data->xgpon_onu_alarm.lopci_mic_error).c_str());

                    onu_alarm_ind->set_los_status(alarm_status_to_string(data->xgpon_onu_alarm.losi));
                    onu_alarm_ind->set_lob_status(alarm_status_to_string(data->xgpon_onu_alarm.lobi));
                    onu_alarm_ind->set_lopc_miss_status(alarm_status_to_string(data->xgpon_onu_alarm.lopci_miss));
                    onu_alarm_ind->set_lopc_mic_error_status(alarm_status_to_string(data->xgpon_onu_alarm.lopci_mic_error));
                    onu_alarm_ind->set_intf_id(key->pon_ni);
                    onu_alarm_ind->set_onu_id(key->onu_id);
                    alarm_ind->set_allocated_onu_alarm_ind(onu_alarm_ind);
                    ind.set_allocated_alarm_ind(alarm_ind);
                    break;
                }
                case BCMOLT_ONU_AUTO_SUBGROUP_GPON_ALARM:
                {
                    bcmolt_onu_gpon_alarm_data *data = &((bcmolt_onu_gpon_alarm *)msg)->data;
                    bcmolt_onu_key *key = &((bcmolt_onu_gpon_alarm *)msg)->key;
                    onu_alarm_ind->set_los_status(alarm_status_to_string(data->gpon_onu_alarm.losi));
                    onu_alarm_ind->set_lofi_status(alarm_status_to_string(data->gpon_onu_alarm.lofi));
                    onu_alarm_ind->set_loami_status(alarm_status_to_string(data->gpon_onu_alarm.loami));
                    onu_alarm_ind->set_intf_id(key->pon_ni);
                    onu_alarm_ind->set_onu_id(key->onu_id);
                    alarm_ind->set_allocated_onu_alarm_ind(onu_alarm_ind);
                    ind.set_allocated_alarm_ind(alarm_ind);
                    break;
                 }
        }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuDyingGaspIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::DyingGaspIndication* dgi_ind = new openolt::DyingGaspIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_DGI:
                {
                    bcmolt_onu_dgi* dgi_data = (bcmolt_onu_dgi *)msg;
                    bcmolt_onu_key *key = &((bcmolt_onu_dgi *)msg)->key;

                    int port_no = interface_key_to_port_no(key->pon_ni, BCMOLT_INTERFACE_TYPE_PON);

                    OPENOLT_LOG(INFO, openolt_log_id, "onu dyinggasp indication, pon_ni %d, onu_id %d, port_no %d, status %s\n",
                        key->pon_ni, key->onu_id, port_no, alarm_status_to_string(dgi_data->data.alarm_status).c_str());

                    dgi_ind->set_status(alarm_status_to_string(dgi_data->data.alarm_status));
                    dgi_ind->set_intf_id(key->pon_ni);
                    dgi_ind->set_onu_id(key->onu_id);

                    alarm_ind->set_allocated_dying_gasp_ind(dgi_ind);
                    ind.set_allocated_alarm_ind(alarm_ind);
                    break;
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuDiscoveryIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    //Ignore the onu discovery when agent is not connected to VOLTHA
    if (!state.is_connected()) {
        bcmolt_msg_free(msg);
        return;
    }

    openolt::Indication ind;
    openolt::OnuDiscIndication* onu_disc_ind = new openolt::OnuDiscIndication;
    openolt::SerialNumber* serial_number = new openolt::SerialNumber;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_PON_INTERFACE:
            switch (msg->subgroup) {
                case BCMOLT_PON_INTERFACE_AUTO_SUBGROUP_ONU_DISCOVERED:
                {
                    bcmolt_pon_interface_key *key =
                        &((bcmolt_pon_interface_onu_discovered *)msg)->key;

                    bcmolt_pon_interface_onu_discovered_data *data =
                        &((bcmolt_pon_interface_onu_discovered *)msg)->data;

                    bcmolt_serial_number *in_serial_number = &(data->serial_number);

                    OPENOLT_LOG(INFO, openolt_log_id, "onu discover indication, pon_ni %d, serial_number %s\n",
                        key->pon_ni, serial_number_to_str(in_serial_number).c_str());

                    onu_disc_ind->set_intf_id(key->pon_ni);
                    serial_number->set_vendor_id(reinterpret_cast<const char *>(in_serial_number->vendor_id.arr), 4);
                    serial_number->set_vendor_specific(reinterpret_cast<const char *>(in_serial_number->vendor_specific.arr), 8);
                    onu_disc_ind->set_allocated_serial_number(serial_number);
                    ind.set_allocated_onu_disc_ind(onu_disc_ind);
                    break;
                }
        }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OmciIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OmciIndication* omci_ind = new openolt::OmciIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_OMCI_PACKET:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_omci_packet*)msg)->key;
                    bcmolt_onu_omci_packet_data *data = &((bcmolt_onu_omci_packet*)msg)->data;

                    OPENOLT_LOG(DEBUG, omci_log_id, "OMCI indication: pon_ni %d, onu_id %d\n",
                        key->pon_ni, key->onu_id);

                    omci_ind->set_intf_id(key->pon_ni);
                    omci_ind->set_onu_id(key->onu_id);
                    omci_ind->set_pkt(data->buffer.arr, data->buffer.len);

                    ind.set_allocated_omci_ind(omci_ind);
                    break;
                }
        }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void PacketIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ACCESS_CONTROL:
            switch (msg->subgroup) {
                case BCMOLT_ACCESS_CONTROL_AUTO_SUBGROUP_RECEIVE_ETH_PACKET:
                {
                    int32_t gemport_id;
                    bcmolt_access_control_receive_eth_packet *pkt =
                        (bcmolt_access_control_receive_eth_packet*)msg;
                    bcmolt_access_control_receive_eth_packet_data *pkt_data =
                        &((bcmolt_access_control_receive_eth_packet*)msg)->data;

                    // Set the gemport_id to be passed to is_packet_allowed function
                    gemport_id = pkt_data->svc_port_id == BCMOLT_SERVICE_PORT_ID_INVALID ? -1 : pkt_data->svc_port_id;

                    // Allow the packet to host only if "is_packet_allowed" routine returns true, else drop the packet.
                    if (! is_packet_allowed(pkt_data, gemport_id) ) {
                        OPENOLT_LOG(WARNING, openolt_log_id, "packet not allowed to host\n");
                        bcmolt_msg_free(msg);
                        return;
                    }
                    openolt::PacketIndication* pkt_ind = new openolt::PacketIndication;
                    pkt_ind->set_intf_type(bcmolt_to_grpc_interface_rf__intf_type(
                                            (bcmolt_interface_type)pkt_data->interface_ref.intf_type));
                    pkt_ind->set_intf_id((bcmolt_interface_id)pkt_data->interface_ref.intf_id);
                    pkt_ind->set_pkt(pkt_data->buffer.arr, pkt_data->buffer.len);
                    pkt_ind->set_gemport_id(pkt_data->svc_port_id);
		    if (pkt_data->svc_port_id != BCMOLT_SERVICE_PORT_ID_INVALID) { // case of packet-in from the PON interface
                        pon_gem pg((uint32_t)pkt_data->interface_ref.intf_id, pkt_data->svc_port_id);
                        // Find to onu-uni mapping for the pon-gem key
                        bcmos_fastlock_lock(&pon_gem_to_onu_uni_map_lock);
                        auto it = pon_gem_to_onu_uni_map.find(pg);
                        bcmos_fastlock_unlock(&pon_gem_to_onu_uni_map_lock, 0);
                        if (it == pon_gem_to_onu_uni_map.end()) {
                            bcmolt_msg_free(msg);
                            OPENOLT_LOG(ERROR, openolt_log_id, "onu-uni reference not found for packet-in on gemport=%d, pon_intf_id=%d", pkt_data->svc_port_id,  pkt_data->interface_ref.intf_id);
                            return;
                        }
                        pkt_ind->set_onu_id(std::get<0>(it->second));
                        pkt_ind->set_uni_id(std::get<1>(it->second));
                    }
                    ind.set_allocated_pkt_ind(pkt_ind);

                    if (pkt_data->interface_ref.intf_type == BCMOLT_INTERFACE_TYPE_PON) {
                        OPENOLT_LOG(INFO, openolt_log_id,"packet indication, ingress intf_type %s, ingress intf_id %d, gem_port %d, onu_id=%d, uni_id=%d\n",
                            pkt_ind->intf_type().c_str(), pkt_ind->intf_id(), pkt_ind->gemport_id(), pkt_ind->onu_id(), pkt_ind->uni_id());
                    } else if (pkt_data->interface_ref.intf_type == BCMOLT_INTERFACE_TYPE_NNI ) {
                        OPENOLT_LOG(INFO, openolt_log_id, "packet indication, ingress intf_type %s, ingress intf_id %d\n",
                            pkt_ind->intf_type().c_str(), pkt_ind->intf_id());
                    }
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void ItuPonAllocConfigCompletedInd(bcmolt_devid olt, bcmolt_msg *msg) {

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ITUPON_ALLOC:
            switch (msg->subgroup) {
                case BCMOLT_ITUPON_ALLOC_AUTO_SUBGROUP_CONFIGURATION_COMPLETED:
                {
                    bcmolt_itupon_alloc_configuration_completed *pkt =
                        (bcmolt_itupon_alloc_configuration_completed*)msg;
                    bcmolt_itupon_alloc_configuration_completed_data *pkt_data =
                        &((bcmolt_itupon_alloc_configuration_completed*)msg)->data;

                    alloc_cfg_compltd_key key((uint32_t)pkt->key.pon_ni, (uint32_t) pkt->key.alloc_id);
                    alloc_cfg_complete_result res;
                    res.pon_intf_id = pkt->key.pon_ni;
                    res.alloc_id = pkt->key.alloc_id;

                    pkt_data->status == BCMOLT_RESULT_SUCCESS ? res.status = ALLOC_CFG_STATUS_SUCCESS: res.status = ALLOC_CFG_STATUS_FAIL;
                    switch (pkt_data->new_state) {
                        case BCMOLT_ACTIVATION_STATE_NOT_CONFIGURED:
                            res.state = ALLOC_OBJECT_STATE_NOT_CONFIGURED;
                            break;
                        case BCMOLT_ACTIVATION_STATE_INACTIVE:
                            res.state = ALLOC_OBJECT_STATE_INACTIVE;
                            break;
                        case BCMOLT_ACTIVATION_STATE_PROCESSING:
                            res.state = ALLOC_OBJECT_STATE_PROCESSING;
                            break;
                        case BCMOLT_ACTIVATION_STATE_ACTIVE:
                            res.state = ALLOC_OBJECT_STATE_ACTIVE;
                            break;
                        default:
                            OPENOLT_LOG(ERROR, openolt_log_id, "invalid itu pon alloc activation new_state, pon_intf %u, alloc_id %u, new_state %d\n",
                                    pkt->key.pon_ni, pkt->key.alloc_id, pkt_data->new_state);
                            res.state = ALLOC_OBJECT_STATE_NOT_CONFIGURED;
                    }
                    OPENOLT_LOG(INFO, openolt_log_id, "received itu pon alloc cfg complete ind, pon intf %u, alloc_id %u, status %u, new_state %u\n",
                            pkt->key.pon_ni, pkt->key.alloc_id, pkt_data->status, pkt_data->new_state);

                    bcmos_fastlock_lock(&alloc_cfg_wait_lock);
                    // Push the result from BAL to queue
                    std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *>::iterator it = alloc_cfg_compltd_map.find(key);
                    if (it == alloc_cfg_compltd_map.end()) {
                        // could be case of spurious aysnc response, OR, the application timed-out waiting for response and cleared the key.
                        bcmolt_msg_free(msg);
                        OPENOLT_LOG(ERROR, openolt_log_id, "alloc config key not found for alloc_id = %u, pon_intf = %u\n", pkt->key.alloc_id, pkt->key.pon_ni);
                        bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
                        return;
                    }
                    if (it->second) {
                        // Push the result
                        it->second->push(res);
                    }
                    bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
                }
            }
    }

    bcmolt_msg_free(msg);
}

static void ItuPonGemConfigCompletedInd(bcmolt_devid olt, bcmolt_msg *msg) {

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ITUPON_GEM:
            switch (msg->subgroup) {
                case BCMOLT_ITUPON_GEM_AUTO_SUBGROUP_CONFIGURATION_COMPLETED:
                {
                    bcmolt_itupon_gem_configuration_completed *pkt =
                        (bcmolt_itupon_gem_configuration_completed*)msg;
                    bcmolt_itupon_gem_configuration_completed_data *pkt_data =
                        &((bcmolt_itupon_gem_configuration_completed*)msg)->data;

                    gem_cfg_compltd_key key((uint32_t)pkt->key.pon_ni, (uint32_t) pkt->key.gem_port_id);
                    gem_cfg_complete_result res;
                    res.pon_intf_id = pkt->key.pon_ni;
                    res.gem_port_id = pkt->key.gem_port_id;

                    pkt_data->status == BCMOLT_RESULT_SUCCESS ? res.status = GEM_CFG_STATUS_SUCCESS: res.status = GEM_CFG_STATUS_FAIL;
                    switch (pkt_data->new_state) {
                        case BCMOLT_ACTIVATION_STATE_NOT_CONFIGURED:
                            res.state = GEM_OBJECT_STATE_NOT_CONFIGURED;
                            break;
                        case BCMOLT_ACTIVATION_STATE_INACTIVE:
                            res.state = GEM_OBJECT_STATE_INACTIVE;
                            break;
                        case BCMOLT_ACTIVATION_STATE_PROCESSING:
                            res.state = GEM_OBJECT_STATE_PROCESSING;
                            break;
                        case BCMOLT_ACTIVATION_STATE_ACTIVE:
                            res.state = GEM_OBJECT_STATE_ACTIVE;
                            break;
                        default:
                            OPENOLT_LOG(ERROR, openolt_log_id, "invalid itu pon gem activation new_state, pon_intf %u, gem_port_id %u, new_state %d\n",
                                    pkt->key.pon_ni, pkt->key.gem_port_id, pkt_data->new_state);
                            res.state = GEM_OBJECT_STATE_NOT_CONFIGURED;
                    }
                    OPENOLT_LOG(INFO, openolt_log_id, "received itu pon gem cfg complete ind, pon intf %u, gem_port_id %u, status %u, new_state %u\n",
                            pkt->key.pon_ni, pkt->key.gem_port_id, pkt_data->status, pkt_data->new_state);

                    uint32_t gem_cfg_key_check_counter = 1;
                    std::map<gem_cfg_compltd_key,  Queue<gem_cfg_complete_result> *>::iterator it;
                    while(true) {
                        bcmos_fastlock_lock(&gem_cfg_wait_lock);
                        // Push the result from BAL to queue
                        it = gem_cfg_compltd_map.find(key);

                        if (it != gem_cfg_compltd_map.end()) {
                            bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
                            break;
                        } else if (it == gem_cfg_compltd_map.end() && gem_cfg_key_check_counter < MAX_GEM_CFG_KEY_CHECK) {
                            /*During removal of gemport, indication from BAL arriving soon even before we start waiting for gemport cfg completion
                            by pushing empty cfg_result for gem_cfg_compltd_key. To handle this scenario delaying to push gem cfg completion indication
                            to Queue by 6ms.*/
                            bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
                            bcmos_usleep(6000);
                        } else {
                            // could be case of spurious aysnc response, OR, the application timed-out waiting for response and cleared the key.
                            bcmolt_msg_free(msg);
                            OPENOLT_LOG(ERROR, openolt_log_id, "gem config key not found for gem_port_id = %u, pon_intf = %u\n", pkt->key.gem_port_id, pkt->key.pon_ni);
                            bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
                            return;
                        }
                        gem_cfg_key_check_counter++;
                    }

                    bcmos_fastlock_lock(&gem_cfg_wait_lock);
                    if (it->second) {
                        // Push the result
                        it->second->push(res);
                    }
                    bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
                }
            }
    }

    bcmolt_msg_free(msg);
}

static void FlowOperIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    OPENOLT_LOG(DEBUG, openolt_log_id, "flow oper state indication\n");
    bcmolt_msg_free(msg);
}

static void FlowIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    OPENOLT_LOG(DEBUG, openolt_log_id, "flow indication\n");
    bcmolt_msg_free(msg);
}

static void TmQIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    OPENOLT_LOG(DEBUG, openolt_log_id, "traffic mgmt queue indication\n");
    bcmolt_msg_free(msg);
}

static void TmSchedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    OPENOLT_LOG(DEBUG, openolt_log_id,  "traffic mgmt sheduler indication\n");
    bcmolt_msg_free(msg);
}

static void McastGroupIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    OPENOLT_LOG(DEBUG, openolt_log_id, "mcast group indication\n");
    bcmolt_msg_free(msg);
}

static void OnuStartupFailureIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuStartupFailureIndication* sufi_ind = new openolt::OnuStartupFailureIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_SUFI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_sufi*)msg)->key;
                    bcmolt_onu_sufi_data *data = &((bcmolt_onu_sufi*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id, "onu startup failure indication, intf_id %d, onu_id %d, alarm %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status);

                    sufi_ind->set_intf_id(key->pon_ni);
                    sufi_ind->set_onu_id(key->onu_id);
                    sufi_ind->set_status(alarm_status_to_string(data->alarm_status));
                    alarm_ind->set_allocated_onu_startup_fail_ind(sufi_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuSignalDegradeIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuSignalDegradeIndication* sdi_ind = new openolt::OnuSignalDegradeIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_SDI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_sdi*)msg)->key;
                    bcmolt_onu_sdi_data *data = &((bcmolt_onu_sdi*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id, "onu signal degrade indication, intf_id %d, onu_id %d, alarm %d, BER %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status, data->ber);

                    sdi_ind->set_intf_id(key->pon_ni);
                    sdi_ind->set_onu_id(key->onu_id);
                    sdi_ind->set_status(alarm_status_to_string(data->alarm_status));
                    sdi_ind->set_inverse_bit_error_rate(data->ber);
                    alarm_ind->set_allocated_onu_signal_degrade_ind(sdi_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuDriftOfWindowIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuDriftOfWindowIndication* dowi_ind = new openolt::OnuDriftOfWindowIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_DOWI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_dowi*)msg)->key;
                    bcmolt_onu_dowi_data *data = &((bcmolt_onu_dowi*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id, "onu drift of window indication, intf_id %d, onu_id %d, alarm %d, drift %d, new_eqd %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status, data->drift_value, data->new_eqd);

                    dowi_ind->set_intf_id(key->pon_ni);
                    dowi_ind->set_onu_id(key->onu_id);
                    dowi_ind->set_status(alarm_status_to_string(data->alarm_status));
                    dowi_ind->set_drift(data->drift_value);
                    dowi_ind->set_new_eqd(data->new_eqd);
                    alarm_ind->set_allocated_onu_drift_of_window_ind(dowi_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuLossOfOmciChannelIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuLossOfOmciChannelIndication* looci_ind = new openolt::OnuLossOfOmciChannelIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_LOOCI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_looci*)msg)->key;
                    bcmolt_onu_looci_data *data = &((bcmolt_onu_looci*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id, "onu loss of OMCI channel indication, intf_id %d, onu_id %d, alarm %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status);

                    looci_ind->set_intf_id(key->pon_ni);
                    looci_ind->set_onu_id(key->onu_id);
                    looci_ind->set_status(alarm_status_to_string(data->alarm_status));
                    alarm_ind->set_allocated_onu_loss_omci_ind(looci_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuSignalsFailureIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuSignalsFailureIndication* sfi_ind = new openolt::OnuSignalsFailureIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_SFI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_sfi*)msg)->key;
                    bcmolt_onu_sfi_data *data = &((bcmolt_onu_sfi*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id,  "onu signals failure indication, intf_id %d, onu_id %d, alarm %d, BER %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status, data->ber);

                    sfi_ind->set_intf_id(key->pon_ni);
                    sfi_ind->set_onu_id(key->onu_id);
                    sfi_ind->set_status(alarm_status_to_string(data->alarm_status));
                    sfi_ind->set_inverse_bit_error_rate(data->ber);
                    alarm_ind->set_allocated_onu_signals_fail_ind(sfi_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuTransmissionInterferenceWarningIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuTransmissionInterferenceWarning* tiwi_ind = new openolt::OnuTransmissionInterferenceWarning;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_TIWI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_tiwi*)msg)->key;
                    bcmolt_onu_tiwi_data *data = &((bcmolt_onu_tiwi*)msg)->data;

                    OPENOLT_LOG(WARNING, openolt_log_id,  "onu transmission interference warning indication, intf_id %d, onu_id %d, alarm %d, drift %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status, data->drift_value);

                    tiwi_ind->set_intf_id(key->pon_ni);
                    tiwi_ind->set_onu_id(key->onu_id);
                    tiwi_ind->set_status(alarm_status_to_string(data->alarm_status));
                    tiwi_ind->set_drift(data->drift_value);
                    alarm_ind->set_allocated_onu_tiwi_ind(tiwi_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuActivationCompletedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OnuIndication* onu_ind = new openolt::OnuIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_ONU_ACTIVATION_COMPLETED:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_onu_activation_completed*)msg)->key;
                    bcmolt_onu_onu_activation_completed_data*data = &((bcmolt_onu_onu_activation_completed*)msg)->data;

                    onu_ind->set_intf_id(key->pon_ni);
                    onu_ind->set_onu_id(key->onu_id);
                    if (ONU_ACTIVATION_COMPLETED_SUCCESS(data->status))
                        onu_ind->set_oper_state("up");
                    if (ONU_ACTIVATION_COMPLETED_FAIL(data->status))
                        onu_ind->set_oper_state("down");
                        switch (data->fail_reason) {
                            case BCMOLT_ACTIVATION_FAIL_REASON_RANGING:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_REASON_RANGING);
                                break;
                            case BCMOLT_ACTIVATION_FAIL_REASON_PASSWORD_AUTHENTICATION:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_REASON_PASSWORD_AUTHENTICATION);
                                break;
                            case BCMOLT_ACTIVATION_FAIL_REASON_LOS:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_REASON_LOS);
                                break;
                            case BCMOLT_ACTIVATION_FAIL_REASON_ONU_ALARM:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_ONU_ALARM);
                                break;
                            case BCMOLT_ACTIVATION_FAIL_REASON_SWITCH_OVER:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_SWITCH_OVER);
                                break;
                            default:
                                onu_ind->set_fail_reason(openolt::OnuIndication_ActivationFailReason_ONU_ACTIVATION_FAIL_REASON_NONE);
			}
                    // Setting the admin_state state field based on a valid onu_id does not make any sense.
                    // The adapter too does not seem to interpret this seriously.
                    // Legacy code, lets keep this as is for now.
                    (key->onu_id)?onu_ind->set_admin_state("up"):onu_ind->set_admin_state("down");
                    ind.set_allocated_onu_ind(onu_ind);
                    OPENOLT_LOG(INFO, openolt_log_id, "onu indication, pon_ni %d, onu_id %d, onu_state %s, onu_admin %s\n",
                        key->pon_ni, key->onu_id, (data->status==BCMOLT_RESULT_SUCCESS)?"up":"down",
                        (key->onu_id)?"up":"down");
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuLossOfKeySyncFailureIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuLossOfKeySyncFailureIndication* loss_of_sync_fail_ind = new openolt::OnuLossOfKeySyncFailureIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_LOKI:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_loki*)msg)->key;
                    bcmolt_onu_loki_data *data = &((bcmolt_onu_loki*)msg)->data;

                    OPENOLT_LOG(INFO, openolt_log_id, "Got onu loss of key sync, intf_id %d, onu_id %d, alarm_status %d\n",
                        key->pon_ni, key->onu_id, data->alarm_status);

                    loss_of_sync_fail_ind->set_intf_id(key->pon_ni);
                    loss_of_sync_fail_ind->set_onu_id(key->onu_id);
                    loss_of_sync_fail_ind->set_status(alarm_status_to_string(data->alarm_status));
                    alarm_ind->set_allocated_onu_loss_of_sync_fail_ind(loss_of_sync_fail_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuItuPonStatsAlarmRaisedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuItuPonStatsIndication* onu_itu_pon_stats_ind = new openolt::OnuItuPonStatsIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_RAISED:
                {
                    bcmolt_onu_key *onu_key = &((bcmolt_onu_itu_pon_stats_alarm_raised*)msg)->key;
                    bcmolt_onu_itu_pon_stats_alarm_raised_data *data = &((bcmolt_onu_itu_pon_stats_alarm_raised*)msg)->data;

                    if (_BCMOLT_FIELD_MASK_BIT_IS_SET(data->presence_mask, BCMOLT_ONU_ITU_PON_STATS_ALARM_RAISED_DATA_ID_STAT)) {
                        switch (data->stat)
                        {
                            case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_RDI_ERRORS:
                                uint64_t rdi_errors;
                                openolt::RdiErrorIndication* rdi_err_ind = new openolt::RdiErrorIndication;
                                rdi_errors = get_pon_stats_alarms_data(onu_key->pon_ni, onu_key->onu_id, data->stat);
                                onu_itu_pon_stats_ind->set_intf_id(onu_key->pon_ni);
                                onu_itu_pon_stats_ind->set_onu_id(onu_key->onu_id);
                                rdi_err_ind->set_rdi_error_count(rdi_errors);
                                rdi_err_ind->set_status("on");
                                onu_itu_pon_stats_ind->set_allocated_rdi_error_ind(rdi_err_ind);
                                OPENOLT_LOG(INFO, openolt_log_id, "Got onu raised alarm indication, intf_id %d, onu_id %d, \
                                        rdi_errors %"PRIu64"\n", onu_key->pon_ni, onu_key->onu_id, rdi_errors);
                                alarm_ind->set_allocated_onu_itu_pon_stats_ind(onu_itu_pon_stats_ind);
                                break;
                            /* It is a further requirement
                            case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_BIP_ERRORS:
                                uint64_t bip_errors;
                                bip_errors = get_pon_stats_alarms_data(onu_key->pon_ni, onu_key->onu_id, data->stat);
                                onu_itu_pon_stats_ind->set_intf_id(onu_key->pon_ni);
                                onu_itu_pon_stats_ind->set_onu_id(onu_key->onu_id);
                                OPENOLT_LOG(INFO, openolt_log_id, "Got onu raised alarm indication, intf_id %d, onu_id %d, \
                                        bip_errors %"PRIu64"\n", onu_key->pon_ni, onu_key->onu_id, bip_errors);
                                alarm_ind->set_allocated_onu_itu_pon_stats_ind(onu_itu_pon_stats_ind);
                                break;
                            */
                        }
                        ind.set_allocated_alarm_ind(alarm_ind);
                    }
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuItuPonStatsAlarmClearedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuItuPonStatsIndication* onu_itu_pon_stats_ind = new openolt::OnuItuPonStatsIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_CLEARED:
                {
                    bcmolt_onu_key *onu_key = &((bcmolt_onu_itu_pon_stats_alarm_cleared*)msg)->key;
                    bcmolt_onu_itu_pon_stats_alarm_cleared_data *data = &((bcmolt_onu_itu_pon_stats_alarm_cleared*)msg)->data;

                    if (_BCMOLT_FIELD_MASK_BIT_IS_SET(data->presence_mask, BCMOLT_ONU_ITU_PON_STATS_ALARM_CLEARED_DATA_ID_STAT)) {
                        switch (data->stat)
                        {
                            case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_RDI_ERRORS:
                                uint64_t rdi_errors;
                                openolt::RdiErrorIndication* rdi_err_ind = new openolt::RdiErrorIndication;

                                rdi_errors = get_pon_stats_alarms_data(onu_key->pon_ni, onu_key->onu_id, data->stat);
                                onu_itu_pon_stats_ind->set_intf_id(onu_key->pon_ni);
                                onu_itu_pon_stats_ind->set_onu_id(onu_key->onu_id);
                                rdi_err_ind->set_rdi_error_count(rdi_errors);
                                rdi_err_ind->set_status("off");
                                onu_itu_pon_stats_ind->set_allocated_rdi_error_ind(rdi_err_ind);
                                OPENOLT_LOG(INFO, openolt_log_id, "Got onu cleared alarm indication, intf_id %d, onu_id %d, \
                                        rdi_errors %"PRIu64"\n", onu_key->pon_ni, onu_key->onu_id, rdi_errors);
                                alarm_ind->set_allocated_onu_itu_pon_stats_ind(onu_itu_pon_stats_ind);
                                break;
                            /* It is a further requirement
                            case BCMOLT_ONU_ITU_PON_STATS_DATA_ID_BIP_ERRORS:
                                uint64_t bip_errors;
                                bip_errors = get_pon_stats_alarms_data(onu_key->pon_ni, onu_key->onu_id, data->stat);
                                onu_itu_pon_stats_ind->set_intf_id(onu_key->pon_ni);
                                onu_itu_pon_stats_ind->set_onu_id(onu_key->onu_id);
                                onu_itu_pon_stats_ind->set_bip_errors(bip_errors);
                                OPENOLT_LOG(INFO, openolt_log_id, "Got onu cleared alarm indication, intf_id %d, onu_id %d, \
                                        bip_errors %"PRIu64"\n", onu_key->pon_ni, onu_key->onu_id, bip_errors);
                                alarm_ind->set_allocated_onu_itu_pon_stats_ind(onu_itu_pon_stats_ind);
                                break;
                            */
                        }
                        ind.set_allocated_alarm_ind(alarm_ind);
                    }
                }
            }
    }

    oltIndQ.push(ind);
    bcmolt_msg_free(msg);
}

static void OnuDeactivationCompletedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;

    openolt::Indication onu_ind;
    openolt::OnuIndication* onu_ind_data = new openolt::OnuIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_ONU_DEACTIVATION_COMPLETED:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_onu_deactivation_completed*)msg)->key;
                    bcmolt_onu_onu_deactivation_completed_data *data =
                        &((bcmolt_onu_onu_deactivation_completed*)msg)->data;

                    OPENOLT_LOG(INFO, openolt_log_id, "Got onu deactivation, intf_id %d, onu_id %d, fail_reason %d, result_status %s\n",
                        key->pon_ni, key->onu_id, data->fail_reason, bcmolt_result_to_string(data->status).c_str());

                    onu_ind_data->set_intf_id(key->pon_ni);
                    onu_ind_data->set_onu_id(key->onu_id);
                    onu_ind_data->set_oper_state("down");
                    onu_ind_data->set_admin_state("down");
                    onu_ind.set_allocated_onu_ind(onu_ind_data);

                    onu_deact_compltd_key onu_key((uint32_t)key->pon_ni, (uint32_t) key->onu_id);
                    onu_deactivate_complete_result res;
                    res.pon_intf_id = (uint32_t)key->pon_ni;
                    res.onu_id = (uint32_t) key->onu_id;
                    res.result = data->status;
                    res.reason = data->fail_reason;

                    OPENOLT_LOG(INFO, openolt_log_id, "received onu deactivate result, pon intf %u, onu_id %u, status %u, reason %u\n",
                            key->pon_ni, key->onu_id, data->status, data->fail_reason);

                    bcmos_fastlock_lock(&onu_deactivate_wait_lock);
                    // Push the result from BAL to queue
                    std::map<onu_deact_compltd_key,  Queue<onu_deactivate_complete_result> *>::iterator it = onu_deact_compltd_map.find(onu_key);
                    if (it == onu_deact_compltd_map.end()) {
                        // could be case of spurious aysnc response, OR, the application timed-out waiting for response and cleared the key.
                        // OR most importantly, could be a case of ONU going down (reboot, PON cable plug-out) where
                        // BCMOLT_ONU_AUTO_SUBGROUP_ONU_DEACTIVATION_COMPLETED is received without any explicit request from the application.
                        // The application has to take care handling spurious indications.
                        OPENOLT_LOG(WARNING, openolt_log_id, "onu deactivate completed key not found for pon intf %u, onu_id %u\n",
                            key->pon_ni, key->onu_id);
                    }
                    else if (it->second) {
                        // Push the result
                        it->second->push(res);
                    }
                    bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);
                }
            }
    }

    oltIndQ.push(onu_ind);
    bcmolt_msg_free(msg);
}

static void OnuRssiMeasurementCompletedIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_RSSI_MEASUREMENT_COMPLETED:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_rssi_measurement_completed*)msg)->key;
                    bcmolt_onu_rssi_measurement_completed_data *data = &((bcmolt_onu_rssi_measurement_completed*)msg)->data;
                    double rx_power_mean_dbm = 0.0;

                    OPENOLT_LOG(INFO, openolt_log_id, "ONU RSSI Measurement Completed indication - pon_id: %d, onu_id: %d, status: %d, fail_reason: %d\n",
                                key->pon_ni, key->onu_id, data->status, data->fail_reason);

                    if (data->status == BCMOLT_RESULT_SUCCESS) {
                        auto trx_eeprom_reader =
#ifdef ASGVOLT64
                            TrxEepromReader{TrxEepromReader::DEVICE_GPON, TrxEepromReader::RX_POWER, key->pon_ni};
#else
                            TrxEepromReader{TrxEepromReader::DEVICE_XGSPON, TrxEepromReader::RX_POWER, key->pon_ni};
#endif
                        auto power = trx_eeprom_reader.read_power_mean_dbm();

                        if (power.second) {
                            rx_power_mean_dbm = power.first.first;
                            OPENOLT_LOG(INFO, openolt_log_id, "ONU RSSI Measurement Completed indication - rx_power_mean_dbm: %f\n", rx_power_mean_dbm);
                        } else {
                            OPENOLT_LOG(ERROR, openolt_log_id, "ONU RSSI Measurement Completed indication - Rx power read failure\n");
                        }
                    } else {
                        OPENOLT_LOG(ERROR, openolt_log_id, "ONU RSSI Measurement Completed indication - failure");
                    }

                    // for internal use insert into map
                    onu_rssi_compltd_key onu_key((uint32_t)key->pon_ni, (uint32_t)key->onu_id);
                    onu_rssi_complete_result res;
                    res.pon_intf_id = (uint32_t)key->pon_ni;
                    res.onu_id = (uint32_t)key->onu_id;
                    res.status = bcmolt_result_to_string(data->status);
                    res.reason = data->fail_reason;
                    res.rx_power_mean_dbm = rx_power_mean_dbm;

                    bcmos_fastlock_lock(&onu_rssi_wait_lock);
                    auto it = onu_rssi_compltd_map.find(onu_key);
                    if (it == onu_rssi_compltd_map.end()) {
                        OPENOLT_LOG(ERROR, openolt_log_id, "ONU RSSI Measurement Completed key not found for pon intf %u, onu_id %u\n",
                            key->pon_ni, key->onu_id);
                    } else if (it->second) {
                        it->second->push(res);
                    } else {
                        OPENOLT_LOG(WARNING, openolt_log_id, "ONU RSSI Measurement Completed queue not found for pon intf %u, onu_id %u\n",
                            key->pon_ni, key->onu_id);
                    }
                    bcmos_fastlock_unlock(&onu_rssi_wait_lock, 0);
                }
            }
    }

    bcmolt_msg_free(msg);
}

/* removed by BAL v3.0
bcmos_errno OnuProcessingErrorIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuProcessingErrorIndication* onu_proc_error_ind = new openolt::OnuProcessingErrorIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_processing_error*)obj)->key);

    OPENOLT_LOG(WARNING, openolt_log_id, "onu processing error indication, intf_id %d, onu_id %d\n",
        key->intf_id, key->sub_term_id);

    onu_proc_error_ind->set_intf_id(key->intf_id);
    onu_proc_error_ind->set_onu_id(key->sub_term_id);

    alarm_ind->set_allocated_onu_processing_error_ind(onu_proc_error_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}
*/

static void GroupIndication(bcmolt_devid olt, bcmolt_msg *msg) {

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_GROUP:
            switch (msg->subgroup) {
                case BCMOLT_GROUP_AUTO_SUBGROUP_COMPLETE_MEMBERS_UPDATE:
                {
                    bcmolt_group_key *key = &((bcmolt_group_complete_members_update*)msg)->key;
                    bcmolt_group_complete_members_update_data *data =
                            &((bcmolt_group_complete_members_update*)msg)->data;

                    if (data->result == BCMOLT_RESULT_SUCCESS) {
                        OPENOLT_LOG(INFO, openolt_log_id, "Complete members update indication for group %d (successful)\n", key->id);
                    } else {
                        OPENOLT_LOG(ERROR, openolt_log_id, "Complete members update indication for group %d (failed with reason %d)\n", key->id, data->fail_reason);
                    }
                }
            }
    }
    bcmolt_msg_free(msg);
}

Status SubscribeIndication() {
    bcmolt_rx_cfg rx_cfg = {};
    bcmos_errno rc;

    if (subscribed) {
        return Status::OK;
    }

    rx_cfg.obj_type = BCMOLT_OBJ_ID_DEVICE;
    rx_cfg.rx_cb = OltOperIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_device_auto_subgroup_connection_complete;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "Olt connection complete state indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_DEVICE;
    rx_cfg.rx_cb = OltOperIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_device_auto_subgroup_disconnection_complete;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "Olt disconnection complete state indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_DEVICE;
    rx_cfg.rx_cb = OltOperIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_device_auto_subgroup_connection_failure;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "Olt connection failure state indication subscribe failed");

    /* Interface LOS indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_PON_INTERFACE;
    rx_cfg.rx_cb = LosIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_pon_interface_auto_subgroup_los;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "LOS indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_PON_INTERFACE;
    rx_cfg.rx_cb = IfOperIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_pon_interface_auto_subgroup_state_change_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "PON Interface operations state change indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_NNI_INTERFACE;
    rx_cfg.rx_cb = IfOperIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_nni_interface_auto_subgroup_state_change;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "NNI Interface operations state change indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuAlarmIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_xgpon_alarm;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu alarm indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuAlarmIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_gpon_alarm;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu alarm indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuDyingGaspIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_dgi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu dying-gasp indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_PON_INTERFACE;
    rx_cfg.rx_cb = OnuDiscoveryIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_pon_interface_auto_subgroup_onu_discovered;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu discovery indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuStartupFailureIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_sufi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "onu startup failure indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuSignalDegradeIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_sdi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu sdi indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuDriftOfWindowIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_dowi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu dowi indication subscribe failed");

    /* LOOCI indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuLossOfOmciChannelIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_looci;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu looci indication subscribe failed");

    /* SFI indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuSignalsFailureIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_sfi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu sfi indication subscribe failed");

    /* TIWI indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuTransmissionInterferenceWarningIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_tiwi;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu tiwi indication subscribe failed");

    /* ONU Activation Completed Indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuActivationCompletedIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_onu_activation_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "onu activation completed indication subscribe failed");

    /* ONU Deactivation Completed Indication */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuDeactivationCompletedIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_onu_deactivation_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL,
            "onu deactivation indication subscribe failed");

    /* ONU Loss of Key Sync Indiction */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuLossOfKeySyncFailureIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_loki;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu loss of key sync indication subscribe failed");

    /* ONU ITU-PON Raised Stats Indiction */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuItuPonStatsAlarmRaisedIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_itu_pon_stats_alarm_raised;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu itu-pon raised stats indication subscribe failed");

    /* ONU ITU-PON Cleared Stats Indiction */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuItuPonStatsAlarmClearedIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_itu_pon_stats_alarm_cleared;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu itu-pon cleared stats indication subscribe failed");

    /* Packet-In by Access_Control */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ACCESS_CONTROL;
    rx_cfg.rx_cb = PacketIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_access_control_auto_subgroup_receive_eth_packet;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "Packet indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ITUPON_ALLOC;
    rx_cfg.rx_cb = ItuPonAllocConfigCompletedInd;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_itupon_alloc_auto_subgroup_configuration_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "ITU PON Alloc Configuration Complete Indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ITUPON_GEM;
    rx_cfg.rx_cb = ItuPonGemConfigCompletedInd;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_itupon_gem_auto_subgroup_configuration_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "ITU PON Gem Configuration Complete Indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_GROUP;
    rx_cfg.rx_cb = GroupIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_group_auto_subgroup_complete_members_update;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "Complete members update indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuRssiMeasurementCompletedIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_rssi_measurement_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "ONU RSSI Measurement indication subscription failed");

    subscribed = true;

    return Status::OK;
}

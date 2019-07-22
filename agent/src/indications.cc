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
#include "indications.h"
#include "core.h"
#include "utils.h"
#include "stats_collection.h"
#include "translation.h"
#include "state.h"

#include <string>

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
#include <bcmolt_host_api.h>
#include <bcmolt_api_model_api_structs.h>
}

using grpc::Status;

extern Queue<openolt::Indication> oltIndQ;
//Queue<openolt::Indication*> oltIndQ;


bool subscribed = false;
uint32_t nni_intf_id = 0;
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
#define ONU_RANGING_STATE_IF_UP(state) \
       ((state == BCMOLT_RESULT_SUCCESS) ? BCMOS_TRUE : BCMOS_FALSE)
#define ONU_RANGING_STATE_IF_DOWN(state) \
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

std::string bcmolt_to_grpc_flow_intf_type(bcmolt_flow_interface_type intf_type)
{
    if (intf_type == BCMOLT_FLOW_INTERFACE_TYPE_NNI) {
        return "nni";
    } else if (intf_type == BCMOLT_FLOW_INTERFACE_TYPE_PON) {
        return "pon";
    } else if (intf_type == BCMOLT_FLOW_INTERFACE_TYPE_HOST) {
        return "host";
    }
    return "unknown";
}

static void OltOperIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OltIndication* olt_ind = new openolt::OltIndication;
    Status status;
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
                    bcmolt_xgpon_onu_alarms *onu_alarms = 
                        &((bcmolt_onu_xgpon_alarm_data *)msg)->xgpon_onu_alarm;
                    onu_alarm_ind->set_los_status(alarm_status_to_string(onu_alarms->losi));
                    onu_alarm_ind->set_lob_status(alarm_status_to_string(onu_alarms->lobi));
                    onu_alarm_ind->set_lopc_miss_status(alarm_status_to_string(
                        onu_alarms->lopci_miss));
                    onu_alarm_ind->set_lopc_mic_error_status(alarm_status_to_string(
                        onu_alarms->lopci_mic_error));

                    alarm_ind->set_allocated_onu_alarm_ind(onu_alarm_ind);
                    ind.set_allocated_alarm_ind(alarm_ind);
                    break;
                }
                case BCMOLT_ONU_AUTO_SUBGROUP_GPON_ALARM:
                {
                    bcmolt_gpon_onu_alarms *onu_alarms = 
                        &((bcmolt_onu_gpon_alarm_data *)msg)->gpon_onu_alarm;
                    onu_alarm_ind->set_los_status(alarm_status_to_string(onu_alarms->losi));
                    /* TODO: need to set lofi and loami 
                        onu_alarm_ind->set_lof_status(alarm_status_to_string(onu_alarms->lofi));
                        onu_alarm_ind->set_loami_status(alarm_status_to_string(
                        onu_alarms->loami));
                    */ 
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
                    bcmolt_onu_dgi_data* dgi_data = (bcmolt_onu_dgi_data *)msg;
                    dgi_ind->set_status(alarm_status_to_string(dgi_data->alarm_status));

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

static void OnuIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OnuIndication* onu_ind = new openolt::OnuIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_RANGING_COMPLETED:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_ranging_completed*)msg)->key;
                    bcmolt_onu_ranging_completed_data *data = &((bcmolt_onu_ranging_completed*)msg)->data;

                    onu_ind->set_intf_id(key->pon_ni);
                    onu_ind->set_onu_id(key->onu_id);
                    if (ONU_RANGING_STATE_IF_UP(data->status))
                        onu_ind->set_oper_state("up");
                    if (ONU_RANGING_STATE_IF_DOWN(data->status))
                        onu_ind->set_oper_state("down");
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

static void OnuOperIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::OnuIndication* onu_ind = new openolt::OnuIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_STATE_CHANGE:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_state_change*)msg)->key;
                    bcmolt_onu_state_change_data *data = &((bcmolt_onu_state_change*)msg)->data;

                    onu_ind->set_intf_id(key->pon_ni);
                    onu_ind->set_onu_id(key->onu_id);
                    if (ONU_STATE_IF_UP(data->new_onu_state))
                        onu_ind->set_oper_state("up");
                    if (ONU_STATE_IF_DOWN(data->new_onu_state))
                        onu_ind->set_oper_state("down");
                    ind.set_allocated_onu_ind(onu_ind);

                    OPENOLT_LOG(INFO, openolt_log_id, "onu oper state indication, intf_id %d, onu_id %d, old oper state %d, new oper state %s\n",
                        key->pon_ni, key->onu_id, data->new_onu_state, onu_ind->oper_state().c_str());
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
    openolt::PacketIndication* pkt_ind = new openolt::PacketIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_FLOW:
            switch (msg->subgroup) {
                case BCMOLT_FLOW_AUTO_SUBGROUP_RECEIVE_ETH_PACKET:
                {
                    bcmolt_flow_receive_eth_packet *pkt = 
                        (bcmolt_flow_receive_eth_packet*)msg;
                    bcmolt_flow_receive_eth_packet_data *pkt_data = 
                        &((bcmolt_flow_receive_eth_packet*)msg)->data;

                    uint32_t port_no = GetPortNum_(pkt->key.flow_id);
                    pkt_ind->set_intf_type(bcmolt_to_grpc_flow_intf_type((bcmolt_flow_interface_type)get_flow_status(pkt->key.flow_id, pkt->key.flow_type, INGRESS_INTF_TYPE)));
                    pkt_ind->set_intf_id(get_flow_status(pkt->key.flow_id, pkt->key.flow_type, INGRESS_INTF_ID));
                    pkt_ind->set_gemport_id(get_flow_status(pkt->key.flow_id, pkt->key.flow_type, SVC_PORT_ID));
                    pkt_ind->set_flow_id(pkt->key.flow_id);
                    pkt_ind->set_pkt(pkt_data->buffer.arr, pkt_data->buffer.len);
                    pkt_ind->set_port_no(port_no);
                    pkt_ind->set_cookie(get_flow_status(pkt->key.flow_id, pkt->key.flow_type, COOKIE));
                    ind.set_allocated_pkt_ind(pkt_ind);

                    OPENOLT_LOG(INFO, openolt_log_id, "packet indication, ingress intf_type %s, ingress intf_id %d, egress intf_type %s, egress intf_id %lu, svc_port %d, flow_type %s, flow_id %d, port_no %d, cookie %"PRIu64"\n",
                        pkt_ind->intf_type().c_str(), pkt_ind->intf_id(), 
                        bcmolt_to_grpc_flow_intf_type((bcmolt_flow_interface_type)get_flow_status(pkt->key.flow_id, pkt->key.flow_type, EGRESS_INTF_TYPE)).c_str(),
                        get_flow_status(pkt->key.flow_id, pkt->key.flow_type, EGRESS_INTF_ID),
                        pkt_ind->gemport_id(), GET_FLOW_TYPE(pkt->key.flow_type), 
                        pkt_ind->flow_id(), port_no, pkt_ind->cookie());
                }
            }
    }

    oltIndQ.push(ind);
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

static void OnuActivationFailureIndication(bcmolt_devid olt, bcmolt_msg *msg) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuActivationFailureIndication* activation_fail_ind = new openolt::OnuActivationFailureIndication;

    switch (msg->obj_type) {
        case BCMOLT_OBJ_ID_ONU:
            switch (msg->subgroup) {
                case BCMOLT_ONU_AUTO_SUBGROUP_ONU_DEACTIVATION_COMPLETED:
                {
                    bcmolt_onu_key *key = &((bcmolt_onu_onu_activation_completed*)msg)->key;
                    bcmolt_onu_onu_activation_completed_data *data = 
                        &((bcmolt_onu_onu_activation_completed*)msg)->data;

                    OPENOLT_LOG(INFO, openolt_log_id, "Got onu deactivation, intf_id %d, onu_id %d, fail_reason %d\n",
                        key->pon_ni, key->onu_id, data->fail_reason);

                    activation_fail_ind->set_intf_id(key->pon_ni);
                    activation_fail_ind->set_onu_id(key->onu_id);
                    activation_fail_ind->set_fail_reason(data->fail_reason);
                    alarm_ind->set_allocated_onu_activation_fail_ind(activation_fail_ind);

                    ind.set_allocated_alarm_ind(alarm_ind);
                }
            }
    }

    oltIndQ.push(ind);
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
    rx_cfg.rx_cb = OnuIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_ranging_completed;

    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "onu indication subscribe failed");

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

    /* ONU Activation Failure Indiction */
    rx_cfg.obj_type = BCMOLT_OBJ_ID_ONU;
    rx_cfg.rx_cb = OnuActivationFailureIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_onu_auto_subgroup_onu_deactivation_completed;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, 
            "onu activation falaire indication subscribe failed");

    rx_cfg.obj_type = BCMOLT_OBJ_ID_FLOW;
    rx_cfg.rx_cb = PacketIndication;
    rx_cfg.flags = BCMOLT_AUTO_FLAGS_NONE;
    rx_cfg.subgroup = bcmolt_flow_auto_subgroup_receive_eth_packet;
    rc = bcmolt_ind_subscribe(current_device, &rx_cfg);
    if(rc != BCM_ERR_OK)
        return Status(grpc::StatusCode::INTERNAL, "Packet indication subscribe failed");

    subscribed = true;

    return Status::OK;
}

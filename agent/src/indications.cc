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
#include <bal_api.h>
#include <bal_api_end.h>
}

using grpc::Status;

extern Queue<openolt::Indication> oltIndQ;
//Queue<openolt::Indication*> oltIndQ;


bool subscribed = false;

bcmos_errno OmciIndication(bcmbal_obj *obj);

std::string bcmbal_to_grpc_intf_type(bcmbal_intf_type intf_type)
{
    if (intf_type == BCMBAL_INTF_TYPE_NNI) {
        return "nni";
    } else if (intf_type == BCMBAL_INTF_TYPE_PON) {
        return "pon";
    }
    return "unknown";
}

bcmos_errno OltOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OltIndication* olt_ind = new openolt::OltIndication;
    Status status;

    bcmbal_access_terminal_oper_status_change *acc_term_ind = (bcmbal_access_terminal_oper_status_change *)obj;
    std::string admin_state;
    if (acc_term_ind->data.admin_state == BCMBAL_STATE_UP) {
        admin_state = "up";
    } else {
        admin_state = "down";
    }

    if (acc_term_ind->data.new_oper_status == BCMBAL_STATUS_UP) {
        // Determine device capabilities before transitionto acive state
        ProbeDeviceCapabilities_();
        olt_ind->set_oper_state("up");
    } else {
        olt_ind->set_oper_state("down");
    }
    ind.set_allocated_olt_ind(olt_ind);

    BCM_LOG(INFO, openolt_log_id, "Olt oper status indication, admin_state: %s oper_state: %s\n",
            admin_state.c_str(),
            olt_ind->oper_state().c_str());

    oltIndQ.push(ind);

    // Enable all PON interfaces. 
    // 
    for (int i = 0; i < NumPonIf_(); i++) {
        status = EnablePonIf_(i);
        if (!status.ok()) {
            // FIXME - raise alarm to report error in enabling PON
        }
    }

    // Enable all NNI interfaces. 
    // 
    for (int i = 0; i < NumNniIf_(); i++) {
        status = EnableUplinkIf_(i);
        if (!status.ok()) {
            // FIXME - raise alarm to report error in enabling PON
        }
    }

    /* register for omci indication */
    {
        bcmbal_cb_cfg cb_cfg = {};
        uint16_t ind_subgroup;

        cb_cfg.module = BCMOS_MODULE_ID_NONE;
        cb_cfg.obj_type = BCMBAL_OBJ_ID_PACKET;
        ind_subgroup = BCMBAL_IND_SUBGROUP(packet, itu_omci_channel_rx);
        cb_cfg.p_object_key_info = NULL;
        cb_cfg.p_subgroup = &ind_subgroup;
        cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OmciIndication;
        bcmbal_subscribe_ind(0, &cb_cfg);
    }

    if (acc_term_ind->data.new_oper_status == BCMBAL_STATUS_UP) {
        ProbePonIfTechnology_();
        state.activate();
    }
    else {
        state.deactivate();
    }

    return BCM_ERR_OK;
}

bcmos_errno LosIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::LosIndication* los_ind = new openolt::LosIndication;

    bcmbal_interface_los* bcm_los_ind = (bcmbal_interface_los *) obj;
    int intf_id = interface_key_to_port_no(bcm_los_ind->key);
    std::string status = alarm_status_to_string(bcm_los_ind->data.status);

    BCM_LOG(INFO, openolt_log_id, "LOS indication : intf_type: %d intf_id: %d port: %d status %s\n", 
            bcm_los_ind->key.intf_type, bcm_los_ind->key.intf_id, intf_id, status.c_str());

    los_ind->set_intf_id(intf_id);
    los_ind->set_status(status);

    alarm_ind->set_allocated_los_ind(los_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno IfIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::IntfIndication* intf_ind = new openolt::IntfIndication;

    BCM_LOG(INFO, openolt_log_id, "intf indication, intf_id: %d\n",
        ((bcmbal_interface_oper_status_change *)obj)->key.intf_id );

    intf_ind->set_intf_id(((bcmbal_interface_oper_status_change *)obj)->key.intf_id);
    if (((bcmbal_interface_oper_status_change *)obj)->data.new_oper_status == BCMBAL_STATUS_UP) {
        intf_ind->set_oper_state("up");
    } else {
        intf_ind->set_oper_state("down");
    }
    ind.set_allocated_intf_ind(intf_ind);

    oltIndQ.push(ind);

    return BCM_ERR_OK;
}

bcmos_errno IfOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::IntfOperIndication* intf_oper_ind = new openolt::IntfOperIndication;
    bcmbal_interface_oper_status_change* bcm_if_oper_ind = (bcmbal_interface_oper_status_change *) obj;

    intf_oper_ind->set_type(bcmbal_to_grpc_intf_type(bcm_if_oper_ind->key.intf_type));
    intf_oper_ind->set_intf_id(bcm_if_oper_ind->key.intf_id);

    if (bcm_if_oper_ind->data.new_oper_status == BCMBAL_STATUS_UP) {
        intf_oper_ind->set_oper_state("up");
    } else {
        intf_oper_ind->set_oper_state("down");
    }

    BCM_LOG(INFO, openolt_log_id, "intf oper state indication, intf_type %s, intf_id %d, oper_state %s, admin_state %d\n",
        intf_oper_ind->type().c_str(),
        bcm_if_oper_ind->key.intf_id,
        intf_oper_ind->oper_state().c_str(),
        bcm_if_oper_ind->data.admin_state);

    ind.set_allocated_intf_oper_ind(intf_oper_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuAlarmIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuAlarmIndication* onu_alarm_ind = new openolt::OnuAlarmIndication;

    bcmbal_subscriber_terminal_key *key =
        &((bcmbal_subscriber_terminal_sub_term_alarm*)obj)->key;

    bcmbal_subscriber_terminal_alarms *alarms =
        &(((bcmbal_subscriber_terminal_sub_term_alarm*)obj)->data.alarm);

    BCM_LOG(WARNING, openolt_log_id, "onu alarm indication intf_id %d, onu_id %d, alarm: los %d, lob %d, lopc_miss %d, lopc_mic_error %d\n",
        key->intf_id, key->sub_term_id, alarms->los, alarms->lob, alarms->lopc_miss, alarms->lopc_mic_error);

    onu_alarm_ind->set_intf_id(key->intf_id);
    onu_alarm_ind->set_onu_id(key->sub_term_id);
    onu_alarm_ind->set_los_status(alarm_status_to_string(alarms->los));
    onu_alarm_ind->set_lob_status(alarm_status_to_string(alarms->lob));
    onu_alarm_ind->set_lopc_miss_status(alarm_status_to_string(alarms->lopc_miss));
    onu_alarm_ind->set_lopc_mic_error_status(alarm_status_to_string(alarms->lopc_mic_error));

    alarm_ind->set_allocated_onu_alarm_ind(onu_alarm_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuDyingGaspIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::DyingGaspIndication* dg_ind = new openolt::DyingGaspIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_dgi*)obj)->key);

    bcmbal_subscriber_terminal_dgi_data *data =
        &(((bcmbal_subscriber_terminal_dgi*)obj)->data);


    BCM_LOG(WARNING, openolt_log_id, "onu dying-gasp indication, intf_id %d, onu_id %d, alarm %d\n",
        key->intf_id, key->sub_term_id, data->dgi_status);

    dg_ind->set_intf_id(key->intf_id);
    dg_ind->set_onu_id(key->sub_term_id);
    dg_ind->set_status(alarm_status_to_string(data->dgi_status));

    alarm_ind->set_allocated_dying_gasp_ind(dg_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuDiscoveryIndication(bcmbal_cfg *obj) {
    openolt::Indication ind;
    openolt::OnuDiscIndication* onu_disc_ind = new openolt::OnuDiscIndication;
    openolt::SerialNumber* serial_number = new openolt::SerialNumber;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_sub_term_disc*)obj)->key);

    bcmbal_subscriber_terminal_sub_term_disc_data *data =
        &(((bcmbal_subscriber_terminal_sub_term_disc*)obj)->data);

    bcmbal_serial_number *in_serial_number = &(data->serial_number);

    BCM_LOG(INFO, openolt_log_id, "onu discover indication, intf_id %d, serial_number %s\n",
        key->intf_id, serial_number_to_str(in_serial_number).c_str());

    onu_disc_ind->set_intf_id(key->intf_id);
    serial_number->set_vendor_id(reinterpret_cast<const char *>(in_serial_number->vendor_id), 4);
    serial_number->set_vendor_specific(reinterpret_cast<const char *>(in_serial_number->vendor_specific), 8);
    onu_disc_ind->set_allocated_serial_number(serial_number);
    ind.set_allocated_onu_disc_ind(onu_disc_ind);

    oltIndQ.push(ind);

    return BCM_ERR_OK;
}

bcmos_errno OnuIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OnuIndication* onu_ind = new openolt::OnuIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_oper_status_change*)obj)->key);

    bcmbal_subscriber_terminal_oper_status_change_data *data =
        &(((bcmbal_subscriber_terminal_oper_status_change*)obj)->data);

    BCM_LOG(INFO, openolt_log_id, "onu indication, intf_id %d, onu_id %d, oper_state %d, admin_state %d\n",
        key->intf_id, key->sub_term_id, data->new_oper_status, data->admin_state);

    onu_ind->set_intf_id(key->intf_id);
    onu_ind->set_onu_id(key->sub_term_id);
    if (data->new_oper_status == BCMBAL_STATUS_UP) {
        onu_ind->set_oper_state("up");
    } else {
        onu_ind->set_oper_state("down");
    }
    if (data->admin_state == BCMBAL_STATE_UP) {
        onu_ind->set_admin_state("up");
    } else {
        onu_ind->set_admin_state("down");
    }

    ind.set_allocated_onu_ind(onu_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OnuIndication* onu_ind = new openolt::OnuIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_oper_status_change*)obj)->key);

    bcmbal_subscriber_terminal_oper_status_change_data *data =
        &(((bcmbal_subscriber_terminal_oper_status_change*)obj)->data);

    onu_ind->set_intf_id(key->intf_id);
    onu_ind->set_onu_id(key->sub_term_id);
    if (data->new_oper_status == BCMBAL_STATUS_UP) {
        onu_ind->set_oper_state("up");
    } else {
        onu_ind->set_oper_state("down");
    }
    if (data->admin_state == BCMBAL_STATE_UP) {
        onu_ind->set_admin_state("up");
    } else {
        onu_ind->set_admin_state("down");
    }

    ind.set_allocated_onu_ind(onu_ind);

    BCM_LOG(INFO, openolt_log_id, "onu oper state indication, intf_id %d, onu_id %d, old oper state %d, new oper state %s, admin_state %s\n",
        key->intf_id, key->sub_term_id, data->old_oper_status, onu_ind->oper_state().c_str(), onu_ind->admin_state().c_str());

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OmciIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OmciIndication* omci_ind = new openolt::OmciIndication;
    bcmbal_packet_itu_omci_channel_rx *in =
        (bcmbal_packet_itu_omci_channel_rx *)obj;

    BCM_LOG(DEBUG, omci_log_id, "OMCI indication: intf_id %d, onu_id %d\n",
        in->key.packet_send_dest.u.itu_omci_channel.intf_id,
        in->key.packet_send_dest.u.itu_omci_channel.sub_term_id);

    omci_ind->set_intf_id(in->key.packet_send_dest.u.itu_omci_channel.intf_id);
    omci_ind->set_onu_id(in->key.packet_send_dest.u.itu_omci_channel.sub_term_id);
    omci_ind->set_pkt(in->data.pkt.val, in->data.pkt.len);

    ind.set_allocated_omci_ind(omci_ind);
    oltIndQ.push(ind);

    return BCM_ERR_OK;
}

bcmos_errno PacketIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::PacketIndication* pkt_ind = new openolt::PacketIndication;
    bcmbal_packet_bearer_channel_rx *in = (bcmbal_packet_bearer_channel_rx *)obj;

    uint32_t port_no = GetPortNum_(in->data.flow_id);
    pkt_ind->set_intf_type(bcmbal_to_grpc_intf_type(in->data.intf_type));
    pkt_ind->set_intf_id(in->data.intf_id);
    pkt_ind->set_gemport_id(in->data.svc_port);
    pkt_ind->set_flow_id(in->data.flow_id);
    pkt_ind->set_pkt(in->data.pkt.val, in->data.pkt.len);
    pkt_ind->set_port_no(port_no);
    pkt_ind->set_cookie(in->data.flow_cookie);

    ind.set_allocated_pkt_ind(pkt_ind);

    BCM_LOG(INFO, openolt_log_id, "packet indication, intf_type %s, intf_id %d, svc_port %d, flow_id %d port_no %d cookie %u\n",
        pkt_ind->intf_type().c_str(), in->data.intf_id, in->data.svc_port, in->data.flow_id, port_no, in->data.flow_cookie);

    oltIndQ.push(ind);

    return BCM_ERR_OK;
}

bcmos_errno FlowOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    BCM_LOG(DEBUG, openolt_log_id, "flow oper state indication\n");
    return BCM_ERR_OK;
}

bcmos_errno FlowIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    BCM_LOG(DEBUG, openolt_log_id, "flow indication\n");
    return BCM_ERR_OK;
}

bcmos_errno TmQIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    BCM_LOG(DEBUG, openolt_log_id, "traffic mgmt queue indication\n");
    return BCM_ERR_OK;
}

bcmos_errno TmSchedIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    BCM_LOG(DEBUG, openolt_log_id,  "traffic mgmt sheduler indication\n");
    return BCM_ERR_OK;
}

bcmos_errno McastGroupIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    BCM_LOG(DEBUG, openolt_log_id, "mcast group indication\n");
    return BCM_ERR_OK;
}

bcmos_errno OnuStartupFailureIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuStartupFailureIndication* sufi_ind = new openolt::OnuStartupFailureIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_sufi*)obj)->key);

    bcmbal_subscriber_terminal_sufi_data *data =
        &(((bcmbal_subscriber_terminal_sufi*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id, "onu startup failure indication, intf_id %d, onu_id %d, alarm %d\n",
        key->intf_id, key->sub_term_id, data->sufi_status);

    sufi_ind->set_intf_id(key->intf_id);
    sufi_ind->set_onu_id(key->sub_term_id);
    sufi_ind->set_status(alarm_status_to_string(data->sufi_status));

    alarm_ind->set_allocated_onu_startup_fail_ind(sufi_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuSignalDegradeIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuSignalDegradeIndication* sdi_ind = new openolt::OnuSignalDegradeIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_sdi*)obj)->key);

    bcmbal_subscriber_terminal_sdi_data *data =
        &(((bcmbal_subscriber_terminal_sdi*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id, "onu signal degrade indication, intf_id %d, onu_id %d, alarm %d, BER %d\n",
        key->intf_id, key->sub_term_id, data->sdi_status, data->ber);

    sdi_ind->set_intf_id(key->intf_id);
    sdi_ind->set_onu_id(key->sub_term_id);
    sdi_ind->set_status(alarm_status_to_string(data->sdi_status));
    sdi_ind->set_inverse_bit_error_rate(data->ber);

    alarm_ind->set_allocated_onu_signal_degrade_ind(sdi_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuDriftOfWindowIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuDriftOfWindowIndication* dowi_ind = new openolt::OnuDriftOfWindowIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_dowi*)obj)->key);

    bcmbal_subscriber_terminal_dowi_data *data =
        &(((bcmbal_subscriber_terminal_dowi*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id, "onu drift of window indication, intf_id %d, onu_id %d, alarm %d, drift %d, new_eqd %d\n",
        key->intf_id, key->sub_term_id, data->dowi_status, data->drift_value, data->new_eqd);

    dowi_ind->set_intf_id(key->intf_id);
    dowi_ind->set_onu_id(key->sub_term_id);
    dowi_ind->set_status(alarm_status_to_string(data->dowi_status));
    dowi_ind->set_drift(data->drift_value);
    dowi_ind->set_new_eqd(data->new_eqd);

    alarm_ind->set_allocated_onu_drift_of_window_ind(dowi_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuLossOfOmciChannelIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuLossOfOmciChannelIndication* looci_ind = new openolt::OnuLossOfOmciChannelIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_looci*)obj)->key);

    bcmbal_subscriber_terminal_looci_data *data =
        &(((bcmbal_subscriber_terminal_looci*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id, "onu loss of OMCI channel indication, intf_id %d, onu_id %d, alarm %d\n",
        key->intf_id, key->sub_term_id, data->looci_status);

    looci_ind->set_intf_id(key->intf_id);
    looci_ind->set_onu_id(key->sub_term_id);
    looci_ind->set_status(alarm_status_to_string(data->looci_status));

    alarm_ind->set_allocated_onu_loss_omci_ind(looci_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuSignalsFailureIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuSignalsFailureIndication* sfi_ind = new openolt::OnuSignalsFailureIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_sfi*)obj)->key);

    bcmbal_subscriber_terminal_sfi_data *data =
        &(((bcmbal_subscriber_terminal_sfi*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id,  "onu signals failure indication, intf_id %d, onu_id %d, alarm %d, BER %d\n",
        key->intf_id, key->sub_term_id, data->sfi_status, data->ber);


    sfi_ind->set_intf_id(key->intf_id);
    sfi_ind->set_onu_id(key->sub_term_id);
    sfi_ind->set_status(alarm_status_to_string(data->sfi_status));
    sfi_ind->set_inverse_bit_error_rate(data->ber);

    alarm_ind->set_allocated_onu_signals_fail_ind(sfi_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuTransmissionInterferenceWarningIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuTransmissionInterferenceWarning* tiwi_ind = new openolt::OnuTransmissionInterferenceWarning;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_tiwi*)obj)->key);

    bcmbal_subscriber_terminal_tiwi_data *data =
        &(((bcmbal_subscriber_terminal_tiwi*)obj)->data);

    BCM_LOG(WARNING, openolt_log_id,  "onu transmission interference warning indication, intf_id %d, onu_id %d, alarm %d, drift %d\n",
        key->intf_id, key->sub_term_id, data->tiwi_status, data->drift_value);

    tiwi_ind->set_intf_id(key->intf_id);
    tiwi_ind->set_onu_id(key->sub_term_id);
    tiwi_ind->set_status(alarm_status_to_string(data->tiwi_status));
    tiwi_ind->set_drift(data->drift_value);

    alarm_ind->set_allocated_onu_tiwi_ind(tiwi_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuActivationFailureIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuActivationFailureIndication* activation_fail_ind = new openolt::OnuActivationFailureIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_sub_term_act_fail*)obj)->key);

    BCM_LOG(WARNING, openolt_log_id, "onu activation failure indication, intf_id %d, onu_id %d\n",
        key->intf_id, key->sub_term_id);


    activation_fail_ind->set_intf_id(key->intf_id);
    activation_fail_ind->set_onu_id(key->sub_term_id);

    alarm_ind->set_allocated_onu_activation_fail_ind(activation_fail_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuProcessingErrorIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::AlarmIndication* alarm_ind = new openolt::AlarmIndication;
    openolt::OnuProcessingErrorIndication* onu_proc_error_ind = new openolt::OnuProcessingErrorIndication;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_processing_error*)obj)->key);

    BCM_LOG(WARNING, openolt_log_id, "onu processing error indication, intf_id %d, onu_id %d\n",
        key->intf_id, key->sub_term_id);


    onu_proc_error_ind->set_intf_id(key->intf_id);
    onu_proc_error_ind->set_onu_id(key->sub_term_id);

    alarm_ind->set_allocated_onu_processing_error_ind(onu_proc_error_ind);
    ind.set_allocated_alarm_ind(alarm_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

Status SubscribeIndication() {
    bcmbal_cb_cfg cb_cfg = {};
    uint16_t ind_subgroup;

    if (subscribed) {
        return Status::OK;
    }

    cb_cfg.module = BCMOS_MODULE_ID_NONE;

    /* OLT device operational state change indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_ACCESS_TERMINAL;
    ind_subgroup = bcmbal_access_terminal_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OltOperIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Olt operations state change indication subscribe failed");
    }

    /* Interface LOS indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_INTERFACE;
    ind_subgroup = bcmbal_interface_auto_id_los;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)LosIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "LOS indication subscribe failed");
    }

    /* Interface indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_INTERFACE;
    ind_subgroup = bcmbal_interface_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)IfIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Interface indication subscribe failed");
    }

    /* Interface operational state change indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_INTERFACE;
    ind_subgroup = bcmbal_interface_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)IfOperIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Interface operations state change indication subscribe failed");
    }

    /* onu alarm indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sub_term_alarm;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuAlarmIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu alarm indication subscribe failed");
    }

    /* onu dying-gasp indication  */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_dgi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuDyingGaspIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu dying-gasp indication subscribe failed");
    }

    /* onu discovery indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sub_term_disc;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuDiscoveryIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu discovery indication subscribe failed");
    }

    /* onu indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu indication subscribe failed");
    }
    /* onu operational state change indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuOperIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu operational state change indication subscribe failed");
    }

    /* Packet (bearer) indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_PACKET;
    ind_subgroup = bcmbal_packet_auto_id_bearer_channel_rx;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)PacketIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Packet indication subscribe failed");
    }

    /* Flow Operational State Change */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_FLOW;
    ind_subgroup = bcmbal_flow_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)FlowOperIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Flow operational state change indication subscribe failed");
    }
#if 0
    /* Flow Indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_FLOW;
    ind_subgroup = bcmbal_flow_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)FlowIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Flow indication subscribe failed");
    }

    /* TM queue indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_TM_QUEUE;
    ind_subgroup = bcmbal_tm_queue_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)TmQIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Traffic mgmt queue indication subscribe failed");
    }
#endif

    /* TM sched indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_TM_SCHED;
    ind_subgroup = bcmbal_tm_sched_auto_id_oper_status_change;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)TmSchedIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Traffic mgmt queue indication subscribe failed");
    }

#if 0
    /* Multicast group indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_GROUP;
    ind_subgroup = bcmbal_group_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)McastGroupIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Multicast group indication subscribe failed");
    }
#endif


    /* ONU startup failure indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sufi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuStartupFailureIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu startup failure indication subscribe failed");
    }

    /* SDI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sdi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuSignalDegradeIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu sdi indication subscribe failed");
    }

    /* DOWI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_dowi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuDriftOfWindowIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu dowi indication subscribe failed");
    }

    /* LOOCI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_looci;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuLossOfOmciChannelIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu looci indication subscribe failed");
    }

    /* SFI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sfi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuSignalsFailureIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu sfi indication subscribe failed");
    }

    /* TIWI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_tiwi;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuTransmissionInterferenceWarningIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu tiwi indication subscribe failed");
    }

    /* TIWI indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_sub_term_act_fail;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuActivationFailureIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu activation falaire indication subscribe failed");
    }

    /* ONU processing error indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_SUBSCRIBER_TERMINAL;
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_processing_error;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OnuProcessingErrorIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "onu processing error indication subscribe failed");
    }

    subscribed = true;

    return Status::OK;
}

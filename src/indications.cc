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
extern "C"
{
#include <bcmos_system.h>
#include <bal_api.h>
#include <bal_api_end.h>
}

using grpc::Status;

Queue<openolt::Indication> oltIndQ;
//Queue<openolt::Indication*> oltIndQ;

bool subscribed = false;

bcmos_errno OmciIndication(bcmbal_obj *obj);

bcmos_errno OltIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OltIndication* olt_ind = new openolt::OltIndication;
    Status status;

    bcmbal_access_terminal_ind *acc_term_ind = (bcmbal_access_terminal_ind *)obj;
    if (acc_term_ind->data.oper_status == BCMBAL_STATUS_UP) {
        olt_ind->set_oper_state("up");
    } else {
        olt_ind->set_oper_state("down");
    }
    ind.set_allocated_olt_ind(olt_ind);
    std::cout << "olt indication, oper_state:" << ind.olt_ind().oper_state() << std::endl;
    oltIndQ.push(ind);

    // Enable all PON interfaces.
    for (int i = 0; i < MAX_SUPPORTED_INTF; i++) {
        status = EnablePonIf_(i);
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

    return BCM_ERR_OK;
}

bcmos_errno LosIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "LOS indication " << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno IfIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::IntfIndication* intf_ind = new openolt::IntfIndication;

    std::cout << "intf indication, intf_id:"
              << ((bcmbal_interface_ind *)obj)->key.intf_id << std::endl;

    intf_ind->set_intf_id(((bcmbal_interface_ind *)obj)->key.intf_id);
    if (((bcmbal_interface_ind *)obj)->data.oper_status == BCMBAL_STATUS_UP) {
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
    std::cout << "intf oper state indication, intf_id:"
              << ((bcmbal_interface_ind *)obj)->key.intf_id
              << " type:" << ((bcmbal_interface_oper_status_change *)obj)->key.intf_type
              << " oper_state:" << ((bcmbal_interface_oper_status_change *)obj)->data.new_oper_status
              << " admin_state:" << ((bcmbal_interface_oper_status_change *)obj)->data.admin_state
              << std::endl;

    intf_oper_ind->set_intf_id(((bcmbal_interface_oper_status_change *)obj)->key.intf_id);

    if (((bcmbal_interface_oper_status_change *)obj)->key.intf_type == BCMBAL_INTF_TYPE_NNI) {
        intf_oper_ind->set_type("nni");
    } else if (((bcmbal_interface_oper_status_change *)obj)->key.intf_type == BCMBAL_INTF_TYPE_PON) {
        intf_oper_ind->set_type("pon");
    } else {
        intf_oper_ind->set_type("unknown");
    }

    if (((bcmbal_interface_oper_status_change *)obj)->data.new_oper_status == BCMBAL_STATUS_UP) {
        intf_oper_ind->set_oper_state("up");
    } else {
        intf_oper_ind->set_oper_state("down");
    }

    ind.set_allocated_intf_oper_ind(intf_oper_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuAlarmIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "onu alarm indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno OnuDyingGaspIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "onu dying-gasp indication" << std::endl;
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

    std::cout << "onu discover indication, intf_id:"
         << key->intf_id
         << " serial_number:"
         << serial_number_to_str(in_serial_number) << std::endl;

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
    openolt::SerialNumber* serial_number = new openolt::SerialNumber;

    bcmbal_subscriber_terminal_key *key =
        &(((bcmbal_subscriber_terminal_ind*)obj)->key);

    bcmbal_subscriber_terminal_ind_data *data =
        &(((bcmbal_subscriber_terminal_ind*)obj)->data);

    bcmbal_serial_number *in_serial_number = &(data->serial_number);

    std::cout << "onu indication, intf_id:"
         << key->intf_id
         << " serial_number:"
         << serial_number_to_str(in_serial_number) << std::endl;

    onu_ind->set_intf_id(key->intf_id);
    onu_ind->set_onu_id(key->sub_term_id);
    serial_number->set_vendor_id(reinterpret_cast<const char *>(in_serial_number->vendor_id), 4);
    serial_number->set_vendor_specific(reinterpret_cast<const char *>(in_serial_number->vendor_specific), 8);
    onu_ind->set_allocated_serial_number(serial_number);
    ind.set_allocated_onu_ind(onu_ind);

    oltIndQ.push(ind);
    return BCM_ERR_OK;
}

bcmos_errno OnuOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "onu oper state indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno OmciIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    openolt::OmciIndication* omci_ind = new openolt::OmciIndication;
    bcmbal_packet_itu_omci_channel_rx *in =
        (bcmbal_packet_itu_omci_channel_rx *)obj;

    std::cout << "omci indication" << std::endl;

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

    std::cout << "packet indication"
              << " intf_id:" << in->data.intf_id
              << " svc_port:" << in->data.svc_port
              << " flow_id:" << in->data.flow_id
              << std::endl;

    pkt_ind->set_intf_id(in->data.intf_id);
    pkt_ind->set_gemport_id(in->data.svc_port);
    pkt_ind->set_flow_id(in->data.flow_id);
    pkt_ind->set_pkt(in->data.pkt.val, in->data.pkt.len);

    ind.set_allocated_pkt_ind(pkt_ind);
    oltIndQ.push(ind);

    return BCM_ERR_OK;
}

bcmos_errno FlowOperIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "flow oper state indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno FlowIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "flow indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno TmQIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "traffic mgmt queue indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno TmSchedIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "traffic mgmt sheduler indication" << std::endl;
    return BCM_ERR_OK;
}

bcmos_errno McastGroupIndication(bcmbal_obj *obj) {
    openolt::Indication ind;
    std::cout << "mcast group indication" << std::endl;
    return BCM_ERR_OK;
}

Status SubscribeIndication() {
    bcmbal_cb_cfg cb_cfg = {};
    uint16_t ind_subgroup;

    if (subscribed) {
        return Status::OK;
    }

    cb_cfg.module = BCMOS_MODULE_ID_NONE;


    /* OLT device indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_ACCESS_TERMINAL;
    ind_subgroup = bcmbal_access_terminal_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)OltIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Olt indication subscribe failed");
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
    ind_subgroup = bcmbal_interface_auto_id_ind;
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
    ind_subgroup = bcmbal_subscriber_terminal_auto_id_ind;
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

    /* TM sched indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_TM_SCHED;
    ind_subgroup = bcmbal_tm_sched_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)TmSchedIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Traffic mgmt queue indication subscribe failed");
    }

    /* Multicast group indication */
    cb_cfg.obj_type = BCMBAL_OBJ_ID_GROUP;
    ind_subgroup = bcmbal_group_auto_id_ind;
    cb_cfg.p_subgroup = &ind_subgroup;
    cb_cfg.ind_cb_hdlr = (f_bcmbal_ind_handler)McastGroupIndication;
    if (BCM_ERR_OK != bcmbal_subscribe_ind(DEFAULT_ATERM_ID, &cb_cfg)) {
        return Status(grpc::StatusCode::INTERNAL, "Multicast group indication subscribe failed");
    }

    subscribed = true;

    return Status::OK;
}

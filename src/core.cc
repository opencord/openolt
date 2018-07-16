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

#include <iostream>
#include <memory>
#include <string>

#include "Queue.h"
#include <iostream>
#include <sstream>

#include "core.h"
#include "indications.h"
#include "stats_collection.h"
#include "error_format.h"

extern "C"
{
#include <bcmos_system.h>
#include <bal_api.h>
#include <bal_api_end.h>
}

Status Enable_() {
    static bool enabled = false;
    bcmbal_access_terminal_cfg acc_term_obj;
    bcmbal_access_terminal_key key = { };

    if (!enabled) {
        std::cout << "Enable OLT" << std::endl;
        key.access_term_id = DEFAULT_ATERM_ID;
        BCMBAL_CFG_INIT(&acc_term_obj, access_terminal, key);
        BCMBAL_CFG_PROP_SET(&acc_term_obj, access_terminal, admin_state, BCMBAL_STATE_UP);
        bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(acc_term_obj.hdr));
        if (err) {
            std::cout << "ERROR: Failed to enable OLT" << std::endl;
            return bcm_to_grpc_err(err, "Failed to enable OLT");
        }
        enabled = true;
    }
    return Status::OK;
}

Status EnablePonIf_(uint32_t intf_id) {
    bcmbal_interface_cfg interface_obj;
    bcmbal_interface_key interface_key;

    interface_key.intf_id = intf_id;
    interface_key.intf_type = BCMBAL_INTF_TYPE_PON;

    BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);
    BCMBAL_CFG_PROP_SET(&interface_obj, interface, admin_state, BCMBAL_STATE_UP);

    bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err) {
        std::cout << "ERROR: Failed to enable PON interface: " << intf_id << std::endl;
        return bcm_to_grpc_err(err, "Failed to enable PON interface");
    }

    return Status::OK;
}

Status DisablePonIf_(uint32_t intf_id) {
    bcmbal_interface_cfg interface_obj;
    bcmbal_interface_key interface_key;

    interface_key.intf_id = intf_id;
    interface_key.intf_type = BCMBAL_INTF_TYPE_PON;

    BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);
    BCMBAL_CFG_PROP_SET(&interface_obj, interface, admin_state, BCMBAL_STATE_DOWN);

    bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err) {
        std::cout << "ERROR: Failed to disable PON interface: " << intf_id << std::endl;
        return bcm_to_grpc_err(err, "Failed to disable PON interface");
    }

    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {

    bcmbal_subscriber_terminal_cfg sub_term_obj = {};
    bcmbal_subscriber_terminal_key subs_terminal_key;
    bcmbal_serial_number serial_num = {};
    bcmbal_registration_id registration_id = {};

    std::cout << "Enabling ONU " << onu_id << " on PON " << intf_id << std::endl;
    std::cout << "Vendor Id " << vendor_id
              << "Vendor Specific Id " << vendor_specific
              << std::endl;

    subs_terminal_key.sub_term_id = onu_id;
    subs_terminal_key.intf_id = intf_id;
    BCMBAL_CFG_INIT(&sub_term_obj, subscriber_terminal, subs_terminal_key);

    memcpy(serial_num.vendor_id, vendor_id, 4);
    memcpy(serial_num.vendor_specific, vendor_specific, 4);
    BCMBAL_CFG_PROP_SET(&sub_term_obj, subscriber_terminal, serial_number, serial_num);

#if 0
    // Commenting out as this is causing issues with onu activation
    // with BAL 2.6 (Broadcom CS5248819).

    // FIXME - Use a default (all zeros) registration id.
    memset(registration_id.arr, 0, sizeof(registration_id.arr));
    BCMBAL_CFG_PROP_SET(&sub_term_obj, subscriber_terminal, registration_id, registration_id);
#endif

    BCMBAL_CFG_PROP_SET(&sub_term_obj, subscriber_terminal, admin_state, BCMBAL_STATE_UP);

    bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(sub_term_obj.hdr));
    if (err) {
        std::cout << "ERROR: Failed to enable ONU: " << std::endl;
        return bcm_to_grpc_err(err, "Failed to enable ONU");
    }

    return SchedAdd_(intf_id, onu_id, mk_agg_port_id(onu_id));

    //return Status::OK;
}

#define MAX_CHAR_LENGTH  20
#define MAX_OMCI_MSG_LENGTH 44
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    bcmbal_u8_list_u32_max_2048 buf; /* A structure with a msg pointer and length value */
    bcmos_errno err = BCM_ERR_OK;

    /* The destination of the OMCI packet is a registered ONU on the OLT PON interface */
    bcmbal_dest proxy_pkt_dest;

    proxy_pkt_dest.type = BCMBAL_DEST_TYPE_ITU_OMCI_CHANNEL;
    proxy_pkt_dest.u.itu_omci_channel.sub_term_id = onu_id;
    proxy_pkt_dest.u.itu_omci_channel.intf_id = intf_id;

    // ???
    if ((pkt.size()/2) > MAX_OMCI_MSG_LENGTH) {
        buf.len = MAX_OMCI_MSG_LENGTH;
    } else {
        buf.len = pkt.size()/2;
    }

    /* Send the OMCI packet using the BAL remote proxy API */
    uint16_t idx1 = 0;
    uint16_t idx2 = 0;
    uint8_t arraySend[buf.len];
    char str1[MAX_CHAR_LENGTH];
    char str2[MAX_CHAR_LENGTH];
    memset(&arraySend, 0, buf.len);

    std::cout << "Sending omci msg to ONU of length is "
         << buf.len
         << std::endl;

    for (idx1=0,idx2=0; idx1<((buf.len)*2); idx1++,idx2++) {
       sprintf(str1,"%c", pkt[idx1]);
       sprintf(str2,"%c", pkt[++idx1]);
       strcat(str1,str2);
       arraySend[idx2] = strtol(str1, NULL, 16);
    }

    buf.val = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.val, (uint8_t *)arraySend, buf.len);

    std::cout << "After converting bytes to hex "
              << buf.val << buf.len << std::endl;

    err = bcmbal_pkt_send(0, proxy_pkt_dest, (const char *)(buf.val), buf.len);

    std::cout << "OMCI request msg of length " << buf.len
              << " sent to ONU" << onu_id
              << " through PON " << intf_id << std::endl;

    free(buf.val);

    return Status::OK;
}

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    bcmos_errno err = BCM_ERR_OK;
    bcmbal_dest proxy_pkt_dest;
    bcmbal_u8_list_u32_max_2048 buf;

    proxy_pkt_dest.type = BCMBAL_DEST_TYPE_SUB_TERM,
    proxy_pkt_dest.u.sub_term.sub_term_id = onu_id;
    proxy_pkt_dest.u.sub_term.intf_id = intf_id;

    buf.len = pkt.size();
    buf.val = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.val, (uint8_t *)pkt.data(), buf.len);

    err = bcmbal_pkt_send(0, proxy_pkt_dest, (const char *)(buf.val), buf.len);

    std::cout << "Packet out of length " << buf.len
              << " sent to ONU" << onu_id
              << " through PON " << intf_id << std::endl;

    free(buf.val);

    return Status::OK;
}

Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt) {
    bcmos_errno err = BCM_ERR_OK;
    bcmbal_dest proxy_pkt_dest;
    bcmbal_u8_list_u32_max_2048 buf;

    proxy_pkt_dest.type = BCMBAL_DEST_TYPE_NNI,
    proxy_pkt_dest.u.nni.intf_id = intf_id;

    buf.len = pkt.size();
    buf.val = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.val, (uint8_t *)pkt.data(), buf.len);

    err = bcmbal_pkt_send(0, proxy_pkt_dest, (const char *)(buf.val), buf.len);

    std::cout << "Packet out of length " << buf.len
              << " sent through uplink port " << intf_id << std::endl;

    free(buf.val);

    return Status::OK;
}

Status FlowAdd_(uint32_t onu_id,
                uint32_t flow_id, const std::string flow_type,
                uint32_t access_intf_id, uint32_t network_intf_id,
                uint32_t gemport_id, uint32_t priority_value,
                const ::openolt::Classifier& classifier,
                const ::openolt::Action& action) {
    bcmos_errno err;
    bcmbal_flow_cfg cfg;
    bcmbal_flow_key key = { };

    std::cout << "flow add -"
              << " intf_id:" << access_intf_id
              << " onu_id:" << onu_id
              << " flow_id:" << flow_id
              << " flow_type:" << flow_type
              << " gemport_id:" << gemport_id
              << " network_intf_id:" << network_intf_id
              << std::endl;

    key.flow_id = flow_id;
    if (flow_type.compare("upstream") == 0 ) {
        key.flow_type = BCMBAL_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare("downstream") == 0) {
        key.flow_type = BCMBAL_FLOW_TYPE_DOWNSTREAM;
    } else {
        std::cout << "Invalid flow type " << flow_type << std::endl;
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    BCMBAL_CFG_INIT(&cfg, flow, key);

    BCMBAL_CFG_PROP_SET(&cfg, flow, admin_state, BCMBAL_STATE_UP);
    BCMBAL_CFG_PROP_SET(&cfg, flow, access_int_id, access_intf_id);
    BCMBAL_CFG_PROP_SET(&cfg, flow, network_int_id, network_intf_id);
    BCMBAL_CFG_PROP_SET(&cfg, flow, sub_term_id, onu_id);
    BCMBAL_CFG_PROP_SET(&cfg, flow, svc_port_id, gemport_id);
    BCMBAL_CFG_PROP_SET(&cfg, flow, priority, priority_value);


    {
        bcmbal_classifier val = { };

        if (classifier.o_tpid()) {
            val.o_tpid = classifier.o_tpid();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_O_TPID;
        }

        if (classifier.o_vid()) {
            val.o_vid = classifier.o_vid();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_O_VID;
        }

        if (classifier.i_tpid()) {
            val.i_tpid = classifier.i_tpid();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_I_TPID;
        }

        if (classifier.i_vid()) {
            val.i_vid = classifier.i_vid();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_I_VID;
        }

        if (classifier.o_pbits()) {
            val.o_pbits = classifier.o_pbits();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_O_PBITS;
        }

        if (classifier.i_pbits()) {
            val.i_pbits = classifier.i_pbits();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_I_PBITS;
        }

        if (classifier.eth_type()) {
            val.ether_type = classifier.eth_type();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_ETHER_TYPE;
        }

        /*
        if (classifier.dst_mac()) {
            val.dst_mac = classifier.dst_mac();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_DST_MAC;
        }

        if (classifier.src_mac()) {
            val.src_mac = classifier.src_mac();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_SRC_MAC;
        }
        */

        if (classifier.ip_proto()) {
            val.ip_proto = classifier.ip_proto();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_IP_PROTO;
        }

        /*
        if (classifier.dst_ip()) {
            val.dst_ip = classifier.dst_ip();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_DST_IP;
        }

        if (classifier.src_ip()) {
            val.src_ip = classifier.src_ip();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_SRC_IP;
        }
        */

        if (classifier.src_port()) {
            val.src_port = classifier.src_port();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_SRC_PORT;
        }

        if (classifier.dst_port()) {
            val.dst_port = classifier.dst_port();
            val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_DST_PORT;
        }

        if (!classifier.pkt_tag_type().empty()) {
            if (classifier.pkt_tag_type().compare("untagged") == 0) {
                val.pkt_tag_type = BCMBAL_PKT_TAG_TYPE_UNTAGGED;
                val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_PKT_TAG_TYPE;
            } else if (classifier.pkt_tag_type().compare("single_tag") == 0) {
                val.pkt_tag_type = BCMBAL_PKT_TAG_TYPE_SINGLE_TAG;
                val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_PKT_TAG_TYPE;
            } else if (classifier.pkt_tag_type().compare("double_tag") == 0) {
                val.pkt_tag_type = BCMBAL_PKT_TAG_TYPE_DOUBLE_TAG;
                val.presence_mask = val.presence_mask | BCMBAL_CLASSIFIER_ID_PKT_TAG_TYPE;
            }
        }

        BCMBAL_CFG_PROP_SET(&cfg, flow, classifier, val);
    }

    {
        bcmbal_action val = { };

        const ::openolt::ActionCmd& cmd = action.cmd();

        if (cmd.add_outer_tag()) {
            val.cmds_bitmask |= BCMBAL_ACTION_CMD_ID_ADD_OUTER_TAG;
            val.presence_mask |= BCMBAL_ACTION_ID_CMDS_BITMASK;
        }

        if (cmd.remove_outer_tag()) {
            val.cmds_bitmask |= BCMBAL_ACTION_CMD_ID_REMOVE_OUTER_TAG;
            val.presence_mask |= BCMBAL_ACTION_ID_CMDS_BITMASK;
        }

        if (cmd.trap_to_host()) {
            val.cmds_bitmask |= BCMBAL_ACTION_CMD_ID_TRAP_TO_HOST;
            val.presence_mask |= BCMBAL_ACTION_ID_CMDS_BITMASK;
        }

        if (action.o_vid()) {
            val.o_vid = action.o_vid();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_O_VID;
        }

        if (action.o_pbits()) {
            val.o_pbits = action.o_pbits();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_O_PBITS;
        }

        if (action.o_tpid()) {
            val.o_tpid = action.o_tpid();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_O_TPID;
        }

        if (action.i_vid()) {
            val.i_vid = action.i_vid();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_I_VID;
        }

        if (action.i_pbits()) {
            val.i_pbits = action.i_pbits();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_I_PBITS;
        }

        if (action.i_tpid()) {
            val.i_tpid = action.i_tpid();
            val.presence_mask = val.presence_mask | BCMBAL_ACTION_ID_I_TPID;
        }

        BCMBAL_CFG_PROP_SET(&cfg, flow, action, val);
    }

    {
        bcmbal_tm_sched_id val;
        val = (bcmbal_tm_sched_id) mk_sched_id(onu_id);
        BCMBAL_CFG_PROP_SET(&cfg, flow, dba_tm_sched_id, val);
    }

    err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
    if (err) {
        std::cout << "ERROR: flow add failed" << std::endl;
        return bcm_to_grpc_err(err, "flow add failed");
    }

    register_new_flow(key);

    return Status::OK;
}

Status SchedAdd_(int intf_id, int onu_id, int agg_port_id) {
    bcmbal_tm_sched_cfg cfg;
    bcmbal_tm_sched_key key = { };
    bcmbal_tm_sched_type sched_type;

    key.id = mk_sched_id(onu_id);
    key.dir = BCMBAL_TM_SCHED_DIR_US;

    BCMBAL_CFG_INIT(&cfg, tm_sched, key);

    {
        bcmbal_tm_sched_owner val = { };

        val.type = BCMBAL_TM_SCHED_OWNER_TYPE_AGG_PORT;
        val.u.agg_port.intf_id = (bcmbal_intf_id) intf_id;
	    val.u.agg_port.presence_mask = val.u.agg_port.presence_mask | BCMBAL_TM_SCHED_OWNER_AGG_PORT_ID_INTF_ID;
        val.u.agg_port.sub_term_id = (bcmbal_sub_id) onu_id;
        val.u.agg_port.presence_mask = val.u.agg_port.presence_mask | BCMBAL_TM_SCHED_OWNER_AGG_PORT_ID_SUB_TERM_ID;
	    val.u.agg_port.agg_port_id = (bcmbal_aggregation_port_id) agg_port_id;
	    val.u.agg_port.presence_mask = val.u.agg_port.presence_mask | BCMBAL_TM_SCHED_OWNER_AGG_PORT_ID_AGG_PORT_ID;

        BCMBAL_CFG_PROP_SET(&cfg, tm_sched, owner, val);
    }

    bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
    if (err) {
        std::cout << "ERROR: Failed to create upstream DBA sched"
                  << " id:" << key.id
                  << " intf_id:" << intf_id
                  << " onu_id:" << onu_id << std::endl;
        return bcm_to_grpc_err(err, "Failed to create upstream DBA sched");
        //return 1;
    }
    std::cout << "create upstream DBA sched"
              << " id:" << key.id
              << " intf_id:" << intf_id
              << " onu_id:" << onu_id << std::endl;

    return Status::OK;
    //return 0;
}

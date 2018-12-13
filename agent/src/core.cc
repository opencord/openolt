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
#include <chrono>
#include <thread>

#include "device.h"
#include "core.h"
#include "indications.h"
#include "stats_collection.h"
#include "error_format.h"
#include "state.h"
#include "utils.h"

extern "C"
{
#include <bcmos_system.h>
#include <bal_api.h>
#include <bal_api_end.h>
// FIXME : dependency problem
// #include <bcm_common_gpon.h>
// #include <bcm_dev_log_task.h>
}
// These need patched into bal_model_types.h directly. But, with above extern "C", it cannot be done
inline bcmbal_action_cmd_id& operator|=(bcmbal_action_cmd_id& a, bcmbal_action_cmd_id b) {return a = static_cast<bcmbal_action_cmd_id>(static_cast<int>(a) | static_cast<int>(b));}
inline bcmbal_action_id& operator|=(bcmbal_action_id& a, bcmbal_action_id b) {return a = static_cast<bcmbal_action_id>(static_cast<int>(a) | static_cast<int>(b));}
inline bcmbal_classifier_id& operator|=(bcmbal_classifier_id& a, bcmbal_classifier_id b) {return a = static_cast<bcmbal_classifier_id>(static_cast<int>(a) | static_cast<int>(b));}
inline bcmbal_tm_sched_owner_agg_port_id& operator|=(bcmbal_tm_sched_owner_agg_port_id& a, bcmbal_tm_sched_owner_agg_port_id b) {return a = static_cast<bcmbal_tm_sched_owner_agg_port_id>(static_cast<int>(a) | static_cast<int>(b));}
inline bcmbal_tm_sched_parent_id& operator|=(bcmbal_tm_sched_parent_id& a, bcmbal_tm_sched_parent_id b) {return a = static_cast<bcmbal_tm_sched_parent_id>(static_cast<int>(a) | static_cast<int>(b));}
inline bcmbal_tm_shaping_id& operator|=(bcmbal_tm_shaping_id& a, bcmbal_tm_shaping_id b) {return a = static_cast<bcmbal_tm_shaping_id>(static_cast<int>(a) | static_cast<int>(b));}

dev_log_id openolt_log_id = bcm_dev_log_id_register("OPENOLT", DEV_LOG_LEVEL_INFO, DEV_LOG_ID_TYPE_BOTH);
dev_log_id omci_log_id = bcm_dev_log_id_register("OMCI", DEV_LOG_LEVEL_INFO, DEV_LOG_ID_TYPE_BOTH);

#define MAX_SUPPORTED_INTF 16
#define BAL_RSC_MANAGER_BASE_TM_SCHED_ID 16384

static unsigned int num_of_nni_ports = 0;
static unsigned int num_of_pon_ports = 0;
static std::string intf_technologies[MAX_SUPPORTED_INTF];
static const std::string UNKNOWN_TECH("unknown");
static const std::string MIXED_TECH("mixed");
static std::string board_technology(UNKNOWN_TECH);

static std::string firmware_version = "Openolt.2018.10.04";

const uint32_t tm_upstream_sched_id_start = 18432;
const uint32_t tm_downstream_sched_id_start = 16384;
const uint32_t tm_queue_id_start = 4; //0 to 3 are default queues. Lets not use them.
const std::string upstream = "upstream";
const std::string downstream = "downstream";

State state;

static std::map<uint32_t, uint32_t> flowid_to_port; // For mapping upstream flows to logical ports
static std::map<uint32_t, uint32_t> flowid_to_gemport; // For mapping downstream flows into gemports
static std::map<uint32_t, std::set<uint32_t> > port_to_flows; // For mapping logical ports to downstream flows
static std::map<uint32_t, uint32_t> port_to_alloc;
static bcmos_fastlock flow_lock;

#define MIN_ALLOC_ID_GPON 256
#define MIN_ALLOC_ID_XGSPON 1024

Status SchedAdd_(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t port_no, 
                 uint32_t alloc_id, openolt::AdditionalBW additional_bw, uint32_t weight, uint32_t priority,
                 openolt::SchedulingPolicy sched_policy);
static Status SchedRemove_(std::string direction, int intf_id, int onu_id, int uni_id, uint32_t port_no, int alloc_id);

static inline int mk_sched_id(int intf_id, int onu_id, std::string direction) {
    if (direction.compare(upstream) == 0) {
        return tm_upstream_sched_id_start + intf_id;
    } else if (direction.compare(downstream) == 0) {
        return tm_downstream_sched_id_start + intf_id;
    }
    else {
        BCM_LOG(ERROR, openolt_log_id, "invalid direction - %s\n", direction.c_str());
        return 0;
    }
}

static inline int mk_queue_id(int pon_intf_id, int onu_id, int uni_id, uint32_t port_no, uint32_t alloc_id) {
    (void) uni_id; // unused
    if(port_no == 0) return tm_queue_id_start + pon_intf_id * 32 + onu_id; // old style

    int start = intf_technologies[pon_intf_id] == "gpon" ? MIN_ALLOC_ID_GPON : MIN_ALLOC_ID_XGSPON; // offset built into all alloc Ids
    return tm_queue_id_start + alloc_id - start; // use alloc_id as a unique queue id. It is unique per UNI, and in the range of IDs supported by BAL
}

static inline int mk_agg_port_id(int intf_id, int onu_id) {
    if (board_technology == "gpon") return 511 + intf_id * 32 + onu_id;
    return 1023 + intf_id * 32 + onu_id;
}

Status GetDeviceInfo_(openolt::DeviceInfo* device_info) {
    device_info->set_vendor(VENDOR_ID);
    device_info->set_model(MODEL_ID);
    device_info->set_hardware_version("");
    device_info->set_firmware_version(firmware_version);
    device_info->set_technology(board_technology);
    device_info->set_pon_ports(num_of_pon_ports);

    // Legacy, device-wide ranges. To be deprecated when adapter
    // is upgraded to support per-interface ranges
    if (board_technology == "xgspon") {
        device_info->set_onu_id_start(1);
        device_info->set_onu_id_end(255);
        device_info->set_alloc_id_start(MIN_ALLOC_ID_XGSPON);
        device_info->set_alloc_id_end(16383);
        device_info->set_gemport_id_start(1024);
        device_info->set_gemport_id_end(65535);
        device_info->set_flow_id_start(1);
        device_info->set_flow_id_end(16383);
    }
    else if (board_technology == "gpon") {
        device_info->set_onu_id_start(1);
        device_info->set_onu_id_end(127);
        device_info->set_alloc_id_start(MIN_ALLOC_ID_GPON);
        device_info->set_alloc_id_end(767);
        device_info->set_gemport_id_start(256);
        device_info->set_gemport_id_end(4095);
        device_info->set_flow_id_start(1);
        device_info->set_flow_id_end(16383);
    }

    std::map<std::string, openolt::DeviceInfo::DeviceResourceRanges*> ranges;
    for (uint32_t intf_id = 0; intf_id < num_of_pon_ports; ++intf_id) {
        std::string intf_technology = intf_technologies[intf_id];
        openolt::DeviceInfo::DeviceResourceRanges *range = ranges[intf_technology];
        if(range == nullptr) {
            range = device_info->add_ranges();
            ranges[intf_technology] = range;
            range->set_technology(intf_technology);

            if (intf_technology == "xgspon") {
                openolt::DeviceInfo::DeviceResourceRanges::Pool* pool;

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ONU_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1);
                pool->set_end(255);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ALLOC_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_SAME_TECH);
                pool->set_start(1024);
                pool->set_end(16383);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::GEMPORT_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_ALL_TECH);
                pool->set_start(1024);
                pool->set_end(65535);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::FLOW_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_ALL_TECH);
                pool->set_start(1);
                pool->set_end(16383);
            }
            else if (intf_technology == "gpon") {
                openolt::DeviceInfo::DeviceResourceRanges::Pool* pool;

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ONU_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1);
                pool->set_end(127);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ALLOC_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_SAME_TECH);
                pool->set_start(256);
                pool->set_end(757);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::GEMPORT_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_ALL_TECH);
                pool->set_start(256);
                pool->set_end(4095);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::FLOW_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_ALL_TECH);
                pool->set_start(1);
                pool->set_end(16383);
            }
        }

        range->add_intf_ids(intf_id);
    }

    // FIXME: Once dependency problem is fixed
    // device_info->set_pon_ports(num_of_pon_ports);
    // device_info->set_onu_id_end(XGPON_NUM_OF_ONUS - 1);
    // device_info->set_alloc_id_start(1024);
    // device_info->set_alloc_id_end(XGPON_NUM_OF_ALLOC_IDS * num_of_pon_ports ? - 1);
    // device_info->set_gemport_id_start(XGPON_MIN_BASE_SERVICE_PORT_ID);
    // device_info->set_gemport_id_end(XGPON_NUM_OF_GEM_PORT_IDS_PER_PON * num_of_pon_ports ? - 1);
    // device_info->set_pon_ports(num_of_pon_ports);

    return Status::OK;
}

Status Enable_(int argc, char *argv[]) {
    bcmbal_access_terminal_cfg acc_term_obj;
    bcmbal_access_terminal_key key = { };

    if (!state.is_activated()) {

        vendor_init();
        bcmbal_init(argc, argv, NULL);
        bcmos_fastlock_init(&flow_lock, 0);

        BCM_LOG(INFO, openolt_log_id, "Enable OLT - %s-%s\n", VENDOR_ID, MODEL_ID);

        Status status = SubscribeIndication();
        if (!status.ok()) {
            BCM_LOG(ERROR, openolt_log_id, "SubscribeIndication failed - %s : %s\n",
                grpc_status_code_to_string(status.error_code()).c_str(),
                status.error_message().c_str());

            return status;
        }

        key.access_term_id = DEFAULT_ATERM_ID;
        BCMBAL_CFG_INIT(&acc_term_obj, access_terminal, key);
        BCMBAL_CFG_PROP_SET(&acc_term_obj, access_terminal, admin_state, BCMBAL_STATE_UP);
        bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(acc_term_obj.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to enable OLT\n");
            return bcm_to_grpc_err(err, "Failed to enable OLT");
        }

        init_stats();
    }

    //If already enabled, generate an extra indication ????
    return Status::OK;
}

Status Disable_() {
    // bcmbal_access_terminal_cfg acc_term_obj;
    // bcmbal_access_terminal_key key = { };
    //
    // if (state::is_activated) {
    //     std::cout << "Disable OLT" << std::endl;
    //     key.access_term_id = DEFAULT_ATERM_ID;
    //     BCMBAL_CFG_INIT(&acc_term_obj, access_terminal, key);
    //     BCMBAL_CFG_PROP_SET(&acc_term_obj, access_terminal, admin_state, BCMBAL_STATE_DOWN);
    //     bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(acc_term_obj.hdr));
    //     if (err) {
    //         std::cout << "ERROR: Failed to disable OLT" << std::endl;
    //         return bcm_to_grpc_err(err, "Failed to disable OLT");
    //     }
    // }
    // //If already disabled, generate an extra indication ????
    // return Status::OK;
    //This fails with Operation Not Supported, bug ???

    //TEMPORARY WORK AROUND
    Status status = DisableUplinkIf_(0);
    if (status.ok()) {
        state.deactivate();
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("down");
        ind.set_allocated_olt_ind(olt_ind);
        BCM_LOG(INFO, openolt_log_id, "Disable OLT, add an extra indication\n");
        oltIndQ.push(ind);
    }
    return status;

}

Status Reenable_() {
    Status status = EnableUplinkIf_(0);
    if (status.ok()) {
        state.activate();
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("up");
        ind.set_allocated_olt_ind(olt_ind);
        BCM_LOG(INFO, openolt_log_id, "Reenable OLT, add an extra indication\n");
        oltIndQ.push(ind);
    }
    return status;
}

Status EnablePonIf_(uint32_t intf_id) {
    bcmbal_interface_cfg interface_obj;
    bcmbal_interface_key interface_key;

    interface_key.intf_id = intf_id;
    interface_key.intf_type = BCMBAL_INTF_TYPE_PON;

    BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);

    BCMBAL_CFG_PROP_GET(&interface_obj, interface, admin_state);
    bcmos_errno err = bcmbal_cfg_get(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err == BCM_ERR_OK && interface_obj.data.admin_state == BCMBAL_STATE_UP) {
        BCM_LOG(DEBUG, openolt_log_id, "PON interface: %d already enabled\n", intf_id);
        return Status::OK;
    }

    BCMBAL_CFG_PROP_SET(&interface_obj, interface, admin_state, BCMBAL_STATE_UP);

    err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable PON interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to enable PON interface");
    }

    return Status::OK;
}

Status DisableUplinkIf_(uint32_t intf_id) {
    bcmbal_interface_cfg interface_obj;
    bcmbal_interface_key interface_key;

    interface_key.intf_id = intf_id;
    interface_key.intf_type = BCMBAL_INTF_TYPE_NNI;

    BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);
    BCMBAL_CFG_PROP_SET(&interface_obj, interface, admin_state, BCMBAL_STATE_DOWN);

    bcmos_errno err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to disable Uplink interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to disable Uplink interface");
    }

    return Status::OK;
}

Status ProbeDeviceCapabilities_() {
    bcmbal_access_terminal_cfg acc_term_obj;
    bcmbal_access_terminal_key key = { };

    key.access_term_id = DEFAULT_ATERM_ID;
    BCMBAL_CFG_INIT(&acc_term_obj, access_terminal, key);
    BCMBAL_CFG_PROP_GET(&acc_term_obj, access_terminal, admin_state);
    BCMBAL_CFG_PROP_GET(&acc_term_obj, access_terminal, oper_status);
    BCMBAL_CFG_PROP_GET(&acc_term_obj, access_terminal, topology);
    BCMBAL_CFG_PROP_GET(&acc_term_obj, access_terminal, sw_version);
    BCMBAL_CFG_PROP_GET(&acc_term_obj, access_terminal, conn_id);
    bcmos_errno err = bcmbal_cfg_get(DEFAULT_ATERM_ID, &(acc_term_obj.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to query OLT\n");
        return bcm_to_grpc_err(err, "Failed to query OLT");
    }

    BCM_LOG(INFO, openolt_log_id, "OLT capabilitites, admin_state: %s oper_state: %s\n", 
            acc_term_obj.data.admin_state == BCMBAL_STATE_UP ? "up" : "down",
            acc_term_obj.data.oper_status == BCMBAL_STATUS_UP ? "up" : "down");

    std::string bal_version;
    bal_version += std::to_string(acc_term_obj.data.sw_version.major_rev)
                + "." + std::to_string(acc_term_obj.data.sw_version.minor_rev)
                + "." + std::to_string(acc_term_obj.data.sw_version.release_rev);
    firmware_version = "BAL." + bal_version + "__" + firmware_version;

    BCM_LOG(INFO, openolt_log_id, "--------------- version %s object model: %d\n", bal_version.c_str(),
            acc_term_obj.data.sw_version.om_version);

    BCM_LOG(INFO, openolt_log_id, "--------------- topology nni:%d pon:%d dev:%d ppd:%d family: %d:%d\n",
            acc_term_obj.data.topology.num_of_nni_ports,
            acc_term_obj.data.topology.num_of_pon_ports,
            acc_term_obj.data.topology.num_of_mac_devs,
            acc_term_obj.data.topology.num_of_pons_per_mac_dev,
            acc_term_obj.data.topology.pon_family,
            acc_term_obj.data.topology.pon_sub_family
            );

    switch(acc_term_obj.data.topology.pon_sub_family)
    {
    case BCMBAL_PON_SUB_FAMILY_GPON:  board_technology = "gpon"; break;
    case BCMBAL_PON_SUB_FAMILY_XGS:   board_technology = "xgspon"; break;
    }

    num_of_nni_ports = acc_term_obj.data.topology.num_of_nni_ports;
    num_of_pon_ports = acc_term_obj.data.topology.num_of_pon_ports;

    BCM_LOG(INFO, openolt_log_id, "PON num_intfs: %d global board_technology: %s\n", num_of_pon_ports, board_technology.c_str());

    return Status::OK;
}

Status ProbePonIfTechnology_() {
    // Probe maximum extent possible as configured into BAL driver to determine
    // which are active in the current BAL topology. And for those
    // that are active, determine each port's access technology, i.e. "gpon" or "xgspon".
    for (uint32_t intf_id = 0; intf_id < num_of_pon_ports; ++intf_id) {
        bcmbal_interface_cfg interface_obj;
        bcmbal_interface_key interface_key;

        interface_key.intf_id = intf_id;
        interface_key.intf_type = BCMBAL_INTF_TYPE_PON;

        BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);
        BCMBAL_CFG_PROP_GET(&interface_obj, interface, admin_state);
        BCMBAL_CFG_PROP_GET(&interface_obj, interface, transceiver_type);

        bcmos_errno err = bcmbal_cfg_get(DEFAULT_ATERM_ID, &(interface_obj.hdr));
        if (err != BCM_ERR_OK) {
            intf_technologies[intf_id] = UNKNOWN_TECH;
            if(err != BCM_ERR_RANGE) BCM_LOG(ERROR, openolt_log_id, "Failed to get PON config: %d\n", intf_id);
        }
        else {
            switch(interface_obj.data.transceiver_type) {
            case BCMBAL_TRX_TYPE_GPON_SPS_43_48:
            case BCMBAL_TRX_TYPE_GPON_SPS_SOG_4321:
            case BCMBAL_TRX_TYPE_GPON_LTE_3680_M:
            case BCMBAL_TRX_TYPE_GPON_SOURCE_PHOTONICS:
            case BCMBAL_TRX_TYPE_GPON_LTE_3680_P:
                intf_technologies[intf_id] = "gpon";
                break;
            default:
                intf_technologies[intf_id] = "xgspon";
                break;
            }
            BCM_LOG(INFO, openolt_log_id, "PON intf_id: %d intf_technologies: %d:%s\n", intf_id,
                    interface_obj.data.transceiver_type, intf_technologies[intf_id].c_str());

            if (board_technology != UNKNOWN_TECH) {
                board_technology = intf_technologies[intf_id];
            } else if (board_technology != MIXED_TECH && board_technology != intf_technologies[intf_id]) {
                intf_technologies[intf_id] = MIXED_TECH;
            }

        }
    }

    return Status::OK;
}

unsigned NumNniIf_() {return num_of_nni_ports;}
unsigned NumPonIf_() {return num_of_pon_ports;}

Status EnableUplinkIf_(uint32_t intf_id) {
    bcmbal_interface_cfg interface_obj;
    bcmbal_interface_key interface_key;

    interface_key.intf_id = intf_id;
    interface_key.intf_type = BCMBAL_INTF_TYPE_NNI;

    BCMBAL_CFG_INIT(&interface_obj, interface, interface_key);

    BCMBAL_CFG_PROP_GET(&interface_obj, interface, admin_state);
    bcmos_errno err = bcmbal_cfg_get(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err == BCM_ERR_OK && interface_obj.data.admin_state == BCMBAL_STATE_UP) {
        BCM_LOG(DEBUG, openolt_log_id, "Uplink interface: %d already enabled\n", intf_id);
        return Status::OK;
    }

    BCMBAL_CFG_PROP_SET(&interface_obj, interface, admin_state, BCMBAL_STATE_UP);

    err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(interface_obj.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable Uplink interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to enable Uplink interface");
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
        BCM_LOG(ERROR, openolt_log_id, "Failed to disable PON interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to disable PON interface");
    }

    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir) {

    bcmbal_subscriber_terminal_cfg sub_term_obj = {};
    bcmbal_subscriber_terminal_key subs_terminal_key;
    bcmbal_serial_number serial_num = {};
    bcmbal_registration_id registration_id = {};

    BCM_LOG(INFO, openolt_log_id,  "Enabling ONU %d on PON %d : vendor id %s, vendor specific %s, pir %d\n",
        onu_id, intf_id, vendor_id, vendor_specific_to_str(vendor_specific).c_str(), pir);

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
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable ONU %d on PON %d\n", onu_id, intf_id);
        return bcm_to_grpc_err(err, "Failed to enable ONU");
    }
    return Status::OK;
}

Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {

    bcmbal_subscriber_terminal_cfg sub_term_obj = {};
    bcmbal_subscriber_terminal_key subs_terminal_key;

    BCM_LOG(INFO, openolt_log_id,  "Deactivating ONU %d on PON %d : vendor id %s, vendor specific %s\n",
        onu_id, intf_id, vendor_id, vendor_specific_to_str(vendor_specific).c_str());

    subs_terminal_key.sub_term_id = onu_id;
    subs_terminal_key.intf_id = intf_id;
    BCMBAL_CFG_INIT(&sub_term_obj, subscriber_terminal, subs_terminal_key);

    BCMBAL_CFG_PROP_SET(&sub_term_obj, subscriber_terminal, admin_state, BCMBAL_STATE_DOWN);

    if (bcmbal_cfg_set(DEFAULT_ATERM_ID, &(sub_term_obj.hdr))) {
        BCM_LOG(ERROR, openolt_log_id,  "Failed to deactivate ONU %d on PON %d\n", onu_id, intf_id);
        return Status(grpc::StatusCode::INTERNAL, "Failed to deactivate ONU");
    }

    return Status::OK;
}

Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {

    BCM_LOG(INFO, openolt_log_id,  "DeleteOnu ONU %d on PON %d : vendor id %s, vendor specific %s\n",
        onu_id, intf_id, vendor_id, vendor_specific_to_str(vendor_specific).c_str());

    // Need to deactivate before removing it (BAL rules)

    DeactivateOnu_(intf_id, onu_id, vendor_id, vendor_specific);
    // Sleep to allow the state to propagate
    // We need the subscriber terminal object to be admin down before removal
    // Without sleep the race condition is lost by ~ 20 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // TODO: Delete the schedulers and queues.

    bcmos_errno err = BCM_ERR_OK;
    bcmbal_subscriber_terminal_cfg cfg;
    bcmbal_subscriber_terminal_key key = { };

    BCM_LOG(INFO, openolt_log_id, "Processing subscriber terminal cfg clear for sub_term_id %d  and intf_id %d\n",
        onu_id, intf_id);

    key.sub_term_id = onu_id ;
    key.intf_id = intf_id ;

    if (0 == key.sub_term_id)
    {
            BCM_LOG(INFO, openolt_log_id,"Invalid Key to handle subscriber terminal clear subscriber_terminal_id %d, Interface ID %d\n",
                onu_id, intf_id);
            return Status(grpc::StatusCode::INTERNAL, "Failed to delete ONU");
    }

    BCMBAL_CFG_INIT(&cfg, subscriber_terminal, key);

    err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &cfg.hdr);
    if (err != BCM_ERR_OK)
    {
       BCM_LOG(ERROR, openolt_log_id, "Failed to clear information for BAL subscriber_terminal_id %d, Interface ID %d\n",
            onu_id, intf_id);
        return Status(grpc::StatusCode::INTERNAL, "Failed to delete ONU");
    }

    return Status::OK;;
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

    for (idx1=0,idx2=0; idx1<((buf.len)*2); idx1++,idx2++) {
       sprintf(str1,"%c", pkt[idx1]);
       sprintf(str2,"%c", pkt[++idx1]);
       strcat(str1,str2);
       arraySend[idx2] = strtol(str1, NULL, 16);
    }

    buf.val = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.val, (uint8_t *)arraySend, buf.len);

    err = bcmbal_pkt_send(0, proxy_pkt_dest, (const char *)(buf.val), buf.len);

    if (err) {
        BCM_LOG(ERROR, omci_log_id, "Error sending OMCI message to ONU %d on PON %d\n", onu_id, intf_id);
    } else {
        BCM_LOG(DEBUG, omci_log_id, "OMCI request msg of length %d sent to ONU %d on PON %d : %s\n",
            buf.len, onu_id, intf_id, pkt.c_str());
    }

    free(buf.val);

    return Status::OK;
}

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, const std::string pkt) {
    bcmos_errno err = BCM_ERR_OK;
    bcmbal_dest proxy_pkt_dest;
    bcmbal_u8_list_u32_max_2048 buf;

    if (port_no > 0) {
        bool found = false;
        uint32_t gemport_id;

        bcmos_fastlock_lock(&flow_lock);
        // Map the port_no to one of the flows that owns it to find a gemport_id for that flow.
        // Pick any flow that is mapped with the same port_no.
        std::map<uint32_t, std::set<uint32_t> >::const_iterator it = port_to_flows.find(port_no);
        if (it != port_to_flows.end() && !it->second.empty()) {
            uint32_t flow_id = *(it->second.begin()); // Pick any flow_id out of the bag set
            std::map<uint32_t, uint32_t>::const_iterator fit = flowid_to_gemport.find(flow_id);
            if (fit != flowid_to_gemport.end()) {
                found = true;
                gemport_id = fit->second;
            }
        }
        bcmos_fastlock_unlock(&flow_lock, 0);

        if (!found) {
            BCM_LOG(ERROR, openolt_log_id, "Packet out failed to find destination for ONU %d port_no %u on PON %d\n",
                onu_id, port_no, intf_id);
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "no flow for port_no");
        }

        proxy_pkt_dest.type = BCMBAL_DEST_TYPE_SVC_PORT;
        proxy_pkt_dest.u.svc_port.svc_port_id = gemport_id;
        proxy_pkt_dest.u.svc_port.intf_id = intf_id;
        BCM_LOG(INFO, openolt_log_id, "Packet out of length %d sent to gemport %d on pon %d port_no %u\n",
            pkt.size(), gemport_id, intf_id, port_no);
    }
    else {
        proxy_pkt_dest.type = BCMBAL_DEST_TYPE_SUB_TERM,
        proxy_pkt_dest.u.sub_term.sub_term_id = onu_id;
        proxy_pkt_dest.u.sub_term.intf_id = intf_id;
        BCM_LOG(INFO, openolt_log_id, "Packet out of length %d sent to onu %d on pon %d\n",
            pkt.size(), onu_id, intf_id);
    }

    buf.len = pkt.size();
    buf.val = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.val, (uint8_t *)pkt.data(), buf.len);

    err = bcmbal_pkt_send(0, proxy_pkt_dest, (const char *)(buf.val), buf.len);

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

    BCM_LOG(INFO, openolt_log_id, "Packet out of length %d sent through uplink port %d\n",
        buf.len, intf_id);

    free(buf.val);

    return Status::OK;
}

uint32_t GetPortNum_(uint32_t flow_id)
{
    bcmos_fastlock_lock(&flow_lock);
    uint32_t port_no = 0;
    std::map<uint32_t, uint32_t >::const_iterator it = flowid_to_port.find(flow_id);
    if (it != flowid_to_port.end()) {
        port_no = it->second;
    }
    bcmos_fastlock_unlock(&flow_lock, 0);
    return port_no;
}

Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value, uint64_t cookie) {
    bcmos_errno err;
    bcmbal_flow_cfg cfg;
    bcmbal_flow_key key = { };

    BCM_LOG(INFO, openolt_log_id, "flow add - intf_id %d, onu_id %d, uni_id %d, port_no %u, flow_id %d, flow_type %s, gemport_id %d, network_intf_id %d, cookie %llu\n",
        access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type.c_str(), gemport_id, network_intf_id, cookie);

    key.flow_id = flow_id;
    if (flow_type.compare("upstream") == 0 ) {
        key.flow_type = BCMBAL_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare("downstream") == 0) {
        key.flow_type = BCMBAL_FLOW_TYPE_DOWNSTREAM;
    } else {
        BCM_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    BCMBAL_CFG_INIT(&cfg, flow, key);

    BCMBAL_CFG_PROP_SET(&cfg, flow, admin_state, BCMBAL_STATE_UP);
    BCMBAL_CFG_PROP_SET(&cfg, flow, cookie, cookie);

    if (access_intf_id >= 0) {
        BCMBAL_CFG_PROP_SET(&cfg, flow, access_int_id, access_intf_id);
    }
    if (network_intf_id >= 0) {
        BCMBAL_CFG_PROP_SET(&cfg, flow, network_int_id, network_intf_id);
    }
    if (onu_id >= 0) {
        BCMBAL_CFG_PROP_SET(&cfg, flow, sub_term_id, onu_id);
    }
    if (gemport_id >= 0) {
        BCMBAL_CFG_PROP_SET(&cfg, flow, svc_port_id, gemport_id);
    }
    if (gemport_id >= 0 && port_no != 0) {
        bcmos_fastlock_lock(&flow_lock);
        if (key.flow_type == BCMBAL_FLOW_TYPE_DOWNSTREAM) {
            port_to_flows[port_no].insert(key.flow_id);
            flowid_to_gemport[key.flow_id] = gemport_id;
        }
        else
        {
            flowid_to_port[key.flow_id] = port_no;
        }
        bcmos_fastlock_unlock(&flow_lock, 0);
    }
    if (priority_value >= 0) {
        BCMBAL_CFG_PROP_SET(&cfg, flow, priority, priority_value);
    }

    {
        bcmbal_classifier val = { };

        if (classifier.o_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify o_tpid 0x%04x\n", classifier.o_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_tpid, classifier.o_tpid());
        }

        if (classifier.o_vid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify o_vid %d\n", classifier.o_vid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_vid, classifier.o_vid());
        }

        if (classifier.i_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify i_tpid 0x%04x\n", classifier.i_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, i_tpid, classifier.i_tpid());
        }

        if (classifier.i_vid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify i_vid %d\n", classifier.i_vid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, i_vid, classifier.i_vid());
        }

        if (classifier.o_pbits()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_pbits, classifier.o_pbits());
        }

        if (classifier.i_pbits()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify i_pbits 0x%x\n", classifier.i_pbits());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, i_pbits, classifier.i_pbits());
        }

        if (classifier.eth_type()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify ether_type 0x%04x\n", classifier.eth_type());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, ether_type, classifier.eth_type());
        }

        /*
        if (classifier.dst_mac()) {
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, dst_mac, classifier.dst_mac());
        }

        if (classifier.src_mac()) {
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, src_mac, classifier.src_mac());
        }
        */

        if (classifier.ip_proto()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify ip_proto %d\n", classifier.ip_proto());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, ip_proto, classifier.ip_proto());
        }

        /*
        if (classifier.dst_ip()) {
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, dst_ip, classifier.dst_ip());
        }

        if (classifier.src_ip()) {
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, src_ip, classifier.src_ip());
        }
        */

        if (classifier.src_port()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify src_port %d\n", classifier.src_port());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, src_port, classifier.src_port());
        }

        if (classifier.dst_port()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify dst_port %d\n", classifier.dst_port());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, dst_port, classifier.dst_port());
        }

        if (!classifier.pkt_tag_type().empty()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify tag_type %s\n", classifier.pkt_tag_type().c_str());
            if (classifier.pkt_tag_type().compare("untagged") == 0) {
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, pkt_tag_type, BCMBAL_PKT_TAG_TYPE_UNTAGGED);
            } else if (classifier.pkt_tag_type().compare("single_tag") == 0) {
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, pkt_tag_type, BCMBAL_PKT_TAG_TYPE_SINGLE_TAG);
            } else if (classifier.pkt_tag_type().compare("double_tag") == 0) {
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, pkt_tag_type, BCMBAL_PKT_TAG_TYPE_DOUBLE_TAG);
            }
        }

        BCMBAL_CFG_PROP_SET(&cfg, flow, classifier, val);
    }

    {
        bcmbal_action val = { };

        const ::openolt::ActionCmd& cmd = action.cmd();

        if (cmd.add_outer_tag()) {
            BCM_LOG(INFO, openolt_log_id, "action add o_tag\n");
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, cmds_bitmask, BCMBAL_ACTION_CMD_ID_ADD_OUTER_TAG);
        }

        if (cmd.remove_outer_tag()) {
            BCM_LOG(INFO, openolt_log_id, "action pop o_tag\n");
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, cmds_bitmask, BCMBAL_ACTION_CMD_ID_REMOVE_OUTER_TAG);
        }

        if (cmd.trap_to_host()) {
            BCM_LOG(INFO, openolt_log_id, "action trap-to-host\n");
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, cmds_bitmask, BCMBAL_ACTION_CMD_ID_TRAP_TO_HOST);
        }

        if (action.o_vid()) {
            BCM_LOG(INFO, openolt_log_id, "action o_vid=%d\n", action.o_vid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, o_vid, action.o_vid());
        }

        if (action.o_pbits()) {
            BCM_LOG(INFO, openolt_log_id, "action o_pbits=0x%x\n", action.o_pbits());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, o_pbits, action.o_pbits());
        }

        if (action.o_tpid()) {
            BCM_LOG(INFO, openolt_log_id, "action o_tpid=0x%04x\n", action.o_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, o_tpid, action.o_tpid());
        }

        if (action.i_vid()) {
            BCM_LOG(INFO, openolt_log_id, "action i_vid=%d\n", action.i_vid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, i_vid, action.i_vid());
        }

        if (action.i_pbits()) {
            BCM_LOG(DEBUG, openolt_log_id, "action i_pbits=0x%x\n", action.i_pbits());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, i_pbits, action.i_pbits());
        }

        if (action.i_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "action i_tpid=0x%04x\n", action.i_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, i_tpid, action.i_tpid());
        }

        BCMBAL_CFG_PROP_SET(&cfg, flow, action, val);
    }

    if ((access_intf_id >= 0) && (onu_id >= 0)) {

        if (key.flow_type == BCMBAL_FLOW_TYPE_DOWNSTREAM) {
            bcmbal_tm_queue_ref val = { };
            val.sched_id = mk_sched_id(access_intf_id, onu_id, "downstream");
            uint32_t alloc_id;
            bcmos_fastlock_lock(&flow_lock);
            alloc_id = port_to_alloc[port_no];
            bcmos_fastlock_unlock(&flow_lock, 0);
            val.queue_id = mk_queue_id(access_intf_id, onu_id, uni_id, port_no, alloc_id);
            BCMBAL_CFG_PROP_SET(&cfg, flow, queue, val);
        } else if (key.flow_type == BCMBAL_FLOW_TYPE_UPSTREAM) {
            bcmbal_tm_sched_id val1;
            if (alloc_id != 0) {
                val1 = alloc_id;
            } else {
                BCM_LOG(ERROR, openolt_log_id, "alloc_id not present");
            }
            BCMBAL_CFG_PROP_SET(&cfg, flow, dba_tm_sched_id, val1);

            bcmbal_tm_queue_ref val2 = { };
            val2.sched_id = mk_sched_id(network_intf_id, onu_id, "upstream");
            BCMBAL_CFG_PROP_SET(&cfg, flow, queue, val2);
        }
    }

    err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id,  "Flow add failed\n");
        return bcm_to_grpc_err(err, "flow add failed");
    }

    // register_new_flow(key);

    return Status::OK;
}

Status FlowRemove_(uint32_t flow_id, const std::string flow_type) {

    bcmbal_flow_cfg cfg;
    bcmbal_flow_key key = { };

    key.flow_id = (bcmbal_flow_id) flow_id;
    key.flow_id = flow_id;
    if (flow_type.compare("upstream") == 0 ) {
        key.flow_type = BCMBAL_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare("downstream") == 0) {
        key.flow_type = BCMBAL_FLOW_TYPE_DOWNSTREAM;
    } else {
        BCM_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    bcmos_fastlock_lock(&flow_lock);
    uint32_t port_no = flowid_to_port[key.flow_id];
    if (key.flow_type == BCMBAL_FLOW_TYPE_DOWNSTREAM) {
        flowid_to_gemport.erase(key.flow_id);
        port_to_flows[port_no].erase(key.flow_id);
        if (port_to_flows[port_no].empty()) port_to_flows.erase(port_no);
    }
    else
    {
        flowid_to_port.erase(key.flow_id);
    }
    bcmos_fastlock_unlock(&flow_lock, 0);

    BCMBAL_CFG_INIT(&cfg, flow, key);


    bcmos_errno err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &cfg.hdr);
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Error %d while removing flow %d, %s\n",
            err, flow_id, flow_type.c_str());
        return Status(grpc::StatusCode::INTERNAL, "Failed to remove flow");
    }

    BCM_LOG(INFO, openolt_log_id, "Flow %d, %s removed\n", flow_id, flow_type.c_str());
    return Status::OK;
}

Status SchedAdd_(std::string direction, uint32_t intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t port_no,
                 uint32_t alloc_id, openolt::AdditionalBW additional_bw, uint32_t weight, uint32_t priority,
                 openolt::SchedulingPolicy sched_policy) {

    bcmos_errno err;

    if (direction == "downstream") {
        bcmbal_tm_queue_cfg cfg;
        bcmbal_tm_queue_key key = { };
        // Note: We use the default scheduler available in the DL.
        key.sched_id = mk_sched_id(intf_id, onu_id, direction);
        key.sched_dir = BCMBAL_TM_SCHED_DIR_DS;
        key.id = mk_queue_id(intf_id, onu_id, uni_id, port_no, alloc_id);

        BCMBAL_CFG_INIT(&cfg, tm_queue, key);
        //Queue must be set with either weight or priority, not both,
        // as its scheduler' sched_type is sp_wfq
        BCMBAL_CFG_PROP_SET(&cfg, tm_queue, priority, priority);
        //BCMBAL_CFG_PROP_SET(&cfg, tm_queue, weight, weight);
        //BCMBAL_CFG_PROP_SET(&cfg, tm_queue, creation_mode, BCMBAL_TM_CREATION_MODE_MANUAL);
        err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &cfg.hdr);

        // TODO: Shaping parameters will be available after meter bands are supported.
        // TODO: The shaping parameters will be applied on the downstream queue on the PON default scheduler.
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create subscriber downstream tm queue, id %d, sched_id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alt_id %d\n",
                    key.id, key.sched_id, intf_id, onu_id, uni_id, port_no, alloc_id);
            return bcm_to_grpc_err(err, "Failed to create subscriber downstream tm queue");
        }

        bcmos_fastlock_lock(&flow_lock);
        port_to_alloc[port_no] = alloc_id;
        bcmos_fastlock_unlock(&flow_lock, 0);

        BCM_LOG(INFO, openolt_log_id, "Create downstream sched, id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alt_id %d\n",
                key.id,intf_id,onu_id,uni_id,port_no,alloc_id);

    } else { //"upstream"
        bcmbal_tm_sched_cfg cfg;
        bcmbal_tm_sched_key key = { };
        bcmbal_tm_sched_type sched_type;

        key.id = alloc_id;
        key.dir = BCMBAL_TM_SCHED_DIR_US;

        BCMBAL_CFG_INIT(&cfg, tm_sched, key);

        {
            bcmbal_tm_sched_owner val = { };

            val.type = BCMBAL_TM_SCHED_OWNER_TYPE_AGG_PORT;
            BCMBAL_ATTRIBUTE_PROP_SET(&val.u.agg_port, tm_sched_owner_agg_port, intf_id, (bcmbal_intf_id) intf_id);
            BCMBAL_ATTRIBUTE_PROP_SET(&val.u.agg_port, tm_sched_owner_agg_port, sub_term_id, (bcmbal_sub_id) onu_id);
            BCMBAL_ATTRIBUTE_PROP_SET(&val.u.agg_port, tm_sched_owner_agg_port, agg_port_id, (bcmbal_aggregation_port_id) alloc_id);

            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, owner, val);

        }
        // TODO: Shaping parameters will be available after meter bands are supported.
        // TODO: The shaping parameters will be applied on the upstream DBA scheduler.

        err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create upstream DBA sched, id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alloc_id %d\n",
                    key.id, intf_id, onu_id,uni_id,port_no,alloc_id);
            return bcm_to_grpc_err(err, "Failed to create upstream DBA sched");
        }
        BCM_LOG(INFO, openolt_log_id, "Create upstream DBA sched, id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alloc_id %d\n",
                key.id,intf_id,onu_id,uni_id,port_no,alloc_id);
    }

    return Status::OK;

}

Status CreateTconts_(const openolt::Tconts *tconts) {
    uint32_t intf_id = tconts->intf_id();
    uint32_t onu_id = tconts->onu_id();
    uint32_t uni_id = tconts->uni_id();
    uint32_t port_no = tconts->port_no();
    std::string direction;
    unsigned int alloc_id;
    openolt::Scheduler scheduler;
    openolt::AdditionalBW additional_bw;
    uint32_t priority;
    uint32_t weight;
    openolt::SchedulingPolicy sched_policy;

    for (int i = 0; i < tconts->tconts_size(); i++) {
        openolt::Tcont tcont = tconts->tconts(i);
        if (tcont.direction() == openolt::Direction::UPSTREAM) {
            direction = "upstream";
        } else if (tcont.direction() == openolt::Direction::DOWNSTREAM) {
            direction = "downstream";
        }
        else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", tcont.direction());
            return Status::CANCELLED;
        }
        alloc_id = tcont.alloc_id();
        scheduler = tcont.scheduler();
        additional_bw = scheduler.additional_bw();
        priority = scheduler.priority();
        weight = scheduler.weight();
        sched_policy = scheduler.sched_policy();
        // TODO: TrafficShapingInfo is not supported for now as meter band support is not there
        SchedAdd_(direction, intf_id, onu_id, uni_id, port_no, alloc_id, additional_bw, weight, priority, sched_policy);
    }
    return Status::OK;
}

Status RemoveTconts_(const openolt::Tconts *tconts) {
    uint32_t intf_id = tconts->intf_id();
    uint32_t onu_id = tconts->onu_id();
    uint32_t uni_id = tconts->uni_id();
    uint32_t port_no = tconts->port_no();
    std::string direction;
    unsigned int alloc_id;

    for (int i = 0; i < tconts->tconts_size(); i++) {
        openolt::Tcont tcont = tconts->tconts(i);
        if (tcont.direction() == openolt::Direction::UPSTREAM) {
            direction = "upstream";
        } else if (tcont.direction() == openolt::Direction::DOWNSTREAM) {
            direction = "downstream";
        }
        else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", tcont.direction());
            return Status::CANCELLED;
        }
        alloc_id = tcont.alloc_id();
        SchedRemove_(direction, intf_id, onu_id, uni_id, port_no, alloc_id);
    }
    return Status::OK;
}

Status SchedRemove_(std::string direction, int intf_id, int onu_id, int uni_id, uint32_t port_no, int alloc_id) {

    bcmos_errno err;


    if (direction == "upstream") {
        // DBA sched
        bcmbal_tm_sched_cfg tm_cfg_us;
        bcmbal_tm_sched_key tm_key_us = { };

        tm_key_us.id = alloc_id;
        tm_key_us.dir = BCMBAL_TM_SCHED_DIR_US;

        BCMBAL_CFG_INIT(&tm_cfg_us, tm_sched, tm_key_us);

        err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &(tm_cfg_us.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to remove upstream DBA sched, id %d, intf_id %d, onu_id %d\n",
                tm_key_us.id, intf_id, onu_id);
            return Status(grpc::StatusCode::INTERNAL, "Failed to remove upstream DBA sched");
        }

        BCM_LOG(INFO, openolt_log_id, "Remove upstream DBA sched, id %d, intf_id %d, onu_id %d\n",
            tm_key_us.id, intf_id, onu_id);

    } else if (direction == "downstream") {
	    // Queue

	    bcmbal_tm_queue_cfg queue_cfg;
	    bcmbal_tm_queue_key queue_key = { };
	    queue_key.sched_id = mk_sched_id(intf_id, onu_id, "downstream");
	    queue_key.sched_dir = BCMBAL_TM_SCHED_DIR_DS;
	    queue_key.id = mk_queue_id(intf_id, onu_id, uni_id, port_no, alloc_id);

	    BCMBAL_CFG_INIT(&queue_cfg, tm_queue, queue_key);

	    err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &(queue_cfg.hdr));
	    if (err) {
		    BCM_LOG(ERROR, openolt_log_id, "Failed to remove downstream tm queue, id %d, sched_id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alt_id %d\n",
				    queue_key.id, queue_key.sched_id, intf_id, onu_id, uni_id, port_no, alloc_id);
		    return Status(grpc::StatusCode::INTERNAL, "Failed to remove downstream tm queue");
	    }

        bcmos_fastlock_lock(&flow_lock);
        port_to_alloc.erase(port_no);
        bcmos_fastlock_unlock(&flow_lock, 0);

	    BCM_LOG(INFO, openolt_log_id, "Remove upstream DBA sched, id %d, sched_id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, alt_id %d\n",
			    queue_key.id, queue_key.sched_id, intf_id, onu_id, uni_id, port_no, alloc_id);
    }

    return Status::OK;
}

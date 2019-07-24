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
#include <bitset>

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
#define MAX_TM_QUEUE_ID 8192
#define MAX_TM_SCHED_ID 16384
#define EAP_ETHER_TYPE 34958

static unsigned int num_of_nni_ports = 0;
static unsigned int num_of_pon_ports = 0;
static std::string intf_technologies[MAX_SUPPORTED_INTF];
static const std::string UNKNOWN_TECH("unknown");
static const std::string MIXED_TECH("mixed");
static std::string board_technology(UNKNOWN_TECH);
static unsigned int OPENOLT_FIELD_LEN = 200;
static std::string firmware_version = "Openolt.2018.10.04";

const uint32_t tm_upstream_sched_id_start = 18432;
const uint32_t tm_downstream_sched_id_start = 16384;
//0 to 3 are default queues. Lets not use them.
const uint32_t tm_queue_id_start = 4;
// Upto 8 fixed Upstream. Queue id 0 to 3 are pre-created, lets not use them.
const uint32_t us_fixed_queue_id_list[8] = {4, 5, 6, 7, 8, 9, 10, 11};
const std::string upstream = "upstream";
const std::string downstream = "downstream";

State state;

static std::map<uint32_t, uint32_t> flowid_to_port; // For mapping upstream flows to logical ports
static std::map<uint32_t, uint32_t> flowid_to_gemport; // For mapping downstream flows into gemports
static std::map<uint32_t, std::set<uint32_t> > port_to_flows; // For mapping logical ports to downstream flows

// This represents the Key to 'queue_map' map.
// Represents (pon_intf_id, onu_id, uni_id, gemport_id, direction)
typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, std::string> queue_map_key_tuple;
// 'queue_map' maps queue_map_key_tuple to downstream queue id present
// on the Subscriber Scheduler
static std::map<queue_map_key_tuple, int> queue_map;
// This represents the Key to 'sched_map' map.
// Represents (pon_intf_id, onu_id, uni_id, direction)

typedef std::tuple<uint32_t, uint32_t, uint32_t, std::string> sched_map_key_tuple;
// 'sched_map' maps sched_map_key_tuple to DBA (Upstream) or
// Subscriber (Downstream) Scheduler ID
static std::map<sched_map_key_tuple, int> sched_map;


std::bitset<MAX_TM_QUEUE_ID> tm_queue_bitset;
std::bitset<MAX_TM_SCHED_ID> tm_sched_bitset;

static bcmos_fastlock data_lock;

#define MIN_ALLOC_ID_GPON 256
#define MIN_ALLOC_ID_XGSPON 1024

static bcmos_errno CreateSched(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, \
                          uint32_t port_no, uint32_t alloc_id, tech_profile::AdditionalBW additional_bw, uint32_t weight, \
                          uint32_t priority, tech_profile::SchedulingPolicy sched_policy,
                          tech_profile::TrafficShapingInfo traffic_shaping_info);
static bcmos_errno RemoveSched(int intf_id, int onu_id, int uni_id, std::string direction);
static bcmos_errno CreateQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, \
                               uint32_t priority, uint32_t gemport_id);
static bcmos_errno RemoveQueue(std::string direction, int intf_id, int onu_id, int uni_id, uint32_t port_no, int alloc_id);

/**
* Returns the default NNI (Upstream direction) or PON (Downstream direction) scheduler
* Every NNI port and PON port have default scheduler.
* The NNI0 default scheduler ID is 18432, and NNI1 is 18433 and so on.
* Similarly, PON0 default scheduler ID is 16384. PON1 is 16385 and so on.
*
* @param intf_id NNI or PON interface ID
* @param direction "upstream" or "downstream"
*
* @return default scheduler ID for the given interface.
*/
static inline int get_default_tm_sched_id(int intf_id, std::string direction) {
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

/**
* Gets a unique tm_queue_id for a given intf_id, onu_id, uni_id, gemport_id, direction
* The tm_queue_id is locally cached in a map, so that it can rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
*
* @return tm_queue_id
*/
int get_tm_queue_id(int intf_id, int onu_id, int uni_id, int gemport_id, std::string direction) {
    queue_map_key_tuple key(intf_id, onu_id, uni_id, gemport_id, direction);
    int queue_id = -1;

    std::map<queue_map_key_tuple, int>::const_iterator it = queue_map.find(key);
    if (it != queue_map.end()) {
        queue_id = it->second;
    }
    if (queue_id != -1) {
        return queue_id;
    }

    bcmos_fastlock_lock(&data_lock);
    // Complexity of O(n). Is there better way that can avoid linear search?
    for (queue_id = 0; queue_id < MAX_TM_QUEUE_ID; queue_id++) {
        if (tm_queue_bitset[queue_id] == 0) {
            tm_queue_bitset[queue_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    if (queue_id < MAX_TM_QUEUE_ID) {
        bcmos_fastlock_lock(&data_lock);
        queue_map[key] = queue_id;
        bcmos_fastlock_unlock(&data_lock, 0);
        return queue_id;
    } else {
        return -1;
    }
}

/**
* Update tm_queue_id for a given intf_id, onu_id, uni_id, gemport_id, direction
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
* @param tm_queue_id tm_queue_id
*/
void update_tm_queue_id(int pon_intf_id, int onu_id, int uni_id, int gemport_id, std::string direction,
                                  uint32_t queue_id) {
    queue_map_key_tuple key(pon_intf_id, onu_id, uni_id, gemport_id, direction);
    bcmos_fastlock_lock(&data_lock);
    queue_map[key] = queue_id;
    bcmos_fastlock_unlock(&data_lock, 0);
}

/**
* Free tm_queue_id for a given intf_id, onu_id, uni_id, gemport_id, direction
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
*/
void free_tm_queue_id(int pon_intf_id, int onu_id, int uni_id, int gemport_id, std::string direction) {
    queue_map_key_tuple key(pon_intf_id, onu_id, uni_id, gemport_id, direction);
    std::map<queue_map_key_tuple, int>::const_iterator it;
    bcmos_fastlock_lock(&data_lock);
    it = queue_map.find(key);
    if (it != queue_map.end()) {
        tm_queue_bitset[it->second] = 0;
        queue_map.erase(it);
    }
    bcmos_fastlock_unlock(&data_lock, 0);
}

/**
* Gets a unique tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction
* The tm_sched_id is locally cached in a map, so that it can rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
*
* @return tm_sched_id
*/
uint32_t get_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction);
    int sched_id = -1;

    std::map<sched_map_key_tuple, int>::const_iterator it = sched_map.find(key);
    if (it != sched_map.end()) {
        sched_id = it->second;
    }
    if (sched_id != -1) {
        return sched_id;
    }

    bcmos_fastlock_lock(&data_lock);
    // Complexity of O(n). Is there better way that can avoid linear search?
    for (sched_id = 0; sched_id < MAX_TM_SCHED_ID; sched_id++) {
        if (tm_sched_bitset[sched_id] == 0) {
            tm_sched_bitset[sched_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    if (sched_id < MAX_TM_SCHED_ID) {
        bcmos_fastlock_lock(&data_lock);
        sched_map[key] = sched_id;
        bcmos_fastlock_unlock(&data_lock, 0);
        return sched_id;
    } else {
        return -1;
    }
}

/**
* Free tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
*/
void free_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction);
    std::map<sched_map_key_tuple, int>::const_iterator it;
    bcmos_fastlock_lock(&data_lock);
    it = sched_map.find(key);
    if (it != sched_map.end()) {
        tm_sched_bitset[it->second] = 0;
        sched_map.erase(it);
    }
    bcmos_fastlock_unlock(&data_lock, 0);
}

bool is_tm_sched_id_present(int pon_intf_id, int onu_id, int uni_id, std::string direction) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction);
    return sched_map.count(key) > 0 ? true: false;
}

bool is_tm_queue_id_present(int pon_intf_id, int onu_id, int uni_id, int gemport_id, std::string direction) {
    queue_map_key_tuple key(pon_intf_id, onu_id, uni_id, gemport_id, direction);
    return queue_map.count(key) > 0 ? true: false;
}

char* openolt_read_sysinfo(char* field_name, char* field_val)
{
   FILE *fp;
   /* Prepare the command*/
   char command[150];

   snprintf(command, sizeof command, "bash -l -c \"onlpdump -s\" | perl -ne 'print $1 if /%s: (\\S+)/'", field_name);
   /* Open the command for reading. */
   fp = popen(command, "r");
   if (fp == NULL) {
       /*The client has to check for a Null field value in this case*/
       BCM_LOG(INFO, openolt_log_id,  "Failed to query the %s\n", field_name);
       return field_val;
   }

   /*Read the field value*/
   if (fp) {
       fread(field_val, OPENOLT_FIELD_LEN, 1, fp);
       pclose(fp);
   }
   return field_val;
}

Status GetDeviceInfo_(openolt::DeviceInfo* device_info) {
    device_info->set_vendor(VENDOR_ID);
    device_info->set_model(MODEL_ID);
    device_info->set_hardware_version("");
    device_info->set_firmware_version(firmware_version);
    device_info->set_technology(board_technology);
    device_info->set_pon_ports(num_of_pon_ports);

    char serial_number[OPENOLT_FIELD_LEN];
    memset(serial_number, '\0', OPENOLT_FIELD_LEN);
    openolt_read_sysinfo("Serial Number", serial_number);
    BCM_LOG(INFO, openolt_log_id, "Fetched device serial number %s\n", serial_number);
    device_info->set_device_serial_number(serial_number);

    char device_id[OPENOLT_FIELD_LEN];
    memset(device_id, '\0', OPENOLT_FIELD_LEN);
    openolt_read_sysinfo("MAC", device_id);
    BCM_LOG(INFO, openolt_log_id, "Fetched device mac address %s\n", device_id);
    device_info->set_device_id(device_id);

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
        bcmos_fastlock_init(&data_lock, 0);

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
    Status status = DisableUplinkIf_(nni_intf_id);
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
            BCM_LOG(INFO, openolt_log_id,"Invalid Key to handle subscriber terminal clear subscriber_terminal_id %d, \
                    Interface ID %d\n", onu_id, intf_id);
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

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, uint32_t gemport_id, const std::string pkt) {
    bcmos_errno err = BCM_ERR_OK;
    bcmbal_dest proxy_pkt_dest;
    bcmbal_u8_list_u32_max_2048 buf;

    if (port_no > 0) {
        bool found = false;
        if (gemport_id == 0) {
            bcmos_fastlock_lock(&data_lock);
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
            bcmos_fastlock_unlock(&data_lock, 0);

            if (!found) {
                BCM_LOG(ERROR, openolt_log_id, "Packet out failed to find destination for ONU %d port_no %u on PON %d\n",
                        onu_id, port_no, intf_id);
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "no flow for port_no");
            }
            BCM_LOG(INFO, openolt_log_id, "Gem port %u found for ONU %d port_no %u on PON %d\n",
                    gemport_id, onu_id, port_no, intf_id);
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
    bcmos_fastlock_lock(&data_lock);
    uint32_t port_no = 0;
    std::map<uint32_t, uint32_t >::const_iterator it = flowid_to_port.find(flow_id);
    if (it != flowid_to_port.end()) {
        port_no = it->second;
    }
    bcmos_fastlock_unlock(&data_lock, 0);
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
    int32_t o_vid = -1;
    bool single_tag = false;
    uint32_t ether_type = 0;

    BCM_LOG(INFO, openolt_log_id, "flow add - intf_id %d, onu_id %d, uni_id %d, port_no %u, flow_id %d, flow_type %s, \
            gemport_id %d, network_intf_id %d, cookie %llu\n", \
            access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type.c_str(), gemport_id, network_intf_id, cookie);

    key.flow_id = flow_id;
    if (flow_type.compare(upstream) == 0 ) {
        key.flow_type = BCMBAL_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare(downstream) == 0) {
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
        bcmos_fastlock_lock(&data_lock);
        if (key.flow_type == BCMBAL_FLOW_TYPE_DOWNSTREAM) {
            port_to_flows[port_no].insert(key.flow_id);
            flowid_to_gemport[key.flow_id] = gemport_id;
        }
        else
        {
            flowid_to_port[key.flow_id] = port_no;
        }
        bcmos_fastlock_unlock(&data_lock, 0);
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

        if (classifier.eth_type()) {
            ether_type = classifier.eth_type();
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
                single_tag = true;

		BCM_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_pbits, classifier.o_pbits());
            } else if (classifier.pkt_tag_type().compare("double_tag") == 0) {
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, pkt_tag_type, BCMBAL_PKT_TAG_TYPE_DOUBLE_TAG);

		BCM_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_pbits, classifier.o_pbits());
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
            o_vid = action.o_vid();
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
            if (single_tag && ether_type == EAP_ETHER_TYPE) {
                val.sched_id = get_default_tm_sched_id(access_intf_id, downstream);
                val.queue_id = 0;

            } else {
                val.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, downstream); // Subscriber Scheduler
                val.queue_id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, downstream);
            }
            BCM_LOG(INFO, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d\n", \
                    downstream.c_str(), val.queue_id, val.sched_id);
            BCMBAL_CFG_PROP_SET(&cfg, flow, queue, val);
        } else if (key.flow_type == BCMBAL_FLOW_TYPE_UPSTREAM) {
            bcmbal_tm_sched_id val1;
            val1 = get_tm_sched_id(access_intf_id, onu_id, uni_id, upstream); // DBA Scheduler ID
            BCMBAL_CFG_PROP_SET(&cfg, flow, dba_tm_sched_id, val1);

            bcmbal_tm_queue_ref val2 = { };
            val2.sched_id = get_default_tm_sched_id(network_intf_id, upstream); // NNI Scheduler ID
            val2.queue_id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, upstream); // Queue on NNI
            BCM_LOG(INFO, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d\n", \
                    upstream.c_str(), val2.queue_id, val2.sched_id);
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
    if (flow_type.compare(upstream) == 0 ) {
        key.flow_type = BCMBAL_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare(downstream) == 0) {
        key.flow_type = BCMBAL_FLOW_TYPE_DOWNSTREAM;
    } else {
        BCM_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    bcmos_fastlock_lock(&data_lock);
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
    bcmos_fastlock_unlock(&data_lock, 0);

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

bcmos_errno CreateSched(std::string direction, uint32_t intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t port_no,
                 uint32_t alloc_id, tech_profile::AdditionalBW additional_bw, uint32_t weight, uint32_t priority,
                 tech_profile::SchedulingPolicy sched_policy, tech_profile::TrafficShapingInfo tf_sh_info) {

    bcmos_errno err;

    if (direction == downstream) {

        bcmbal_tm_sched_cfg cfg;
        bcmbal_tm_sched_key key = { };
        key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction);
        key.dir = BCMBAL_TM_SCHED_DIR_DS;

        BCMBAL_CFG_INIT(&cfg, tm_sched, key);

        {
            // bcmbal_tm_sched_owner
            // In downstream it is sub_term scheduler
            bcmbal_tm_sched_owner tm_sched_owner = { };
            tm_sched_owner.type = BCMBAL_TM_SCHED_OWNER_TYPE_SUB_TERM;
            tm_sched_owner.u.sub_term.intf_id = intf_id;
            tm_sched_owner.u.sub_term.sub_term_id = onu_id;
            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, owner, tm_sched_owner);

            // bcmbal_tm_sched_type
            // set the deafult policy to strict priority
            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, sched_type, BCMBAL_TM_SCHED_TYPE_SP);

            // bcmbal_tm_sched_parent
            // The parent for the sub_term scheduler is the PON scheduler in the downstream
            bcmbal_tm_sched_parent tm_sched_parent = { };
            tm_sched_parent.presence_mask |= (BCMBAL_TM_SCHED_PARENT_ID_SCHED_ID);
            tm_sched_parent.sched_id = get_default_tm_sched_id(intf_id, downstream);
            tm_sched_parent.presence_mask |= (BCMBAL_TM_SCHED_PARENT_ID_PRIORITY);
            tm_sched_parent.priority = 1; // TODO: Hardcoded priority as 1
            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, sched_parent, tm_sched_parent);

            // num_priorities: Max number of strict priority scheduling elements
            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, num_priorities, 8); // TODO: hardcoded 8 priorities.

            // bcmbal_tm_shaping
            if (tf_sh_info.cir() >= 0 && tf_sh_info.pir() > 0) {
                bcmbal_tm_shaping rate = {};
                uint32_t cir = tf_sh_info.cir();
                uint32_t pir = tf_sh_info.pir();
                uint32_t burst = tf_sh_info.pbs();
                BCM_LOG(INFO, openolt_log_id, "applying traffic shaping in DL cir=%u, pir=%u, burst=%u\n",
                   cir, pir, burst);
                rate.presence_mask = BCMBAL_TM_SHAPING_ID_NONE;
                rate.presence_mask |= BCMBAL_TM_SHAPING_ID_PIR;
                rate.presence_mask |= BCMBAL_TM_SHAPING_ID_BURST;
                // FIXME: Setting CIR, results in BAL throwing error 'tm_sched minimum rate is not supported yet'
                // rate.cir = cir;
                rate.pir = pir;
                rate.burst = burst;

                BCMBAL_CFG_PROP_SET(&cfg, tm_sched, rate, rate);
            }

            // creation_mode
            // BCMBAL_CFG_PROP_SET(&cfg, tm_sched, creation_mode, BCMBAL_TM_CREATION_MODE_MANUAL);
        }

        err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create downstream subscriber scheduler, id %d, intf_id %d, \
                    onu_id %d, uni_id %d, port_no %u\n", key.id, intf_id, onu_id,uni_id,port_no);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Create downstream subscriber sched, id %d, intf_id %d, onu_id %d, \
                uni_id %d, port_no %u\n", key.id,intf_id,onu_id,uni_id,port_no);

    } else { //upstream
        bcmbal_tm_sched_cfg cfg;
        bcmbal_tm_sched_key key = { };

        key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction);
        key.dir = BCMBAL_TM_SCHED_DIR_US;

        BCMBAL_CFG_INIT(&cfg, tm_sched, key);

        {
            // bcmbal_tm_sched_owner: AGG PORT
            bcmbal_tm_sched_owner tm_sched_owner = { };
            tm_sched_owner.type = BCMBAL_TM_SCHED_OWNER_TYPE_AGG_PORT;
            tm_sched_owner.u.agg_port.presence_mask |= bcmbal_tm_sched_owner_agg_port_id_all;
            tm_sched_owner.u.agg_port.intf_id = intf_id;
            tm_sched_owner.u.agg_port.sub_term_id = onu_id;
            tm_sched_owner.u.agg_port.agg_port_id = alloc_id;
            BCMBAL_CFG_PROP_SET(&cfg, tm_sched, owner, tm_sched_owner);

            // bcmbal_tm_shaping
            if (tf_sh_info.cir() >= 0 && tf_sh_info.pir() > 0) {
                bcmbal_tm_shaping rate = {};
                uint32_t cir = tf_sh_info.cir();
                uint32_t pir = tf_sh_info.pir();
                uint32_t burst = tf_sh_info.pbs();
                BCM_LOG(INFO, openolt_log_id, "applying traffic shaping in UL cir=%u, pir=%u, burst=%u\n",
                   cir, pir, burst);
                rate.presence_mask = BCMBAL_TM_SHAPING_ID_ALL;
                rate.cir = cir;
                rate.pir = pir;
                rate.burst = burst;

                BCMBAL_CFG_PROP_SET(&cfg, tm_sched, rate, rate);
            }
        }

        err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create upstream DBA sched, id %d, intf_id %d, onu_id %d, uni_id %d,\
                    port_no %u, alloc_id %d\n", key.id, intf_id, onu_id,uni_id,port_no,alloc_id);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Create upstream DBA sched, id %d, intf_id %d, onu_id %d, uni_id %d, port_no %u, \
                alloc_id %d\n", key.id,intf_id,onu_id,uni_id,port_no,alloc_id);
    }

    return BCM_ERR_OK;
}

Status CreateTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds) {
    uint32_t intf_id = traffic_scheds->intf_id();
    uint32_t onu_id = traffic_scheds->onu_id();
    uint32_t uni_id = traffic_scheds->uni_id();
    uint32_t port_no = traffic_scheds->port_no();
    std::string direction;
    unsigned int alloc_id;
    tech_profile::SchedulerConfig sched_config;
    tech_profile::AdditionalBW additional_bw;
    uint32_t priority;
    uint32_t weight;
    tech_profile::SchedulingPolicy sched_policy;
    tech_profile::TrafficShapingInfo traffic_shaping_info;
    bcmos_errno err;

    for (int i = 0; i < traffic_scheds->traffic_scheds_size(); i++) {
        tech_profile::TrafficScheduler traffic_sched = traffic_scheds->traffic_scheds(i);
        if (traffic_sched.direction() == tech_profile::Direction::UPSTREAM) {
            direction = upstream;
        } else if (traffic_sched.direction() == tech_profile::Direction::DOWNSTREAM) {
            direction = downstream;
        }
        else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", traffic_sched.direction());
            return Status::CANCELLED;
        }
        alloc_id = traffic_sched.alloc_id();
        sched_config = traffic_sched.scheduler();
        additional_bw = sched_config.additional_bw();
        priority = sched_config.priority();
        weight = sched_config.weight();
        sched_policy = sched_config.sched_policy();
        traffic_shaping_info = traffic_sched.traffic_shaping_info();
        err =  CreateSched(direction, intf_id, onu_id, uni_id, port_no, alloc_id, additional_bw, weight, priority,
                           sched_policy, traffic_shaping_info);
        if (err) {
            return bcm_to_grpc_err(err, "Failed to create scheduler");
        }
    }
    return Status::OK;
}

bcmos_errno RemoveSched(int intf_id, int onu_id, int uni_id, std::string direction) {

    bcmos_errno err;

    bcmbal_tm_sched_cfg tm_cfg_us;
    bcmbal_tm_sched_key tm_key_us = { };

    if (is_tm_sched_id_present(intf_id, onu_id, uni_id, direction)) {
        tm_key_us.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction);
    } else {
        BCM_LOG(INFO, openolt_log_id, "schduler not present in %s\n", direction.c_str());
        return BCM_ERR_OK;
    }
    if (direction == upstream) {
        tm_key_us.dir = BCMBAL_TM_SCHED_DIR_US;
    } else {
        tm_key_us.dir = BCMBAL_TM_SCHED_DIR_DS;
    }

    BCMBAL_CFG_INIT(&tm_cfg_us, tm_sched, tm_key_us);

    err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &(tm_cfg_us.hdr));
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to remove scheduler sched, direction = %s, id %d, intf_id %d, onu_id %d\n", \
                direction.c_str(), tm_key_us.id, intf_id, onu_id);
        return err;
    }

    free_tm_sched_id(intf_id, onu_id, uni_id, direction);

    BCM_LOG(INFO, openolt_log_id, "Removed sched, direction = %s, id %d, intf_id %d, onu_id %d\n", \
            direction.c_str(), tm_key_us.id, intf_id, onu_id);

    return BCM_ERR_OK;
}

Status RemoveTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds) {
    uint32_t intf_id = traffic_scheds->intf_id();
    uint32_t onu_id = traffic_scheds->onu_id();
    uint32_t uni_id = traffic_scheds->uni_id();
    std::string direction;
    bcmos_errno err;

    for (int i = 0; i < traffic_scheds->traffic_scheds_size(); i++) {
        tech_profile::TrafficScheduler traffic_sched = traffic_scheds->traffic_scheds(i);
        if (traffic_sched.direction() == tech_profile::Direction::UPSTREAM) {
            direction = upstream;
        } else if (traffic_sched.direction() == tech_profile::Direction::DOWNSTREAM) {
            direction = downstream;
        }
        else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", traffic_sched.direction());
            return Status::CANCELLED;
        }
        err = RemoveSched(intf_id, onu_id, uni_id, direction);
        if (err) {
            return bcm_to_grpc_err(err, "error-removing-traffic-scheduler");
        }
    }
    return Status::OK;
}

bcmos_errno CreateQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t priority,
                        uint32_t gemport_id) {
    bcmos_errno err;
    bcmbal_tm_queue_cfg cfg;
    bcmbal_tm_queue_key key = { };
    BCM_LOG(INFO, openolt_log_id, "creating queue. access_intf_id = %d, onu_id = %d, uni_id = %d \
            gemport_id = %d, direction = %s\n", access_intf_id, onu_id, uni_id, gemport_id, direction.c_str());
    if (direction == downstream) {
        // In the downstream, the queues are on the 'sub term' scheduler
        // There is one queue per gem port
        key.sched_dir = BCMBAL_TM_SCHED_DIR_DS;
        key.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, direction);
        key.id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);

    } else {
        queue_map_key_tuple map_key(access_intf_id, onu_id, uni_id, gemport_id, direction);
        if (queue_map.count(map_key) > 0) {
            BCM_LOG(INFO, openolt_log_id, "upstream queue exists for intf_id %d, onu_id %d, uni_id %d\n. Not re-creating", \
                    access_intf_id, onu_id, uni_id); 
            return BCM_ERR_OK;
        }
        key.sched_dir = BCMBAL_TM_SCHED_DIR_US;
        key.sched_id = get_default_tm_sched_id(nni_intf_id, direction);
        if (priority > 7) {
            return BCM_ERR_RANGE;
        }
        // There are 8 queues (one per p-bit)
        key.id = us_fixed_queue_id_list[priority];
        update_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction, key.id);
        // FIXME: The upstream queues have to be created once only.
        // The upstream queues on the NNI scheduler are shared by all subscribers.
        // When the first scheduler comes in, the queues get created, and are re-used by all others.
        // Also, these queues should be present until the last subscriber exits the system.
        // One solution is to have these queues always, i.e., create it as soon as OLT is enabled.
    }
    BCM_LOG(INFO, openolt_log_id, "queue assigned queue_id = %d\n", key.id);

    BCMBAL_CFG_INIT(&cfg, tm_queue, key);

    BCMBAL_CFG_PROP_SET(&cfg, tm_queue, priority, priority);

    // BCMBAL_CFG_PROP_SET(&cfg, tm_queue, creation_mode, BCMBAL_TM_CREATION_MODE_MANUAL);


    err = bcmbal_cfg_set(DEFAULT_ATERM_ID, &cfg.hdr);
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to create subscriber tm queue, direction = %s, id %d, sched_id %d, \
                intf_id %d, onu_id %d, uni_id %d\n", \
                direction.c_str(), key.id, key.sched_id, access_intf_id, onu_id, uni_id);
        return err;
    }

    BCM_LOG(INFO, openolt_log_id, "Created tm_queue, direction %s, id %d, intf_id %d, onu_id %d, uni_id %d", \
            direction.c_str(), key.id, access_intf_id, onu_id, uni_id);

    return BCM_ERR_OK;

}

Status CreateTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues) {
    uint32_t intf_id = traffic_queues->intf_id();
    uint32_t onu_id = traffic_queues->onu_id();
    uint32_t uni_id = traffic_queues->uni_id();
    std::string direction;
    unsigned int alloc_id;
    bcmos_errno err;

    for (int i = 0; i < traffic_queues->traffic_queues_size(); i++) {
        tech_profile::TrafficQueue traffic_queue = traffic_queues->traffic_queues(i);
        if (traffic_queue.direction() == tech_profile::Direction::UPSTREAM) {
            direction = upstream;
        } else if (traffic_queue.direction() == tech_profile::Direction::DOWNSTREAM) {
            direction = downstream;
        }
        else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", traffic_queue.direction());
            return Status::CANCELLED;
        }
        err = CreateQueue(direction, intf_id, onu_id, uni_id, traffic_queue.priority(), traffic_queue.gemport_id());
        // If the queue exists already, lets not return failure and break the loop.
        if (err && err != BCM_ERR_ALREADY) {
            return bcm_to_grpc_err(err, "Failed to create queue");
        }
    }
    return Status::OK;
}


bcmos_errno RemoveQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t priority,
                        uint32_t gemport_id) {
    bcmbal_tm_queue_cfg queue_cfg;
    bcmbal_tm_queue_key queue_key = { };
    bcmos_errno err;

    if (direction == downstream) {
        queue_key.sched_dir = BCMBAL_TM_SCHED_DIR_DS;
        if (is_tm_queue_id_present(access_intf_id, onu_id, uni_id, gemport_id, direction) && \
            is_tm_sched_id_present(access_intf_id, onu_id, uni_id, direction)) {
            queue_key.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, direction);
            queue_key.id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);
        } else {
            BCM_LOG(INFO, openolt_log_id, "queue not present in DS. Not clearing");
            return BCM_ERR_OK;
        }
    } else {
        free_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);
        // In the upstream we use pre-created queues on the NNI scheduler that are used by all subscribers.
        // They should not be removed. So, lets return OK.
        return BCM_ERR_OK;
    }

    BCMBAL_CFG_INIT(&queue_cfg, tm_queue, queue_key);

    err = bcmbal_cfg_clear(DEFAULT_ATERM_ID, &(queue_cfg.hdr));

    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to remove queue, direction = %s, id %d, sched_id %d, intf_id %d, onu_id %d, uni_id %d\n",
                direction.c_str(), queue_key.id, queue_key.sched_id, access_intf_id, onu_id, uni_id);
        return err;
    }

    free_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);

    return BCM_ERR_OK;
}

Status RemoveTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues) {
    uint32_t intf_id = traffic_queues->intf_id();
    uint32_t onu_id = traffic_queues->onu_id();
    uint32_t uni_id = traffic_queues->uni_id();
    uint32_t port_no = traffic_queues->port_no();
    std::string direction;
    unsigned int alloc_id;
    bcmos_errno err;

    for (int i = 0; i < traffic_queues->traffic_queues_size(); i++) {
        tech_profile::TrafficQueue traffic_queue = traffic_queues->traffic_queues(i);
        if (traffic_queue.direction() == tech_profile::Direction::UPSTREAM) {
            direction = upstream;
        } else if (traffic_queue.direction() == tech_profile::Direction::DOWNSTREAM) {
            direction = downstream;
        } else {
            BCM_LOG(ERROR, openolt_log_id, "direction-not-supported %d", traffic_queue.direction());
            return Status::CANCELLED;
        }
        err = RemoveQueue(direction, intf_id, onu_id, uni_id, traffic_queue.priority(), traffic_queue.gemport_id());
        if (err) {
            return bcm_to_grpc_err(err, "Failed to remove queue");
        }
    }

    return Status::OK;
}

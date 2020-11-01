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
#include <iostream>
#include <memory>
#include <string>

#include "Queue.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <bitset>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "device.h"
#include "core.h"
#include "core_data.h"
#include "indications.h"
#include "stats_collection.h"
#include "error_format.h"
#include "state.h"
#include "core_utils.h"

extern "C"
{
#include <bcmolt_api.h>
#include <bcmolt_host_api.h>
#include <bcmolt_api_model_supporting_enums.h>

#include <bcmolt_api_conn_mgr.h>
//CLI header files
#include <bcmcli_session.h>
#include <bcmcli.h>
#include <bcm_api_cli.h>

#include <bcmos_common.h>
#include <bcm_config.h>
// FIXME : dependency problem
// #include <bcm_common_gpon.h>
// #include <bcm_dev_log_task.h>
}

dev_log_id openolt_log_id = bcm_dev_log_id_register("OPENOLT", DEV_LOG_LEVEL_INFO, DEV_LOG_ID_TYPE_BOTH);
dev_log_id omci_log_id = bcm_dev_log_id_register("OMCI", DEV_LOG_LEVEL_INFO, DEV_LOG_ID_TYPE_BOTH);


unsigned int num_of_nni_ports = 0;
unsigned int num_of_pon_ports = 0;

const uint32_t tm_upstream_sched_id_start = (MAX_SUPPORTED_PON == 16 ? \
    MAX_TM_SCHED_ID-3 : MAX_TM_SCHED_ID-9);
const uint32_t tm_downstream_sched_id_start = (MAX_SUPPORTED_PON == 16 ? \
    tm_upstream_sched_id_start-16 : tm_upstream_sched_id_start-64);

/* Max Queue ID supported is 7 so based on priority_q configured for GEMPORTS
in TECH PROFILE respective Queue ID from this list will be used for both
US and DS Queues*/
const uint32_t queue_id_list[8] = {0, 1, 2, 3, 4, 5, 6, 7};

const std::string upstream = "upstream";
const std::string downstream = "downstream";
const std::string multicast = "multicast";
bcmolt_oltid dev_id = 0;

/* Constants used for retrying some BAL APIs */
const uint32_t BAL_API_RETRY_TIME_IN_USECS = 1000000;
const uint32_t MAX_BAL_API_RETRY_COUNT = 5;

const unsigned int OPENOLT_FIELD_LEN = 200;

/* Current session */
bcmcli_session *current_session;
bcmcli_entry *api_parent_dir;
bcmos_bool status_bcm_cli_quit = BCMOS_FALSE;
bcmos_task bal_cli_thread;
const char *bal_cli_thread_name = "bal_cli_thread";
uint16_t flow_id_counters = 0;
State state;

std::map<uint32_t, uint32_t> flowid_to_port; // For mapping upstream flows to logical ports
std::map<uint32_t, uint32_t> flowid_to_gemport; // For mapping downstream flows into gemports
std::map<uint32_t, std::set<uint32_t> > port_to_flows; // For mapping logical ports to downstream flows

/* This represents the Key to 'sched_map' map.
 Represents (pon_intf_id, onu_id, uni_id, direction, tech_profile_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t, std::string, uint32_t> sched_map_key_tuple;
/* 'sched_map' maps sched_map_key_tuple to DBA (Upstream) or
 Subscriber (Downstream) Scheduler ID */
std::map<sched_map_key_tuple, int> sched_map;

/* Flow control is for flow_id and flow_type */
typedef std::pair<uint16_t, uint16_t> flow_pair;
std::map<flow_pair, int32_t> flow_map;

/* This represents the Key to 'qos_type_map' map.
 Represents (pon_intf_id, onu_id, uni_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t> qos_type_map_key_tuple;
/* 'qos_type_map' maps qos_type_map_key_tuple to qos_type*/
std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type> qos_type_map;

/* This represents the Key to 'sched_qmp_id_map' map.
Represents (sched_id, pon_intf_id, onu_id, uni_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> sched_qmp_id_map_key_tuple;
/* 'sched_qmp_id_map' maps sched_qmp_id_map_key_tuple to TM Queue Mapping Profile ID */
std::map<sched_qmp_id_map_key_tuple, int> sched_qmp_id_map;
/* 'qmp_id_to_qmp_map' maps TM Queue Mapping Profile ID to TM Queue Mapping Profile */
std::map<int, std::vector < uint32_t > > qmp_id_to_qmp_map;

// Map used to track response from BAL for ITU PON Alloc Configuration.
// The key is alloc_cfg_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *> alloc_cfg_compltd_map;
// Lock to protect critical section data structure used for handling AllocObject configuration response.
bcmos_fastlock alloc_cfg_wait_lock;

// Map used to track response from BAL for Onu Deactivation Completed Indication
// The key is alloc_cfg_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
std::map<onu_deact_compltd_key,  Queue<onu_deactivate_complete_result> *> onu_deact_compltd_map;
// Lock to protect critical section data structure used for handling Onu Deactivation Completed Indication
bcmos_fastlock onu_deactivate_wait_lock;

/*** ACL Handling related data start ***/

std::map<acl_classifier_key, uint16_t> acl_classifier_to_acl_id_map;

bool operator<(const acl_classifier_key& a1, const acl_classifier_key& a2)
{
    return ((a1.ether_type + 2*a1.ip_proto + 3*a1.src_port + 4*a1.dst_port + 5*a1.o_vid) <
            (a2.ether_type + 2*a2.ip_proto + 3*a2.src_port + 4*a2.dst_port + 5*a2.o_vid));
}

typedef std::tuple<uint64_t, std::string> flow_id_flow_direction;

typedef std::tuple<int16_t, int32_t> acl_id_intf_id;
std::map<flow_id_flow_direction, acl_id_intf_id> flow_to_acl_map;

// Keeps a reference count of how many flows are referencing a given ACL ID.
// Key represents the ACL-ID and value is number of flows referencing the given ACL-ID.
// When there is at least one flow referencing the ACL-ID, the ACL should be installed.
// When there are no flows referencing the ACL-ID, the ACL should be removed.
std::map<uint16_t, uint16_t> acl_ref_cnt;

// Needed to keep track of how many flows for a given acl_id, intf_id and intf_type are
// installed. When there is at least on flow for this key, we should have interface registered
// for the given ACL-ID. When there are no flows, the intf should unregister itself from
// the ACL-ID.
typedef std::tuple<uint16_t, uint8_t, std::string> acl_id_intf_id_intf_type;
std::map<acl_id_intf_id_intf_type, uint16_t> intf_acl_registration_ref_cnt;

// Data structures to work around ACL limits on BAL -- start --

// This flag is set as soon as ACL count reaches MAX_ACL_WITH_VLAN_CLASSIFIER is hit.
// It is not reset when ACL count comes below MAX_ACL_WITH_VLAN_CLASSIFIER again
bool max_acls_with_vlan_classifiers_hit = false;
// Map of flow_id -> trap_to_host_pkt_info_with_vlan
std::map<uint64_t, trap_to_host_pkt_info_with_vlan> trap_to_host_pkt_info_with_vlan_for_flow_id;
// Map of trap_to_host_pkt_info -> cvid_list
std::map<trap_to_host_pkt_info, std::list<uint16_t> > trap_to_host_vlan_ids_for_trap_to_host_pkt_info;

// Data structures to work around ACL limits on BAL -- end --


std::bitset<MAX_ACL_ID> acl_id_bitset;
bcmos_fastlock acl_id_bitset_lock;

/*** ACL Handling related data end ***/

// Used to manage a pool of Scheduler IDs
std::bitset<MAX_TM_SCHED_ID> tm_sched_bitset;
bcmos_fastlock tm_sched_bitset_lock;

// Used to manage a pool of TM_QMP IDs
std::bitset<MAX_TM_QMP_ID> tm_qmp_bitset;
bcmos_fastlock tm_qmp_bitset_lock;

// Used to manage a pool of Flow IDs
std::bitset<MAX_FLOW_ID> flow_id_bitset;
bcmos_fastlock flow_id_bitset_lock;

// Maps voltha flow-id to device flow
std::map<uint64_t, device_flow> voltha_flow_to_device_flow;
bcmos_fastlock voltha_flow_to_device_flow_lock;

// Lock to protect critical section around handling data associated with ACL trap packet handling
bcmos_fastlock acl_packet_trap_handler_lock;

// General purpose lock used to gaurd critical section during various API handling at the core_api_handler
bcmos_fastlock data_lock;


char* grpc_server_interface_name = NULL;

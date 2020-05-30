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
#ifndef OPENOLT_CORE_DATA_H_
#define OPENOLT_CORE_DATA_H_

#include <bitset>

#include "core.h"
#include "Queue.h"

extern "C"
{
#include <bcmolt_api.h>
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

#define ALLOC_CFG_COMPLETE_WAIT_TIMEOUT 5000 // in milli-seconds

#define ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT 5000 // in milli-seconds

#define MIN_ALLOC_ID_GPON 256
#define MIN_ALLOC_ID_XGSPON 1024

#define MAX_ACL_ID 33

// **************************************//
// Enums and structures used by the core //
// **************************************//
enum FLOW_CFG {
    ONU_ID = 0,
    FLOW_TYPE = 1,
    SVC_PORT_ID = 2,
    PRIORITY = 3,
    COOKIE = 4,
    INGRESS_INTF_TYPE= 5,
    EGRESS_INTF_TYPE= 6,
    INGRESS_INTF_ID = 7,
    EGRESS_INTF_ID = 8,
    CLASSIFIER_O_VID = 9,
    CLASSIFIER_O_PBITS = 10,
    CLASSIFIER_I_VID = 11,
    CLASSIFIER_I_PBITS = 12,
    CLASSIFIER_ETHER_TYPE = 13,
    CLASSIFIER_IP_PROTO =14,
    CLASSIFIER_SRC_PORT = 15,
    CLASSIFIER_DST_PORT = 16,
    CLASSIFIER_PKT_TAG_TYPE = 17,
    EGRESS_QOS_TYPE = 18,
    EGRESS_QOS_QUEUE_ID = 19,
    EGRESS_QOS_TM_SCHED_ID = 20,
    ACTION_CMDS_BITMASK = 21,
    ACTION_O_VID = 22,
    ACTION_O_PBITS = 23,
    ACTION_I_VID = 24,
    ACTION_I_PBITS = 25,
    STATE = 26,
    GROUP_ID = 27
};

enum AllocCfgAction {
    ALLOC_OBJECT_CREATE,
    ALLOC_OBJECT_DELETE
};

enum AllocObjectState {
    ALLOC_OBJECT_STATE_NOT_CONFIGURED,
    ALLOC_OBJECT_STATE_INACTIVE,
    ALLOC_OBJECT_STATE_PROCESSING,
    ALLOC_OBJECT_STATE_ACTIVE
};

enum AllocCfgStatus {
    ALLOC_CFG_STATUS_SUCCESS,
    ALLOC_CFG_STATUS_FAIL
};

typedef struct {
    uint32_t pon_intf_id;
    uint32_t alloc_id;
    AllocObjectState state;
    AllocCfgStatus status;
} alloc_cfg_complete_result;

typedef struct {
    uint32_t pon_intf_id;
    uint32_t onu_id;
    bcmolt_result result;
    bcmolt_deactivation_fail_reason reason;
} onu_deactivate_complete_result;

// key for map used for tracking ITU PON Alloc Configuration results from BAL
typedef std::tuple<uint32_t, uint32_t> alloc_cfg_compltd_key;

// key for map used for tracking Onu Deactivation Completed Indication
typedef std::tuple<uint32_t, uint32_t> onu_deact_compltd_key;

// The elements in this acl_classifier_key structure constitute key to
// acl_classifier_to_acl_id_map.
// Fill invalid values in the acl_classifier_key structure to -1.
typedef struct acl_classifier_key {
    int32_t ether_type;
    int16_t ip_proto;
    int32_t src_port;
    int32_t dst_port;
    // Add more classifiers elements as needed here
    // For now, ACLs will be classified only based on
    // above elements.
} acl_classifier_key;

// *******************************************************//
// Extern Variable/Constant declarations used by the core //
// *******************************************************//
extern State state;

extern dev_log_id openolt_log_id;
extern dev_log_id omci_log_id;

extern unsigned int num_of_nni_ports;
extern unsigned int num_of_pon_ports;

extern const uint32_t tm_upstream_sched_id_start;
extern const uint32_t tm_downstream_sched_id_start;

extern const uint32_t queue_id_list[8];

extern const std::string upstream;
extern const std::string downstream;
extern const std::string multicast;
extern bcmolt_oltid dev_id;

extern const uint32_t BAL_API_RETRY_TIME_IN_USECS;
extern const uint32_t MAX_BAL_API_RETRY_COUNT;

extern const unsigned int OPENOLT_FIELD_LEN;

/* Current session */
extern bcmcli_session *current_session;
extern bcmcli_entry *api_parent_dir;
extern bcmos_bool status_bcm_cli_quit;
extern bcmos_task bal_cli_thread;
extern const char *bal_cli_thread_name;
extern uint16_t flow_id_counters;

extern std::map<uint32_t, uint32_t> flowid_to_port; // For mapping upstream flows to logical ports
extern std::map<uint32_t, uint32_t> flowid_to_gemport; // For mapping downstream flows into gemports
extern std::map<uint32_t, std::set<uint32_t> > port_to_flows; // For mapping logical ports to downstream flows

/* This represents the Key to 'sched_map' map.
 Represents (pon_intf_id, onu_id, uni_id, direction, tech_profile_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t, std::string, uint32_t> sched_map_key_tuple;
/* 'sched_map' maps sched_map_key_tuple to DBA (Upstream) or
 Subscriber (Downstream) Scheduler ID */
extern std::map<sched_map_key_tuple, int> sched_map;

/* Flow control is for flow_id and flow_type */
typedef std::pair<uint16_t, uint16_t> flow_pair;
extern std::map<flow_pair, int32_t> flow_map;

/* This represents the Key to 'qos_type_map' map.
 Represents (pon_intf_id, onu_id, uni_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t> qos_type_map_key_tuple;
/* 'qos_type_map' maps qos_type_map_key_tuple to qos_type*/
extern std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type> qos_type_map;

/* This represents the Key to 'sched_qmp_id_map' map.
Represents (sched_id, pon_intf_id, onu_id, uni_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> sched_qmp_id_map_key_tuple;
/* 'sched_qmp_id_map' maps sched_qmp_id_map_key_tuple to TM Queue Mapping Profile ID */
extern std::map<sched_qmp_id_map_key_tuple, int> sched_qmp_id_map;
/* 'qmp_id_to_qmp_map' maps TM Queue Mapping Profile ID to TM Queue Mapping Profile */
extern std::map<int, std::vector < uint32_t > > qmp_id_to_qmp_map;

// Map used to track response from BAL for ITU PON Alloc Configuration.
// The key is alloc_cfg_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
extern std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *> alloc_cfg_compltd_map;
// Map used to track response from BAL for Onu Deactivation Completed Indication
// The key is alloc_cfg_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
extern std::map<onu_deact_compltd_key,  Queue<onu_deactivate_complete_result> *> onu_deact_compltd_map;

// Lock to protect critical section data structure used for handling AllocObject configuration response.
extern bcmos_fastlock alloc_cfg_wait_lock;
// Lock to protect critical section data structure used for handling Onu deactivation completed Indication
extern bcmos_fastlock onu_deactivate_wait_lock;


/*** ACL Handling related data start ***/

extern std::map<acl_classifier_key, uint16_t> acl_classifier_to_acl_id_map;
extern bool operator<(const acl_classifier_key& a1, const acl_classifier_key& a2);

typedef std::tuple<uint16_t, std::string> flow_id_flow_direction;
typedef std::tuple<int16_t, uint16_t, int32_t> acl_id_gem_id_intf_id;
extern std::map<flow_id_flow_direction, acl_id_gem_id_intf_id> flow_to_acl_map;

// Keeps a reference count of how many flows are referencing a given ACL ID.
// Key represents the ACL-ID and value is number of flows referencing the given ACL-ID.
// When there is at least one flow referencing the ACL-ID, the ACL should be installed.
// When there are no flows referencing the ACL-ID, the ACL should be removed.
extern std::map<uint16_t, uint16_t> acl_ref_cnt;

typedef std::tuple<uint16_t, uint16_t> gem_id_intf_id; // key to gem_ref_cnt
// Keeps a reference count of how many ACL related flows are referencing a given (gem-id, pon_intf_id).
// When there is at least on flow, we should install the gem. When there are no flows
// the gem should be removed.
extern std::map<gem_id_intf_id, uint16_t> gem_ref_cnt;

// Needed to keep track of how many flows for a given acl_id, intf_id and intf_type are
// installed. When there is at least on flow for this key, we should have interface registered
// for the given ACL-ID. When there are no flows, the intf should unregister itself from
// the ACL-ID.
typedef std::tuple<uint16_t, uint8_t, std::string> acl_id_intf_id_intf_type;
extern std::map<acl_id_intf_id_intf_type, uint16_t> intf_acl_registration_ref_cnt;

extern std::bitset<MAX_ACL_ID> acl_id_bitset;

/*** ACL Handling related data end ***/

extern std::bitset<MAX_TM_SCHED_ID> tm_sched_bitset;
extern std::bitset<MAX_TM_QMP_ID> tm_qmp_bitset;

extern Queue<openolt::Indication> oltIndQ;

extern bcmos_fastlock data_lock;


#endif // OPENOLT_CORE_DATA_H_

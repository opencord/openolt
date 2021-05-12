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
#include "device.h"

// pcapplusplus packet decoder include files
#include "Packet.h"
#include "EthLayer.h"
#include "IPv4Layer.h"
#include "UdpLayer.h"
#include "VlanLayer.h"

#include <time.h>
#include <arpa/inet.h>

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
#define GEM_CFG_COMPLETE_WAIT_TIMEOUT 5000 // in milli-seconds

// max retry count to find gem config key in gem_cfg_compltd_map
#define MAX_GEM_CFG_KEY_CHECK 5

#define ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT 5000 // in milli-seconds


#define MAX_ACL_ID 33
#define MAX_ACL_WITH_VLAN_CLASSIFIER 10

#define ANY_VLAN 4095

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

enum GemCfgAction {
    GEM_OBJECT_CREATE,
    GEM_OBJECT_DELETE,
    GEM_OBJECT_ENCRYPT
};

enum GemObjectState {
    GEM_OBJECT_STATE_NOT_CONFIGURED,
    GEM_OBJECT_STATE_INACTIVE,
    GEM_OBJECT_STATE_PROCESSING,
    GEM_OBJECT_STATE_ACTIVE
};

enum GemCfgStatus {
    GEM_CFG_STATUS_SUCCESS,
    GEM_CFG_STATUS_FAIL
};

typedef struct {
    uint32_t pon_intf_id;
    uint32_t gem_port_id;
    GemObjectState state;
    GemCfgStatus status;
} gem_cfg_complete_result;

typedef struct {
    uint32_t pon_intf_id;
    uint32_t onu_id;
    bcmolt_result result;
    bcmolt_deactivation_fail_reason reason;
} onu_deactivate_complete_result;

// key for map used for tracking ITU PON Alloc Configuration results from BAL
typedef std::tuple<uint32_t, uint32_t> alloc_cfg_compltd_key;

// key for map used for tracking ITU PON Gem Configuration results from BAL
typedef std::tuple<uint32_t, uint32_t> gem_cfg_compltd_key;

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
    int16_t o_vid;
    // Add more classifiers elements as needed here
    // For now, ACLs will be classified only based on
    // above elements.
} acl_classifier_key;

typedef struct device_flow_params {
    uint8_t pbit; // If pbit classifier is not present in flow, use 0xff to invalidate
    uint32_t gemport_id;
    uint16_t flow_id;
} device_flow_params;

typedef struct device_flow {
    bool is_flow_replicated; // If true number of replicated flows is to the NUMBER_OF_PBITS, else 1
    device_flow_params params[NUMBER_OF_REPLICATED_FLOWS]; // A voltha-flow cannot be replicated more than the number of pbits
    uint64_t voltha_flow_id; // This is corresponding voltha flow-id.
    uint64_t symmetric_voltha_flow_id; // Is not applicable for trap-to-controller voltha flows.
                                       // 0 value means invalid or not-applicable
                                       // Applicable for bi-directional data path flows (one flow per direction)
                                       // Symmetric flows should share the same device_flow_id.

} device_flow;

// key for map used for tracking Onu RSSI Measurement Completed Indication
typedef std::tuple<uint32_t, uint32_t> onu_rssi_compltd_key;

typedef struct {
    uint32_t pon_intf_id;
    uint32_t onu_id;
    std::string status;
    bcmolt_rssi_measurement_fail_reason reason;
    double rx_power_mean_dbm;
} onu_rssi_complete_result;

// Key for map used for tracking symmetric datapath flows ==> <pon, onu, uni, tp_id, flow_type>
typedef std::tuple<int32_t, int32_t, int32_t, uint32_t, std::string> symmetric_datapath_flow_id_map_key;

// Key and value for the pon_gem_to_onu_uni_map
typedef std::tuple<uint32_t, uint32_t> pon_gem;
typedef std::tuple<uint32_t, uint32_t> onu_uni;

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

// Map used to track response from BAL for ITU PON Gem Configuration.
// The key is gem_cfg_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
extern std::map<gem_cfg_compltd_key,  Queue<gem_cfg_complete_result> *> gem_cfg_compltd_map;

/* This represents the Key to 'gemport_status_map' map.
 Represents (pon_intf_id, onu_id, uni_id, gemport_id) */
typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> gemport_status_map_key_tuple;
/* 'gemport_status_map' maps gemport_status_map_key_tuple to boolean value */
extern std::map<gemport_status_map_key_tuple, bool> gemport_status_map;

// Map used to track response from BAL for Onu Deactivation Completed Indication
// The key is onu_deact_compltd_key and value is a concurrent thread-safe queue which is
// used for pushing (from BAL) and popping (at application) the results.
extern std::map<onu_deact_compltd_key,  Queue<onu_deactivate_complete_result> *> onu_deact_compltd_map;

// Lock to protect critical section data structure used for handling AllocObject configuration response.
extern bcmos_fastlock alloc_cfg_wait_lock;
// Lock to protect critical section data structure used for handling GemObject configuration response.
extern bcmos_fastlock gem_cfg_wait_lock;
// Lock to protect critical section data structure used for handling Onu deactivation completed Indication
extern bcmos_fastlock onu_deactivate_wait_lock;


/*** ACL Handling related data start ***/

extern std::map<acl_classifier_key, uint16_t> acl_classifier_to_acl_id_map;
extern bool operator<(const acl_classifier_key& a1, const acl_classifier_key& a2);

typedef std::tuple<uint64_t, std::string> flow_id_flow_direction;
typedef std::tuple<int16_t, int32_t> acl_id_intf_id;
extern std::map<flow_id_flow_direction, acl_id_intf_id> flow_to_acl_map;

// Keeps a reference count of how many flows are referencing a given ACL ID.
// Key represents the ACL-ID and value is number of flows referencing the given ACL-ID.
// When there is at least one flow referencing the ACL-ID, the ACL should be installed.
// When there are no flows referencing the ACL-ID, the ACL should be removed.
extern std::map<uint16_t, uint16_t> acl_ref_cnt;

// Needed to keep track of how many flows for a given acl_id, intf_id and intf_type are
// installed. When there is at least on flow for this key, we should have interface registered
// for the given ACL-ID. When there are no flows, the intf should unregister itself from
// the ACL-ID.
typedef std::tuple<uint16_t, uint8_t, std::string> acl_id_intf_id_intf_type;
extern std::map<acl_id_intf_id_intf_type, uint16_t> intf_acl_registration_ref_cnt;

// Data structures to work around ACL limits on BAL -- start --

enum trap_to_host_packet_type {
    dhcpv4 = 0,
    lldp = 1,
    eap = 2,
    igmpv4 = 3,
    pppoed = 4,
    unsupported_trap_to_host_pkt_type = 0xff
};

// Constants useful during trap-to-host packet header field classification
#define EAP_ETH_TYPE    0x888e
#define LLDP_ETH_TYPE   0x88cc
#define IPV4_ETH_TYPE   0x0800
#define VLAN_ETH_TYPE   0x8100
#define PPPoED_ETH_TYPE 0x8863

#define IGMPv4_PROTOCOL 2
#define UDP_PROTOCOL    17

#define DHCP_SERVER_SRC_PORT 67
#define DHCP_CLIENT_SRC_PORT 68

// This flag is set as soon as ACL count reaches MAX_ACL_WITH_VLAN_CLASSIFIER is hit.
// It is not reset when ACL count comes below MAX_ACL_WITH_VLAN_CLASSIFIER again
extern bool max_acls_with_vlan_classifiers_hit;

// Tuple of -> {bcmolt_flow_interface_type, intf-id, trap_to_host_packet_type, gemport-id, c-vid}
typedef std::tuple<int32_t, uint32_t, int32_t, int32_t, uint16_t> trap_to_host_pkt_info_with_vlan;

// Tuple of -> {bcmolt_flow_interface_type, intf-id, trap_to_host_packet_type, gemport-id}
typedef std::tuple<int32_t, uint32_t, int32_t, int32_t> trap_to_host_pkt_info;

// Map of flow_id -> trap_to_host_pkt_info_with_vlan
extern std::map<uint64_t, trap_to_host_pkt_info_with_vlan> trap_to_host_pkt_info_with_vlan_for_flow_id;

// Map of trap_to_host_pkt_info -> cvid_list
extern std::map<trap_to_host_pkt_info, std::list<uint16_t> > trap_to_host_vlan_ids_for_trap_to_host_pkt_info;

// Data structures to work around ACL limits on BAL -- end --

extern std::bitset<MAX_ACL_ID> acl_id_bitset;
extern bcmos_fastlock acl_id_bitset_lock;

/*** ACL Handling related data end ***/

extern std::bitset<MAX_TM_SCHED_ID> tm_sched_bitset;
extern bcmos_fastlock tm_sched_bitset_lock;

extern std::bitset<MAX_TM_QMP_ID> tm_qmp_bitset;
extern bcmos_fastlock tm_qmp_bitset_lock;

extern std::bitset<MAX_FLOW_ID> flow_id_bitset;
extern bcmos_fastlock flow_id_bitset_lock;

extern std::map<uint64_t, device_flow> voltha_flow_to_device_flow;
extern bcmos_fastlock voltha_flow_to_device_flow_lock;

extern std::map<symmetric_datapath_flow_id_map_key, uint64_t> symmetric_datapath_flow_id_map;
extern bcmos_fastlock symmetric_datapath_flow_id_lock;

extern std::map<pon_gem, onu_uni> pon_gem_to_onu_uni_map;
extern bcmos_fastlock pon_gem_to_onu_uni_map_lock;

// Lock to protect critical section around handling data associated with ACL trap packet handling
extern bcmos_fastlock acl_packet_trap_handler_lock;

extern Queue<openolt::Indication> oltIndQ;

/*** ACL Handling related data end ***/

extern bcmos_fastlock data_lock;

// Interface name on which grpc server is running on
// and this can be used to get the mac adress based on interface name.
extern char* grpc_server_interface_name;

extern std::map<onu_rssi_compltd_key, Queue<onu_rssi_complete_result>*> onu_rssi_compltd_map;
extern bcmos_fastlock onu_rssi_wait_lock;
#endif // OPENOLT_CORE_DATA_H_

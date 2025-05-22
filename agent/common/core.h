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

#ifndef OPENOLT_CORE_H_
#define OPENOLT_CORE_H_

#include <grpc++/grpc++.h>
using grpc::Status;
#include <voltha_protos/openolt.grpc.pb.h>
#include <voltha_protos/common.grpc.pb.h>
#include <voltha_protos/ext_config.grpc.pb.h>

#include "state.h"
#include "vendor.h"

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
// ************************//
// Macros used by the core //
// ************************//

#define NONE "\033[m"
#define LIGHT_RED "\033[1;31m"
#define BROWN "\033[0;33m"
#define LIGHT_GREEN "\033[1;32m"
#define OPENOLT_LOG(level, id, fmt, ...)  \
    if (DEV_LOG_LEVEL_##level == DEV_LOG_LEVEL_ERROR) \
        BCM_LOG(level, id, "%s" fmt "%s", LIGHT_RED, ##__VA_ARGS__, NONE); \
    else if (DEV_LOG_LEVEL_##level == DEV_LOG_LEVEL_INFO) \
        BCM_LOG(level, id, "%s" fmt "%s", NONE, ##__VA_ARGS__, NONE); \
    else if (DEV_LOG_LEVEL_##level == DEV_LOG_LEVEL_WARNING) \
        BCM_LOG(level, id, "%s" fmt "%s", BROWN, ##__VA_ARGS__, NONE); \
    else if (DEV_LOG_LEVEL_##level == DEV_LOG_LEVEL_DEBUG) \
        BCM_LOG(level, id, "%s" fmt "%s", LIGHT_GREEN, ##__VA_ARGS__, NONE); \
    else \
        BCM_LOG(INFO, id, fmt, ##__VA_ARGS__);


#define ACL_LOG(level,msg,err) \
    do { \
    OPENOLT_LOG(level, openolt_log_id, "--------> %s (acl_id %d) err: %d <--------\n", msg, key.id, err); \
    OPENOLT_LOG(level, openolt_log_id, "action_type %s\n", \
        GET_ACL_ACTION_TYPE(action_type)); \
    OPENOLT_LOG(level, openolt_log_id, "classifier(ether type %d), ip_proto %d, src_port %d, dst_port %d\n", \
        acl_key.ether_type, acl_key.ip_proto, acl_key.src_port, acl_key.dst_port); \
    } while(0)

#define FLOW_LOG(level,msg,err) \
    do { \
    OPENOLT_LOG(level, openolt_log_id, "--------> %s (flow_id %d) err: %d <--------\n", msg, key.flow_id, err); \
    OPENOLT_LOG(level, openolt_log_id, "intf_id %d, onu_id %d, uni_id %d, port_no %u, cookie %"PRIu64"\n", \
        access_intf_id, onu_id, uni_id, port_no, cookie); \
    OPENOLT_LOG(level, openolt_log_id, "flow_type %s, queue_id %d, sched_id %d\n", flow_type.c_str(), \
        cfg.data.egress_qos.u.fixed_queue.queue_id, cfg.data.egress_qos.tm_sched.id); \
    OPENOLT_LOG(level, openolt_log_id, "Ingress(intfd_type %s, intf_id %d), Egress(intf_type %s, intf_id %d)\n", \
        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type), cfg.data.ingress_intf.intf_id,  \
        GET_FLOW_INTERFACE_TYPE(cfg.data.egress_intf.intf_type), cfg.data.egress_intf.intf_id); \
    OPENOLT_LOG(level, openolt_log_id, "classifier(o_vid %d, o_pbits %d, i_vid %d, i_pbits %d, ether type 0x%x)\n", \
        c_val.o_vid, c_val.o_pbits, c_val.i_vid, c_val.i_pbits,  classifier.eth_type()); \
    OPENOLT_LOG(level, openolt_log_id, "classifier(ip_proto 0x%x, gemport_id %d, src_port %d, dst_port %d, pkt_tag_type %s)\n", \
        c_val.ip_proto, gemport_id, c_val.src_port,  c_val.dst_port, GET_PKT_TAG_TYPE(c_val.pkt_tag_type)); \
    OPENOLT_LOG(level, openolt_log_id, "action(cmds_bitmask %s, o_vid %d, o_pbits %d, i_vid %d, i_pbits %d)\n\n", \
        get_flow_acton_command(a_val.cmds_bitmask), a_val.o_vid, a_val.o_pbits, a_val.i_vid, a_val.i_pbits); \
    } while(0)

#define FLOW_PARAM_LOG() \
    do { \
    OPENOLT_LOG(INFO, openolt_log_id, "--------> flow comparison (now before) <--------\n"); \
    OPENOLT_LOG(INFO, openolt_log_id, "flow_id (%d %d)\n", \
        key.flow_id, it->first.first); \
    OPENOLT_LOG(INFO, openolt_log_id, "onu_id (%d %lu)\n", \
        cfg.data.onu_id , get_flow_status(it->first.first, it->first.second, ONU_ID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "type (%d %lu)\n", \
        key.flow_type, get_flow_status(it->first.first, it->first.second, FLOW_TYPE)); \
    OPENOLT_LOG(INFO, openolt_log_id, "svc_port_id (%d %lu)\n", \
        cfg.data.svc_port_id, get_flow_status(it->first.first, it->first.second, SVC_PORT_ID));  \
    OPENOLT_LOG(INFO, openolt_log_id, "priority (%d %lu)\n", \
        cfg.data.priority, get_flow_status(it->first.first, it->first.second, PRIORITY)); \
    OPENOLT_LOG(INFO, openolt_log_id, "cookie (%lu %lu)\n", \
        cfg.data.cookie, get_flow_status(it->first.first, it->first.second, COOKIE)); \
    OPENOLT_LOG(INFO, openolt_log_id, "ingress intf_type (%s %s)\n", \
        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type), \
        GET_FLOW_INTERFACE_TYPE(get_flow_status(it->first.first, it->first.second, INGRESS_INTF_TYPE))); \
    OPENOLT_LOG(INFO, openolt_log_id, "ingress intf id (%d %lu)\n", \
        cfg.data.ingress_intf.intf_id , get_flow_status(it->first.first, it->first.second, INGRESS_INTF_ID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "egress intf_type (%d %lu)\n", \
        cfg.data.egress_intf.intf_type , get_flow_status(it->first.first, it->first.second, EGRESS_INTF_TYPE)); \
    OPENOLT_LOG(INFO, openolt_log_id, "egress intf_id (%d %lu)\n", \
        cfg.data.egress_intf.intf_id , get_flow_status(it->first.first, it->first.second, EGRESS_INTF_ID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier o_vid (%d %lu)\n", \
        c_val.o_vid , get_flow_status(it->first.first, it->first.second, CLASSIFIER_O_VID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier o_pbits (%d %lu)\n", \
        c_val.o_pbits , get_flow_status(it->first.first, it->first.second, CLASSIFIER_O_PBITS)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier i_vid (%d %lu)\n", \
        c_val.i_vid , get_flow_status(it->first.first, it->first.second, CLASSIFIER_I_VID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier i_pbits (%d %lu)\n", \
        c_val.i_pbits , get_flow_status(it->first.first, it->first.second, CLASSIFIER_I_PBITS)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier ether_type (0x%x 0x%lx)\n", \
        c_val.ether_type , get_flow_status(it->first.first, it->first.second, CLASSIFIER_ETHER_TYPE));  \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier ip_proto (%d %lu)\n", \
        c_val.ip_proto , get_flow_status(it->first.first, it->first.second, CLASSIFIER_IP_PROTO)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier src_port (%d %lu)\n", \
        c_val.src_port , get_flow_status(it->first.first, it->first.second, CLASSIFIER_SRC_PORT)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier dst_port (%d %lu)\n", \
        c_val.dst_port , get_flow_status(it->first.first, it->first.second, CLASSIFIER_DST_PORT)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier pkt_tag_type (%s %s)\n", \
        GET_PKT_TAG_TYPE(c_val.pkt_tag_type), \
        GET_PKT_TAG_TYPE(get_flow_status(it->first.first, it->first.second, CLASSIFIER_PKT_TAG_TYPE))); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier egress_qos type (%d %lu)\n", \
        cfg.data.egress_qos.type , get_flow_status(it->first.first, it->first.second, EGRESS_QOS_TYPE)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier egress_qos queue_id (%d %lu)\n", \
        cfg.data.egress_qos.u.fixed_queue.queue_id, \
        get_flow_status(it->first.first, it->first.second, EGRESS_QOS_QUEUE_ID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier egress_qos sched_id (%d %lu)\n", \
        cfg.data.egress_qos.tm_sched.id, \
        get_flow_status(it->first.first, it->first.second, EGRESS_QOS_TM_SCHED_ID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "classifier cmds_bitmask (%s %s)\n", \
        get_flow_acton_command(a_val.cmds_bitmask), \
        get_flow_acton_command(get_flow_status(it->first.first, it->first.second, ACTION_CMDS_BITMASK))); \
    OPENOLT_LOG(INFO, openolt_log_id, "action o_vid (%d %lu)\n", \
        a_val.o_vid , get_flow_status(it->first.first, it->first.second, ACTION_O_VID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "action i_vid (%d %lu)\n", \
        a_val.i_vid , get_flow_status(it->first.first, it->first.second, ACTION_I_VID)); \
    OPENOLT_LOG(INFO, openolt_log_id, "action o_pbits (%d %lu)\n", \
        a_val.o_pbits , get_flow_status(it->first.first, it->first.second, ACTION_O_PBITS)); \
    OPENOLT_LOG(INFO, openolt_log_id, "action i_pbits (%d %lu)\n\n", \
        a_val.i_pbits, get_flow_status(it->first.first, it->first.second, ACTION_I_PBITS)); \
    OPENOLT_LOG(INFO, openolt_log_id, "group_id (%d %lu)\n\n", \
        a_val.group_id, get_flow_status(it->first.first, it->first.second, GROUP_ID)); \
    } while(0)

#define COLLECTION_PERIOD 15 // in seconds
#define BAL_DYNAMIC_LIST_BUFFER_SIZE (32 * 1024)
#define MAX_REGID_LENGTH  36

#define BAL_RSC_MANAGER_BASE_TM_SCHED_ID 16384
#define MAX_TM_QMP_ID 16
#define TMQ_MAP_PROFILE_SIZE 8
#define MAX_TM_SCHED_ID 1023
#define MAX_SUBS_TM_SCHED_ID (MAX_SUPPORTED_PON == 16 ? MAX_TM_SCHED_ID-4-16 : MAX_TM_SCHED_ID-10-64)
#define EAP_ETHER_TYPE 34958
#define XGS_BANDWIDTH_GRANULARITY 16000
#define GPON_BANDWIDTH_GRANULARITY 32000
#define NUM_OF_PRIORITIES 8
#define NUMBER_OF_DEFAULT_INTERFACE_QUEUES 4 // <= NUM_OF_PRIORITIES
#define FILL_ARRAY(ARRAY,START,END,VALUE) for(int i=START;i<END;ARRAY[i++]=VALUE);
#define FILL_ARRAY2(ARRAY,START,END,VALUE) for(int i=START;i<END;ARRAY[i]=VALUE,i+=2);
#define COUNT_OF(array) (sizeof(array) / sizeof(array[0]))
#define NUMBER_OF_PBITS 8
#define MAX_NUMBER_OF_REPLICATED_FLOWS NUMBER_OF_PBITS
#define GRPC_THREAD_POOL_SIZE 150

#define GET_FLOW_INTERFACE_TYPE(type) \
       (type == BCMOLT_FLOW_INTERFACE_TYPE_PON) ? "PON" : \
       (type == BCMOLT_FLOW_INTERFACE_TYPE_NNI) ? "NNI" : \
       (type == BCMOLT_FLOW_INTERFACE_TYPE_HOST) ? "HOST" : "unknown"
#define GET_PKT_TAG_TYPE(type) \
       (type == BCMOLT_PKT_TAG_TYPE_UNTAGGED) ? "UNTAG" : \
       (type == BCMOLT_PKT_TAG_TYPE_SINGLE_TAG) ? "SINGLE_TAG" : \
       (type == BCMOLT_PKT_TAG_TYPE_DOUBLE_TAG) ? "DOUBLE_TAG" : "unknown"
#define GET_ACL_ACTION_TYPE(type) \
       (type == BCMOLT_ACCESS_CONTROL_FWD_ACTION_TYPE_TRAP_TO_HOST) ? "trap_to_host" : \
       (type == BCMOLT_ACCESS_CONTROL_FWD_ACTION_TYPE_DROP) ? "drop" : \
       (type == BCMOLT_ACCESS_CONTROL_FWD_ACTION_TYPE_REDIRECT) ? "redirction" : "unknown"
#define GET_ACL_MEMBERS_UPDATE_COMMAND(command) \
       (command == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD) ? "add" : \
       (command == BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE) ? "remove" : \
       (command == BCMOLT_MEMBERS_UPDATE_COMMAND_SET) ? "set" : "unknown"
#define GET_INTERFACE_TYPE(type) \
       (type == BCMOLT_INTERFACE_TYPE_PON) ? "PON" : \
       (type == BCMOLT_INTERFACE_TYPE_NNI) ? "NNI" : \
       (type == BCMOLT_INTERFACE_TYPE_HOST) ? "HOST" : "unknown"

#define LOGICAL_DISTANCE(MLD,EQD,TD) (MLD-(EQD*TD)*102) /* Round-trip time of 102 meters is 1us */
extern State state;
extern PonTrx ponTrx;

//***************************************//
// Function declations used by the core. //
//***************************************//
Status Enable_(int argc, char *argv[]);
Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir, bool omcc_encryption_mode);
Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status EnablePonIf_(uint32_t intf_id);
Status DisablePonIf_(uint32_t intf_id);
Status SetStateUplinkIf_(uint32_t intf_id, bool set_state);
uint32_t GetNniSpeed_(uint32_t intf_id);
unsigned NumNniIf_();
unsigned NumPonIf_();
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt);
Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, uint32_t gemport_id, const std::string pkt);
Status ProbeDeviceCapabilities_();
Status ProbePonIfTechnology_();
Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt);
Status FlowAddWrapper_(const openolt::Flow* request);
Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value,
                uint64_t cookie, int32_t group_id, uint32_t tech_profile_id, bool enable_encryption = false);
Status FlowRemoveWrapper_(const openolt::Flow* request);
Status FlowRemove_(uint32_t flow_id, const std::string flow_type);
Status Disable_();
Status Reenable_();
Status GetDeviceInfo_(openolt::DeviceInfo* device_info);
Status CreateTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds);
Status RemoveTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds);
Status CreateTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues);
Status RemoveTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues);
Status PerformGroupOperation_(const openolt::Group *group_cfg);
Status DeleteGroup_(uint32_t group_id);
Status OnuItuPonAlarmSet_(const config::OnuItuPonAlarm* request);
uint32_t GetPortNum_(uint32_t flow_id);
Status GetLogicalOnuDistanceZero_(uint32_t intf_id, openolt::OnuLogicalDistance* response);
Status GetLogicalOnuDistance_(uint32_t intf_id, uint32_t onu_id, openolt::OnuLogicalDistance* response);
Status GetOnuStatistics_(uint32_t intf_id, uint32_t onu_id, openolt::OnuStatistics *onu_stats);
Status GetGemPortStatistics_(uint32_t intf_id, uint32_t gemport_id, openolt::GemPortStatistics* gemport_stats);
Status GetPonRxPower_(uint32_t intf_id, uint32_t onu_id, openolt::PonRxPowerData* response);
Status GetOnuInfo_(uint32_t intf_id, uint32_t onu_id, openolt::OnuInfo *response);
Status GetPonInterfaceInfo_(uint32_t intf_id, openolt::PonIntfInfo *response);
Status GetPonPortStatistics_(uint32_t intf_id, common::PortStatistics* pon_stats);
Status GetNniPortStatistics_(uint32_t intf_id, common::PortStatistics* nni_stats);
int get_status_bcm_cli_quit(void);
uint16_t get_dev_id(void);
Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state, uint32_t speed);
uint64_t get_flow_status(uint16_t flow_id, uint16_t flow_type, uint16_t data_id);

void stats_collection();
Status check_connection();
Status check_bal_ready();
std::string get_ip_address(const char* nw_intf);

// Stubbed defntions of bcmolt_cfg_get required for unit-test
#ifdef TEST_MODE
extern bcmos_errno bcmolt_cfg_get__bal_state_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__onu_state_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__tm_sched_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__pon_intf_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__nni_intf_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__olt_topology_stub(bcmolt_oltid olt_id, void* ptr);
extern bcmos_errno bcmolt_cfg_get__flow_stub(bcmolt_oltid olt_id, void* ptr);
#endif //TEST_MODE

#endif

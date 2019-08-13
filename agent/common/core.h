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

#ifndef OPENOLT_CORE_H_
#define OPENOLT_CORE_H_

#include <grpc++/grpc++.h>
using grpc::Status;
#include <openolt.grpc.pb.h>

#include "state.h"

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

#define COLLECTION_PERIOD 15

extern State state;

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
    STATE = 26
};

Status Enable_(int argc, char *argv[]);
Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir);
Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status EnablePonIf_(uint32_t intf_id);
Status DisablePonIf_(uint32_t intf_id);
Status SetStateUplinkIf_(uint32_t intf_id, bool set_state);
unsigned NumNniIf_();
unsigned NumPonIf_();
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt);
Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, uint32_t gemport_id, const std::string pkt);
Status ProbeDeviceCapabilities_();
Status ProbePonIfTechnology_();
Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt, uint16_t flow_id);
Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value, uint64_t cookie);
Status FlowRemove_(uint32_t flow_id, const std::string flow_type);
Status Disable_();
Status Reenable_();
Status GetDeviceInfo_(openolt::DeviceInfo* device_info);
Status CreateTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds);
Status RemoveTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds);
Status CreateTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues);
Status RemoveTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues);
uint32_t GetPortNum_(uint32_t flow_id);
int get_status_bcm_cli_quit(void);
uint16_t get_dev_id(void); 
Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state);
uint64_t get_flow_status(uint16_t flow_id, uint16_t flow_type, uint16_t data_id);

void stats_collection();
#endif

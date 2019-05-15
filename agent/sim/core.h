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
#include "Queue.h"

extern Queue<openolt::Indication> oltIndQ;

Status Enable_(int argc, char *argv[]);
Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir);
Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status EnablePonIf_(uint32_t intf_id);
Status DisablePonIf_(uint32_t intf_id);
Status EnableUplinkIf_(uint32_t intf_id);
Status DisableUplinkIf_(uint32_t intf_id);
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt);
Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, const std::string pkt);
Status UplinkPacketOut_(bcmolt_flow_id flow_id, uint32_t intf_id, const std::string pkt);
Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value, uint64_t cookie);
Status Disable_();
Status Reenable_();


static Status SchedAdd_(int intf_id, int onu_id, int agg_port_id);
static Status SchedRemove_(int intf_id, int onu_id, int agg_port_id);

static inline int mk_sched_id(int onu_id) {
    return 1023 + onu_id;
}

static inline int mk_agg_port_id(int onu_id) {
    return 1023 + onu_id;
}

#endif

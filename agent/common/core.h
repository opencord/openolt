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

#define COLLECTION_PERIOD 15

extern State state;

Status Enable_(int argc, char *argv[]);
Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir,
    uint32_t alloc_id);
Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific);
Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific,
    uint32_t alloc_id);
Status EnablePonIf_(uint32_t intf_id);
Status DisablePonIf_(uint32_t intf_id);
Status EnableUplinkIf_(uint32_t intf_id);
Status DisableUplinkIf_(uint32_t intf_id);
unsigned NumNniIf_();
unsigned NumPonIf_();
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt);
Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt);
Status ProbeDeviceCapabilities_();
Status ProbePonIfTechnology_();
Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt);
Status FlowAdd_(int32_t onu_id,
                uint32_t flow_id, const std::string flow_type,
                int32_t access_intf_id, int32_t network_intf_id,
                uint32_t gemport_id, uint32_t sched_id,
                int32_t priority_value,
                const ::openolt::Classifier& classifier,
                const ::openolt::Action& action);
Status FlowRemove_(uint32_t flow_id, const std::string flow_type);
Status Disable_();
Status Reenable_();
Status GetDeviceInfo_(openolt::DeviceInfo* device_info);

void stats_collection();
#endif

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

Status Enable_() {
    return Status::OK;
}

Status Disable_() {
    return Status::OK;
}

Status Reenable_() {
    return Status::OK;
}

Status EnablePonIf_(uint32_t intf_id) {
    return Status::OK;
}

Status DisableUplinkIf_(uint32_t intf_id) {
    return Status::OK;
}

Status EnableUplinkIf_(uint32_t intf_id) {
    return Status::OK;
}

Status DisablePonIf_(uint32_t intf_id) {
    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir) {
    return Status::OK;
}

Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    return Status::OK;
}

Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    return Status::OK;;
}

Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    return Status::OK;
}

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    return Status::OK;
}

Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt) {
    return Status::OK;
}

Status FlowAdd_(uint32_t onu_id,
                uint32_t flow_id, const std::string flow_type,
                uint32_t access_intf_id, uint32_t network_intf_id,
                uint32_t gemport_id, uint32_t priority_value,
                const ::openolt::Classifier& classifier,
                const ::openolt::Action& action) {
    return Status::OK;
}

Status SchedAdd_(int intf_id, int onu_id, int agg_port_id) {
    return Status::OK;
}

Status SchedRemove_(int intf_id, int onu_id, int agg_port_id) {
    return Status::OK;
}

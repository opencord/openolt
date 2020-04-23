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

static std::string intf_technologies[MAX_SUPPORTED_PON];
static const std::string UNKNOWN_TECH("unknown");
static const std::string MIXED_TECH("mixed");
static std::string board_technology(UNKNOWN_TECH);
static std::string chip_family(UNKNOWN_TECH);
static std::string firmware_version = "Openolt.2019.07.01";

static bcmos_errno CreateSched(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, \
                          uint32_t port_no, uint32_t alloc_id, tech_profile::AdditionalBW additional_bw, uint32_t weight, \
                          uint32_t priority, tech_profile::SchedulingPolicy sched_policy,
                          tech_profile::TrafficShapingInfo traffic_shaping_info, uint32_t tech_profile_id);
static bcmos_errno RemoveSched(int intf_id, int onu_id, int uni_id, int alloc_id, std::string direction, int tech_profile_id);
static bcmos_errno CreateQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, \
                               bcmolt_egress_qos_type qos_type, uint32_t priority, uint32_t gemport_id, uint32_t tech_profile_id);
static bcmos_errno RemoveQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id, \
                               bcmolt_egress_qos_type qos_type, uint32_t priority, uint32_t gemport_id, uint32_t tech_profile_id);
static bcmos_errno CreateDefaultSched(uint32_t intf_id, const std::string direction);
static bcmos_errno CreateDefaultQueue(uint32_t intf_id, const std::string direction);

inline const char *get_flow_acton_command(uint32_t command) {
    char actions[200] = { };
    char *s_actions_ptr = actions;
    if (command & BCMOLT_ACTION_CMD_ID_ADD_OUTER_TAG) strcat(s_actions_ptr, "ADD_OUTER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_REMOVE_OUTER_TAG) strcat(s_actions_ptr, "REMOVE_OUTER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_XLATE_OUTER_TAG) strcat(s_actions_ptr, "TRANSLATE_OUTER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_ADD_INNER_TAG) strcat(s_actions_ptr, "ADD_INNTER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_REMOVE_INNER_TAG) strcat(s_actions_ptr, "REMOVE_INNER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_XLATE_INNER_TAG) strcat(s_actions_ptr, "TRANSLATE_INNER_TAG|");
    if (command & BCMOLT_ACTION_CMD_ID_REMARK_OUTER_PBITS) strcat(s_actions_ptr, "REMOVE_OUTER_PBITS|");
    if (command & BCMOLT_ACTION_CMD_ID_REMARK_INNER_PBITS) strcat(s_actions_ptr, "REMAKE_INNER_PBITS|");
    return s_actions_ptr;
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
    OPENOLT_LOG(INFO, openolt_log_id, "Fetched device serial number %s\n", serial_number);
    device_info->set_device_serial_number(serial_number);

    char device_id[OPENOLT_FIELD_LEN];
    memset(device_id, '\0', OPENOLT_FIELD_LEN);
    openolt_read_sysinfo("MAC", device_id);
    OPENOLT_LOG(INFO, openolt_log_id, "Fetched device mac address %s\n", device_id);
    device_info->set_device_id(device_id);

    // Legacy, device-wide ranges. To be deprecated when adapter
    // is upgraded to support per-interface ranges
    if (board_technology == "XGS-PON") {
        device_info->set_onu_id_start(1);
        device_info->set_onu_id_end(255);
        device_info->set_alloc_id_start(MIN_ALLOC_ID_XGSPON);
        device_info->set_alloc_id_end(16383);
        device_info->set_gemport_id_start(1024);
        device_info->set_gemport_id_end(65535);
        device_info->set_flow_id_start(1);
        device_info->set_flow_id_end(16383);
    }
    else if (board_technology == "GPON") {
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

            if (intf_technology == "XGS-PON") {
                openolt::DeviceInfo::DeviceResourceRanges::Pool* pool;

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ONU_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1);
                pool->set_end(255);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ALLOC_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1024);
                pool->set_end(16383);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::GEMPORT_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1024);
                pool->set_end(65535);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::FLOW_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::SHARED_BY_ALL_INTF_ALL_TECH);
                pool->set_start(1);
                pool->set_end(16383);
            }
            else if (intf_technology == "GPON") {
                openolt::DeviceInfo::DeviceResourceRanges::Pool* pool;

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ONU_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(1);
                pool->set_end(127);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::ALLOC_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
                pool->set_start(256);
                pool->set_end(757);

                pool = range->add_pools();
                pool->set_type(openolt::DeviceInfo::DeviceResourceRanges::Pool::GEMPORT_ID);
                pool->set_sharing(openolt::DeviceInfo::DeviceResourceRanges::Pool::DEDICATED_PER_INTF);
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
    bcmos_errno err;
    bcmolt_host_init_parms init_parms = {};
    init_parms.transport.type = BCM_HOST_API_CONN_LOCAL;
    unsigned int failed_enable_device_cnt = 0;

    if (!state.is_activated()) {

        vendor_init();
        /* Initialize host subsystem */
        err = bcmolt_host_init(&init_parms);
        if (BCM_ERR_OK != err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to init OLT, err = %s\n",bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to init OLT");
        }

        bcmcli_session_parm mon_session_parm;
        /* Create CLI session */
        memset(&mon_session_parm, 0, sizeof(mon_session_parm));
        mon_session_parm.get_prompt = openolt_cli_get_prompt_cb;
        mon_session_parm.access_right = BCMCLI_ACCESS_ADMIN;
        bcmos_errno rc = bcmcli_session_open(&mon_session_parm, &current_session);
        BUG_ON(rc != BCM_ERR_OK);

        /* API CLI */
        bcm_openolt_api_cli_init(NULL, current_session);

        /* Add quit command */
        BCMCLI_MAKE_CMD_NOPARM(NULL, "quit", "Quit", bcm_cli_quit);

        err = bcmolt_apiend_cli_init();
        if (BCM_ERR_OK != err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to add apiend init, err = %s\n",bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to add apiend init");
        }

        bcmos_fastlock_init(&data_lock, 0);
        bcmos_fastlock_init(&alloc_cfg_wait_lock, 0);
        bcmos_fastlock_init(&onu_deactivate_wait_lock, 0);
        OPENOLT_LOG(INFO, openolt_log_id, "Enable OLT - %s-%s\n", VENDOR_ID, MODEL_ID);

        //check BCM daemon is connected or not
        Status status = check_connection();
        if (!status.ok()) {
            return status;
        }
        else {
            Status status = SubscribeIndication();
            if (!status.ok()) {
                OPENOLT_LOG(ERROR, openolt_log_id, "SubscribeIndication failed - %s : %s\n",
                    grpc_status_code_to_string(status.error_code()).c_str(),
                    status.error_message().c_str());
                return status;
            }

            //check BAL state in initial stage
            status = check_bal_ready();
            if (!status.ok()) {
                return status;
            }
        }

        {
            bcmos_errno err;
            bcmolt_odid dev;
            OPENOLT_LOG(INFO, openolt_log_id, "Enabling PON %d Devices ... \n", BCM_MAX_DEVS_PER_LINE_CARD);
            for (dev = 0; dev < BCM_MAX_DEVS_PER_LINE_CARD; dev++) {
                bcmolt_device_cfg dev_cfg = { };
                bcmolt_device_key dev_key = { };
                dev_key.device_id = dev;
                BCMOLT_CFG_INIT(&dev_cfg, device, dev_key);
                BCMOLT_MSG_FIELD_GET(&dev_cfg, system_mode);
                err = bcmolt_cfg_get(dev_id, &dev_cfg.hdr);
                if (err == BCM_ERR_NOT_CONNECTED) {
                    bcmolt_device_key key = {.device_id = dev};
                    bcmolt_device_connect oper;
                    BCMOLT_OPER_INIT(&oper, device, connect, key);
                    if (MODEL_ID == "asfvolt16") {
                        BCMOLT_MSG_FIELD_SET(&oper, inni_config.mode, BCMOLT_INNI_MODE_ALL_10_G_XFI);
                        BCMOLT_MSG_FIELD_SET (&oper, system_mode, BCMOLT_SYSTEM_MODE_XGS__2_X);
                    } else if (MODEL_ID == "asgvolt64") {
                        BCMOLT_MSG_FIELD_SET(&oper, inni_config.mode, BCMOLT_INNI_MODE_ALL_10_G_XFI);
                        BCMOLT_MSG_FIELD_SET(&oper, inni_config.mux, BCMOLT_INNI_MUX_FOUR_TO_ONE);
                        BCMOLT_MSG_FIELD_SET (&oper, system_mode, BCMOLT_SYSTEM_MODE_GPON__16_X);
                    }
                    err = bcmolt_oper_submit(dev_id, &oper.hdr);
                    if (err) {
                        failed_enable_device_cnt ++;
                        OPENOLT_LOG(ERROR, openolt_log_id, "Enable PON device %d failed, err = %s\n", dev, bcmos_strerror(err));
                        if (failed_enable_device_cnt == BCM_MAX_DEVS_PER_LINE_CARD) {
                            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to enable all the pon ports, err = %s\n", bcmos_strerror(err));
                            return Status(grpc::StatusCode::INTERNAL, "Failed to activate all PON ports");
                        }
                    }
                    bcmos_usleep(200000);
                }
                else {
                    OPENOLT_LOG(WARNING, openolt_log_id, "PON deivce %d already connected\n", dev);
                    state.activate();
                }
            }
            init_stats();
        }
    }

    /* Start CLI */
    OPENOLT_LOG(INFO, def_log_id, "Starting CLI\n");
    //If already enabled, generate an extra indication ????
    return Status::OK;
}

Status Disable_() {
    //In the earlier implementation Disabling olt is done by disabling the NNI port associated with that.
    //In inband scenario instead of using management interface to establish connection with adapter ,NNI interface will be used.
    //Disabling NNI port on olt disable causes connection loss between adapter and agent.
    //To overcome this disable is implemented by disabling all the PON ports
    //associated with the device so as to support both in-band
    //and out of band scenarios.

    Status status;
    int failedCount = 0;
    for (int i = 0; i < NumPonIf_(); i++) {
        status = DisablePonIf_(i);
        if (!status.ok()) {
            failedCount+=1;
            BCM_LOG(ERROR, openolt_log_id, "Failed to disable PON interface: %d\n", i);
        }
    }
    if (failedCount == 0) {
        state.deactivate();
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("down");
        ind.set_allocated_olt_ind(olt_ind);
        BCM_LOG(INFO, openolt_log_id, "Disable OLT, add an extra indication\n");
        oltIndQ.push(ind);
        return Status::OK;
    }
    if (failedCount ==NumPonIf_()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "failed to disable olt ,all the PON ports are still in enabled state");
    }

    return grpc::Status(grpc::StatusCode::UNKNOWN, "failed to disable olt ,few PON ports are still in enabled state");
}

Status Reenable_() {
    Status status;
    int failedCount = 0;
    for (int i = 0; i < NumPonIf_(); i++) {
        status = EnablePonIf_(i);
        if (!status.ok()) {
            failedCount+=1;
            BCM_LOG(ERROR, openolt_log_id, "Failed to enable PON interface: %d\n", i);
        }
    }
    if (failedCount == 0) {
        state.activate();
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("up");
        ind.set_allocated_olt_ind(olt_ind);
        BCM_LOG(INFO, openolt_log_id, "Reenable OLT, add an extra indication\n");
        oltIndQ.push(ind);
        return Status::OK;
    }
    if (failedCount ==NumPonIf_()) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "failed to re-enable olt ,all the PON ports are still in disabled state");
    }
    return grpc::Status(grpc::StatusCode::UNKNOWN, "failed to re-enable olt ,few PON ports are still in disabled state");
}

inline uint64_t get_flow_status(uint16_t flow_id, uint16_t flow_type, uint16_t data_id) {
    bcmos_errno err;
    bcmolt_flow_key flow_key;
    bcmolt_flow_cfg flow_cfg;

    flow_key.flow_id = flow_id;
    flow_key.flow_type = (bcmolt_flow_type)flow_type;

    BCMOLT_CFG_INIT(&flow_cfg, flow, flow_key);

    switch (data_id) {
        case ONU_ID: //onu_id
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, onu_id);
            #ifdef TEST_MODE
            // It is impossible to mock the setting of flow_cfg.data.state because
            // the actual bcmolt_cfg_get passes the address of flow_cfg.hdr and we cannot
            // set the flow_cfg.data. So a new stub function is created and address
            // of flow_cfg is passed. This is one-of case where we need to add test specific
            // code in production code.
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get onu_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.onu_id;
        case FLOW_TYPE:
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get flow_type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.key.flow_type;
        case SVC_PORT_ID: //svc_port_id
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, svc_port_id);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get svc_port_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.svc_port_id;
        case PRIORITY:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, priority);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get priority, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.priority;
        case COOKIE: //cookie
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, cookie);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get cookie, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.cookie;
        case INGRESS_INTF_TYPE: //ingress intf_type
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, ingress_intf);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get ingress intf_type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.ingress_intf.intf_type;
        case EGRESS_INTF_TYPE: //egress intf_type
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, egress_intf);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get egress intf_type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.egress_intf.intf_type;
        case INGRESS_INTF_ID: //ingress intf_id
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, ingress_intf);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get ingress intf_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.ingress_intf.intf_id;
        case EGRESS_INTF_ID: //egress intf_id
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, egress_intf);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get egress intf_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.egress_intf.intf_id;
        case CLASSIFIER_O_VID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier o_vid, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.o_vid;
        case CLASSIFIER_O_PBITS:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier o_pbits, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.o_pbits;
        case CLASSIFIER_I_VID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier i_vid, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.i_vid;
        case CLASSIFIER_I_PBITS:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier i_pbits, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.i_pbits;
        case CLASSIFIER_ETHER_TYPE:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier ether_type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.ether_type;
        case CLASSIFIER_IP_PROTO:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier ip_proto, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.ip_proto;
        case CLASSIFIER_SRC_PORT:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier src_port, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.src_port;
        case CLASSIFIER_DST_PORT:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier dst_port, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.dst_port;
        case CLASSIFIER_PKT_TAG_TYPE:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, classifier);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get classifier pkt_tag_type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.classifier.pkt_tag_type;
        case EGRESS_QOS_TYPE:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, egress_qos);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get egress_qos type, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.egress_qos.type;
        case EGRESS_QOS_QUEUE_ID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, egress_qos);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get egress_qos queue_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            switch (flow_cfg.data.egress_qos.type) {
                case BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE:
                    return flow_cfg.data.egress_qos.u.fixed_queue.queue_id;
                case BCMOLT_EGRESS_QOS_TYPE_TC_TO_QUEUE:
                    return flow_cfg.data.egress_qos.u.tc_to_queue.tc_to_queue_id;
                case BCMOLT_EGRESS_QOS_TYPE_PBIT_TO_TC:
                    return flow_cfg.data.egress_qos.u.pbit_to_tc.tc_to_queue_id;
                case BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE:
                    return flow_cfg.data.egress_qos.u.priority_to_queue.tm_q_set_id;
                case BCMOLT_EGRESS_QOS_TYPE_NONE:
                default:
                    return -1;
            }
        case EGRESS_QOS_TM_SCHED_ID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, egress_qos);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get egress_qos tm_sched_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.egress_qos.tm_sched.id;
        case ACTION_CMDS_BITMASK:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, action);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get action cmds_bitmask, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.action.cmds_bitmask;
        case ACTION_O_VID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, action);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get action o_vid, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.action.o_vid;
        case ACTION_O_PBITS:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, action);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get action o_pbits, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.action.o_pbits;
        case ACTION_I_VID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, action);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get action i_vid, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.action.i_vid;
        case ACTION_I_PBITS:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, action);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get action i_pbits, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.action.i_pbits;
        case STATE:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, state);
            #ifdef TEST_MODE
            err = bcmolt_cfg_get__flow_stub(dev_id, &flow_cfg);
            #else
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            #endif
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get state, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.state;
        case GROUP_ID:
            BCMOLT_FIELD_SET_PRESENT(&flow_cfg.data, flow_cfg_data, group_id);
            err = bcmolt_cfg_get(dev_id, &flow_cfg.hdr);
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to get group_id, err = %s\n",bcmos_strerror(err));
                return err;
            }
            return flow_cfg.data.group_id;
        default:
            return BCM_ERR_INTERNAL;
    }

    return err;
}

Status EnablePonIf_(uint32_t intf_id) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_pon_interface_cfg interface_obj;
    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)intf_id};
    bcmolt_pon_interface_set_pon_interface_state pon_interface_set_state;
    bcmolt_interface_state state;
    bcmolt_status los_status;

    err = get_pon_interface_status((bcmolt_interface)intf_id, &state, &los_status);
    if (err == BCM_ERR_OK) {
        if (state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING) {
            OPENOLT_LOG(WARNING, openolt_log_id, "PON interface: %d already enabled\n", intf_id);
            return Status::OK;
        }
    }
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);
    BCMOLT_OPER_INIT(&pon_interface_set_state, pon_interface, set_pon_interface_state, intf_key);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.control, BCMOLT_CONTROL_STATE_ENABLE);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.interval, 5000);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.onu_post_discovery_mode,
        BCMOLT_ONU_POST_DISCOVERY_MODE_NONE);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.los, true);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.onu_alarms, true);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.tiwi, true);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.ack_timeout, true);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.sfi, true);
    BCMOLT_MSG_FIELD_SET(&interface_obj, itu.automatic_onu_deactivation.loki, true);
    BCMOLT_FIELD_SET(&pon_interface_set_state.data, pon_interface_set_pon_interface_state_data,
        operation, BCMOLT_INTERFACE_OPERATION_ACTIVE_WORKING);

    err = bcmolt_cfg_set(dev_id, &interface_obj.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to enable discovery onu, PON interface %d, err = %s\n", intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Failed to enable discovery onu");
    }
    err = bcmolt_oper_submit(dev_id, &pon_interface_set_state.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to enable PON interface: %d, err = %s\n", intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Failed to enable PON interface");
    }
    else {
        OPENOLT_LOG(INFO, openolt_log_id, "Successfully enabled PON interface: %d\n", intf_id);
        OPENOLT_LOG(INFO, openolt_log_id, "Initializing tm sched creation for PON interface: %d\n", intf_id);
        CreateDefaultSched(intf_id, downstream);
        CreateDefaultQueue(intf_id, downstream);
    }

    return Status::OK;
}

Status ProbeDeviceCapabilities_() {
    bcmos_errno err;
    bcmolt_device_cfg dev_cfg = { };
    bcmolt_device_key dev_key = { };
    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };
    bcmolt_topology_map topo_map[BCM_MAX_PONS_PER_OLT] = { };
    bcmolt_topology topo = { };

    topo.topology_maps.len = BCM_MAX_PONS_PER_OLT;
    topo.topology_maps.arr = &topo_map[0];
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    BCMOLT_MSG_FIELD_GET(&olt_cfg, bal_state);
    BCMOLT_FIELD_SET_PRESENT(&olt_cfg.data, olt_cfg_data, topology);
    BCMOLT_CFG_LIST_BUF_SET(&olt_cfg, olt, topo.topology_maps.arr,
        sizeof(bcmolt_topology_map) * topo.topology_maps.len);
    #ifdef TEST_MODE
        // It is impossible to mock the setting of olt_cfg.data.bal_state because
        // the actual bcmolt_cfg_get passes the address of olt_cfg.hdr and we cannot
        // set the olt_cfg.data.topology. So a new stub function is created and address
        // of olt_cfg is passed. This is one-of case where we need to test add specific
        // code in production code.
    err = bcmolt_cfg_get__olt_topology_stub(dev_id, &olt_cfg);
    #else
    err = bcmolt_cfg_get_mult_retry(dev_id, &olt_cfg.hdr);
    #endif
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "cfg: Failed to query OLT topology, err = %s\n", bcmos_strerror(err));
        return bcm_to_grpc_err(err, "cfg: Failed to query OLT topology");
    }

    num_of_nni_ports = olt_cfg.data.topology.num_switch_ports;
    num_of_pon_ports = olt_cfg.data.topology.topology_maps.len;

    OPENOLT_LOG(INFO, openolt_log_id, "OLT capabilitites, oper_state: %s\n",
            olt_cfg.data.bal_state == BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY
            ? "up" : "down");

    OPENOLT_LOG(INFO, openolt_log_id, "topology nni: %d pon: %d dev: %d\n",
            num_of_nni_ports,
            num_of_pon_ports,
            BCM_MAX_DEVS_PER_LINE_CARD);

    uint32_t num_failed_cfg_gets = 0;
    static std::string openolt_version = firmware_version;
    for (int devid = 0; devid < BCM_MAX_DEVS_PER_LINE_CARD; devid++) {
        dev_key.device_id = devid;
        BCMOLT_CFG_INIT(&dev_cfg, device, dev_key);
        BCMOLT_MSG_FIELD_GET(&dev_cfg, firmware_sw_version);
        BCMOLT_MSG_FIELD_GET(&dev_cfg, chip_family);
        BCMOLT_MSG_FIELD_GET(&dev_cfg, system_mode);
        err = bcmolt_cfg_get_mult_retry(dev_id, &dev_cfg.hdr);
        if (err) {
            OPENOLT_LOG(WARNING, openolt_log_id,"Failed to query PON MAC Device %d (errno = %s). Skipping the device.\n", devid, bcmos_strerror(err));
            num_failed_cfg_gets++;
            continue;
        }

        std::string bal_version;
        bal_version += std::to_string(dev_cfg.data.firmware_sw_version.major)
                    + "." + std::to_string(dev_cfg.data.firmware_sw_version.minor)
                    + "." + std::to_string(dev_cfg.data.firmware_sw_version.revision);
        firmware_version = "BAL." + bal_version + "__" + openolt_version;

        switch(dev_cfg.data.system_mode) {
            case 10: board_technology = "GPON"; FILL_ARRAY(intf_technologies,devid*4,(devid+1)*4,"GPON"); break;
            case 11: board_technology = "GPON"; FILL_ARRAY(intf_technologies,devid*8,(devid+1)*8,"GPON"); break;
            case 12: board_technology = "GPON"; FILL_ARRAY(intf_technologies,devid*16,(devid+1)*16,"GPON"); break;
            case 13: board_technology = "XGPON"; FILL_ARRAY(intf_technologies,devid*2,(devid+1)*2,"XGPON"); break;
            case 14: board_technology = "XGPON"; FILL_ARRAY(intf_technologies,devid*4,(devid+1)*4,"XGPON"); break;
            case 15: board_technology = "XGPON"; FILL_ARRAY(intf_technologies,devid*8,(devid+1)*8,"XGPON"); break;
            case 16: board_technology = "XGPON"; FILL_ARRAY(intf_technologies,devid*16,(devid+1)*16,"XGPON"); break;
            case 18: board_technology = "XGS-PON"; FILL_ARRAY(intf_technologies,devid*2,(devid+1)*2,"XGS-PON"); break;
            case 19: board_technology = "XGS-PON"; FILL_ARRAY(intf_technologies,devid*16,(devid+1)*16,"XGS-PON"); break;
            case 20: board_technology = MIXED_TECH; FILL_ARRAY(intf_technologies,devid*2,(devid+1)*2,MIXED_TECH); break;
        }

        switch(dev_cfg.data.chip_family) {
            case BCMOLT_CHIP_FAMILY_CHIP_FAMILY_6862_X: chip_family = "Maple"; break;
            case BCMOLT_CHIP_FAMILY_CHIP_FAMILY_6865_X: chip_family = "Aspen"; break;
        }

        OPENOLT_LOG(INFO, openolt_log_id, "device %d, pon: %d, version %s, family: %s, board_technology: %s\n",
            devid, BCM_MAX_PONS_PER_DEV, bal_version.c_str(), chip_family.c_str(), board_technology.c_str());

        bcmos_usleep(500000);
    }

    /* If all the devices returned errors then we tell the caller that this is an error else we work with
       only the devices that retured success*/
    if (num_failed_cfg_gets == BCM_MAX_DEVS_PER_LINE_CARD) {
        OPENOLT_LOG(ERROR, openolt_log_id, "device: Query of all the devices failed\n");
        return bcm_to_grpc_err(err, "device: All devices failed query");
    }

    return Status::OK;
}

Status SetStateUplinkIf_(uint32_t intf_id, bool set_state) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_nni_interface_key intf_key = {.id = (bcmolt_interface)intf_id};
    bcmolt_nni_interface_set_nni_state nni_interface_set_state;
    bcmolt_interface_state state;

    err = get_nni_interface_status((bcmolt_interface)intf_id, &state);
    if (err == BCM_ERR_OK) {
        if (set_state && state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING) {
            OPENOLT_LOG(WARNING, openolt_log_id, "NNI interface: %d already enabled\n", intf_id);
            OPENOLT_LOG(INFO, openolt_log_id, "Initializing tm sched creation for NNI interface: %d\n", intf_id);
            CreateDefaultSched(intf_id, upstream);
            CreateDefaultQueue(intf_id, upstream);
            return Status::OK;
        } else if (!set_state && state == BCMOLT_INTERFACE_STATE_INACTIVE) {
            OPENOLT_LOG(INFO, openolt_log_id, "NNI interface: %d already disabled\n", intf_id);
            return Status::OK;
        }
    }

    BCMOLT_OPER_INIT(&nni_interface_set_state, nni_interface, set_nni_state, intf_key);
    if (set_state) {
        BCMOLT_FIELD_SET(&nni_interface_set_state.data, nni_interface_set_nni_state_data,
            nni_state, BCMOLT_INTERFACE_OPERATION_ACTIVE_WORKING);
    } else {
        BCMOLT_FIELD_SET(&nni_interface_set_state.data, nni_interface_set_nni_state_data,
            nni_state, BCMOLT_INTERFACE_OPERATION_INACTIVE);
    }
    err = bcmolt_oper_submit(dev_id, &nni_interface_set_state.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to %s NNI interface: %d, err = %s\n",
            (set_state)?"enable":"disable", intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Failed to enable NNI interface");
    }
    else {
        OPENOLT_LOG(INFO, openolt_log_id, "Successfully %s NNI interface: %d\n", (set_state)?"enable":"disable", intf_id);
        if (set_state) {
            OPENOLT_LOG(INFO, openolt_log_id, "Initializing tm sched creation for NNI interface: %d\n", intf_id);
            CreateDefaultSched(intf_id, upstream);
            CreateDefaultQueue(intf_id, upstream);
        }
    }

    return Status::OK;
}

Status DisablePonIf_(uint32_t intf_id) {
    bcmos_errno err;
    bcmolt_pon_interface_cfg interface_obj;
    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)intf_id};
    bcmolt_pon_interface_set_pon_interface_state pon_interface_set_state;

    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);
    BCMOLT_OPER_INIT(&pon_interface_set_state, pon_interface, set_pon_interface_state, intf_key);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.control, BCMOLT_CONTROL_STATE_DISABLE);

    err = bcmolt_cfg_set(dev_id, &interface_obj.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to disable discovery of onu, PON interface %d, err %d\n", intf_id, err);
        return bcm_to_grpc_err(err, "Failed to disable discovery of onu");
    }

    BCMOLT_FIELD_SET(&pon_interface_set_state.data, pon_interface_set_pon_interface_state_data,
    operation, BCMOLT_INTERFACE_OPERATION_INACTIVE);

    err = bcmolt_oper_submit(dev_id, &pon_interface_set_state.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to disable PON interface: %d\n , err %d\n", intf_id, err);
        return bcm_to_grpc_err(err, "Failed to disable PON interface");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Successfully disabled PON interface: %d\n", intf_id);
    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    bcmolt_serial_number serial_number; /**< ONU serial number */
    bcmolt_bin_str_36 registration_id; /**< ONU registration ID */

    bcmolt_onu_set_onu_state onu_oper; /* declare main API struct */
    bcmolt_onu_state onu_state;

    onu_key.onu_id = onu_id;
    onu_key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);
#ifdef TEST_MODE
    // It is impossible to mock the setting of onu_cfg.data.onu_state because
    // the actual bcmolt_cfg_get passes the address of onu_cfg.hdr and we cannot
    // set the onu_cfg.data.onu_state. So a new stub function is created and address
    // of onu_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__onu_state_stub(dev_id, &onu_cfg);
#else
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
#endif
    OPENOLT_LOG(INFO, openolt_log_id, "Activate ONU : old state = %d, current state = %d\n",
            onu_cfg.data.onu_old_state, onu_cfg.data.onu_state);
    if (err == BCM_ERR_OK) {
        if (onu_cfg.data.onu_state == BCMOLT_ONU_STATE_ACTIVE) {
            OPENOLT_LOG(INFO, openolt_log_id, "ONU is already in ACTIVE state, \
not processing this request for pon_intf=%d onu_id=%d\n", intf_id, onu_id);
            return Status::OK;
        } else if (onu_cfg.data.onu_state != BCMOLT_ONU_STATE_NOT_CONFIGURED &&
                onu_cfg.data.onu_state != BCMOLT_ONU_STATE_INACTIVE) {
            // We need the ONU to be in NOT_CONFIGURED or INACTIVE state to further process the request
            OPENOLT_LOG(ERROR, openolt_log_id, "ONU in an invalid state to process the request, \
state=%d pon_intf=%d onu_id=%d\n", onu_cfg.data.onu_state, intf_id, onu_id);
            return bcm_to_grpc_err(err, "Failed to activate ONU, invalid ONU state");
        }
    } else {
        // This should never happen. BAL GET should succeed for non-existant ONUs too. The state of such ONUs will be NOT_CONFIGURED
        OPENOLT_LOG(ERROR, openolt_log_id, "ONU state query failed pon_intf=%d onu_id=%d\n", intf_id, onu_id);
        return bcm_to_grpc_err(err, "onu get failed");
    }

    // If the ONU is not configured at all we need to first configure it
    if (onu_cfg.data.onu_state == BCMOLT_ONU_STATE_NOT_CONFIGURED) {
        OPENOLT_LOG(INFO, openolt_log_id,  "Configuring ONU %d on PON %d : vendor id %s, \
vendor specific %s, pir %d\n", onu_id, intf_id, vendor_id,
                vendor_specific_to_str(vendor_specific).c_str(), pir);

        memcpy(serial_number.vendor_id.arr, vendor_id, 4);
        memcpy(serial_number.vendor_specific.arr, vendor_specific, 4);
        BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
        BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.serial_number, serial_number);
        BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.auto_learning, BCMOS_TRUE);
        /*set burst and data profiles to fec disabled*/
        if (board_technology == "XGS-PON") {
            BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.xgpon.ranging_burst_profile, 2);
            BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.xgpon.data_burst_profile, 1);
        } else if (board_technology == "GPON") {
            BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.gpon.ds_ber_reporting_interval, 1000000);
            BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.gpon.omci_port_id, onu_id);
        }
        err = bcmolt_cfg_set(dev_id, &onu_cfg.hdr);
        if (err != BCM_ERR_OK) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to configure ONU %d on PON %d, err = %s\n", onu_id, intf_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to configure ONU");
        }
    }

    // Now that the ONU is configured, move the ONU to ACTIVE state
    memset(&onu_cfg, 0, sizeof(bcmolt_onu_cfg));
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);
    BCMOLT_OPER_INIT(&onu_oper, onu, set_onu_state, onu_key);
    BCMOLT_FIELD_SET(&onu_oper.data, onu_set_onu_state_data,
            onu_state, BCMOLT_ONU_OPERATION_ACTIVE);
    err = bcmolt_oper_submit(dev_id, &onu_oper.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to activate ONU %d on PON %d, err = %s\n", onu_id, intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Failed to activate ONU");
    }
    // ONU will eventually get activated after we have submitted the operation request. The adapter will receive an asynchronous
    // ONU_ACTIVATION_COMPLETED_INDICATION

    OPENOLT_LOG(INFO, openolt_log_id, "Activated ONU, onu_id %d on PON %d\n", onu_id, intf_id);

    return Status::OK;
}

Status DeactivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_onu_set_onu_state onu_oper; /* declare main API struct */
    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key; /**< Object key. */
    bcmolt_onu_state onu_state;

    onu_key.onu_id = onu_id;
    onu_key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of onu_cfg.data.onu_state because
    // the actual bcmolt_cfg_get passes the address of onu_cfg.hdr and we cannot
    // set the onu_cfg.data.onu_state. So a new stub function is created and address
    // of onu_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__onu_state_stub(dev_id, &onu_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
    #endif
    onu_state = onu_cfg.data.onu_state;
    if (err == BCM_ERR_OK) {
        switch (onu_state) {
            case BCMOLT_ONU_STATE_ACTIVE:
                BCMOLT_OPER_INIT(&onu_oper, onu, set_onu_state, onu_key);
                BCMOLT_FIELD_SET(&onu_oper.data, onu_set_onu_state_data,
                    onu_state, BCMOLT_ONU_OPERATION_INACTIVE);
                err = bcmolt_oper_submit(dev_id, &onu_oper.hdr);
                if (err != BCM_ERR_OK) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Failed to deactivate ONU %d on PON %d, err = %s\n", onu_id, intf_id, bcmos_strerror(err));
                    return bcm_to_grpc_err(err, "Failed to deactivate ONU");
                }
                OPENOLT_LOG(INFO, openolt_log_id, "Deactivated ONU, onu_id %d on PON %d\n", onu_id, intf_id);
                break;
        }
    }

    return Status::OK;
}

Status DeleteOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_onu_state onu_state;

    OPENOLT_LOG(INFO, openolt_log_id,  "DeleteOnu ONU %d on PON %d : vendor id %s, vendor specific %s\n",
        onu_id, intf_id, vendor_id, vendor_specific_to_str(vendor_specific).c_str());

    // Need to deactivate before removing it (BAL rules)
    DeactivateOnu_(intf_id, onu_id, vendor_id, vendor_specific);

    err = get_onu_status((bcmolt_interface)intf_id, onu_id, &onu_state);
    if (err == BCM_ERR_OK) {
        if (onu_state != BCMOLT_ONU_STATE_INACTIVE) {
            OPENOLT_LOG(INFO, openolt_log_id, "waiting for onu deactivate complete response: intf_id=%d, onu_id=%d\n",
                intf_id, onu_id);
            err = wait_for_onu_deactivate_complete(intf_id, onu_id);
            if (err) {
                OPENOLT_LOG(ERROR, openolt_log_id, "failed to delete onu intf_id %d, onu_id %d\n",
                        intf_id, onu_id);
                return bcm_to_grpc_err(err, "Failed to delete ONU");
            }
        }
        else {
            OPENOLT_LOG(INFO, openolt_log_id, "Onu is Inactive, onu_id: %d, not waiting for onu deactivate complete response\n",
                intf_id);
        }
    }
    else {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to fetch Onu status, onu_id = %d, intf_id = %d, err = %s\n",
            onu_id, intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Failed to delete ONU");
    }

    bcmolt_onu_cfg cfg_obj;
    bcmolt_onu_key key;

    OPENOLT_LOG(INFO, openolt_log_id, "Processing onu cfg clear for onu_id %d  and intf_id %d\n",
        onu_id, intf_id);

    key.onu_id = onu_id;
    key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&cfg_obj, onu, key);

    err = bcmolt_cfg_clear(dev_id, &cfg_obj.hdr);
    if (err != BCM_ERR_OK)
    {
       OPENOLT_LOG(ERROR, openolt_log_id, "Failed to clear information for BAL onu_id %d, Interface ID %d, err = %s\n", onu_id, intf_id, bcmos_strerror(err));
        return Status(grpc::StatusCode::INTERNAL, "Failed to delete ONU");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Deleted ONU, onu_id %d on PON %d\n", onu_id, intf_id);
    return Status::OK;
}

#define MAX_CHAR_LENGTH  20
#define MAX_OMCI_MSG_LENGTH 44
Status OmciMsgOut_(uint32_t intf_id, uint32_t onu_id, const std::string pkt) {
    bcmolt_bin_str buf = {};
    bcmolt_onu_cpu_packets omci_cpu_packets;
    bcmolt_onu_key key;

    key.pon_ni = intf_id;
    key.onu_id = onu_id;

    BCMOLT_OPER_INIT(&omci_cpu_packets, onu, cpu_packets, key);
    BCMOLT_MSG_FIELD_SET(&omci_cpu_packets, packet_type, BCMOLT_PACKET_TYPE_OMCI);
    BCMOLT_MSG_FIELD_SET(&omci_cpu_packets, calc_crc, BCMOS_TRUE);

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

    buf.arr = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
    memcpy(buf.arr, (uint8_t *)arraySend, buf.len);

    BCMOLT_MSG_FIELD_SET(&omci_cpu_packets, number_of_packets, 1);
    BCMOLT_MSG_FIELD_SET(&omci_cpu_packets, packet_size, buf.len);
    BCMOLT_MSG_FIELD_SET(&omci_cpu_packets, buffer, buf);

    bcmos_errno err = bcmolt_oper_submit(dev_id, &omci_cpu_packets.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Error sending OMCI message to ONU %d on PON %d, err = %s\n", onu_id, intf_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "send OMCI failed");
    } else {
        OPENOLT_LOG(DEBUG, omci_log_id, "OMCI request msg of length %d sent to ONU %d on PON %d : %s\n",
            buf.len, onu_id, intf_id, pkt.c_str());
    }
    free(buf.arr);

    return Status::OK;
}

Status OnuPacketOut_(uint32_t intf_id, uint32_t onu_id, uint32_t port_no, uint32_t gemport_id, const std::string pkt) {
    bcmolt_pon_interface_cpu_packets pon_interface_cpu_packets; /**< declare main API struct */
    bcmolt_pon_interface_key key = {.pon_ni = (bcmolt_interface)intf_id}; /**< declare key */
    bcmolt_bin_str buf = {};
    bcmolt_gem_port_id gem_port_id_array[1];
    bcmolt_gem_port_id_list_u8_max_16 gem_port_list = {};

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
                OPENOLT_LOG(ERROR, openolt_log_id, "Packet out failed to find destination for ONU %d port_no %u on PON %d\n",
                        onu_id, port_no, intf_id);
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "no flow for port_no");
            }
            OPENOLT_LOG(INFO, openolt_log_id, "Gem port %u found for ONU %d port_no %u on PON %d\n",
                    gemport_id, onu_id, port_no, intf_id);
        }

        gem_port_id_array[0] = gemport_id;
        gem_port_list.len = 1;
        gem_port_list.arr = gem_port_id_array;
        buf.len = pkt.size();
        buf.arr = (uint8_t *)malloc((buf.len)*sizeof(uint8_t));
        memcpy(buf.arr, (uint8_t *)pkt.data(), buf.len);

        /* init the API struct */
        BCMOLT_OPER_INIT(&pon_interface_cpu_packets, pon_interface, cpu_packets, key);
        BCMOLT_MSG_FIELD_SET(&pon_interface_cpu_packets, packet_type, BCMOLT_PACKET_TYPE_ETH);
        BCMOLT_MSG_FIELD_SET(&pon_interface_cpu_packets, calc_crc, BCMOS_TRUE);
        BCMOLT_MSG_FIELD_SET(&pon_interface_cpu_packets, gem_port_list, gem_port_list);
        BCMOLT_MSG_FIELD_SET(&pon_interface_cpu_packets, buffer, buf);

        OPENOLT_LOG(INFO, openolt_log_id, "Packet out of length %d sent to gemport %d on pon %d port_no %u\n",
            (uint8_t)pkt.size(), gemport_id, intf_id, port_no);

        /* call API */
        bcmolt_oper_submit(dev_id, &pon_interface_cpu_packets.hdr);
    }
    else {
        //TODO: Port No is 0, it is coming sender requirement.
        OPENOLT_LOG(INFO, openolt_log_id, "port_no %d onu %d on pon %d\n",
            port_no, onu_id, intf_id);
    }
    free(buf.arr);

    return Status::OK;
}

Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt) {
    bcmolt_flow_key key = {}; /* declare key */
    bcmolt_bin_str buffer = {};
    bcmolt_flow_send_eth_packet oper; /* declare main API struct */

    // TODO: flow_id is currently not passed in UplinkPacket message from voltha.
    bcmolt_flow_id flow_id = 0;

    //validate flow_id and find flow_id/flow type: upstream/ingress type: PON/egress type: NNI
    if (get_flow_status(flow_id, BCMOLT_FLOW_TYPE_UPSTREAM, FLOW_TYPE) == BCMOLT_FLOW_TYPE_UPSTREAM && \
        get_flow_status(flow_id, BCMOLT_FLOW_TYPE_UPSTREAM, INGRESS_INTF_TYPE) == BCMOLT_FLOW_INTERFACE_TYPE_PON && \
        get_flow_status(flow_id, BCMOLT_FLOW_TYPE_UPSTREAM, EGRESS_INTF_TYPE) == BCMOLT_FLOW_INTERFACE_TYPE_NNI)
        key.flow_id = flow_id;
    else {
        if (flow_id_counters) {
            std::map<flow_pair, int>::iterator it;
            for(it = flow_map.begin(); it != flow_map.end(); it++) {
                int flow_index = it->first.first;
                if (get_flow_status(flow_index, BCMOLT_FLOW_TYPE_UPSTREAM, FLOW_TYPE) == BCMOLT_FLOW_TYPE_UPSTREAM && \
                    get_flow_status(flow_index, BCMOLT_FLOW_TYPE_UPSTREAM, INGRESS_INTF_TYPE) == BCMOLT_FLOW_INTERFACE_TYPE_PON && \
                    get_flow_status(flow_index, BCMOLT_FLOW_TYPE_UPSTREAM, EGRESS_INTF_TYPE) == BCMOLT_FLOW_INTERFACE_TYPE_NNI) {
                    key.flow_id = flow_index;
                    break;
                }
            }
        }
        else {
            OPENOLT_LOG(ERROR, openolt_log_id, "no flow id found for uplink packetout\n");
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "no flow id found");
        }
    }

    key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM; /* send from uplink direction */

    /* Initialize the API struct. */
    BCMOLT_OPER_INIT(&oper, flow, send_eth_packet, key);

    buffer.len = pkt.size();
    buffer.arr = (uint8_t *)malloc((buffer.len)*sizeof(uint8_t));
    memcpy(buffer.arr, (uint8_t *)pkt.data(), buffer.len);
    if (buffer.arr == NULL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "allocate packet buffer failed\n");
        return bcm_to_grpc_err(BCM_ERR_PARM, "allocate packet buffer failed");
    }
    BCMOLT_FIELD_SET(&oper.data, flow_send_eth_packet_data, buffer, buffer);

    bcmos_errno err = bcmolt_oper_submit(dev_id, &oper.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Error sending packets via nni port %d, flow_id %d, err = %s\n", intf_id, key.flow_id, bcmos_strerror(err));
        return bcm_to_grpc_err(BCM_ERR_SYSCALL_ERR, "Error sending packets via nni port");
    } else {
        OPENOLT_LOG(INFO, openolt_log_id, "sent packets to port %d in upstream direction, flow_id %d \n", intf_id, key.flow_id);
    }

    return Status::OK;
}
Status FlowAdd_(int32_t access_intf_id, int32_t onu_id, int32_t uni_id, uint32_t port_no,
                uint32_t flow_id, const std::string flow_type,
                int32_t alloc_id, int32_t network_intf_id,
                int32_t gemport_id, const ::openolt::Classifier& classifier,
                const ::openolt::Action& action, int32_t priority_value, uint64_t cookie,
                int32_t group_id, uint32_t tech_profile_id) {
    bcmolt_flow_cfg cfg;
    bcmolt_flow_key key = { }; /**< Object key. */
    int32_t o_vid = -1;
    bool single_tag = false;
    uint32_t ether_type = 0;
    bcmolt_classifier c_val = { };
    bcmolt_action a_val = { };
    bcmolt_tm_queue_ref tm_val = { };
    int tm_qmp_id, tm_q_set_id;
    bcmolt_egress_qos_type qos_type;

    OPENOLT_LOG(INFO, openolt_log_id, "flow add received for flow_id=%u, flow_type=%s\n", flow_id, flow_type.c_str());

    key.flow_id = flow_id;
    if (flow_type.compare(upstream) == 0 ) {
        key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare(downstream) == 0) {
        key.flow_type = BCMOLT_FLOW_TYPE_DOWNSTREAM;
    } else if (flow_type.compare(multicast) == 0) {
        key.flow_type = BCMOLT_FLOW_TYPE_MULTICAST;
    } else {
        OPENOLT_LOG(ERROR, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    BCMOLT_CFG_INIT(&cfg, flow, key);
    BCMOLT_MSG_FIELD_SET(&cfg, cookie, cookie);

    if (action.cmd().trap_to_host()) {
        Status resp = handle_acl_rule_install(onu_id, flow_id, flow_type, access_intf_id,
                                              network_intf_id, gemport_id, classifier);
        return resp;
    }

    if (key.flow_type != BCMOLT_FLOW_TYPE_MULTICAST) {

        if (access_intf_id >= 0 && network_intf_id >= 0) {
            if (key.flow_type == BCMOLT_FLOW_TYPE_UPSTREAM) { //upstream
                BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_PON);
                BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_id, access_intf_id);
                BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_NNI);
                BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_id, network_intf_id);
            } else if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) { //downstream
                BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_NNI);
                BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_id, network_intf_id);
                BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_PON);
                BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_id, access_intf_id);
            }
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id, "flow network setting invalid\n");
            return bcm_to_grpc_err(BCM_ERR_PARM, "flow network setting invalid");
        }

        if (onu_id >= 0) {
            BCMOLT_MSG_FIELD_SET(&cfg, onu_id, onu_id);
        }
        if (gemport_id >= 0) {
            BCMOLT_MSG_FIELD_SET(&cfg, svc_port_id, gemport_id);
        }
        if (gemport_id >= 0 && port_no != 0) {
            bcmos_fastlock_lock(&data_lock);
            if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
                port_to_flows[port_no].insert(key.flow_id);
                flowid_to_gemport[key.flow_id] = gemport_id;
            }
            else
            {
                flowid_to_port[key.flow_id] = port_no;
            }
            bcmos_fastlock_unlock(&data_lock, 0);
        }
        if (gemport_id >= 0 && access_intf_id >= 0) {
            // Update the flow_to_acl_map. Note that since this is a datapath flow, acl_id is -1
            // This info is needed during flow remove where we need to retrieve the gemport_id
            // and access_intf id for the given flow id and flow direction.
            // After retrieving the ACL ID and GEM PORT ID, we decrement the corresponding
            // reference counters for those ACL ID and GEMPORT ID.
            acl_id_gem_id_intf_id ac_id_gm_id_if_id(-1, gemport_id, access_intf_id);
            flow_id_flow_direction fl_id_fl_dir(flow_id, flow_type);
            bcmos_fastlock_lock(&data_lock);
            flow_to_acl_map[fl_id_fl_dir] = ac_id_gm_id_if_id;
            bcmos_fastlock_unlock(&data_lock, 0);
        }
        if (priority_value >= 0) {
            BCMOLT_MSG_FIELD_SET(&cfg, priority, priority_value);
        }

    } else { // MULTICAST FLOW
        if (group_id >= 0) {
            BCMOLT_MSG_FIELD_SET(&cfg, group_id, group_id);
        }
        BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_NNI);
        BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_id, network_intf_id);
    }

    {
        if (classifier.eth_type()) {
            ether_type = classifier.eth_type();
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ether_type 0x%04x\n", classifier.eth_type());
            BCMOLT_FIELD_SET(&c_val, classifier, ether_type, classifier.eth_type());
        }

        if (classifier.dst_mac().size() > 0) {
            bcmos_mac_address d_mac = {};
            bcmos_mac_address_init(&d_mac);
            memcpy(d_mac.u8, classifier.dst_mac().data(), sizeof(d_mac.u8));
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify dst_mac %02x:%02x:%02x:%02x:%02x:%02x\n", d_mac.u8[0],
                        d_mac.u8[1], d_mac.u8[2], d_mac.u8[3], d_mac.u8[4], d_mac.u8[5]);
            BCMOLT_FIELD_SET(&c_val, classifier, dst_mac, d_mac);
        }

        /*
        if (classifier.src_mac()) {
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, src_mac, classifier.src_mac());
        }
        */

        if (classifier.ip_proto()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ip_proto %d\n", classifier.ip_proto());
            BCMOLT_FIELD_SET(&c_val, classifier, ip_proto, classifier.ip_proto());
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
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify src_port %d\n", classifier.src_port());
            BCMOLT_FIELD_SET(&c_val, classifier, src_port, classifier.src_port());
        }

        if (classifier.dst_port()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify dst_port %d\n", classifier.dst_port());
            BCMOLT_FIELD_SET(&c_val, classifier, dst_port, classifier.dst_port());
        }

        if (!classifier.pkt_tag_type().empty()) {
            if (classifier.o_vid()) {
                OPENOLT_LOG(DEBUG, openolt_log_id, "classify o_vid %d\n", classifier.o_vid());
                BCMOLT_FIELD_SET(&c_val, classifier, o_vid, classifier.o_vid());
            }

            if (classifier.i_vid()) {
                OPENOLT_LOG(DEBUG, openolt_log_id, "classify i_vid %d\n", classifier.i_vid());
                BCMOLT_FIELD_SET(&c_val, classifier, i_vid, classifier.i_vid());
            }

            OPENOLT_LOG(DEBUG, openolt_log_id, "classify tag_type %s\n", classifier.pkt_tag_type().c_str());
            if (classifier.pkt_tag_type().compare("untagged") == 0) {
                BCMOLT_FIELD_SET(&c_val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_UNTAGGED);
            } else if (classifier.pkt_tag_type().compare("single_tag") == 0) {
                BCMOLT_FIELD_SET(&c_val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_SINGLE_TAG);
                single_tag = true;

                OPENOLT_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                // OpenOlt adapter will send 0xFF in case of no pbit classification
                // If it is any other value (0 to 7), it is for outer pbit classification.
                // OpenFlow protocol does not provide inner pbit classification (in case of double tagged packets),
                // and VOLTHA has not used any workaround to solve this problem (for ex: use metadata field).
                // Also there exists no use case for i-pbit classification, so we can safely ignore this for now.
                if(0xFF != classifier.o_pbits()){
                    BCMOLT_FIELD_SET(&c_val, classifier, o_pbits, classifier.o_pbits());
                }
            } else if (classifier.pkt_tag_type().compare("double_tag") == 0) {
                BCMOLT_FIELD_SET(&c_val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_DOUBLE_TAG);

                OPENOLT_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                // Same comments as in case of "single_tag" packets.
                // 0xFF means no pbit classification, otherwise a valid PCP (0 to 7).
                if(0xFF != classifier.o_pbits()){
                    BCMOLT_FIELD_SET(&c_val, classifier, o_pbits, classifier.o_pbits());
                }
            }
        }
        BCMOLT_MSG_FIELD_SET(&cfg, classifier, c_val);
    }

    const ::openolt::ActionCmd& cmd = action.cmd();

    if (cmd.add_outer_tag()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action add o_tag\n");
        BCMOLT_FIELD_SET(&a_val, action, cmds_bitmask, BCMOLT_ACTION_CMD_ID_ADD_OUTER_TAG);
    }

    if (cmd.remove_outer_tag()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action pop o_tag\n");
        BCMOLT_FIELD_SET(&a_val, action, cmds_bitmask, BCMOLT_ACTION_CMD_ID_REMOVE_OUTER_TAG);
    }

    if (action.o_vid()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action o_vid=%d\n", action.o_vid());
        o_vid = action.o_vid();
        BCMOLT_FIELD_SET(&a_val, action, o_vid, action.o_vid());
    }

    if (action.o_pbits()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action o_pbits=0x%x\n", action.o_pbits());
        BCMOLT_FIELD_SET(&a_val, action, o_pbits, action.o_pbits());
    }

    if (action.i_vid()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action i_vid=%d\n", action.i_vid());
        BCMOLT_FIELD_SET(&a_val, action, i_vid, action.i_vid());
    }

    if (action.i_pbits()) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "action i_pbits=0x%x\n", action.i_pbits());
        BCMOLT_FIELD_SET(&a_val, action, i_pbits, action.i_pbits());
    }

    BCMOLT_MSG_FIELD_SET(&cfg, action, a_val);

    if ((access_intf_id >= 0) && (onu_id >= 0)) {
        qos_type = get_qos_type(access_intf_id, onu_id, uni_id);
        if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
            tm_val.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, downstream, tech_profile_id);

            if (qos_type == BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE) {
                // Queue 0 on DS subscriber scheduler
                tm_val.queue_id = 0;

                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, qos_type);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, tm_val.sched_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.fixed_queue.queue_id, tm_val.queue_id);

                OPENOLT_LOG(DEBUG, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d, intf_type %s\n", \
                        downstream.c_str(), tm_val.queue_id, tm_val.sched_id, \
                        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type));

            } else if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE) {
                /* Fetch TM QMP ID mapped to DS subscriber scheduler */
                tm_qmp_id = tm_q_set_id = get_tm_qmp_id(tm_val.sched_id, access_intf_id, onu_id, uni_id);

                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, qos_type);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, tm_val.sched_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.priority_to_queue.tm_qmp_id, tm_qmp_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.priority_to_queue.tm_q_set_id, tm_q_set_id);

                OPENOLT_LOG(DEBUG, openolt_log_id, "direction = %s, q_set_id = %d, sched_id = %d, intf_type %s\n", \
                        downstream.c_str(), tm_q_set_id, tm_val.sched_id, \
                        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type));
            }
        } else if (key.flow_type == BCMOLT_FLOW_TYPE_UPSTREAM) {
            // NNI Scheduler ID
            tm_val.sched_id = get_default_tm_sched_id(network_intf_id, upstream);
            if (qos_type == BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE) {
                // Queue 0 on NNI scheduler
                tm_val.queue_id = 0;
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, qos_type);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, tm_val.sched_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.fixed_queue.queue_id, tm_val.queue_id);

                OPENOLT_LOG(DEBUG, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d, intf_type %s\n", \
                        upstream.c_str(), tm_val.queue_id, tm_val.sched_id, \
                        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type));

            } else if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE) {
                /* Fetch TM QMP ID mapped to US NNI scheduler */
                tm_qmp_id = tm_q_set_id = get_tm_qmp_id(tm_val.sched_id, access_intf_id, onu_id, uni_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, qos_type);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, tm_val.sched_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.priority_to_queue.tm_qmp_id, tm_qmp_id);
                BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.priority_to_queue.tm_q_set_id, tm_q_set_id);

                OPENOLT_LOG(DEBUG, openolt_log_id, "direction = %s, q_set_id = %d, sched_id = %d, intf_type %s\n", \
                        upstream.c_str(), tm_q_set_id, tm_val.sched_id, \
                        GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type));
            }
        }
    } else {
        tm_val.sched_id = get_default_tm_sched_id(network_intf_id, upstream);
        tm_val.queue_id = 0;

        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE);
        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, tm_val.sched_id);
        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.fixed_queue.queue_id, tm_val.queue_id);

        OPENOLT_LOG(DEBUG, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d, intf_type %s\n", \
                flow_type.c_str(), tm_val.queue_id, tm_val.sched_id, \
                GET_FLOW_INTERFACE_TYPE(cfg.data.ingress_intf.intf_type));
    }

    BCMOLT_MSG_FIELD_SET(&cfg, state, BCMOLT_FLOW_STATE_ENABLE);

    // BAL 3.1 supports statistics only for unicast flows.
    if (key.flow_type != BCMOLT_FLOW_TYPE_MULTICAST) {
        BCMOLT_MSG_FIELD_SET(&cfg, statistics, BCMOLT_CONTROL_STATE_ENABLE);
    }

#ifdef FLOW_CHECKER
    //Flow Checker, To avoid duplicate flow.
    if (flow_id_counters != 0) {
        bool b_duplicate_flow = false;
        std::map<flow_pair, int>::iterator it;

        for(it = flow_map.begin(); it != flow_map.end(); it++) {
            b_duplicate_flow = (cfg.data.onu_id == get_flow_status(it->first.first, it->first.second, ONU_ID)) && \
                (key.flow_type == it->first.second) && \
                (cfg.data.svc_port_id == get_flow_status(it->first.first, it->first.second, SVC_PORT_ID)) && \
                (cfg.data.priority == get_flow_status(it->first.first, it->first.second, PRIORITY)) && \
                (cfg.data.cookie == get_flow_status(it->first.first, it->first.second, COOKIE)) && \
                (cfg.data.ingress_intf.intf_type == get_flow_status(it->first.first, it->first.second, INGRESS_INTF_TYPE)) && \
                (cfg.data.ingress_intf.intf_id == get_flow_status(it->first.first, it->first.second, INGRESS_INTF_ID)) && \
                (cfg.data.egress_intf.intf_type == get_flow_status(it->first.first, it->first.second, EGRESS_INTF_TYPE)) && \
                (cfg.data.egress_intf.intf_id == get_flow_status(it->first.first, it->first.second, EGRESS_INTF_ID)) && \
                (c_val.o_vid == get_flow_status(it->first.first, it->first.second, CLASSIFIER_O_VID)) && \
                (c_val.o_pbits == get_flow_status(it->first.first, it->first.second, CLASSIFIER_O_PBITS)) && \
                (c_val.i_vid == get_flow_status(it->first.first, it->first.second, CLASSIFIER_I_VID)) && \
                (c_val.i_pbits == get_flow_status(it->first.first, it->first.second, CLASSIFIER_I_PBITS)) && \
                (c_val.ether_type == get_flow_status(it->first.first, it->first.second, CLASSIFIER_ETHER_TYPE)) && \
                (c_val.ip_proto == get_flow_status(it->first.first, it->first.second, CLASSIFIER_IP_PROTO)) && \
                (c_val.src_port == get_flow_status(it->first.first, it->first.second, CLASSIFIER_SRC_PORT)) && \
                (c_val.dst_port == get_flow_status(it->first.first, it->first.second, CLASSIFIER_DST_PORT)) && \
                (c_val.pkt_tag_type == get_flow_status(it->first.first, it->first.second, CLASSIFIER_PKT_TAG_TYPE)) && \
                (cfg.data.egress_qos.type == get_flow_status(it->first.first, it->first.second, EGRESS_QOS_TYPE)) && \
                (cfg.data.egress_qos.u.fixed_queue.queue_id == get_flow_status(it->first.first, it->first.second, EGRESS_QOS_QUEUE_ID)) && \
                (cfg.data.egress_qos.tm_sched.id == get_flow_status(it->first.first, it->first.second, EGRESS_QOS_TM_SCHED_ID)) && \
                (a_val.cmds_bitmask == get_flow_status(it->first.first, it->first.second, ACTION_CMDS_BITMASK)) && \
                (a_val.o_vid == get_flow_status(it->first.first, it->first.second, ACTION_O_VID)) && \
                (a_val.i_vid == get_flow_status(it->first.first, it->first.second, ACTION_I_VID)) && \
                (a_val.o_pbits == get_flow_status(it->first.first, it->first.second, ACTION_O_PBITS)) && \
                (a_val.i_pbits == get_flow_status(it->first.first, it->first.second, ACTION_I_PBITS)) && \
                (cfg.data.state == get_flow_status(it->first.first, it->first.second, STATE)) && \
                (cfg.data.group_id == get_flow_status(it->first.first, it->first.second, GROUP_ID));
#ifdef SHOW_FLOW_PARAM
            // Flow Parameter
            FLOW_PARAM_LOG();
#endif
            if (b_duplicate_flow) {
                FLOW_LOG(WARNING, "Flow duplicate", 0);
                return bcm_to_grpc_err(BCM_ERR_ALREADY, "flow exists");
            }
        }
    }
#endif

    bcmos_errno err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if (err) {
        FLOW_LOG(ERROR, "Flow add failed", err);
        return bcm_to_grpc_err(err, "flow add failed");
    } else {
        FLOW_LOG(INFO, "Flow add ok", err);
        bcmos_fastlock_lock(&data_lock);
        flow_map[std::pair<int, int>(key.flow_id,key.flow_type)] = flow_map.size();
        flow_id_counters = flow_map.size();
        if (gemport_id > 0 && access_intf_id >= 0) {
            gem_id_intf_id gem_intf(gemport_id, access_intf_id);
            if (gem_ref_cnt.count(gem_intf) > 0) {
                // The gem port is already installed
                // Increment the ref counter indicating number of flows referencing this gem port
                gem_ref_cnt[gem_intf]++;
                OPENOLT_LOG(DEBUG, openolt_log_id, "incremented gem_ref_cnt, gem_ref_cnt=%d\n", gem_ref_cnt[gem_intf]);
            } else {
                // Initialize the refence count for the gemport.
                gem_ref_cnt[gem_intf] = 1;
                OPENOLT_LOG(DEBUG, openolt_log_id, "initialized gem_ref_cnt\n");
            }
        } else {
            OPENOLT_LOG(DEBUG, openolt_log_id, "not incrementing gem_ref_cnt flow_id=%d gemport_id=%d access_intf_id=%d\n", flow_id, gemport_id, access_intf_id);
        }

        bcmos_fastlock_unlock(&data_lock, 0);
    }

    return Status::OK;
}

Status FlowRemove_(uint32_t flow_id, const std::string flow_type) {

    bcmolt_flow_cfg cfg;
    bcmolt_flow_key key = { };

    key.flow_id = (bcmolt_flow_id) flow_id;
    key.flow_id = flow_id;
    if (flow_type.compare(upstream) == 0 ) {
        key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare(downstream) == 0) {
        key.flow_type = BCMOLT_FLOW_TYPE_DOWNSTREAM;
    } else if(flow_type.compare(multicast) == 0) {
        key.flow_type = BCMOLT_FLOW_TYPE_MULTICAST;
    } else {
        OPENOLT_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "flow remove received for flow_id=%u, flow_type=%s\n",
            flow_id, flow_type.c_str());

    bcmos_fastlock_lock(&data_lock);
    flow_id_flow_direction fl_id_fl_dir(flow_id, flow_type);
    int32_t gemport_id = -1;
    int32_t intf_id = -1;
    int16_t acl_id = -1;
    if (flow_to_acl_map.count(fl_id_fl_dir) > 0) {
        acl_id_gem_id_intf_id ac_id_gm_id_if_id = flow_to_acl_map[fl_id_fl_dir];
        acl_id = std::get<0>(ac_id_gm_id_if_id);
        gemport_id = std::get<1>(ac_id_gm_id_if_id);
        intf_id = std::get<2>(ac_id_gm_id_if_id);
        // cleanup acl only if it is a valid acl. If not valid acl, it may be datapath flow.
        if (acl_id >= 0) {
            Status resp = handle_acl_rule_cleanup(acl_id, gemport_id, intf_id, flow_type);
            bcmos_fastlock_unlock(&data_lock, 0);
            if (resp.ok()) {
                OPENOLT_LOG(INFO, openolt_log_id, "acl removed ok for flow_id = %u with acl_id = %d\n", flow_id, acl_id);
                flow_to_acl_map.erase(fl_id_fl_dir);
            } else {
                OPENOLT_LOG(ERROR, openolt_log_id, "acl remove error for flow_id = %u with acl_id = %d\n", flow_id, acl_id);
            }
            return resp;
        }
    }

    uint32_t port_no = flowid_to_port[key.flow_id];
    if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
        flowid_to_gemport.erase(key.flow_id);
        port_to_flows[port_no].erase(key.flow_id);
        if (port_to_flows[port_no].empty()) port_to_flows.erase(port_no);
    }
    else
    {
        flowid_to_port.erase(key.flow_id);
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    BCMOLT_CFG_INIT(&cfg, flow, key);

    bcmos_errno err = bcmolt_cfg_clear(dev_id, &cfg.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Error while removing %s flow, flow_id=%d, err = %s\n", flow_type.c_str(), flow_id, bcmos_strerror(err));
        return Status(grpc::StatusCode::INTERNAL, "Failed to remove flow");
    }

    bcmos_fastlock_lock(&data_lock);
    if (flow_id_counters != 0) {
        std::map<flow_pair, int>::iterator it;
        for(it = flow_map.begin(); it != flow_map.end(); it++) {
            if (it->first.first == flow_id && it->first.second == key.flow_type) {
                flow_id_counters -= 1;
                flow_map.erase(it);
            }
        }
    }
    OPENOLT_LOG(INFO, openolt_log_id, "Flow %d, %s removed\n", flow_id, flow_type.c_str());

    clear_gem_port(gemport_id, intf_id);

    flow_to_acl_map.erase(fl_id_fl_dir);

    bcmos_fastlock_unlock(&data_lock, 0);

    return Status::OK;
}

bcmos_errno CreateDefaultSched(uint32_t intf_id, const std::string direction) {
    bcmos_errno err;
    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1};
    tm_sched_key.id = get_default_tm_sched_id(intf_id, direction);

    //check TM scheduler has configured or not
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    BCMOLT_MSG_FIELD_GET(&tm_sched_cfg, state);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of tm_sched_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of tm_sched_cfg.hdr and we cannot
    // set the tm_sched_cfg.data.state. So a new stub function is created and address
    // of tm_sched_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__tm_sched_stub(dev_id, &tm_sched_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &tm_sched_cfg.hdr);
    #endif
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "cfg: Failed to query TM scheduler, err = %s\n",bcmos_strerror(err));
        return err;
    }
    else if (tm_sched_cfg.data.state == BCMOLT_CONFIG_STATE_CONFIGURED) {
        OPENOLT_LOG(WARNING, openolt_log_id, "tm scheduler default config has already with id %d\n", tm_sched_key.id);
        return BCM_ERR_OK;
    }

    // bcmbal_tm_sched_owner
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);

    /**< The output of the tm_sched object instance */
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.type, BCMOLT_TM_SCHED_OUTPUT_TYPE_INTERFACE);

    if (direction.compare(upstream) == 0) {
        // In upstream it is NNI scheduler
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.u.interface.interface_ref.intf_type, BCMOLT_INTERFACE_TYPE_NNI);
    } else if (direction.compare(downstream) == 0) {
        // In downstream it is PON scheduler
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.u.interface.interface_ref.intf_type, BCMOLT_INTERFACE_TYPE_PON);
    }

    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.u.interface.interface_ref.intf_id, intf_id);

    // bcmbal_tm_sched_type
    // set the deafult policy to strict priority
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, sched_type, BCMOLT_TM_SCHED_TYPE_SP);

    // num_priorities: Max number of strict priority scheduling elements
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, num_priorities, NUM_OF_PRIORITIES);

    err = bcmolt_cfg_set(dev_id, &tm_sched_cfg.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create %s scheduler, id %d, intf_id %d, err = %s\n",
            direction.c_str(), tm_sched_key.id, intf_id, bcmos_strerror(err));
        return err;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Create %s scheduler success, id %d, intf_id %d\n", \
        direction.c_str(), tm_sched_key.id, intf_id);
    return BCM_ERR_OK;
}

bcmos_errno CreateSched(std::string direction, uint32_t intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t port_no,
                 uint32_t alloc_id, tech_profile::AdditionalBW additional_bw, uint32_t weight, uint32_t priority,
                 tech_profile::SchedulingPolicy sched_policy, tech_profile::TrafficShapingInfo tf_sh_info,
                 uint32_t tech_profile_id) {

    bcmos_errno err;

    if (direction == downstream) {
        bcmolt_tm_sched_cfg tm_sched_cfg;
        bcmolt_tm_sched_key tm_sched_key = {.id = 1};
        tm_sched_key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction, tech_profile_id);

        // bcmbal_tm_sched_owner
        // In downstream it is sub_term scheduler
        BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);

        /**< The output of the tm_sched object instance */
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.type, BCMOLT_TM_SCHED_OUTPUT_TYPE_TM_SCHED);

        // bcmbal_tm_sched_parent
        // The parent for the sub_term scheduler is the PON scheduler in the downstream
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.u.tm_sched.tm_sched_id, get_default_tm_sched_id(intf_id, direction));
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, attachment_point.u.tm_sched.tm_sched_param.u.priority.priority, priority);
        /* removed by BAL v3.0, N/A - No direct attachment point of type ONU, same functionality may
           be achieved using the' virtual' type of attachment.
        tm_sched_owner.u.sub_term.intf_id = intf_id;
        tm_sched_owner.u.sub_term.sub_term_id = onu_id;
        */

        // bcmbal_tm_sched_type
        // set the deafult policy to strict priority
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, sched_type, BCMOLT_TM_SCHED_TYPE_SP);

        // num_priorities: Max number of strict priority scheduling elements
        BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, num_priorities, 8);

        // bcmbal_tm_shaping
        if (tf_sh_info.cir() >= 0 && tf_sh_info.pir() > 0) {
            uint32_t cir = tf_sh_info.cir();
            uint32_t pir = tf_sh_info.pir();
            uint32_t burst = tf_sh_info.pbs();
            OPENOLT_LOG(INFO, openolt_log_id, "applying traffic shaping in DL cir=%u, pir=%u, burst=%u\n",
               cir, pir, burst);
            BCMOLT_FIELD_SET_PRESENT(&tm_sched_cfg.data.rate, tm_shaping, pir);
            BCMOLT_FIELD_SET_PRESENT(&tm_sched_cfg.data.rate, tm_shaping, burst);
            // FIXME: Setting CIR, results in BAL throwing error 'tm_sched minimum rate is not supported yet'
            //BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.cir, cir);
            BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.pir, pir);
            BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.burst, burst);
        }

        err = bcmolt_cfg_set(dev_id, &tm_sched_cfg.hdr);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create downstream subscriber scheduler, id %d, \
intf_id %d, onu_id %d, uni_id %d, port_no %u, err = %s\n", tm_sched_key.id, intf_id, onu_id, uni_id, \
port_no, bcmos_strerror(err));
            return err;
        }
        OPENOLT_LOG(INFO, openolt_log_id, "Create downstream subscriber sched, id %d, intf_id %d, onu_id %d, \
uni_id %d, port_no %u\n", tm_sched_key.id, intf_id, onu_id, uni_id, port_no);

    } else { //upstream
        bcmolt_itupon_alloc_cfg cfg;
        bcmolt_itupon_alloc_key key = { };
        key.pon_ni = intf_id;
        key.alloc_id = alloc_id;
        int bw_granularity = (board_technology == "XGS-PON")?XGS_BANDWIDTH_GRANULARITY:GPON_BANDWIDTH_GRANULARITY;
        int pir_bw = tf_sh_info.pir()*125; // conversion from kbps to bytes/sec
        int cir_bw = tf_sh_info.cir()*125; // conversion from kbps to bytes/sec
        //offset to match bandwidth granularity
        int offset_pir_bw = pir_bw%bw_granularity;
        int offset_cir_bw = cir_bw%bw_granularity;

        pir_bw = pir_bw - offset_pir_bw;
        cir_bw = cir_bw - offset_cir_bw;

        BCMOLT_CFG_INIT(&cfg, itupon_alloc, key);

        switch (additional_bw) {
            case 2: //AdditionalBW_BestEffort
                if (pir_bw == 0) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth was set to 0, must be at least \
%d bytes/sec\n", (board_technology == "XGS-PON")?XGS_BANDWIDTH_GRANULARITY:GPON_BANDWIDTH_GRANULARITY);
                   return BCM_ERR_PARM;
                } else if (pir_bw < cir_bw) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth (%d) can't be less than Guaranteed \
bandwidth (%d)\n", pir_bw, cir_bw);
                   return BCM_ERR_PARM;
                } else if (pir_bw == cir_bw) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth must be greater than Guaranteed \
bandwidth for additional bandwidth eligibility of type best_effort\n");
                   return BCM_ERR_PARM;
                }
                BCMOLT_MSG_FIELD_SET(&cfg, sla.additional_bw_eligibility, BCMOLT_ADDITIONAL_BW_ELIGIBILITY_BEST_EFFORT);
                break;
            case 1: //AdditionalBW_NA
                if (pir_bw == 0) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth was set to 0, must be at least \
%d bytes/sec\n", (board_technology == "XGS-PON")?XGS_BANDWIDTH_GRANULARITY:GPON_BANDWIDTH_GRANULARITY);
                    return BCM_ERR_PARM;
                } else if (cir_bw == 0) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Guaranteed bandwidth must be greater than zero for \
additional bandwidth eligibility of type Non-Assured (NA)\n");
                    return BCM_ERR_PARM;
                } else if (pir_bw < cir_bw) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth (%d) can't be less than Guaranteed \
bandwidth (%d)\n", pir_bw, cir_bw);
                    return BCM_ERR_PARM;
                } else if (pir_bw == cir_bw) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth must be greater than Guaranteed \
bandwidth for additional bandwidth eligibility of type non_assured\n");
                    return BCM_ERR_PARM;
                }
                BCMOLT_MSG_FIELD_SET(&cfg, sla.additional_bw_eligibility, BCMOLT_ADDITIONAL_BW_ELIGIBILITY_NON_ASSURED);
                break;
            case 0: //AdditionalBW_None
                if (pir_bw == 0) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth was set to 0, must be at least \
16000 bytes/sec\n");
                    return BCM_ERR_PARM;
                } else if (cir_bw == 0) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth must be equal to Guaranteed bandwidth \
for additional bandwidth eligibility of type None\n");
                    return BCM_ERR_PARM;
                } else if (pir_bw > cir_bw) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth must be equal to Guaranteed bandwidth \
for additional bandwidth eligibility of type None\n");
                   OPENOLT_LOG(ERROR, openolt_log_id, "Setting Maximum bandwidth (%d) to Guaranteed \
bandwidth in None eligibility\n", pir_bw);
                   cir_bw = pir_bw;
                } else if (pir_bw < cir_bw) {
                   OPENOLT_LOG(ERROR, openolt_log_id, "Maximum bandwidth (%d) can't be less than Guaranteed \
bandwidth (%d)\n", pir_bw, cir_bw);
                   OPENOLT_LOG(ERROR, openolt_log_id, "Setting Maximum bandwidth (%d) to Guaranteed \
bandwidth in None eligibility\n", pir_bw);
                   cir_bw = pir_bw;
                }
                BCMOLT_MSG_FIELD_SET(&cfg, sla.additional_bw_eligibility, BCMOLT_ADDITIONAL_BW_ELIGIBILITY_NONE);
                break;
            default:
                return BCM_ERR_PARM;
        }
        /* CBR Real Time Bandwidth which require shaping of the bandwidth allocations
           in a fine granularity. */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_rt_bw, 0);
        /* Fixed Bandwidth with no critical requirement of shaping */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_nrt_bw, 0);
        /* Dynamic bandwidth which the OLT is committed to allocate upon demand */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.guaranteed_bw, cir_bw);
        /* Maximum allocated bandwidth allowed for this alloc ID */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.maximum_bw, pir_bw);
        BCMOLT_MSG_FIELD_SET(&cfg, sla.alloc_type, BCMOLT_ALLOC_TYPE_NSR);
        /* Set to True for AllocID with CBR RT Bandwidth that requires compensation
           for skipped allocations during quiet window */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_rt_compensation, BCMOS_FALSE);
        /**< Allocation Profile index for CBR non-RT Bandwidth */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_nrt_ap_index, 0);
        /**< Allocation Profile index for CBR RT Bandwidth */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_rt_ap_index, 0);
        /**< Alloc ID Weight used in case of Extended DBA mode */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.weight, 0);
        /**< Alloc ID Priority used in case of Extended DBA mode */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.priority, 0);
        BCMOLT_MSG_FIELD_SET(&cfg, onu_id, onu_id);

        err = bcmolt_cfg_set(dev_id, &cfg.hdr);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create upstream bandwidth allocation, intf_id %d, onu_id %d, uni_id %d,\
port_no %u, alloc_id %d, err = %s\n", intf_id, onu_id,uni_id,port_no,alloc_id, bcmos_strerror(err));
            return err;
        }

        err = wait_for_alloc_action(intf_id, alloc_id, ALLOC_OBJECT_CREATE);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create upstream bandwidth allocation, intf_id %d, onu_id %d, uni_id %d,\
port_no %u, alloc_id %d, err = %s\n", intf_id, onu_id,uni_id,port_no,alloc_id, bcmos_strerror(err));
            return err;
        }

        OPENOLT_LOG(INFO, openolt_log_id, "create upstream bandwidth allocation success, intf_id %d, onu_id %d, uni_id %d,\
port_no %u, alloc_id %d\n", intf_id, onu_id,uni_id,port_no,alloc_id);

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
    uint32_t tech_profile_id;
    bcmos_errno err;

    for (int i = 0; i < traffic_scheds->traffic_scheds_size(); i++) {
        tech_profile::TrafficScheduler traffic_sched = traffic_scheds->traffic_scheds(i);

        direction = GetDirection(traffic_sched.direction());
        if (direction.compare("direction-not-supported") == 0)
            return bcm_to_grpc_err(BCM_ERR_PARM, "direction-not-supported");

        alloc_id = traffic_sched.alloc_id();
        sched_config = traffic_sched.scheduler();
        additional_bw = sched_config.additional_bw();
        priority = sched_config.priority();
        weight = sched_config.weight();
        sched_policy = sched_config.sched_policy();
        traffic_shaping_info = traffic_sched.traffic_shaping_info();
        tech_profile_id = traffic_sched.tech_profile_id();
        err =  CreateSched(direction, intf_id, onu_id, uni_id, port_no, alloc_id, additional_bw, weight, priority,
                           sched_policy, traffic_shaping_info, tech_profile_id);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create scheduler, err = %s\n", bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to create scheduler");
        }
    }
    return Status::OK;
}

bcmos_errno RemoveSched(int intf_id, int onu_id, int uni_id, int alloc_id, std::string direction, int tech_profile_id) {

    bcmos_errno err;
    bcmolt_interface_state state;
    bcmolt_status los_status;
    uint16_t sched_id;

    if (direction == upstream) {
        bcmolt_itupon_alloc_cfg cfg;
        bcmolt_itupon_alloc_key key = { };
        key.pon_ni = intf_id;
        key.alloc_id = alloc_id;
        sched_id = alloc_id;

        BCMOLT_CFG_INIT(&cfg, itupon_alloc, key);
        err = bcmolt_cfg_clear(dev_id, &cfg.hdr);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove scheduler, direction = %s, intf_id %d, alloc_id %d, err = %s\n",
                direction.c_str(), intf_id, alloc_id, bcmos_strerror(err));
            return err;
        }

        err = get_pon_interface_status((bcmolt_interface)intf_id, &state, &los_status);
        if (err == BCM_ERR_OK) {
            if (state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING && los_status == BCMOLT_STATUS_OFF) {
                OPENOLT_LOG(INFO, openolt_log_id, "PON interface: %d is enabled and LoS status is OFF, waiting for alloc cfg clear response\n",
                    intf_id);
                err = wait_for_alloc_action(intf_id, alloc_id, ALLOC_OBJECT_DELETE);
                if (err) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove scheduler, direction = %s, intf_id %d, alloc_id %d, err = %s\n",
                        direction.c_str(), intf_id, alloc_id, bcmos_strerror(err));
                    return err;
                }
            }
            else if (state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING && los_status == BCMOLT_STATUS_ON) {
                OPENOLT_LOG(INFO, openolt_log_id, "PON interface: %d is enabled but LoS status is ON, not waiting for alloc cfg clear response\n",
                    intf_id);
            }
            else if (state == BCMOLT_INTERFACE_STATE_INACTIVE) {
                OPENOLT_LOG(INFO, openolt_log_id, "PON interface: %d is disabled, not waiting for alloc cfg clear response\n",
                    intf_id);
            }
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to fetch PON interface status, intf_id = %d, err = %s\n",
                intf_id, bcmos_strerror(err));
            return err;
        }
    } else if (direction == downstream) {
        bcmolt_tm_sched_cfg cfg;
        bcmolt_tm_sched_key key = { };

        if (is_tm_sched_id_present(intf_id, onu_id, uni_id, direction, tech_profile_id)) {
            key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction, tech_profile_id);
            sched_id = key.id;
        } else {
            OPENOLT_LOG(INFO, openolt_log_id, "schduler not present in %s, err %d\n", direction.c_str(), err);
            return BCM_ERR_OK;
        }

        BCMOLT_CFG_INIT(&cfg, tm_sched, key);
        err = bcmolt_cfg_clear(dev_id, &(cfg.hdr));
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove scheduler, direction = %s, sched_id %d, \
intf_id %d, onu_id %d, tech_profile_id %d, err = %s\n", direction.c_str(), key.id, intf_id, onu_id, tech_profile_id, bcmos_strerror(err));
            return err;
        }
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Removed sched, direction = %s, id %d, intf_id %d, onu_id %d, tech_profile_id %d\n",
                direction.c_str(), sched_id, intf_id, onu_id, tech_profile_id);
    free_tm_sched_id(intf_id, onu_id, uni_id, direction, tech_profile_id);
    return BCM_ERR_OK;
}

Status RemoveTrafficSchedulers_(const tech_profile::TrafficSchedulers *traffic_scheds) {
    uint32_t intf_id = traffic_scheds->intf_id();
    uint32_t onu_id = traffic_scheds->onu_id();
    uint32_t uni_id = traffic_scheds->uni_id();
    std::string direction;
    uint32_t tech_profile_id;
    bcmos_errno err;

    for (int i = 0; i < traffic_scheds->traffic_scheds_size(); i++) {
        tech_profile::TrafficScheduler traffic_sched = traffic_scheds->traffic_scheds(i);

        direction = GetDirection(traffic_sched.direction());
        if (direction.compare("direction-not-supported") == 0)
            return bcm_to_grpc_err(BCM_ERR_PARM, "direction-not-supported");

        int alloc_id = traffic_sched.alloc_id();
        int tech_profile_id = traffic_sched.tech_profile_id();
        err = RemoveSched(intf_id, onu_id, uni_id, alloc_id, direction, tech_profile_id);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Error-removing-traffic-scheduler, err = %s\n",bcmos_strerror(err));
            return bcm_to_grpc_err(err, "error-removing-traffic-scheduler");
        }
    }
    return Status::OK;
}

bcmos_errno CreateTrafficQueueMappingProfile(uint32_t sched_id, uint32_t intf_id, uint32_t onu_id, uint32_t uni_id, \
                                             std::string direction, std::vector<uint32_t> tmq_map_profile) {
    bcmos_errno err;
    bcmolt_tm_qmp_cfg tm_qmp_cfg;
    bcmolt_tm_qmp_key tm_qmp_key;
    bcmolt_arr_u8_8 pbits_to_tmq_id = {0};

    int tm_qmp_id = get_tm_qmp_id(sched_id, intf_id, onu_id, uni_id, tmq_map_profile);
    if (tm_qmp_id == -1) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create tm queue mapping profile. Max allowed tm queue mapping profile count is 16.\n");
        return BCM_ERR_RANGE;
    }

    tm_qmp_key.id = tm_qmp_id;
    for (uint32_t priority=0; priority<tmq_map_profile.size(); priority++) {
        pbits_to_tmq_id.arr[priority] = tmq_map_profile[priority];
    }

    BCMOLT_CFG_INIT(&tm_qmp_cfg, tm_qmp, tm_qmp_key);
    BCMOLT_MSG_FIELD_SET(&tm_qmp_cfg, type, BCMOLT_TM_QMP_TYPE_PBITS);
    BCMOLT_MSG_FIELD_SET(&tm_qmp_cfg, pbits_to_tmq_id, pbits_to_tmq_id);
    //BCMOLT_MSG_FIELD_SET(&tm_qmp_cfg, ref_count, 0);
    //BCMOLT_MSG_FIELD_SET(&tm_qmp_cfg, state, BCMOLT_CONFIG_STATE_CONFIGURED);

    err = bcmolt_cfg_set(dev_id, &tm_qmp_cfg.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create tm queue mapping profile, tm_qmp_id %d, err = %s\n",
            tm_qmp_key.id, bcmos_strerror(err));
        return err;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Create tm queue mapping profile success, id %d\n", \
        tm_qmp_key.id);
    return BCM_ERR_OK;
}

bcmos_errno RemoveTrafficQueueMappingProfile(uint32_t tm_qmp_id) {
    bcmos_errno err;
    bcmolt_tm_qmp_cfg tm_qmp_cfg;
    bcmolt_tm_qmp_key tm_qmp_key;
    tm_qmp_key.id = tm_qmp_id;

    BCMOLT_CFG_INIT(&tm_qmp_cfg, tm_qmp, tm_qmp_key);
    err = bcmolt_cfg_clear(dev_id, &tm_qmp_cfg.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove tm queue mapping profile, tm_qmp_id %d, err = %s\n",
            tm_qmp_key.id, bcmos_strerror(err));
        return err;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Remove tm queue mapping profile success, id %d\n", \
        tm_qmp_key.id);
    return BCM_ERR_OK;
}

bcmos_errno CreateDefaultQueue(uint32_t intf_id, const std::string direction) {
    bcmos_errno err;

    /* Create default queues on the given PON/NNI scheduler */
    for (int queue_id = 0; queue_id < NUMBER_OF_DEFAULT_INTERFACE_QUEUES; queue_id++) {
        bcmolt_tm_queue_cfg tm_queue_cfg;
        bcmolt_tm_queue_key tm_queue_key = {};
        tm_queue_key.sched_id = get_default_tm_sched_id(intf_id, direction);
        tm_queue_key.id = queue_id;
        /* DefaultQueues on PON/NNI schedulers are created with egress_qos_type as
           BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE - with tm_q_set_id 32768 */
        tm_queue_key.tm_q_set_id = BCMOLT_TM_QUEUE_SET_ID_QSET_NOT_USE;

        BCMOLT_CFG_INIT(&tm_queue_cfg, tm_queue, tm_queue_key);
        BCMOLT_MSG_FIELD_SET(&tm_queue_cfg, tm_sched_param.type, BCMOLT_TM_SCHED_PARAM_TYPE_PRIORITY);
        BCMOLT_MSG_FIELD_SET(&tm_queue_cfg, tm_sched_param.u.priority.priority, queue_id);

        err = bcmolt_cfg_set(dev_id, &tm_queue_cfg.hdr);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create %s tm queue, id %d, sched_id %d, tm_q_set_id %d, err = %s\n", \
                    direction.c_str(), tm_queue_key.id, tm_queue_key.sched_id, tm_queue_key.tm_q_set_id, bcmos_strerror(err));
            return err;
        }

        OPENOLT_LOG(INFO, openolt_log_id, "Create %s tm_queue success, id %d, sched_id %d, tm_q_set_id %d\n", \
                direction.c_str(), tm_queue_key.id, tm_queue_key.sched_id, tm_queue_key.tm_q_set_id);
    }
    return BCM_ERR_OK;
}

bcmos_errno CreateQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id,
                        bcmolt_egress_qos_type qos_type, uint32_t priority, uint32_t gemport_id, uint32_t tech_profile_id) {
    bcmos_errno err;
    bcmolt_tm_queue_cfg cfg;
    bcmolt_tm_queue_key key = { };
    OPENOLT_LOG(INFO, openolt_log_id, "creating %s queue. access_intf_id = %d, onu_id = %d, uni_id = %d \
gemport_id = %d, tech_profile_id = %d\n", direction.c_str(), access_intf_id, onu_id, uni_id, gemport_id, tech_profile_id);

    key.sched_id = (direction.compare(upstream) == 0) ? get_default_tm_sched_id(nni_intf_id, direction) : \
        get_tm_sched_id(access_intf_id, onu_id, uni_id, direction, tech_profile_id);

    if (priority > 7) {
        return BCM_ERR_RANGE;
    }

    /* FIXME: The upstream queues have to be created once only.
    The upstream queues on the NNI scheduler are shared by all subscribers.
    When the first scheduler comes in, the queues get created, and are re-used by all others.
    Also, these queues should be present until the last subscriber exits the system.
    One solution is to have these queues always, i.e., create it as soon as OLT is enabled.

    There is one queue per gem port and Queue ID is fetched based on priority_q configuration
    for each GEM in TECH PROFILE */
    key.id = queue_id_list[priority];

    if (qos_type == BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE) {
        // Reset the Queue ID to 0, if it is fixed queue, i.e., there is only one queue for subscriber.
        key.id = 0;
        key.tm_q_set_id = BCMOLT_TM_QUEUE_SET_ID_QSET_NOT_USE;
    }
    else if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE) {
        key.tm_q_set_id = get_tm_qmp_id(key.sched_id, access_intf_id, onu_id, uni_id);
    }
    else {
        key.tm_q_set_id = BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "queue assigned queue_id = %d\n", key.id);

    BCMOLT_CFG_INIT(&cfg, tm_queue, key);
    BCMOLT_MSG_FIELD_SET(&cfg, tm_sched_param.u.priority.priority, priority);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create subscriber tm queue, direction = %s, queue_id %d, \
sched_id %d, tm_q_set_id %d, intf_id %d, onu_id %d, uni_id %d, tech_profile_id %d, err = %s\n", \
            direction.c_str(), key.id, key.sched_id, key.tm_q_set_id, access_intf_id, onu_id, uni_id, tech_profile_id, bcmos_strerror(err));
        return err;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Created tm_queue, direction %s, id %d, sched_id %d, tm_q_set_id %d, \
intf_id %d, onu_id %d, uni_id %d, tech_profiled_id %d\n", direction.c_str(), key.id, key.sched_id, key.tm_q_set_id, access_intf_id, onu_id, uni_id, tech_profile_id);
    return BCM_ERR_OK;
}

Status CreateTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues) {
    uint32_t intf_id = traffic_queues->intf_id();
    uint32_t onu_id = traffic_queues->onu_id();
    uint32_t uni_id = traffic_queues->uni_id();
    uint32_t tech_profile_id = traffic_queues->tech_profile_id();
    uint32_t sched_id;
    std::string direction;
    bcmos_errno err;
    bcmolt_egress_qos_type qos_type = get_qos_type(intf_id, onu_id, uni_id, traffic_queues->traffic_queues_size());

    if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE) {
        uint32_t queues_priority_q[traffic_queues->traffic_queues_size()] = {0};
        std::string queues_pbit_map[traffic_queues->traffic_queues_size()];
        for (int i = 0; i < traffic_queues->traffic_queues_size(); i++) {
            tech_profile::TrafficQueue traffic_queue = traffic_queues->traffic_queues(i);

            direction = GetDirection(traffic_queue.direction());
            if (direction.compare("direction-not-supported") == 0)
                return bcm_to_grpc_err(BCM_ERR_PARM, "direction-not-supported");

            queues_priority_q[i] = traffic_queue.priority();
            queues_pbit_map[i] = traffic_queue.pbit_map();
        }

        std::vector<uint32_t> tmq_map_profile(8, 0);
        tmq_map_profile = get_tmq_map_profile(get_valid_queues_pbit_map(queues_pbit_map, COUNT_OF(queues_pbit_map)), \
                                              queues_priority_q, COUNT_OF(queues_priority_q));
        sched_id = (direction.compare(upstream) == 0) ? get_default_tm_sched_id(nni_intf_id, direction) : \
            get_tm_sched_id(intf_id, onu_id, uni_id, direction, tech_profile_id);

        int tm_qmp_id = get_tm_qmp_id(tmq_map_profile);
        if (tm_qmp_id == -1) {
            err = CreateTrafficQueueMappingProfile(sched_id, intf_id, onu_id, uni_id, direction, tmq_map_profile);
            if (err != BCM_ERR_OK) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create tm queue mapping profile, err = %s\n", bcmos_strerror(err));
                return bcm_to_grpc_err(err, "Failed to create tm queue mapping profile");
            }
        } else if (tm_qmp_id != -1 && get_tm_qmp_id(sched_id, intf_id, onu_id, uni_id) == -1) {
            OPENOLT_LOG(INFO, openolt_log_id, "tm queue mapping profile present already with id %d\n", tm_qmp_id);
            update_sched_qmp_id_map(sched_id, intf_id, onu_id, uni_id, tm_qmp_id);
        }
    }

    for (int i = 0; i < traffic_queues->traffic_queues_size(); i++) {
        tech_profile::TrafficQueue traffic_queue = traffic_queues->traffic_queues(i);

        direction = GetDirection(traffic_queue.direction());
        if (direction.compare("direction-not-supported") == 0)
            return bcm_to_grpc_err(BCM_ERR_PARM, "direction-not-supported");

        err = CreateQueue(direction, intf_id, onu_id, uni_id, qos_type, traffic_queue.priority(), traffic_queue.gemport_id(), tech_profile_id);

        // If the queue exists already, lets not return failure and break the loop.
        if (err && err != BCM_ERR_ALREADY) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to create queue, err = %s\n",bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to create queue");
        }
    }
    return Status::OK;
}

bcmos_errno RemoveQueue(std::string direction, uint32_t access_intf_id, uint32_t onu_id, uint32_t uni_id,
                        bcmolt_egress_qos_type qos_type, uint32_t priority, uint32_t gemport_id, uint32_t tech_profile_id) {
    bcmolt_tm_queue_cfg cfg;
    bcmolt_tm_queue_key key = { };
    bcmos_errno err;

    if (direction == downstream) {
        if (is_tm_sched_id_present(access_intf_id, onu_id, uni_id, direction, tech_profile_id)) {
            key.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, direction, tech_profile_id);
            key.id = queue_id_list[priority];
        } else {
            OPENOLT_LOG(INFO, openolt_log_id, "queue not present in DS. Not clearing, access_intf_id %d, onu_id %d, uni_id %d, gemport_id %d, direction %s\n", access_intf_id, onu_id, uni_id, gemport_id, direction.c_str());
            return BCM_ERR_OK;
        }
    } else {
        /* In the upstream we use pre-created queues on the NNI scheduler that are used by all subscribers.
        They should not be removed. So, lets return OK. */
        return BCM_ERR_OK;
    }

    if (qos_type == BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE) {
         key.tm_q_set_id = BCMOLT_TM_QUEUE_SET_ID_QSET_NOT_USE;
        // Reset the queue id to 0 when using fixed queue.
        key.id = 0;
    }
    else if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE) {
         key.tm_q_set_id = get_tm_qmp_id(key.sched_id, access_intf_id, onu_id, uni_id);
    }
    else {
         key.tm_q_set_id = BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT;
    }

    BCMOLT_CFG_INIT(&cfg, tm_queue, key);
    err = bcmolt_cfg_clear(dev_id, &(cfg.hdr));
    if (err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove queue, direction = %s, queue_id %d, sched_id %d, \
tm_q_set_id %d, intf_id %d, onu_id %d, uni_id %d, err = %s\n",
                direction.c_str(), key.id, key.sched_id, key.tm_q_set_id, access_intf_id, onu_id, uni_id, bcmos_strerror(err));
        return err;
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Removed tm_queue, direction %s, id %d, sched_id %d, tm_q_set_id %d, \
intf_id %d, onu_id %d, uni_id %d\n", direction.c_str(), key.id, key.sched_id, key.tm_q_set_id, access_intf_id, onu_id, uni_id);

    return BCM_ERR_OK;
}

Status RemoveTrafficQueues_(const tech_profile::TrafficQueues *traffic_queues) {
    uint32_t intf_id = traffic_queues->intf_id();
    uint32_t onu_id = traffic_queues->onu_id();
    uint32_t uni_id = traffic_queues->uni_id();
    uint32_t port_no = traffic_queues->port_no();
    uint32_t tech_profile_id = traffic_queues->tech_profile_id();
    uint32_t sched_id;
    std::string direction;
    bcmos_errno err;
    bcmolt_egress_qos_type qos_type = get_qos_type(intf_id, onu_id, uni_id, traffic_queues->traffic_queues_size());

    for (int i = 0; i < traffic_queues->traffic_queues_size(); i++) {
        tech_profile::TrafficQueue traffic_queue = traffic_queues->traffic_queues(i);

        direction = GetDirection(traffic_queue.direction());
        if (direction.compare("direction-not-supported") == 0)
            return bcm_to_grpc_err(BCM_ERR_PARM, "direction-not-supported");

        err = RemoveQueue(direction, intf_id, onu_id, uni_id, qos_type, traffic_queue.priority(), traffic_queue.gemport_id(), tech_profile_id);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove queue, err = %s\n",bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Failed to remove queue");
        }
    }

    if (qos_type == BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE && (direction.compare(upstream) == 0 || direction.compare(downstream) == 0 && is_tm_sched_id_present(intf_id, onu_id, uni_id, direction, tech_profile_id))) {
        sched_id = (direction.compare(upstream) == 0) ? get_default_tm_sched_id(nni_intf_id, direction) : \
            get_tm_sched_id(intf_id, onu_id, uni_id, direction, tech_profile_id);

        int tm_qmp_id = get_tm_qmp_id(sched_id, intf_id, onu_id, uni_id);
        if (free_tm_qmp_id(sched_id, intf_id, onu_id, uni_id, tm_qmp_id)) {
            err = RemoveTrafficQueueMappingProfile(tm_qmp_id);
            if (err != BCM_ERR_OK) {
                OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove tm queue mapping profile, err = %s\n", bcmos_strerror(err));
                return bcm_to_grpc_err(err, "Failed to remove tm queue mapping profile");
            }
        }
    }
    clear_qos_type(intf_id, onu_id, uni_id);
    return Status::OK;
}

Status PerformGroupOperation_(const openolt::Group *group_cfg) {

    bcmos_errno err;
    bcmolt_group_key key = {};
    bcmolt_group_cfg grp_cfg_obj;
    bcmolt_group_members_update grp_mem_upd;
    bcmolt_members_update_command grp_mem_upd_cmd;
    bcmolt_group_member_info member_info = {};
    bcmolt_group_member_info_list_u8 members = {};
    bcmolt_intf_ref interface_ref = {};
    bcmolt_egress_qos egress_qos = {};
    bcmolt_tm_sched_ref tm_sched_ref = {};
    bcmolt_action a_val = {};

    uint32_t group_id = group_cfg->group_id();

    OPENOLT_LOG(INFO, openolt_log_id, "PerformGroupOperation request received for Group %d\n", group_id);

    if (group_id >= 0) {
        key.id = group_id;
    }
    else {
        OPENOLT_LOG(ERROR, openolt_log_id, "Invalid group id %d.\n", group_id);
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid group id");
    }

    BCMOLT_CFG_INIT(&grp_cfg_obj, group, key);
    BCMOLT_FIELD_SET_PRESENT(&grp_cfg_obj.data, group_cfg_data, state);

    OPENOLT_LOG(INFO, openolt_log_id, "Checking if Group %d exists...\n",group_id);

    err = bcmolt_cfg_get(dev_id, &(grp_cfg_obj.hdr));
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Error in querying Group %d, err = %s\n", group_id, bcmos_strerror(err));
        return bcm_to_grpc_err(err, "Error in querying group");
    }

    members.len = group_cfg->members_size();

    // IMPORTANT: A member cannot be added to a group if the group type is not determined.
    // Group type is determined after a flow is assigned to it.
    // Therefore, a group must be created first, then a flow (with multicast type) must be assigned to it.
    // Only then we can add members to the group.

    // if group does not exist, create it and return.
    if (grp_cfg_obj.data.state == BCMOLT_GROUP_STATE_NOT_CONFIGURED) {

        if (members.len != 0) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Member list is not empty for non-existent Group %d. Members can be added only after a flow is assigned to this newly-created group.\n", group_id);
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Non-empty member list given for non-existent group");
        } else {

            BCMOLT_CFG_INIT(&grp_cfg_obj, group, key);
            BCMOLT_MSG_FIELD_SET(&grp_cfg_obj, cookie, key.id);

            /*  Setting group actions and action parameters, if any.
                Only remove_outer_tag and translate_inner_tag actions and i_vid action parameter
                are supported for multicast groups in BAL 3.1.
            */
            const ::openolt::Action& action = group_cfg->action();
            const ::openolt::ActionCmd &cmd = action.cmd();

            bcmolt_action_cmd_id cmd_bmask = BCMOLT_ACTION_CMD_ID_NONE;
            if (cmd.remove_outer_tag()) {
                OPENOLT_LOG(INFO, openolt_log_id, "Action remove_outer_tag applied to Group %d.\n", group_id);
                cmd_bmask = (bcmolt_action_cmd_id) (cmd_bmask | BCMOLT_ACTION_CMD_ID_REMOVE_OUTER_TAG);
            }

            if (cmd.translate_inner_tag()) {
                OPENOLT_LOG(INFO, openolt_log_id, "Action translate_inner_tag applied to Group %d.\n", group_id);
                cmd_bmask = (bcmolt_action_cmd_id) (cmd_bmask | BCMOLT_ACTION_CMD_ID_XLATE_INNER_TAG);
            }

            BCMOLT_FIELD_SET(&a_val, action, cmds_bitmask, cmd_bmask);

            if (action.i_vid()) {
                OPENOLT_LOG(INFO, openolt_log_id, "Setting action parameter i_vid=%d for Group %d.\n", action.i_vid(), group_id);
                BCMOLT_FIELD_SET(&a_val, action, i_vid, action.i_vid());
            }

            BCMOLT_MSG_FIELD_SET(&grp_cfg_obj, action, a_val);

            // Create group
            err = bcmolt_cfg_set(dev_id, &(grp_cfg_obj.hdr));

            if (BCM_ERR_OK != err) {
                BCM_LOG(ERROR, openolt_log_id, "Failed to create Group %d, err = %s (%d)\n", key.id, bcmos_strerror(err), err);
                return bcm_to_grpc_err(err, "Error in creating group");
            }

            BCM_LOG(INFO, openolt_log_id, "Group %d has been created and configured with empty member list.\n", key.id);
            return Status::OK;
        }
    }

    // The group already exists. Continue configuring it according to the update member command.

    OPENOLT_LOG(INFO, openolt_log_id, "Configuring existing Group %d.\n",group_id);

    // MEMBER LIST CONSTRUCTION
    // Note that members.len can be 0 here. if the group already exists and the command is SET then sending
    // empty list to the group is a legit operation and this actually empties the member list.
    members.arr = (bcmolt_group_member_info*)bcmos_calloc((members.len)*sizeof(bcmolt_group_member_info));

    if (!members.arr) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to allocate memory for group member list.\n");
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Memory exhausted during member list creation");
    }

    /* SET GROUP MEMBERS UPDATE COMMAND */
    openolt::Group::GroupMembersCommand command = group_cfg->command();
    switch(command) {
        case openolt::Group::SET_MEMBERS :
            grp_mem_upd_cmd = BCMOLT_MEMBERS_UPDATE_COMMAND_SET;
            OPENOLT_LOG(INFO, openolt_log_id, "Setting %d members for Group %d.\n", members.len, group_id);
            break;
        case openolt::Group::ADD_MEMBERS :
            grp_mem_upd_cmd = BCMOLT_MEMBERS_UPDATE_COMMAND_ADD;
            OPENOLT_LOG(INFO, openolt_log_id, "Adding %d members to Group %d.\n", members.len, group_id);
            break;
        case openolt::Group::REMOVE_MEMBERS :
            grp_mem_upd_cmd = BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE;
            OPENOLT_LOG(INFO, openolt_log_id, "Removing %d members from Group %d.\n", members.len, group_id);
            break;
        default :
            OPENOLT_LOG(ERROR, openolt_log_id, "Invalid value %d for group member command.\n", command);
            bcmos_free(members.arr);
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid group member command");
    }

    // SET MEMBERS LIST
    for (int i = 0; i < members.len; i++) {

        if (command ==  openolt::Group::REMOVE_MEMBERS) {
            OPENOLT_LOG(INFO, openolt_log_id, "Removing group member %d from group %d\n",i,key.id);
        } else {
            OPENOLT_LOG(INFO, openolt_log_id, "Adding group member %d to group %d\n",i,key.id);
        }

        openolt::GroupMember *member = (openolt::GroupMember *) &group_cfg->members()[i];

        // Set member interface type
        openolt::GroupMember::InterfaceType if_type = member->interface_type();
        switch(if_type){
            case openolt::GroupMember::PON :
                BCMOLT_FIELD_SET(&interface_ref, intf_ref, intf_type, BCMOLT_INTERFACE_TYPE_PON);
                OPENOLT_LOG(INFO, openolt_log_id, "Interface type PON is assigned to GroupMember %d\n",i);
                break;
            case openolt::GroupMember::EPON_1G_PATH :
                BCMOLT_FIELD_SET(&interface_ref, intf_ref, intf_type, BCMOLT_INTERFACE_TYPE_EPON_1_G);
                OPENOLT_LOG(INFO, openolt_log_id, "Interface type EPON_1G is assigned to GroupMember %d\n",i);
                break;
            case openolt::GroupMember::EPON_10G_PATH :
                BCMOLT_FIELD_SET(&interface_ref, intf_ref, intf_type, BCMOLT_INTERFACE_TYPE_EPON_10_G);
                OPENOLT_LOG(INFO, openolt_log_id, "Interface type EPON_10G is assigned to GroupMember %d\n",i);
                break;
            default :
                OPENOLT_LOG(ERROR, openolt_log_id, "Invalid interface type value %d for GroupMember %d.\n",if_type,i);
                bcmos_free(members.arr);
                return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid interface type for a group member");
        }

        // Set member interface id
        if (member->interface_id() >= 0) {
            BCMOLT_FIELD_SET(&interface_ref, intf_ref, intf_id, member->interface_id());
            OPENOLT_LOG(INFO, openolt_log_id, "Interface %d is assigned to GroupMember %d\n", member->interface_id(), i);
        } else {
            bcmos_free(members.arr);
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid interface id for a group member");
        }

        // Set member interface_ref
        BCMOLT_FIELD_SET(&member_info, group_member_info, intf, interface_ref);

        // Set member gem_port_id. This must be a multicast gemport.
        if (member->gem_port_id() >= 0) {
            BCMOLT_FIELD_SET(&member_info, group_member_info, svc_port_id, member->gem_port_id());
            OPENOLT_LOG(INFO, openolt_log_id, "GEM Port %d is assigned to GroupMember %d\n", member->gem_port_id(), i);
        } else {
            bcmos_free(members.arr);
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid gem port id for a group member");
        }

        // Set member scheduler id and queue_id
        uint32_t tm_sched_id = get_default_tm_sched_id(member->interface_id(), downstream);
        OPENOLT_LOG(INFO, openolt_log_id, "Scheduler %d is assigned to GroupMember %d\n", tm_sched_id, i);
        BCMOLT_FIELD_SET(&tm_sched_ref, tm_sched_ref, id, tm_sched_id);
        BCMOLT_FIELD_SET(&egress_qos, egress_qos, tm_sched, tm_sched_ref);

        // We assume that all multicast traffic destined to a PON port is using the same fixed queue.
        uint32_t tm_queue_id;
        if (member->priority() >= 0 && member->priority() < NUMBER_OF_DEFAULT_INTERFACE_QUEUES) {
            tm_queue_id = queue_id_list[member->priority()];
            OPENOLT_LOG(INFO, openolt_log_id, "Queue %d is assigned to GroupMember %d\n", tm_queue_id, i);
            BCMOLT_FIELD_SET(&egress_qos, egress_qos, type, BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE);
            BCMOLT_FIELD_SET(&egress_qos.u.fixed_queue, egress_qos_fixed_queue, queue_id, tm_queue_id);
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id, "Invalid fixed queue priority/ID %d for GroupMember %d\n", member->priority(), i);
            bcmos_free(members.arr);
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid queue priority for a group member");
        }

        BCMOLT_FIELD_SET(&member_info, group_member_info, egress_qos, egress_qos);
        BCMOLT_ARRAY_ELEM_SET(&(members), i, member_info);
    }

    BCMOLT_OPER_INIT(&grp_mem_upd, group, members_update, key);
    BCMOLT_MSG_FIELD_SET(&grp_mem_upd, members_cmd.members, members);
    BCMOLT_MSG_FIELD_SET(&grp_mem_upd, members_cmd.command, grp_mem_upd_cmd);

    err = bcmolt_oper_submit(dev_id, &(grp_mem_upd.hdr));
    bcmos_free(members.arr);

    if (BCM_ERR_OK != err) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Failed to submit members update operation for Group %d err = %s (%d)\n", key.id, bcmos_strerror(err), err);
        return bcm_to_grpc_err(err, "Failed to submit members update operation for the group");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "Successfully submitted members update operation for Group %d\n", key.id);

    return Status::OK;
}

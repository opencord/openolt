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
#include <inttypes.h>

#include "device.h"
#include "core.h"
#include "indications.h"
#include "stats_collection.h"
#include "error_format.h"
#include "state.h"
#include "utils.h"

extern "C"
{
#include <bcmolt_api.h>
#include <bcmolt_host_api.h>
#include <bcmolt_api_model_supporting_enums.h>
#include <bal_version.h>
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

#define MAX_SUPPORTED_INTF 16
#define BAL_RSC_MANAGER_BASE_TM_SCHED_ID 16384
#define MAX_TM_QUEUE_ID 8192
#define MAX_TM_SCHED_ID 1003
#define EAP_ETHER_TYPE 34958

static unsigned int num_of_nni_ports = 0;
static unsigned int num_of_pon_ports = 0;
static std::string intf_technologies[MAX_SUPPORTED_INTF];
static const std::string UNKNOWN_TECH("unknown");
static const std::string MIXED_TECH("mixed");
static std::string board_technology(UNKNOWN_TECH);
static std::string chip_family(UNKNOWN_TECH);
static unsigned int OPENOLT_FIELD_LEN = 200;
static std::string firmware_version = "Openolt.2018.10.04";

const uint32_t tm_upstream_sched_id_start = 1020;
const uint32_t tm_downstream_sched_id_start = 1004;
//0 to 3 are default queues. Lets not use them.
const uint32_t tm_queue_id_start = 4;
// Upto 8 fixed Upstream. Queue id 0 to 3 are pre-created, lets not use them.
const uint32_t us_fixed_queue_id_list[8] = {4, 5, 6, 7, 8, 9, 10, 11};
const std::string upstream = "upstream";
const std::string downstream = "downstream";
bcmolt_oltid dev_id = 0;
/* Current session */
static bcmcli_session *current_session;
static bcmcli_entry *api_parent_dir;
bcmos_bool status_bcm_cli_quit = BCMOS_FALSE;
bcmos_task bal_cli_thread;
const char *bal_cli_thread_name = "bal_cli_thread";

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

uint16_t get_dev_id(void) {
    return dev_id;
}

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

char* openolt_read_sysinfo(const char* field_name, char* field_val)
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
       uint8_t ret;
       ret = fread(field_val, OPENOLT_FIELD_LEN, 1, fp);
       if (ret >= OPENOLT_FIELD_LEN)
           BCM_LOG(INFO, openolt_log_id,  "Read data length %u\n", ret);
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

Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state)
{
	 openolt::Indication ind;
	 openolt::IntfOperIndication* intf_oper_ind = new openolt::IntfOperIndication;

	 intf_oper_ind->set_type(type);
	 intf_oper_ind->set_intf_id(intf_id);
	 intf_oper_ind->set_oper_state(state);
	 ind.set_allocated_intf_oper_ind(intf_oper_ind);
	 oltIndQ.push(ind);
    return Status::OK;
}

#define CLI_HOST_PROMPT_FORMAT "BCM.%u> "

/* Build CLI prompt */
static void openolt_cli_get_prompt_cb(bcmcli_session *session, char *buf, uint32_t max_len)
{
    snprintf(buf, max_len, CLI_HOST_PROMPT_FORMAT, dev_id);
}

static int _bal_apiend_cli_thread_handler(long data)
{
    char init_string[]="\n";
    bcmcli_session *sess = current_session;
    bcmos_task_parm bal_cli_task_p_dummy;
    
    /* Switch to interactive mode if not stopped in the init script */
    if (!bcmcli_is_stopped(sess))
    {       
        /* Force a CLI command prompt
         * The string passed into the parse function
         * must be modifiable, so a string constant like
         * bcmcli_parse(current_session, "\n") will not
         * work.
         */
        bcmcli_parse(sess, init_string);
        
        /* Process user input until EOF or quit command */
        bcmcli_driver(sess);
    };      
    BCM_LOG(INFO, openolt_log_id, "BAL API End CLI terminated\n");

    /* Cleanup */
    bcmcli_session_close(current_session);
    bcmcli_token_destroy(NULL);	 
    return 0;
}

/* Init API CLI commands for the current device */
bcmos_errno bcm_openolt_api_cli_init(bcmcli_entry *parent_dir, bcmcli_session *session)
{
    bcmos_errno rc;

    api_parent_dir = parent_dir;

    rc = bcm_api_cli_set_commands(session);

#ifdef BCM_SUBSYSTEM_HOST
    /* Subscribe for device change indication */
    rc = rc ? rc : bcmolt_olt_sel_ind_register(_api_cli_olt_change_ind);
#endif

    return rc;
}

static bcmos_errno bcm_cli_quit(bcmcli_session *session, const bcmcli_cmd_parm parm[], uint16_t n_parms)
{
    bcmcli_stop(session);
    bcmcli_session_print(session, "CLI terminated by 'Quit' command\n");
    status_bcm_cli_quit = BCMOS_TRUE;

    return BCM_ERR_OK;
}

int get_status_bcm_cli_quit(void) {
     return status_bcm_cli_quit;
}

bcmos_errno bcmolt_apiend_cli_init() {
    bcmos_errno ret;
    bcmos_task_parm bal_cli_task_p = {};
    bcmos_task_parm bal_cli_task_p_dummy;

    /** before creating the task, check if it is already created by the other half of BAL i.e. Core side */
    if (BCM_ERR_OK != bcmos_task_query(&bal_cli_thread, &bal_cli_task_p_dummy))
    {
        /* Create BAL CLI thread */
        bal_cli_task_p.name = bal_cli_thread_name;
        bal_cli_task_p.handler = _bal_apiend_cli_thread_handler;
        bal_cli_task_p.priority = TASK_PRIORITY_CLI;
 
        ret = bcmos_task_create(&bal_cli_thread, &bal_cli_task_p);
        if (BCM_ERR_OK != ret)
        {
            bcmos_printf("Couldn't create BAL API end CLI thread\n");
            return ret;
        }
    }
}

Status Enable_(int argc, char *argv[]) {
    bcmos_errno err;
    bcmolt_host_init_parms init_parms = {};
    init_parms.transport.type = BCM_HOST_API_CONN_LOCAL;
    bcmcli_session_parm mon_session_parm;

    if (!state.is_activated()) {

        vendor_init();
        /* Initialize host subsystem */
        err = bcmolt_host_init(&init_parms);
        if (BCM_ERR_OK != err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to init OLT\n");
            return bcm_to_grpc_err(err, "Failed to init OLT");
        }

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
            BCM_LOG(ERROR, openolt_log_id, "Failed to add apiend init\n");
            return bcm_to_grpc_err(err, "Failed to add apiend init");
        }

        bcmos_fastlock_init(&data_lock, 0);
        BCM_LOG(INFO, openolt_log_id, "Enable OLT - %s-%s\n", VENDOR_ID, MODEL_ID);

        if (bcmolt_api_conn_mgr_is_connected(dev_id))
        {
            Status status = SubscribeIndication();
            if (!status.ok()) {
                BCM_LOG(ERROR, openolt_log_id, "SubscribeIndication failed - %s : %s\n",
                    grpc_status_code_to_string(status.error_code()).c_str(),
                    status.error_message().c_str());

                return status;
            }
            bcmos_errno err;
            bcmolt_odid dev;
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
                    bcmolt_device_cfg cfg;
                    BCM_LOG(INFO, openolt_log_id, "Enable Maple - %d/%d\n", dev + 1, BCM_MAX_DEVS_PER_LINE_CARD);
                    BCMOLT_OPER_INIT(&oper, device, connect, key);
                    BCMOLT_MSG_FIELD_SET(&oper, inni_config.mode, BCMOLT_INNI_MODE_ALL_10_G_XFI);
                    BCMOLT_MSG_FIELD_SET (&oper, system_mode, BCMOLT_SYSTEM_MODE_XGS__2_X);
                    err = bcmolt_oper_submit(dev_id, &oper.hdr);
                    if (err) 
                        BCM_LOG(ERROR, openolt_log_id, "Enable Maple deivce %d failed\n", dev);
                    bcmos_usleep(200000);
                }
                else {
                    BCM_LOG(ERROR, openolt_log_id, "Maple deivce %d already connected\n", dev);
                    state.activate();
                }
            }
            init_stats();
        }
    }

    /* Start CLI */
    BCM_LOG(INFO, def_log_id, "Starting CLI\n");
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
        BCM_LOG(INFO, openolt_log_id, "Disable OLT, add an extra indication\n");
        pushOltOperInd(nni_intf_id, "nni", "up");
    }
    return status;

}

Status Reenable_() {
    Status status = EnableUplinkIf_(0);
    if (status.ok()) {
        state.activate();
        BCM_LOG(INFO, openolt_log_id, "Reenable OLT, add an extra indication\n");
        pushOltOperInd(0, "nni", "up");
    }
    return status;
}

bcmos_errno get_pon_interface_status(bcmolt_interface pon_ni, bcmolt_interface_state *state) {
    bcmos_errno err;
    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_ni;

    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, state);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, itu);
    err = bcmolt_cfg_get(dev_id, &pon_cfg.hdr);
    *state = pon_cfg.data.state;
    return err;
}

Status EnablePonIf_(uint32_t intf_id) {
    bcmos_errno err = BCM_ERR_OK; 
    bcmolt_pon_interface_cfg interface_obj;
    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)intf_id};
    bcmolt_pon_interface_set_pon_interface_state pon_interface_set_state;
    bcmolt_interface_state state;

    err = get_pon_interface_status((bcmolt_interface)intf_id, &state); 
    if (err == BCM_ERR_OK) {
        if (state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING) {
            BCM_LOG(INFO, openolt_log_id, "PON interface: %d already enabled\n", intf_id);
            return Status::OK;
        }
    } 
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);
    BCMOLT_OPER_INIT(&pon_interface_set_state, pon_interface, set_pon_interface_state, intf_key);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.control, BCMOLT_CONTROL_STATE_ENABLE);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.interval, 5000);
    BCMOLT_MSG_FIELD_SET(&interface_obj, discovery.onu_post_discovery_mode,
        BCMOLT_ONU_POST_DISCOVERY_MODE_ACTIVATE);
    BCMOLT_FIELD_SET(&pon_interface_set_state.data, pon_interface_set_pon_interface_state_data,
        operation, BCMOLT_INTERFACE_OPERATION_ACTIVE_WORKING);

    err = bcmolt_cfg_set(dev_id, &interface_obj.hdr);
    if (err != BCM_ERR_OK) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable discovery onu: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to enable discovery onu");
    }
    err = bcmolt_oper_submit(dev_id, &pon_interface_set_state.hdr);
    if (err != BCM_ERR_OK) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable PON interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to enable PON interface");
    }
    else {
        BCM_LOG(INFO, openolt_log_id, "Successfully enabled PON interface: %d\n", intf_id);
        BCM_LOG(INFO, openolt_log_id, "Initializing tm sched creation for PON interface: %d\n", intf_id);
        CreateDefaultSchedQueue_(intf_id, downstream);
    }

    return Status::OK;
}

Status DisableUplinkIf_(uint32_t intf_id) {
    bcmolt_pon_interface_key intf_key = {.pon_ni = (uint8_t)intf_id};
    bcmolt_pon_interface_set_pon_interface_state pon_interface_set_state;

    BCMOLT_OPER_INIT(&pon_interface_set_state, pon_interface, set_pon_interface_state, intf_key);
    BCMOLT_MSG_FIELD_SET(&pon_interface_set_state, operation, BCMOLT_INTERFACE_OPERATION_ACTIVE_WORKING );

    bcmos_errno err = bcmolt_oper_submit(dev_id, &pon_interface_set_state.hdr);
    if (err == BCM_ERR_OK) {
        /* Wait for the interface activation to successfully complete */
        BCM_LOG(DEBUG, openolt_log_id, "PON interface: %d already enabled\n", intf_id);
        return Status::OK;
    } else {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable PON interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to enable PON interface");
    }

    return Status::OK;
}

Status ProbeDeviceCapabilities_() {
    bcmos_errno err;
    bcmolt_device_cfg dev_cfg = { }; 
    bcmolt_device_key dev_key = { };
    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };

    dev_key.device_id = dev_id;
    BCMOLT_CFG_INIT(&dev_cfg, device, dev_key);
    BCMOLT_MSG_FIELD_GET(&dev_cfg, firmware_sw_version);
    BCMOLT_MSG_FIELD_GET(&dev_cfg, chip_family);
    BCMOLT_MSG_FIELD_GET(&dev_cfg, system_mode);
    err = bcmolt_cfg_get(dev_id, &dev_cfg.hdr);
    if (err) { 
        BCM_LOG(ERROR, openolt_log_id, "device: Failed to query OLT\n");
        return bcm_to_grpc_err(err, "device: Failed to query OLT");
    }

    bcmolt_topology_map topo_map[BCM_MAX_PONS_PER_OLT] = { };
    bcmolt_topology topo = { };
    topo.topology_maps.len = BCM_MAX_PONS_PER_OLT;
    topo.topology_maps.arr = &topo_map[0];
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    BCMOLT_MSG_FIELD_GET(&olt_cfg, bal_state);
    BCMOLT_FIELD_SET_PRESENT(&olt_cfg.data, olt_cfg_data, topology);
    BCMOLT_CFG_LIST_BUF_SET(&olt_cfg, olt, topo.topology_maps.arr, 
        sizeof(bcmolt_topology_map) * topo.topology_maps.len);
    err = bcmolt_cfg_get(dev_id, &olt_cfg.hdr);
    if (err) { 
        BCM_LOG(ERROR, openolt_log_id, "cfg: Failed to query OLT\n");
        return bcm_to_grpc_err(err, "cfg: Failed to query OLT");
    }

    std::string bal_version;
    bal_version += std::to_string(dev_cfg.data.firmware_sw_version.major)
                + "." + std::to_string(dev_cfg.data.firmware_sw_version.minor)
                + "." + std::to_string(dev_cfg.data.firmware_sw_version.revision);
    firmware_version = "BAL." + bal_version + "__" + firmware_version;

    switch(dev_cfg.data.system_mode) {
        case 9 ... 12:  board_technology = "GPON"; break;
        case 13 ... 16: board_technology = "XGPON"; break;
        case 18 ... 20: board_technology = "XGS-PON"; break;
    }

    switch(dev_cfg.data.chip_family) {
        case BCMOLT_CHIP_FAMILY_CHIP_FAMILY_6862_X_: chip_family = "Maple"; break;
        case BCMOLT_CHIP_FAMILY_CHIP_FAMILY_6865_X_: chip_family = "Aspen"; break;
    }

    num_of_nni_ports = olt_cfg.data.topology.num_switch_ports;
    num_of_pon_ports = olt_cfg.data.topology.topology_maps.len;

    BCM_LOG(INFO, openolt_log_id, "OLT capabilitites, oper_state: %s\n", 
            olt_cfg.data.bal_state == BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY 
            ? "up" : "down");

    BCM_LOG(INFO, openolt_log_id, "version %s object model: %d\n", 
        bal_version.c_str(), BAL_API_VERSION);

    BCM_LOG(INFO, openolt_log_id, "topology nni:%d pon:%d dev:%d ppd:%d family: %s\n",
            num_of_nni_ports,
            num_of_pon_ports,
            BCM_MAX_DEVS_PER_LINE_CARD,
            BCM_MAX_PONS_PER_DEV,
            chip_family.c_str());

    BCM_LOG(INFO, openolt_log_id, "PON num_intfs: %d global board_technology: %s\n", 
        num_of_pon_ports, board_technology.c_str());

    return Status::OK;
}

Status ProbePonIfTechnology_() {
    // Probe maximum extent possible as configured into BAL driver to determine
    // which are active in the current BAL topology. And for those
    // that are active, determine each port's access technology, i.e. "gpon" or "xgspon".
    for (uint32_t intf_id = 0; intf_id < num_of_pon_ports; ++intf_id) {
        bcmolt_pon_interface_cfg interface_obj;
        bcmolt_pon_interface_key interface_key;

        interface_key.pon_ni = intf_id;
        BCMOLT_CFG_INIT(&interface_obj, pon_interface, interface_key);
        BCMOLT_MSG_FIELD_GET(&interface_obj, gpon_trx);
        BCMOLT_MSG_FIELD_GET(&interface_obj, xgpon_trx);

        bcmos_errno err = bcmolt_cfg_get(dev_id, &interface_obj.hdr);
        if (err != BCM_ERR_OK) {
            intf_technologies[intf_id] = UNKNOWN_TECH;
            if(err != BCM_ERR_RANGE) BCM_LOG(ERROR, openolt_log_id, "Failed to get PON config: %d\n", intf_id);
        }
        else {
            if (board_technology == "XGS-PON") {
                switch(interface_obj.data.xgpon_trx.transceiver_type) {
                    case BCMOLT_XGPON_TRX_TYPE_LTH_7222_PC:
                    case BCMOLT_XGPON_TRX_TYPE_WTD_RTXM266_702: 
                    case BCMOLT_XGPON_TRX_TYPE_LTH_7222_BC_PLUS: 
                    case BCMOLT_XGPON_TRX_TYPE_LTH_7226_PC:
                    case BCMOLT_XGPON_TRX_TYPE_LTH_5302_PC: 
                    case BCMOLT_XGPON_TRX_TYPE_LTH_7226_A_PC_PLUS: 
                    case BCMOLT_XGPON_TRX_TYPE_D272RR_SSCB_DM: 
                        intf_technologies[intf_id] = "XGS-PON";
                        break;
                }
            } else if (board_technology == "GPON") {
                switch(interface_obj.data.gpon_trx.transceiver_type) {
                    case BCMOLT_TRX_TYPE_SPS_43_48_H_HP_CDE_SD_2013: 
                    case BCMOLT_TRX_TYPE_LTE_3680_M:
                    case BCMOLT_TRX_TYPE_SOURCE_PHOTONICS:
                    case BCMOLT_TRX_TYPE_LTE_3680_P_TYPE_C_PLUS:
			           case BCMOLT_TRX_TYPE_LTE_3680_P_BC: 
                        intf_technologies[intf_id] = "GPON";
                        break;
                }
            }

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

bcmos_errno get_nni_interface_status(bcmolt_interface id, bcmolt_interface_state *state) {
    bcmos_errno err;
    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = id;

    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    BCMOLT_FIELD_SET_PRESENT(&nni_cfg.data, nni_interface_cfg_data, state);
    err = bcmolt_cfg_get(dev_id, &nni_cfg.hdr);
    *state = nni_cfg.data.state;
    return err;
}

Status EnableUplinkIf_(uint32_t intf_id) {
    bcmos_errno err = BCM_ERR_OK; 
    bcmolt_nni_interface_key intf_key = {.id = (bcmolt_interface)intf_id};
    bcmolt_nni_interface_set_nni_state nni_interface_set_state;
    bcmolt_interface_state state;

    err = get_nni_interface_status((bcmolt_interface)intf_id, &state); 
    if (err == BCM_ERR_OK) {
        if (state == BCMOLT_INTERFACE_STATE_ACTIVE_WORKING) {
            BCM_LOG(INFO, openolt_log_id, "NNI interface: %d already enabled\n", intf_id);
            BCM_LOG(INFO, openolt_log_id, "Initializing tm sched creation for NNI interface: %d\n", intf_id);
            CreateDefaultSchedQueue_(intf_id, upstream);
            return Status::OK;
        }
    }

    BCMOLT_OPER_INIT(&nni_interface_set_state, nni_interface, set_nni_state, intf_key);
    BCMOLT_FIELD_SET(&nni_interface_set_state.data, nni_interface_set_nni_state_data,
        nni_state, BCMOLT_INTERFACE_OPERATION_ACTIVE_WORKING);
    err = bcmolt_oper_submit(dev_id, &nni_interface_set_state.hdr);
    if (err != BCM_ERR_OK) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to enable NNI interface: %d, err %d\n", intf_id, err);
        return bcm_to_grpc_err(err, "Failed to enable NNI interface");
    }
    else {
        BCM_LOG(INFO, openolt_log_id, "Successfully enabled NNI interface: %d\n", intf_id);
        BCM_LOG(INFO, openolt_log_id, "Initializing tm sched creation for NNI interface: %d\n", intf_id);
        CreateDefaultSchedQueue_(intf_id, upstream);
    }

    return Status::OK;
}

Status DisablePonIf_(uint32_t intf_id) {
    bcmolt_pon_interface_cfg interface_obj;
    bcmolt_pon_interface_key interface_key;

    interface_key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, interface_key);
    BCMOLT_MSG_FIELD_GET(&interface_obj, state);
    bcmos_errno err = bcmolt_cfg_get(dev_id, &interface_obj.hdr);
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to disable PON interface: %d\n", intf_id);
        return bcm_to_grpc_err(err, "Failed to disable PON interface");
    }

    return Status::OK;
}

Status ActivateOnu_(uint32_t intf_id, uint32_t onu_id,
    const char *vendor_id, const char *vendor_specific, uint32_t pir) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    bcmolt_serial_number serial_number; /**< ONU serial number */
    bcmolt_bin_str_36 registration_id; /**< ONU registration ID */

    onu_key.onu_id = onu_id;
    onu_key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
    if (err == BCM_ERR_OK) { 
        if ((onu_cfg.data.onu_state == BCMOLT_ONU_STATE_PROCESSING || 
             onu_cfg.data.onu_state == BCMOLT_ONU_STATE_ACTIVE) ||
           (onu_cfg.data.onu_state == BCMOLT_ONU_STATE_INACTIVE &&
				onu_cfg.data.onu_old_state == BCMOLT_ONU_STATE_NOT_CONFIGURED))
            return Status::OK;
    }

    BCM_LOG(INFO, openolt_log_id,  "Enabling ONU %d on PON %d : vendor id %s, \
        vendor specific %s, pir %d\n", onu_id, intf_id, vendor_id, 
        vendor_specific_to_str(vendor_specific).c_str(), pir);

    memcpy(serial_number.vendor_id.arr, vendor_id, 4);
    memcpy(serial_number.vendor_specific.arr, vendor_specific, 4);
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_MSG_FIELD_SET(&onu_cfg, onu_rate, BCMOLT_ONU_RATE_RATE_10G_DS_10G_US);
    BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.serial_number, serial_number);
    BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.auto_learning, BCMOS_TRUE);
    /*set burst and data profiles to fec disabled*/
    BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.xgpon.ranging_burst_profile, 0);
    BCMOLT_MSG_FIELD_SET(&onu_cfg, itu.xgpon.data_burst_profile, 1);
    err = bcmolt_cfg_set(dev_id, &onu_cfg.hdr);	
    if (err != BCM_ERR_OK) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to set activate ONU %d on PON %d, err %d\n", onu_id, intf_id, err);
        return bcm_to_grpc_err(err, "Failed to activate ONU");
    }

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
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
    if (err == BCM_ERR_OK) { 
        switch (onu_state) {
            case BCMOLT_ONU_OPERATION_ACTIVE:
                BCMOLT_OPER_INIT(&onu_oper, onu, set_onu_state, onu_key);
                BCMOLT_FIELD_SET(&onu_oper.data, onu_set_onu_state_data, 
                    onu_state, BCMOLT_ONU_OPERATION_INACTIVE);
                err = bcmolt_oper_submit(dev_id, &onu_oper.hdr);
                if (err != BCM_ERR_OK) {
                    BCM_LOG(ERROR, openolt_log_id, "Failed to \
                        deactivate ONU %d on PON %d, err %d\n", onu_id, intf_id, err);
                    return bcm_to_grpc_err(err, "Failed to deactivate ONU");
                }
                break;
        }
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

    bcmolt_onu_cfg cfg_obj;
    bcmolt_onu_key key;

    BCM_LOG(INFO, openolt_log_id, "Processing subscriber terminal cfg clear for sub_term_id %d  and intf_id %d\n",
        onu_id, intf_id);

    key.onu_id = onu_id;
    key.pon_ni = intf_id;
    BCMOLT_CFG_INIT(&cfg_obj, onu, key);

    bcmos_errno err = bcmolt_cfg_clear(dev_id, &cfg_obj.hdr);	
    if (err != BCM_ERR_OK)
    {
       BCM_LOG(ERROR, openolt_log_id, "Failed to clear information for BAL subscriber_terminal_id %d, Interface ID %d\n",
            onu_id, intf_id);
        return Status(grpc::StatusCode::INTERNAL, "Failed to delete ONU");
    }

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
        BCM_LOG(ERROR, omci_log_id, "Error sending OMCI message to ONU %d on PON %d\n", onu_id, intf_id);
    } else {
        BCM_LOG(DEBUG, omci_log_id, "OMCI request msg of length %d sent to ONU %d on PON %d : %s\n",
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
                BCM_LOG(ERROR, openolt_log_id, "Packet out failed to find destination for ONU %d port_no %u on PON %d\n",
                        onu_id, port_no, intf_id);
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "no flow for port_no");
            }
            BCM_LOG(INFO, openolt_log_id, "Gem port %u found for ONU %d port_no %u on PON %d\n",
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

        BCM_LOG(INFO, openolt_log_id, "Packet out of length %d sent to gemport %d on pon %d port_no %u\n",
            (uint8_t)pkt.size(), gemport_id, intf_id, port_no);

        /* call API */
        bcmolt_oper_submit(0, &pon_interface_cpu_packets.hdr);
    }
    else {
        //TODO: Port No is 0, it is coming sender requirement.
        BCM_LOG(INFO, openolt_log_id, "port_no %d onu %d on pon %d\n",
            port_no, onu_id, intf_id);
    }
    free(buf.arr);

    return Status::OK;
}

Status UplinkPacketOut_(uint32_t intf_id, const std::string pkt, bcmolt_flow_id flow_id) {
    bcmolt_flow_cfg cfg;
    bcmolt_flow_key key = {}; /* declare key */
    bcmolt_bin_str buffer = {};
    bcmolt_flow_send_eth_packet oper; /* declare main API struct */

    key.flow_id = flow_id;
    key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM; /* send from uplink direction */

    /* Initialize the API struct. */
    BCMOLT_OPER_INIT(&oper, flow, send_eth_packet, key);

    buffer.len = pkt.size();
    buffer.arr = (uint8_t *)malloc((buffer.len)*sizeof(uint8_t));
    memcpy(buffer.arr, (uint8_t *)pkt.data(), buffer.len);
    if (buffer.arr == NULL) {
        BCM_LOG(ERROR, openolt_log_id, "allocate pakcet buffer failed\n");
        return bcm_to_grpc_err(BCM_ERR_PARM, "allocate pakcet buffer failed");
    }
    BCMOLT_FIELD_SET(&oper.data, flow_send_eth_packet_data, buffer, buffer);

    bcmos_errno err = bcmolt_oper_submit(dev_id, &oper.hdr);
    if (err)
        BCM_LOG(ERROR, omci_log_id, "Error sending packets to port %d\n", intf_id);

    return Status::OK;
}

uint32_t GetPortNum_(uint32_t flow_id) {
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
    bcmolt_flow_cfg cfg;
    bcmolt_flow_key key = { }; /**< Object key. */
    int32_t o_vid = -1;
    bool single_tag = false;
    uint32_t ether_type = 0;

    BCM_LOG(INFO, openolt_log_id, "flow add - intf_id %d, onu_id %d, uni_id %d, port_no %u, flow_id %d, flow_type %s, \
            gemport_id %d, network_intf_id %d, cookie %"PRIu64"\n", \
            access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type.c_str(), gemport_id, network_intf_id, cookie);

    key.flow_id = flow_id;
    if (flow_type.compare(upstream) == 0 ) {
        key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
    } else if (flow_type.compare(downstream) == 0) {
        key.flow_type = BCMOLT_FLOW_TYPE_DOWNSTREAM;
    } else {
        BCM_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    BCMOLT_CFG_INIT(&cfg, flow, key);
    BCMOLT_MSG_FIELD_SET(&cfg, cookie, cookie);

    if (access_intf_id >= 0) {
        if (key.flow_type == BCMOLT_FLOW_TYPE_UPSTREAM) {
            BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_PON);
            BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.intf_id, access_intf_id);
        } else if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
            BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_PON);
            BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_id, access_intf_id);
        }
    }
    if (network_intf_id >= 0) {
        if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
            //TODO: The ingress type is NNI condition
            //BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.access_int_id.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_NNI);
            //BCMOLT_MSG_FIELD_SET(&cfg, ingress_intf.access_int_id.intf_id, network_intf_id);
            //The egress type is PON condition
            BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_type, BCMOLT_FLOW_INTERFACE_TYPE_PON);
            BCMOLT_MSG_FIELD_SET(&cfg, egress_intf.intf_id, access_intf_id);
        }
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
    if (priority_value >= 0) {
        BCMOLT_MSG_FIELD_SET(&cfg, priority, priority_value);
    }

    {
        bcmolt_classifier val = { };
        /* removed by BAL v3.0
        if (classifier.o_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify o_tpid 0x%04x\n", classifier.o_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, o_tpid, classifier.o_tpid());
        }
        */
        if (classifier.o_vid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify o_vid %d\n", classifier.o_vid());
            BCMOLT_FIELD_SET(&val, classifier, o_vid, classifier.o_vid());
        }
        /* removed by BAL v3.0
        if (classifier.i_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify i_tpid 0x%04x\n", classifier.i_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, classifier, i_tpid, classifier.i_tpid());
        }
        */
        if (classifier.i_vid()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify i_vid %d\n", classifier.i_vid());
            BCMOLT_FIELD_SET(&val, classifier, i_vid, classifier.i_vid());
        }

        if (classifier.eth_type()) {
            ether_type = classifier.eth_type();
            BCM_LOG(DEBUG, openolt_log_id, "classify ether_type 0x%04x\n", classifier.eth_type());
            BCMOLT_FIELD_SET(&val, classifier, ether_type, classifier.eth_type());
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
            BCMOLT_FIELD_SET(&val, classifier, ip_proto, classifier.ip_proto());
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
            BCMOLT_FIELD_SET(&val, classifier, src_port, classifier.src_port());
        }

        if (classifier.dst_port()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify dst_port %d\n", classifier.dst_port());
            BCMOLT_FIELD_SET(&val, classifier, dst_port, classifier.dst_port());
        }

        if (!classifier.pkt_tag_type().empty()) {
            BCM_LOG(DEBUG, openolt_log_id, "classify tag_type %s\n", classifier.pkt_tag_type().c_str());
            if (classifier.pkt_tag_type().compare("untagged") == 0) {
				    BCMOLT_FIELD_SET(&val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_UNTAGGED);
            } else if (classifier.pkt_tag_type().compare("single_tag") == 0) {
                BCMOLT_FIELD_SET(&val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_SINGLE_TAG);
                single_tag = true;

                BCM_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                BCMOLT_FIELD_SET(&val, classifier, o_pbits, classifier.o_pbits());
            } else if (classifier.pkt_tag_type().compare("double_tag") == 0) {
                BCMOLT_FIELD_SET(&val, classifier, pkt_tag_type, BCMOLT_PKT_TAG_TYPE_DOUBLE_TAG);

                BCM_LOG(DEBUG, openolt_log_id, "classify o_pbits 0x%x\n", classifier.o_pbits());
                BCMOLT_FIELD_SET(&val, classifier, o_pbits, classifier.o_pbits());
            }
        }
        BCMOLT_MSG_FIELD_SET(&cfg, classifier, val);
    }

    {
        bcmolt_action val = { };
        bcmolt_api_prop_path path;

        const ::openolt::ActionCmd& cmd = action.cmd();

        if (cmd.add_outer_tag()) {
            BCM_LOG(INFO, openolt_log_id, "action add o_tag\n");
            BCMOLT_FIELD_SET(&val, action, cmds_bitmask, BCMOLT_ACTION_CMD_ID_ADD_OUTER_TAG);
        }

        if (cmd.remove_outer_tag()) {
            BCM_LOG(INFO, openolt_log_id, "action pop o_tag\n");
            BCMOLT_FIELD_SET(&val, action, cmds_bitmask, BCMOLT_ACTION_CMD_ID_REMOVE_OUTER_TAG);
        }
        /* removed by BAL v3.0
        if (cmd.trap_to_host()) {
            BCM_LOG(INFO, openolt_log_id, "action trap-to-host\n");
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, cmds_bitmask, BCMBAL_ACTION_CMD_ID_TRAP_TO_HOST);
        }
        */
        if (action.o_vid()) {
            BCM_LOG(INFO, openolt_log_id, "action o_vid=%d\n", action.o_vid());
            o_vid = action.o_vid();
            BCMOLT_FIELD_SET(&val, action, o_vid, action.o_vid());
        }

        if (action.o_pbits()) {
            BCM_LOG(INFO, openolt_log_id, "action o_pbits=0x%x\n", action.o_pbits());
            BCMOLT_FIELD_SET(&val, action, o_pbits, action.o_pbits());
        }
        /* removed by BAL v3.0
        if (action.o_tpid()) {
            BCM_LOG(INFO, openolt_log_id, "action o_tpid=0x%04x\n", action.o_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, o_tpid, action.o_tpid());
        }
        */
        if (action.i_vid()) {
            BCM_LOG(INFO, openolt_log_id, "action i_vid=%d\n", action.i_vid());
            BCMOLT_FIELD_SET(&val, action, i_vid, action.i_vid());
        }

        if (action.i_pbits()) {
            BCM_LOG(DEBUG, openolt_log_id, "action i_pbits=0x%x\n", action.i_pbits());
            BCMOLT_FIELD_SET(&val, action, i_pbits, action.i_pbits());
        }
        /* removed by BAL v3.0
        if (action.i_tpid()) {
            BCM_LOG(DEBUG, openolt_log_id, "action i_tpid=0x%04x\n", action.i_tpid());
            BCMBAL_ATTRIBUTE_PROP_SET(&val, action, i_tpid, action.i_tpid());
        }
        */
        BCMOLT_MSG_FIELD_SET(&cfg, action, val);
    }

    if ((access_intf_id >= 0) && (onu_id >= 0)) {
        bcmolt_tm_queue_ref val = { };

        if (key.flow_type == BCMOLT_FLOW_TYPE_DOWNSTREAM) {
            if (single_tag && ether_type == EAP_ETHER_TYPE) {
                val.sched_id = get_default_tm_sched_id(access_intf_id, downstream);
                val.queue_id = 0;
            } else {
                val.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, downstream); // Subscriber Scheduler
                val.queue_id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, downstream);
            }
            BCM_LOG(INFO, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d\n", \
                    downstream.c_str(), val.queue_id, val.sched_id);

            BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE);
            BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, val.sched_id);
            BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.fixed_queue.queue_id, val.queue_id);
        } else if (key.flow_type == BCMOLT_FLOW_TYPE_UPSTREAM) {
            /* removed by BAL v3.0. N/A - Alloc ID is out of the scope of BAL. Used for OMCI only.
            bcmbal_tm_sched_id val1;
            val1 = get_tm_sched_id(access_intf_id, onu_id, uni_id, upstream); // DBA Scheduler ID
            BCMBAL_CFG_PROP_SET(&cfg, flow, dba_tm_sched_id, val1);
            */
            val.sched_id = get_default_tm_sched_id(network_intf_id, upstream); // NNI Scheduler ID
            val.queue_id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, upstream); // Queue on NNI
            BCM_LOG(INFO, openolt_log_id, "direction = %s, queue_id = %d, sched_id = %d\n", \
                    upstream.c_str(), val.queue_id, val.sched_id);
        }
        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.type, BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE);
        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.tm_sched.id, val.sched_id);
        BCMOLT_MSG_FIELD_SET(&cfg , egress_qos.u.fixed_queue.queue_id, val.queue_id);
    }

    BCMOLT_MSG_FIELD_SET(&cfg, state, BCMOLT_FLOW_STATE_ENABLE);
    bcmos_errno err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if (err) {
        BCM_LOG(ERROR, openolt_log_id,  "Flow add failed\n");
        return bcm_to_grpc_err(err, "flow add failed");
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
    } else {
        BCM_LOG(WARNING, openolt_log_id, "Invalid flow type %s\n", flow_type.c_str());
        return bcm_to_grpc_err(BCM_ERR_PARM, "Invalid flow type");
    }

    bcmos_fastlock_lock(&data_lock);
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
        BCM_LOG(ERROR, openolt_log_id, "Error %d while removing flow %d, %s\n",
            err, flow_id, flow_type.c_str());
        return Status(grpc::StatusCode::INTERNAL, "Failed to remove flow");
    }

    BCM_LOG(INFO, openolt_log_id, "Flow %d, %s removed\n", flow_id, flow_type.c_str());
    return Status::OK;
}

Status CreateDefaultSchedQueue_(uint32_t intf_id, const std::string direction) {
    bcmos_errno err;
    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1};
    tm_sched_key.id = get_default_tm_sched_id(intf_id, direction);

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
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, num_priorities, 4);

    // bcmbal_tm_shaping
    uint32_t cir = 1000000;
    uint32_t pir = 1000000;
    uint32_t burst = 65536;
    BCM_LOG(INFO, openolt_log_id, "applying traffic shaping in %s pir=%u, burst=%u\n",
       direction.c_str(), pir, burst);
    BCMOLT_FIELD_SET_PRESENT(&tm_sched_cfg.data.rate, tm_shaping, pir);
    BCMOLT_FIELD_SET_PRESENT(&tm_sched_cfg.data.rate, tm_shaping, burst);
    // FIXME: Setting CIR, results in BAL throwing error 'tm_sched minimum rate is not supported yet'
    // BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.cir, cir);
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.pir, pir);
    BCMOLT_MSG_FIELD_SET(&tm_sched_cfg, rate.burst, burst);

    err = bcmolt_cfg_set(dev_id, &tm_sched_cfg.hdr);
    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to create %s scheduler, id %d, intf_id %d\n", direction.c_str(), tm_sched_key.id, intf_id);
        return Status(grpc::StatusCode::INTERNAL, "Failed to create %s scheduler", direction.c_str());
    }
    BCM_LOG(INFO, openolt_log_id, "Create %s scheduler success, id %d, intf_id %d\n", direction.c_str(), tm_sched_key.id, intf_id);

    // Create 4 Queues for each default PON scheduler
    for (int queue_id = 0; queue_id < 4; queue_id++) {
        bcmolt_tm_queue_cfg tm_queue_cfg;
        bcmolt_tm_queue_key tm_queue_key = {};
        tm_queue_key.sched_id = get_default_tm_sched_id(intf_id, direction);
        tm_queue_key.id = queue_id;

        BCMOLT_CFG_INIT(&tm_queue_cfg, tm_queue, tm_queue_key);
        BCMOLT_MSG_FIELD_SET(&tm_queue_cfg, tm_sched_param.type, BCMOLT_TM_SCHED_PARAM_TYPE_PRIORITY);
        BCMOLT_MSG_FIELD_SET(&tm_queue_cfg, tm_sched_param.u.priority.priority, queue_id);

        err = bcmolt_cfg_set(dev_id, &tm_queue_cfg.hdr);
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create %s tm queue, id %d, sched_id %d\n", \
                    direction.c_str(), tm_queue_key.id, tm_queue_key.sched_id);
            return Status(grpc::StatusCode::INTERNAL, "Failed to create %s tm queue", direction.c_str());
        }

        BCM_LOG(INFO, openolt_log_id, "Create %s tm_queue success, id %d, sched_id %d\n", \
                direction.c_str(), tm_queue_key.id, tm_queue_key.sched_id);
    }
    return Status::OK;
}

bcmos_errno CreateSched(std::string direction, uint32_t intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t port_no,
                 uint32_t alloc_id, tech_profile::AdditionalBW additional_bw, uint32_t weight, uint32_t priority,
                 tech_profile::SchedulingPolicy sched_policy, tech_profile::TrafficShapingInfo tf_sh_info) {

    bcmos_errno err;

    if (direction == downstream) {
        bcmolt_tm_sched_cfg tm_sched_cfg;
        bcmolt_tm_sched_key tm_sched_key = {.id = 1};
        tm_sched_key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction);

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
            BCM_LOG(INFO, openolt_log_id, "applying traffic shaping in DL cir=%u, pir=%u, burst=%u\n",
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
            BCM_LOG(ERROR, openolt_log_id, "Failed to create downstream subscriber scheduler, id %d, \
                    intf_id %d, onu_id %d, uni_id %d, port_no %u\n", tm_sched_key.id, intf_id, onu_id, \
                    uni_id, port_no);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Create downstream subscriber sched, id %d, intf_id %d, onu_id %d, \
                uni_id %d, port_no %u\n", tm_sched_key.id, intf_id, onu_id, uni_id, port_no);

    } else { //upstream
        bcmolt_itupon_alloc_cfg cfg;
        bcmolt_itupon_alloc_key key = { };
        key.pon_ni = intf_id;
        key.alloc_id = alloc_id;

        BCMOLT_CFG_INIT(&cfg, itupon_alloc, key); 
        BCMOLT_MSG_FIELD_SET(&cfg, onu_id, onu_id);
        if (additional_bw == 2) //AdditionalBW_BestEffort
            BCMOLT_MSG_FIELD_SET(&cfg, sla.additional_bw_eligibility, 
                BCMOLT_ADDITIONAL_BW_ELIGIBILITY_BEST_EFFORT);
        else if (additional_bw == 3) //AdditionalBW_Auto
            BCMOLT_MSG_FIELD_SET(&cfg, sla.additional_bw_eligibility, 
                BCMOLT_ADDITIONAL_BW_ELIGIBILITY_NON_ASSURED);
        /* CBR Real Time Bandwidth which require shaping of the bandwidth allocations 
           in a fine granularity. */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_rt_bw, 0);
        /* Fixed Bandwidth with no critical requirement of shaping */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.cbr_nrt_bw, 0);
        /* Dynamic bandwidth which the OLT is committed to allocate upon demand */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.guaranteed_bw, 0);
        /* Maximum allocated bandwidth allowed for this alloc ID */
        BCMOLT_MSG_FIELD_SET(&cfg, sla.maximum_bw, tf_sh_info.pir());
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

        err = bcmolt_cfg_set(dev_id, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to create upstream DBA sched, intf_id %d, onu_id %d, uni_id %d,\
                    port_no %u, alloc_id %d\n", intf_id, onu_id,uni_id,port_no,alloc_id);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Create upstream DBA sched, intf_id %d, onu_id %d, uni_id %d, port_no %u, \
                alloc_id %d\n", intf_id,onu_id,uni_id,port_no,alloc_id);
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
    
    if (direction == upstream) {
        bcmolt_itupon_alloc_cfg cfg;
        bcmolt_itupon_alloc_key key = { };
        key.pon_ni = intf_id;

        BCMOLT_CFG_INIT(&cfg, itupon_alloc, key); 
        BCMOLT_MSG_FIELD_SET(&cfg, onu_id, onu_id);
        BCMOLT_CFG_INIT(&cfg, itupon_alloc, key); 
        err = bcmolt_cfg_clear(dev_id, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to remove scheduler sched, direction = %s, intf_id %d, onu_id %d\n", \
                    direction.c_str(), intf_id, onu_id);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Removed sched, direction = %s, intf_id %d, onu_id %d\n", \
                direction.c_str(), intf_id, onu_id);
    } else if (direction == downstream) {
        bcmolt_tm_sched_cfg cfg;
        bcmolt_tm_sched_key key = { };

        if (is_tm_sched_id_present(intf_id, onu_id, uni_id, direction)) {
            key.id = get_tm_sched_id(intf_id, onu_id, uni_id, direction);
        } else {
            BCM_LOG(INFO, openolt_log_id, "schduler not present in %s\n", direction.c_str());
            return BCM_ERR_OK;
        }
        BCMOLT_CFG_INIT(&cfg, tm_sched, key);
        err = bcmolt_cfg_clear(dev_id, &(cfg.hdr));
        if (err) {
            BCM_LOG(ERROR, openolt_log_id, "Failed to remove scheduler sched, direction = %s, id %d, intf_id %d, onu_id %d\n", \
                    direction.c_str(), key.id, intf_id, onu_id);
            return err;
        }
        BCM_LOG(INFO, openolt_log_id, "Removed sched, direction = %s, id %d, intf_id %d, onu_id %d\n", \
                direction.c_str(), key.id, intf_id, onu_id);
    }

    free_tm_sched_id(intf_id, onu_id, uni_id, direction);
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
    bcmolt_tm_queue_cfg cfg;
    bcmolt_tm_queue_key key = { };
    BCM_LOG(INFO, openolt_log_id, "creating queue. access_intf_id = %d, onu_id = %d, uni_id = %d \
            gemport_id = %d, direction = %s\n", access_intf_id, onu_id, uni_id, gemport_id, direction.c_str());
    if (direction == downstream) {
        // There is one queue per gem port
        key.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, direction);
        key.id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);

    } else {
        queue_map_key_tuple map_key(access_intf_id, onu_id, uni_id, gemport_id, direction);
        if (queue_map.count(map_key) > 0) {
            BCM_LOG(INFO, openolt_log_id, "upstream queue exists for intf_id %d, onu_id %d, uni_id %d\n. Not re-creating", \
                    access_intf_id, onu_id, uni_id); 
            return BCM_ERR_OK;
        }
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

    BCMOLT_CFG_INIT(&cfg, tm_queue, key);
    BCMOLT_MSG_FIELD_SET(&cfg, tm_sched_param.u.priority.priority, priority);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
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
    bcmolt_tm_queue_cfg cfg;
    bcmolt_tm_queue_key key = { };
    bcmos_errno err;

    if (direction == downstream) {
        if (is_tm_queue_id_present(access_intf_id, onu_id, uni_id, gemport_id, direction) && \
            is_tm_sched_id_present(access_intf_id, onu_id, uni_id, direction)) {
            key.sched_id = get_tm_sched_id(access_intf_id, onu_id, uni_id, direction);
            key.id = get_tm_queue_id(access_intf_id, onu_id, uni_id, gemport_id, direction);
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

    BCMOLT_CFG_INIT(&cfg, tm_queue, key);

    err = bcmolt_cfg_clear(dev_id, &(cfg.hdr));

    if (err) {
        BCM_LOG(ERROR, openolt_log_id, "Failed to remove queue, direction = %s, id %d, sched_id %d, intf_id %d, onu_id %d, uni_id %d\n",
                direction.c_str(), key.id, key.sched_id, access_intf_id, onu_id, uni_id);
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

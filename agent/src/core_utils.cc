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

#include <fstream>
#include <sstream>
#include "core_utils.h"

// save the TLS option
static std::string tls_option_arg{};

std::string serial_number_to_str(bcmolt_serial_number* serial_number) {
#define SERIAL_NUMBER_SIZE 12
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%c%c%c%c%1X%1X%1X%1X%1X%1X%1X%1X",
            serial_number->vendor_id.arr[0],
            serial_number->vendor_id.arr[1],
            serial_number->vendor_id.arr[2],
            serial_number->vendor_id.arr[3],
            serial_number->vendor_specific.arr[0]>>4 & 0x0f,
            serial_number->vendor_specific.arr[0] & 0x0f,
            serial_number->vendor_specific.arr[1]>>4 & 0x0f,
            serial_number->vendor_specific.arr[1] & 0x0f,
            serial_number->vendor_specific.arr[2]>>4 & 0x0f,
            serial_number->vendor_specific.arr[2] & 0x0f,
            serial_number->vendor_specific.arr[3]>>4 & 0x0f,
            serial_number->vendor_specific.arr[3] & 0x0f);

    return buff;
}

std::string vendor_specific_to_str(char const * const vendor_specific) {
    char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%1X%1X%1X%1X%1X%1X%1X%1X",
            vendor_specific[0]>>4 & 0x0f,
            vendor_specific[0] & 0x0f,
            vendor_specific[1]>>4 & 0x0f,
            vendor_specific[1] & 0x0f,
            vendor_specific[2]>>4 & 0x0f,
            vendor_specific[2] & 0x0f,
            vendor_specific[3]>>4 & 0x0f,
            vendor_specific[3] & 0x0f);

    return buff;
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

uint16_t get_dev_id(void) {
    return dev_id;
}

int get_default_tm_sched_id(int intf_id, std::string direction) {
    if (direction == upstream) {
        return tm_upstream_sched_id_start + intf_id;
    } else if (direction == downstream) {
        return tm_downstream_sched_id_start + intf_id;
    }
    else {
        OPENOLT_LOG(ERROR, openolt_log_id, "invalid direction - %s\n", direction.c_str());
        return 0;
    }
}

/**
* Gets a unique tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction
* The tm_sched_id is locally cached in a map, so that it can rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem.
* Note that tech_profile_id is used to differentiate service schedulers in downstream direction.
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
* @param tech_profile_id Technology Profile ID
*
* @return tm_sched_id
*/
uint32_t get_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    int sched_id = -1;

    bcmos_fastlock_lock(&tm_sched_bitset_lock);

    std::map<sched_map_key_tuple, int>::const_iterator it = sched_map.find(key);
    if (it != sched_map.end()) {
        sched_id = it->second;
    }
    if (sched_id != -1) {
        bcmos_fastlock_unlock(&tm_sched_bitset_lock, 0);
        return sched_id;
    }

    // Complexity of O(n). Is there better way that can avoid linear search?
    for (sched_id = 0; sched_id < MAX_TM_SCHED_ID; sched_id++) {
        if (tm_sched_bitset[sched_id] == 0) {
            tm_sched_bitset[sched_id] = 1;
            break;
        }
    }

    if (sched_id < MAX_TM_SCHED_ID) {
        sched_map[key] = sched_id;
        bcmos_fastlock_unlock(&tm_sched_bitset_lock, 0);
        return sched_id;
    } else {
        bcmos_fastlock_unlock(&tm_sched_bitset_lock, 0);
        return -1;
    }
}

/**
* Free tm_sched_id for a given intf_id, onu_id, uni_id, gemport_id, direction, tech_profile_id
*
* @param intf_id NNI or PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param gemport_id GEM Port ID
* @param direction Upstream or downstream
* @param tech_profile_id Technology Profile ID
*/
void free_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    std::map<sched_map_key_tuple, int>::const_iterator it;
    bcmos_fastlock_lock(&tm_sched_bitset_lock);
    it = sched_map.find(key);
    if (it != sched_map.end()) {
        tm_sched_bitset[it->second] = 0;
        sched_map.erase(it);
    }
    bcmos_fastlock_unlock(&tm_sched_bitset_lock, 0);
}

bool is_tm_sched_id_present(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id) {
    sched_map_key_tuple key(pon_intf_id, onu_id, uni_id, direction, tech_profile_id);
    std::map<sched_map_key_tuple, int>::const_iterator it = sched_map.find(key);
    if (it != sched_map.end()) {
        return true;
    }
    return false;
}

/**
* Check whether given two tm qmp profiles are equal or not
*
* @param tmq_map_profileA <vector> TM QUEUE MAPPING PROFILE
* @param tmq_map_profileB <vector> TM QUEUE MAPPING PROFILE
*
* @return boolean, true if given tmq_map_profiles are equal else false
*/

bool check_tm_qmp_equality(std::vector<uint32_t> tmq_map_profileA, std::vector<uint32_t> tmq_map_profileB) {
    for (uint32_t i = 0; i < TMQ_MAP_PROFILE_SIZE; i++) {
        if (tmq_map_profileA[i] != tmq_map_profileB[i]) {
            return false;
        }
    }
    return true;
}

/**
* Modifies given queues_pbit_map to parsable format
* e.g: Modifes "0b00000101" to "10100000"
*
* @param queues_pbit_map PBIT MAP configured for each GEM in TECH PROFILE
* @param size Queue count
*
* @return string queues_pbit_map
*/
std::string* get_valid_queues_pbit_map(std::string *queues_pbit_map, uint32_t size) {
    for(uint32_t i=0; i < size; i++) {
        /* Deletes 2 characters from index number 0 */
        queues_pbit_map[i].erase(0, 2);
        std::reverse(queues_pbit_map[i].begin(), queues_pbit_map[i].end());
    }
    return queues_pbit_map;
}

/**
* Creates TM QUEUE MAPPING PROFILE for given queues_pbit_map and queues_priority_q
*
* @param queues_pbit_map PBIT MAP configured for each GEM in TECH PROFILE
* @param queues_priority_q PRIORITY_Q configured for each GEM in TECH PROFILE
* @param size Queue count
*
* @return <vector> TM QUEUE MAPPING PROFILE
*/
std::vector<uint32_t> get_tmq_map_profile(std::string *queues_pbit_map, uint32_t *queues_priority_q, uint32_t size) {
    std::vector<uint32_t> tmq_map_profile(8,0);

    for(uint32_t i=0; i < size; i++) {
        for (uint32_t j = 0; j < queues_pbit_map[i].size(); j++) {
            if (queues_pbit_map[i][j]=='1') {
                tmq_map_profile.at(j) = queue_id_list[queues_priority_q[i]];
            }
        }
    }
    return tmq_map_profile;
}

/**
* Gets corresponding tm_qmp_id for a given tmq_map_profile
*
* @param <vector> TM QUEUE MAPPING PROFILE
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(std::vector<uint32_t> tmq_map_profile) {
    int tm_qmp_id = -1;

    std::map<int, std::vector < uint32_t > >::const_iterator it = qmp_id_to_qmp_map.begin();
    while(it != qmp_id_to_qmp_map.end()) {
        if(check_tm_qmp_equality(tmq_map_profile, it->second)) {
            tm_qmp_id = it->first;
            break;
        }
        it++;
    }
    return tm_qmp_id;
}

/**
* Updates sched_qmp_id_map with given sched_id, pon_intf_id, onu_id, uni_id, tm_qmp_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param tm_qmp_id TM QUEUE MAPPING PROFILE ID
*/
void update_sched_qmp_id_map(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, \
                             uint32_t uni_id, int tm_qmp_id) {
   bcmos_fastlock_lock(&tm_qmp_bitset_lock);
   sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
   sched_qmp_id_map.insert(make_pair(key, tm_qmp_id));
   bcmos_fastlock_unlock(&tm_qmp_bitset_lock, 0);
}

/**
* Gets corresponding tm_qmp_id for a given sched_id, pon_intf_id, onu_id, uni_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id) {
    sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
    int tm_qmp_id = -1;

    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it = sched_qmp_id_map.find(key);
    if (it != sched_qmp_id_map.end()) {
        tm_qmp_id = it->second;
    }
    return tm_qmp_id;
}

/**
* Gets a unique tm_qmp_id for a given tmq_map_profile
* The tm_qmp_id is locally cached in a map, so that it can be rendered when necessary.
* VOLTHA replays whole configuration on OLT reboot, so caching locally is not a problem
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param <vector> TM QUEUE MAPPING PROFILE
*
* @return tm_qmp_id
*/
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id, \
                  std::vector<uint32_t> tmq_map_profile) {
    int tm_qmp_id;

    bcmos_fastlock_lock(&tm_qmp_bitset_lock);
    /* Complexity of O(n). Is there better way that can avoid linear search? */
    for (tm_qmp_id = 0; tm_qmp_id < MAX_TM_QMP_ID; tm_qmp_id++) {
        if (tm_qmp_bitset[tm_qmp_id] == 0) {
            tm_qmp_bitset[tm_qmp_id] = 1;
            break;
        }
    }

    if (tm_qmp_id < MAX_TM_QMP_ID) {
        qmp_id_to_qmp_map.insert(make_pair(tm_qmp_id, tmq_map_profile));
        bcmos_fastlock_unlock(&tm_qmp_bitset_lock, 0);
        update_sched_qmp_id_map(sched_id, pon_intf_id, onu_id, uni_id, tm_qmp_id);
        return tm_qmp_id;
    } else {
        bcmos_fastlock_unlock(&tm_qmp_bitset_lock, 0);
        return -1;
    }
}

/**
* Free tm_qmp_id for a given sched_id, pon_intf_id, onu_id, uni_id
*
* @param upstream/downstream sched_id
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param tm_qmp_id TM QUEUE MAPPING PROFILE ID
*
* @return boolean, true if no more reference for TM QMP else false
*/
bool free_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, \
                    uint32_t uni_id, int tm_qmp_id) {
    bool result;
    sched_qmp_id_map_key_tuple key(sched_id, pon_intf_id, onu_id, uni_id);
    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it = sched_qmp_id_map.find(key);
    bcmos_fastlock_lock(&tm_qmp_bitset_lock);
    if (it != sched_qmp_id_map.end()) {
        sched_qmp_id_map.erase(it);
    }

    uint32_t tm_qmp_ref_count = 0;
    std::map<sched_qmp_id_map_key_tuple, int>::const_iterator it2 = sched_qmp_id_map.begin();
    while(it2 != sched_qmp_id_map.end()) {
        if(it2->second == tm_qmp_id) {
            tm_qmp_ref_count++;
        }
        it2++;
    }

    if (tm_qmp_ref_count == 0) {
        std::map<int, std::vector < uint32_t > >::const_iterator it3 = qmp_id_to_qmp_map.find(tm_qmp_id);
        if (it3 != qmp_id_to_qmp_map.end()) {
            tm_qmp_bitset[tm_qmp_id] = 0;
            qmp_id_to_qmp_map.erase(it3);
            OPENOLT_LOG(INFO, openolt_log_id, "Reference count for tm qmp profile id %d is : %d. So clearing it\n", \
                        tm_qmp_id, tm_qmp_ref_count);
            result = true;
        }
    } else {
        OPENOLT_LOG(INFO, openolt_log_id, "Reference count for tm qmp profile id %d is : %d. So not clearing it\n", \
                    tm_qmp_id, tm_qmp_ref_count);
        result = false;
    }
    bcmos_fastlock_unlock(&tm_qmp_bitset_lock, 0);

    return result;
}

/* ACL ID is a shared resource, caller of this function has to ensure atomicity using locks
   Gets free ACL ID if available, else -1. */
int get_acl_id() {
    int acl_id;

    bcmos_fastlock_lock(&acl_id_bitset_lock);
    /* Complexity of O(n). Is there better way that can avoid linear search? */
    for (acl_id = 0; acl_id < MAX_ACL_ID; acl_id++) {
        if (acl_id_bitset[acl_id] == 0) {
            acl_id_bitset[acl_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&acl_id_bitset_lock, 0);

    if (acl_id < MAX_ACL_ID) {
        return acl_id ;
    } else {
        return -1;
    }
}

/* ACL ID is a shared resource, caller of this function has to ensure atomicity using locks
   Frees up the ACL ID. */
void free_acl_id (int acl_id) {
    bcmos_fastlock_lock(&acl_id_bitset_lock);
    if (acl_id < MAX_ACL_ID) {
        acl_id_bitset[acl_id] = 0;
    }
    bcmos_fastlock_unlock(&acl_id_bitset_lock, 0);
}

/*  Gets a free Flow ID if available, else INVALID_FLOW_ID */
uint16_t get_flow_id() {
    uint16_t flow_id;

    bcmos_fastlock_lock(&flow_id_bitset_lock);
    /* Complexity of O(n). Is there better way that can avoid linear search? */
    // start flow_id from 1 as 0 is invalid
    for (flow_id = FLOW_ID_START; flow_id <= FLOW_ID_END; flow_id++) {
        if (flow_id_bitset[flow_id] == 0) {
            flow_id_bitset[flow_id] = 1;
            break;
        }
    }
    bcmos_fastlock_unlock(&flow_id_bitset_lock, 0);

    if (flow_id <= MAX_FLOW_ID) {
        return flow_id ;
    } else {
        return INVALID_FLOW_ID;
    }
}

/*  Gets requested number of Flow IDs.
    'num_of_flow_ids' is number of flow_ids requested. This cannot be more than NUMBER_OF_PBITS
    'flow_ids' is pointer to array of size NUMBER_OF_PBITS
    If the operation is successful, returns true else false
    The operation is successful if we can allocate fully the number of flow_ids requested.
 */
bool get_flow_ids(int num_of_flow_ids, uint16_t *flow_ids) {
    if (num_of_flow_ids > NUMBER_OF_PBITS) {
        OPENOLT_LOG(ERROR, openolt_log_id, "requested number of flow_ids is more than 8\n");
        return false;
    }
    int cnt = 0;

    bcmos_fastlock_lock(&flow_id_bitset_lock);
    /* Complexity of O(n). Is there better way that can avoid linear search? */
    // start flow_id from 1 as 0 is invalid
    for (uint16_t flow_id = FLOW_ID_START; flow_id <= FLOW_ID_END && cnt < num_of_flow_ids; flow_id++) {
        if (flow_id_bitset[flow_id] == 0) {
            flow_id_bitset[flow_id] = 1;
            flow_ids[cnt] = flow_id;
            cnt++;
        }
    }
    bcmos_fastlock_unlock(&flow_id_bitset_lock, 0);
    // If we could not allocate the requested number of flow_ids free the allocated flow_ids
    // and return false
    if (cnt != num_of_flow_ids) {
        OPENOLT_LOG(ERROR, openolt_log_id, "could not allocated the rquested number of flows ids. requested=%d, allocated=%d", num_of_flow_ids, cnt);
        if (cnt > 0) {
            for(int i=0; i < cnt; i++) {
                free_flow_id(flow_ids[i]);
            }
        }
        return false;
    }
    return true;
}

/*  Frees up the FLOW ID. */
void free_flow_id (uint16_t flow_id) {
    bcmos_fastlock_lock(&flow_id_bitset_lock);
    if (flow_id <= MAX_FLOW_ID) {
        flow_id_bitset[flow_id] = 0;
    }
    bcmos_fastlock_unlock(&flow_id_bitset_lock, 0);
}

void free_flow_ids(uint8_t num_flows, uint16_t *flow_ids) {
    for (uint8_t i = 0; i < num_flows; i++) {
        bcmos_fastlock_lock(&flow_id_bitset_lock);
        if (flow_ids[i] <= MAX_FLOW_ID) {
            flow_id_bitset[flow_ids[i]] = 0;
        }
        bcmos_fastlock_unlock(&flow_id_bitset_lock, 0);
    }
}

/**
* Returns qos type as string
*
* @param qos_type bcmolt_egress_qos_type enum
*/
std::string get_qos_type_as_string(bcmolt_egress_qos_type qos_type) {
    switch (qos_type)
    {
        case BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE: return "FIXED_QUEUE";
        case BCMOLT_EGRESS_QOS_TYPE_TC_TO_QUEUE: return "TC_TO_QUEUE";
        case BCMOLT_EGRESS_QOS_TYPE_PBIT_TO_TC: return "PBIT_TO_TC";
        case BCMOLT_EGRESS_QOS_TYPE_NONE: return "NONE";
        case BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE: return "PRIORITY_TO_QUEUE";
        default: OPENOLT_LOG(ERROR, openolt_log_id, "qos-type-not-supported %d\n", qos_type);
                 return "qos-type-not-supported";
    }
}

/**
* Gets/Updates qos type for given pon_intf_id, onu_id, uni_id
*
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
* @param queue_size TrafficQueues Size
*
* @return qos_type
*/
bcmolt_egress_qos_type get_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t queue_size) {
    qos_type_map_key_tuple key(pon_intf_id, onu_id, uni_id);
    bcmolt_egress_qos_type egress_qos_type = BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
    std::string qos_string;

    std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type>::const_iterator it = qos_type_map.find(key);
    if (it != qos_type_map.end()) {
        egress_qos_type = it->second;
        qos_string = get_qos_type_as_string(egress_qos_type);
        OPENOLT_LOG(INFO, openolt_log_id, "Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d is %s\n", \
                    pon_intf_id, onu_id, uni_id, qos_string.c_str());
    }
    else {
        /* QOS Type has been pre-defined as Fixed Queue but it will be updated based on number of GEMPORTS
           associated for a given subscriber. If GEM count = 1 for a given subscriber, qos_type will be Fixed Queue
           else Priority to Queue */
        egress_qos_type = (queue_size > 1) ? \
            BCMOLT_EGRESS_QOS_TYPE_PRIORITY_TO_QUEUE : BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
        bcmos_fastlock_lock(&data_lock);
        qos_type_map.insert(make_pair(key, egress_qos_type));
        bcmos_fastlock_unlock(&data_lock, 0);
        qos_string = get_qos_type_as_string(egress_qos_type);
        OPENOLT_LOG(INFO, openolt_log_id, "Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d is %s\n", \
                    pon_intf_id, onu_id, uni_id, qos_string.c_str());
    }
    return egress_qos_type;
}

/**
* Clears qos type for given pon_intf_id, onu_id, uni_id
*
* @param PON intf ID
* @param onu_id ONU ID
* @param uni_id UNI ID
*/
void clear_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id) {
    qos_type_map_key_tuple key(pon_intf_id, onu_id, uni_id);
    std::map<qos_type_map_key_tuple, bcmolt_egress_qos_type>::const_iterator it = qos_type_map.find(key);
    bcmos_fastlock_lock(&data_lock);
    if (it != qos_type_map.end()) {
        qos_type_map.erase(it);
        OPENOLT_LOG(INFO, openolt_log_id, "Cleared Qos-type for subscriber connected to pon_intf_id %d, onu_id %d and uni_id %d\n", \
                    pon_intf_id, onu_id, uni_id);
    }
    bcmos_fastlock_unlock(&data_lock, 0);
}

/**
* Returns Scheduler/Queue direction as string
*
* @param direction as specified in tech_profile.proto
*/
std::string GetDirection(int direction) {
    switch (direction)
    {
        case tech_profile::Direction::UPSTREAM: return upstream;
        case tech_profile::Direction::DOWNSTREAM: return downstream;
        default: OPENOLT_LOG(ERROR, openolt_log_id, "direction-not-supported %d\n", direction);
                 return "direction-not-supported";
    }
}

// This method handles waiting for AllocObject configuration.
// Returns error if the AllocObject is not in the appropriate state based on action requested.
bcmos_errno wait_for_alloc_action(uint32_t intf_id, uint32_t alloc_id, AllocCfgAction action) {
    Queue<alloc_cfg_complete_result> cfg_result;
    alloc_cfg_compltd_key k(intf_id, alloc_id);
    bcmos_fastlock_lock(&alloc_cfg_wait_lock);
    alloc_cfg_compltd_map[k] =  &cfg_result;
    bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
    bcmos_errno err = BCM_ERR_OK;

    // Try to pop the result from BAL with a timeout of ALLOC_CFG_COMPLETE_WAIT_TIMEOUT ms
    std::pair<alloc_cfg_complete_result, bool> result = cfg_result.pop(ALLOC_CFG_COMPLETE_WAIT_TIMEOUT);
    if (result.second == false) {
        OPENOLT_LOG(ERROR, openolt_log_id, "timeout waiting for alloc cfg complete indication intf_id %d, alloc_id %d, action = %d\n",
                    intf_id, alloc_id, action);
        // Invalidate the queue pointer.
        bcmos_fastlock_lock(&alloc_cfg_wait_lock);
        alloc_cfg_compltd_map[k] = NULL;
        bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
        err = BCM_ERR_INTERNAL;
        // If the Alloc object is already in the right state after the performed operation, return OK.
        bcmolt_activation_state state;
        err = get_alloc_obj_state(intf_id, alloc_id, &state);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "error fetching alloc obj state intf_id = %d, alloc_id %d, action = %d, err = %d\n",
                        intf_id, alloc_id, action, err);
            return err;
        }
        if ((state == BCMOLT_ACTIVATION_STATE_NOT_CONFIGURED && action == ALLOC_OBJECT_DELETE) ||
            (state == BCMOLT_ACTIVATION_STATE_ACTIVE && action == ALLOC_OBJECT_CREATE)) {
            OPENOLT_LOG(WARNING, openolt_log_id, "operation timed out, but the alloc object is the right state intf_id = %d, gem_port_id %d, action = %d\n",
                        intf_id, alloc_id, action);
            return BCM_ERR_OK;
        }
    }
    else if (result.first.status == ALLOC_CFG_STATUS_FAIL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error processing alloc cfg request intf_id %d, alloc_id %d, action = %d\n",
                    intf_id, alloc_id, action);
        err = BCM_ERR_INTERNAL;
    }

    if (err == BCM_ERR_OK) {
        if (action == ALLOC_OBJECT_CREATE) {
            if (result.first.state != ALLOC_OBJECT_STATE_ACTIVE) {
                OPENOLT_LOG(ERROR, openolt_log_id, "alloc object not in active state intf_id %d, alloc_id %d alloc_obj_state %d\n",
                            intf_id, alloc_id, result.first.state);
               err = BCM_ERR_INTERNAL;
            } else {
                OPENOLT_LOG(INFO, openolt_log_id, "Create upstream bandwidth allocation success, intf_id %d, alloc_id %d\n",
                            intf_id, alloc_id);
            }
        } else { // ALLOC_OBJECT_DELETE
              if (result.first.state != ALLOC_OBJECT_STATE_NOT_CONFIGURED) {
                  OPENOLT_LOG(ERROR, openolt_log_id, "alloc object is not reset intf_id %d, alloc_id %d alloc_obj_state %d\n",
                              intf_id, alloc_id, result.first.state);
                  err = BCM_ERR_INTERNAL;
              } else {
                  OPENOLT_LOG(INFO, openolt_log_id, "Remove alloc object success, intf_id %d, alloc_id %d\n",
                              intf_id, alloc_id);
              }
        }
    }

    // Remove entry from map
    bcmos_fastlock_lock(&alloc_cfg_wait_lock);
    alloc_cfg_compltd_map.erase(k);
    bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
    return err;
}

// This method handles waiting for GemObject configuration.
// Returns error if the GemObject is not in the appropriate state based on action requested.
bcmos_errno wait_for_gem_action(uint32_t intf_id, uint32_t gem_port_id, GemCfgAction action) {
    Queue<gem_cfg_complete_result> cfg_result;
    gem_cfg_compltd_key k(intf_id, gem_port_id);
    bcmos_fastlock_lock(&gem_cfg_wait_lock);
    gem_cfg_compltd_map[k] =  &cfg_result;
    bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
    bcmos_errno err = BCM_ERR_OK;

    // Try to pop the result from BAL with a timeout of GEM_CFG_COMPLETE_WAIT_TIMEOUT ms
    std::pair<gem_cfg_complete_result, bool> result = cfg_result.pop(GEM_CFG_COMPLETE_WAIT_TIMEOUT);
    if (result.second == false) {
        OPENOLT_LOG(ERROR, openolt_log_id, "timeout waiting for gem cfg complete indication intf_id %d, gem_port_id %d, action = %d\n",
                    intf_id, gem_port_id, action);
        // Invalidate the queue pointer.
        bcmos_fastlock_lock(&gem_cfg_wait_lock);
        gem_cfg_compltd_map[k] = NULL;
        bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
        err = BCM_ERR_INTERNAL;
        // If the GEM object is already in the right state after the performed operation, return OK.
        bcmolt_activation_state state;
        err = get_gem_obj_state(intf_id, gem_port_id, &state);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "error fetching gem obj state intf_id = %d, gem_port_id %d, action = %d, err = %d\n",
                        intf_id, gem_port_id, action, err);
            return err;
        }
        if ((state == BCMOLT_ACTIVATION_STATE_NOT_CONFIGURED && action == GEM_OBJECT_DELETE) ||
            (state == BCMOLT_ACTIVATION_STATE_ACTIVE && action == GEM_OBJECT_CREATE)) {
            OPENOLT_LOG(WARNING, openolt_log_id, "operation timed out, but the gem object is the right state intf_id = %d, gem_port_id %d, action = %d\n",
                        intf_id, gem_port_id, action);
            return BCM_ERR_OK;
        }
    }
    else if (result.first.status == GEM_CFG_STATUS_FAIL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error processing gem cfg request intf_id %d, gem_port_id %d, action = %d\n",
                    intf_id, gem_port_id, action);
        err = BCM_ERR_INTERNAL;
    }

    if (err == BCM_ERR_OK) {
        if (action == GEM_OBJECT_CREATE) {
            if (result.first.state != GEM_OBJECT_STATE_ACTIVE) {
                OPENOLT_LOG(ERROR, openolt_log_id, "gem object not in active state intf_id %d, gem_port_id %d gem_obj_state %d\n",
                            intf_id, gem_port_id, result.first.state);
               err = BCM_ERR_INTERNAL;
            } else {
                OPENOLT_LOG(INFO, openolt_log_id, "Create itupon gem object success, intf_id %d, gem_port_id %d\n",
                            intf_id, gem_port_id);
            }
        } else if (action == GEM_OBJECT_ENCRYPT) {
            if (result.first.state != GEM_OBJECT_STATE_ACTIVE) {
                OPENOLT_LOG(ERROR, openolt_log_id, "gem object not in active state intf_id %d, gem_port_id %d gem_obj_state %d\n",
                            intf_id, gem_port_id, result.first.state);
               err = BCM_ERR_INTERNAL;
            } else {
                OPENOLT_LOG(INFO, openolt_log_id, "Enable itupon gem object encryption success, intf_id %d, gem_port_id %d\n",
                            intf_id, gem_port_id);
            }
        } else { // GEM_OBJECT_DELETE
              if (result.first.state != GEM_OBJECT_STATE_NOT_CONFIGURED) {
                  OPENOLT_LOG(ERROR, openolt_log_id, "gem object is not reset intf_id %d, gem_port_id %d gem_obj_state %d\n",
                              intf_id, gem_port_id, result.first.state);
                  err = BCM_ERR_INTERNAL;
              } else {
                  OPENOLT_LOG(INFO, openolt_log_id, "Remove itupon gem object success, intf_id %d, gem_port_id %d\n",
                              intf_id, gem_port_id);
              }
        }
    }

    // Remove entry from map
    bcmos_fastlock_lock(&gem_cfg_wait_lock);
    gem_cfg_compltd_map.erase(k);
    bcmos_fastlock_unlock(&gem_cfg_wait_lock, 0);
    return err;
}

// This method handles waiting for OnuDeactivate Completed Indication
bcmos_errno wait_for_onu_deactivate_complete(uint32_t intf_id, uint32_t onu_id) {
    Queue<onu_deactivate_complete_result> deact_result;
    onu_deact_compltd_key k(intf_id, onu_id);
    onu_deact_compltd_map[k] =  &deact_result;
    bcmos_errno err = BCM_ERR_OK;

    // Try to pop the result from BAL with a timeout of ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT ms
    std::pair<onu_deactivate_complete_result, bool> result = deact_result.pop(ONU_DEACTIVATE_COMPLETE_WAIT_TIMEOUT);
    if (result.second == false) {
        OPENOLT_LOG(ERROR, openolt_log_id, "timeout waiting for onu deactivate complete indication intf_id %d, onu_id %d\n",
                    intf_id, onu_id);
        // Invalidate the queue pointer.
        bcmos_fastlock_lock(&onu_deactivate_wait_lock);
        onu_deact_compltd_map[k] = NULL;
        bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);
        err = BCM_ERR_INTERNAL;
    }
    else if (result.first.result == BCMOLT_RESULT_FAIL) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error processing onu deactivate request intf_id %d, onu_id %d, fail_reason %d\n",
                    intf_id, onu_id, result.first.reason);
        err = BCM_ERR_INTERNAL;
    } else if (result.first.result == BCMOLT_RESULT_SUCCESS) {
        OPENOLT_LOG(INFO, openolt_log_id, "success processing onu deactivate request intf_id %d, onu_id %d\n",
                    intf_id, onu_id);
    }

    // Remove entry from map
    bcmos_fastlock_lock(&onu_deactivate_wait_lock);
    onu_deact_compltd_map.erase(k);
    bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);

    return err;
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
       OPENOLT_LOG(INFO, openolt_log_id,  "Failed to query the %s\n", field_name);
       return field_val;
   }

   /*Read the field value*/
   if (fp) {
       uint8_t ret;
       ret = fread(field_val, OPENOLT_FIELD_LEN, 1, fp);
       if (ret >= OPENOLT_FIELD_LEN)
           OPENOLT_LOG(INFO, openolt_log_id,  "Read data length %u\n", ret);
       pclose(fp);
   }
   return field_val;
}

Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state, uint32_t speed)
{
    ::openolt::Indication ind;
    ::openolt::IntfOperIndication* intf_oper_ind = new ::openolt::IntfOperIndication;

    intf_oper_ind->set_type(type);
    intf_oper_ind->set_intf_id(intf_id);
    intf_oper_ind->set_oper_state(state);
    intf_oper_ind->set_speed(speed);
    ind.set_allocated_intf_oper_ind(intf_oper_ind);
    oltIndQ.push(ind);
    return Status::OK;
}

#define CLI_HOST_PROMPT_FORMAT "BCM.%u> "

/* Build CLI prompt */
void openolt_cli_get_prompt_cb(bcmcli_session *session, char *buf, uint32_t max_len)
{
    snprintf(buf, max_len, CLI_HOST_PROMPT_FORMAT, dev_id);
}

int _bal_apiend_cli_thread_handler(long data)
{
    char init_string[]="\n";
    bcmcli_session *sess = current_session;
    bcmos_task_parm bal_cli_task_p_dummy;

    /* Switch to interactive mode if not stopped in the init script */
    if (!bcmcli_is_stopped(sess)) {
        /* Force a CLI command prompt
         * The string passed into the parse function
         * must be modifiable, so a string constant like
         * bcmcli_parse(current_session, "\n") will not
         * work.
         */
        bcmcli_parse(sess, init_string);

        /* Process user input until EOF or quit command */
        bcmcli_driver(sess);
    }
    OPENOLT_LOG(INFO, openolt_log_id, "BAL API End CLI terminated\n");

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

bcmos_errno bcm_cli_quit(bcmcli_session *session, const bcmcli_cmd_parm parm[], uint16_t n_parms)
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
    if (BCM_ERR_OK != bcmos_task_query(&bal_cli_thread, &bal_cli_task_p_dummy)) {
        /* Create BAL CLI thread */
        bal_cli_task_p.name = bal_cli_thread_name;
        bal_cli_task_p.handler = _bal_apiend_cli_thread_handler;
        bal_cli_task_p.priority = TASK_PRIORITY_CLI;

        ret = bcmos_task_create(&bal_cli_thread, &bal_cli_task_p);
        if (BCM_ERR_OK != ret) {
            bcmos_printf("Couldn't create BAL API end CLI thread\n");
            return ret;
        }
    }
}

bcmos_errno get_onu_state(bcmolt_interface pon_ni, int onu_id, bcmolt_onu_state *onu_state) {
    bcmos_errno err;
    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;

    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_MSG_FIELD_GET(&onu_cfg, onu_state);
    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);
    *onu_state = onu_cfg.data.onu_state;
    return err;
}

bcmos_errno get_gpon_onu_info(bcmolt_interface pon_ni, int onu_id, bcmolt_onu_state *onu_state, bcmolt_status *losi, bcmolt_status *lofi, bcmolt_status *loami)
{

    bcmos_errno err;
    bcmolt_onu_cfg onu_cfg;
    bcmolt_itu_onu_params itu = {};
    bcmolt_gpon_onu_alarm_state alarm_state = {};
    bcmolt_gpon_onu_params gpon = {};

    bcmolt_onu_cfg onu_cfg_itu;
    bcmolt_onu_key onu_key;
    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;

    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, onu_state);

    BCMOLT_FIELD_SET_PRESENT(&onu_cfg.data, onu_cfg_data, itu);

    BCMOLT_FIELD_SET_PRESENT(&itu, itu_onu_params, gpon);
    BCMOLT_FIELD_SET_PRESENT(&gpon, gpon_onu_params, alarm_state);

    BCMOLT_FIELD_SET_PRESENT(&alarm_state, gpon_onu_alarm_state, losi);
    BCMOLT_FIELD_SET_PRESENT(&alarm_state, gpon_onu_alarm_state, lofi);
    BCMOLT_FIELD_SET_PRESENT(&alarm_state, gpon_onu_alarm_state, loami);

    BCMOLT_FIELD_SET(&gpon, gpon_onu_params, alarm_state, alarm_state);

    BCMOLT_FIELD_SET(&itu, itu_onu_params, gpon, gpon);

    BCMOLT_FIELD_SET(&onu_cfg.data, onu_cfg_data, itu, itu);

    err = bcmolt_cfg_get(dev_id, &onu_cfg.hdr);

    *onu_state = onu_cfg.data.onu_state;
    // fetch onu itu
    *losi = onu_cfg.data.itu.gpon.alarm_state.losi;

    *lofi = onu_cfg.data.itu.gpon.alarm_state.lofi;

    *loami = onu_cfg.data.itu.gpon.alarm_state.loami;

    return err;
}

bcmos_errno get_gem_obj_state(bcmolt_interface pon_ni, bcmolt_gem_port_id id, bcmolt_activation_state *state) {
    bcmos_errno err;
    bcmolt_itupon_gem_cfg cfg;
    bcmolt_itupon_gem_key key;
    key.pon_ni = pon_ni;
    key.gem_port_id = id;

    BCMOLT_CFG_INIT(&cfg, itupon_gem, key);
    BCMOLT_MSG_FIELD_GET(&cfg, state);
    err = bcmolt_cfg_get(dev_id, &cfg.hdr);
    *state = cfg.data.state;
    return err;
}

bcmos_errno get_alloc_obj_state(bcmolt_interface pon_ni, bcmolt_alloc_id id, bcmolt_activation_state *state) {
    bcmos_errno err;
    bcmolt_itupon_alloc_cfg cfg;
    bcmolt_itupon_alloc_key key;
    key.pon_ni = pon_ni;
    key.alloc_id = id;

    BCMOLT_CFG_INIT(&cfg, itupon_alloc, key);
    BCMOLT_MSG_FIELD_GET(&cfg, state);
    err = bcmolt_cfg_get(dev_id, &cfg.hdr);
    *state = cfg.data.state;
    return err;
}

bcmos_errno get_pon_interface_status(bcmolt_interface pon_ni, bcmolt_interface_state *state, bcmolt_status *los_status) {
    bcmos_errno err;
    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_ni;

    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, state);
    BCMOLT_FIELD_SET_PRESENT(&pon_cfg.data, pon_interface_cfg_data, los_status);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of pon_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of pon_cfg.hdr and we cannot
    // set the pon_cfg.data.state. So a new stub function is created and address
    // of pon_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__pon_intf_stub(dev_id, &pon_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &pon_cfg.hdr);
    #endif
    *state = pon_cfg.data.state;
    *los_status = pon_cfg.data.los_status;
    return err;
}

/* Same as bcmolt_cfg_get but with added logic of retrying the API
   in case of some specific failures like timeout or object not yet ready
*/
bcmos_errno bcmolt_cfg_get_mult_retry(bcmolt_oltid olt, bcmolt_cfg *cfg) {
    bcmos_errno err;
    uint32_t current_try = 0;

    while (current_try < MAX_BAL_API_RETRY_COUNT) {
        err = bcmolt_cfg_get(olt, cfg);
        current_try++;

        if (err == BCM_ERR_STATE || err == BCM_ERR_TIMEOUT) {
            OPENOLT_LOG(WARNING, openolt_log_id, "bcmolt_cfg_get: err = %s\n", bcmos_strerror(err));
            bcmos_usleep(BAL_API_RETRY_TIME_IN_USECS);
            continue;
        }
        else {
           break;
        }
    }

    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "bcmolt_cfg_get tried (%d) times with retry time(%d usecs) err = %s\n",
                           current_try,
                           BAL_API_RETRY_TIME_IN_USECS,
                           bcmos_strerror(err));
    }
    return err;
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
    #ifdef TEST_MODE
    // It is impossible to mock the setting of nni_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of nni_cfg.hdr and we cannot
    // set the nni_cfg.data.state. So a new stub function is created and address
    // of nni_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__nni_intf_stub(dev_id, &nni_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &nni_cfg.hdr);
    #endif
    *state = nni_cfg.data.state;
    return err;
}

bcmos_errno get_nni_interface_speed(bcmolt_interface id, uint32_t *speed)
{
    bcmos_errno err;
    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = id;

    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    BCMOLT_FIELD_SET_PRESENT(&nni_cfg.data, nni_interface_cfg_data, speed);
    err = bcmolt_cfg_get(dev_id, &nni_cfg.hdr);
    *speed = nni_cfg.data.speed;
    return err;
}

Status install_gem_port(int32_t intf_id, int32_t onu_id, int32_t uni_id, int32_t gemport_id, std::string board_technology) {
    gemport_status_map_key_tuple gem_status_key(intf_id, onu_id, uni_id, gemport_id);

    bcmos_fastlock_lock(&data_lock);
    std::map<gemport_status_map_key_tuple, bool>::const_iterator it = gemport_status_map.find(gem_status_key);
    if (it != gemport_status_map.end()) {
        if (it->second) {
            bcmos_fastlock_unlock(&data_lock, 0);
            OPENOLT_LOG(INFO, openolt_log_id, "gem port already installed = %d\n", gemport_id);
            return Status::OK;
        }
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    bcmos_errno err;
    bcmolt_itupon_gem_cfg cfg; /* declare main API struct */
    bcmolt_itupon_gem_key key = {}; /* declare key */
    bcmolt_gem_port_configuration configuration = {};
    bcmolt_onu_state onu_state;

    key.pon_ni = intf_id;
    key.gem_port_id = gemport_id;

    BCMOLT_CFG_INIT(&cfg, itupon_gem, key);

    bcmolt_gem_port_direction configuration_direction;
    configuration_direction = BCMOLT_GEM_PORT_DIRECTION_BIDIRECTIONAL;
    BCMOLT_FIELD_SET(&configuration, gem_port_configuration, direction, configuration_direction);

    bcmolt_gem_port_type configuration_type;
    configuration_type = BCMOLT_GEM_PORT_TYPE_UNICAST;
    BCMOLT_FIELD_SET(&configuration, gem_port_configuration, type, configuration_type);

    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, configuration, configuration);

    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, onu_id, onu_id);

    bcmolt_control_state encryption_mode;
    encryption_mode = BCMOLT_CONTROL_STATE_DISABLE;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, encryption_mode, encryption_mode);

    bcmolt_us_gem_port_destination upstream_destination_queue;
    upstream_destination_queue = BCMOLT_US_GEM_PORT_DESTINATION_DATA;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, upstream_destination_queue, upstream_destination_queue);

    bcmolt_control_state control;
    control = BCMOLT_CONTROL_STATE_ENABLE;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, control, control);

    bool wait_for_gem_cfg_complt = false;
    if (board_technology == "GPON") {
        // Wait for gem cfg complete indication only if ONU state is ACTIVE
        err = get_onu_state((bcmolt_interface)intf_id, (bcmolt_onu_id)onu_id, &onu_state);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to get onu status onu_id = %d, gem_port = %d err = %s\n", onu_id, gemport_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "failed to get onu status");
        } else if (onu_state == BCMOLT_ONU_STATE_ACTIVE) {
            wait_for_gem_cfg_complt = true;
        }
    }

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if(err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to install gem_port = %d err = %s (%d)\n", gemport_id, cfg.hdr.hdr.err_text, err);
        return bcm_to_grpc_err(err, "Access_Control set ITU PON Gem port failed");
    }

    // Wait for gem cfg complete indication only if ONU state is ACTIVE
    if (wait_for_gem_cfg_complt) {
        err = wait_for_gem_action(intf_id, gemport_id, GEM_OBJECT_CREATE);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to install gem_port = %d err = %s\n", gemport_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Access_Control set ITU PON Gem port failed");
        }
    } else {
        OPENOLT_LOG(DEBUG, openolt_log_id, "not waiting for gem config complete indication = %d\n", gemport_id);
    }

    OPENOLT_LOG(INFO, openolt_log_id, "gem port installed successfully = %d\n", gemport_id);

    bcmos_fastlock_lock(&data_lock);
    gemport_status_map[gem_status_key] = true;
    bcmos_fastlock_unlock(&data_lock, 0);

    return Status::OK;
}

Status remove_gem_port(int32_t intf_id, int32_t onu_id, int32_t uni_id, int32_t gemport_id, std::string board_technology) {
    gemport_status_map_key_tuple gem_status_key(intf_id, onu_id, uni_id, gemport_id);
    bcmolt_onu_state onu_state;

    bcmos_fastlock_lock(&data_lock);
    std::map<gemport_status_map_key_tuple, bool>::const_iterator it = gemport_status_map.find(gem_status_key);
    if (it == gemport_status_map.end()) {
        bcmos_fastlock_unlock(&data_lock, 0);
        OPENOLT_LOG(INFO, openolt_log_id, "gem port already removed = %d\n", gemport_id);
        return Status::OK;
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    bcmolt_itupon_gem_cfg gem_cfg;
    bcmolt_itupon_gem_key key = {
        .pon_ni = (bcmolt_interface)intf_id,
        .gem_port_id = (bcmolt_gem_port_id)gemport_id
    };
    bcmos_errno err;

    bool wait_for_gem_cfg_complt = false;
    if (board_technology == "GPON") {
        // Wait for gem cfg complete indication only if ONU state is ACTIVE
        err = get_onu_state((bcmolt_interface)intf_id, (bcmolt_onu_id)onu_id, &onu_state);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to get onu status onu_id = %d, gem_port = %d err = %s\n", onu_id, gemport_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "failed to get onu status");
        } else if (onu_state == BCMOLT_ONU_STATE_ACTIVE) {
            wait_for_gem_cfg_complt = true;
        }
    }

    BCMOLT_CFG_INIT(&gem_cfg, itupon_gem, key);
    err = bcmolt_cfg_clear(dev_id, &gem_cfg.hdr);
    if (err != BCM_ERR_OK)
    {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to remove gem_port = %d err = %s (%d)\n", gemport_id, gem_cfg.hdr.hdr.err_text, err);
        return bcm_to_grpc_err(err, "Access_Control clear ITU PON Gem port failed");
    }

    if (wait_for_gem_cfg_complt) {
        OPENOLT_LOG(INFO, openolt_log_id, "onu state is active waiting for gem cfg complete indication intf = %d onu = %d\n",
                    intf_id, onu_id);
        err = wait_for_gem_action(intf_id, gemport_id, GEM_OBJECT_DELETE);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to remove gem, intf_id %d, gemport_id %d, err = %s\n",
                        intf_id, gemport_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "failed to remove gem");
        }
    } else {
        OPENOLT_LOG(DEBUG, openolt_log_id, "onu not active or/and not gpon tech, not waiting for gem cfg complete, onu_state = %d, intf = %d, gemport_id = %d, onu=%d\n",
                    onu_state, intf_id, gemport_id, onu_id);
    }

    OPENOLT_LOG(INFO, openolt_log_id, "gem port removed successfully = %d\n", gemport_id);

    bcmos_fastlock_lock(&data_lock);
    it = gemport_status_map.find(gem_status_key);
    if (it != gemport_status_map.end()) {
        gemport_status_map.erase(it);
    }
    bcmos_fastlock_unlock(&data_lock, 0);

    return Status::OK;
}

Status enable_encryption_for_gem_port(int32_t intf_id, int32_t gemport_id, std::string board_technology) {
    bcmos_errno err;
    bcmolt_itupon_gem_cfg cfg;
    bcmolt_itupon_gem_key key = {
        .pon_ni = (bcmolt_interface)intf_id,
        .gem_port_id = (bcmolt_gem_port_id)gemport_id
    };

    BCMOLT_CFG_INIT(&cfg, itupon_gem, key);
    BCMOLT_MSG_FIELD_GET(&cfg, encryption_mode);
    err = bcmolt_cfg_get(dev_id, &cfg.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "GEM port get encryption_mode failed err = %s (%d)\n", cfg.hdr.hdr.err_text, err);
        return bcm_to_grpc_err(err, "GEM port get encryption_mode failed");
    }

    if (cfg.data.encryption_mode == BCMOLT_CONTROL_STATE_ENABLE) {
        OPENOLT_LOG(INFO, openolt_log_id, "gem port already encrypted = %d\n", gemport_id);
        return Status::OK;
    }

    bcmolt_control_state encryption_mode;
    encryption_mode = BCMOLT_CONTROL_STATE_ENABLE;
    BCMOLT_FIELD_SET(&cfg.data, itupon_gem_cfg_data, encryption_mode, encryption_mode);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if(err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to set encryption on pon = %d gem_port = %d, err = %s (%d)\n",
            intf_id, gemport_id, cfg.hdr.hdr.err_text, err);
        return bcm_to_grpc_err(err, "Failed to set encryption on GEM port");;
    }

#ifndef SCALE_AND_PERF
    if (board_technology == "GPON") {
        err = wait_for_gem_action(intf_id, gemport_id, GEM_OBJECT_ENCRYPT);
        if (err) {
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to enable gemport encryption, gem_port = %d err = %s\n", gemport_id, bcmos_strerror(err));
            return bcm_to_grpc_err(err, "Access_Control ITU PON Gem port encryption failed");
        }
    }
#endif

    OPENOLT_LOG(INFO, openolt_log_id, "encryption set successfully on pon = %d gem_port = %d\n", intf_id, gemport_id);

    return Status::OK;
}

Status update_acl_interface(int32_t intf_id, bcmolt_interface_type intf_type, uint32_t access_control_id,
                bcmolt_members_update_command acl_cmd) {
    bcmos_errno err;
    bcmolt_access_control_interfaces_update oper; /* declare main API struct */
    bcmolt_access_control_key acl_key = {}; /* declare key */
    bcmolt_intf_ref interface_ref_list_elem = {};
    bcmolt_interface_type interface_ref_list_elem_intf_type;
    bcmolt_interface_id interface_ref_list_elem_intf_id;
    bcmolt_intf_ref_list_u8 interface_ref_list = {};

    if (acl_cmd != BCMOLT_MEMBERS_UPDATE_COMMAND_ADD && acl_cmd != BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE) {
        OPENOLT_LOG(ERROR, openolt_log_id, "acl cmd = %d not supported currently\n", acl_cmd);
        return bcm_to_grpc_err(BCM_ERR_PARM, "unsupported acl cmd");
    }
    interface_ref_list.arr = (bcmolt_intf_ref*)bcmos_calloc(sizeof(bcmolt_intf_ref)*1);

    if (interface_ref_list.arr == NULL)
         return bcm_to_grpc_err(BCM_ERR_PARM, "allocate interface_ref_list failed");
    OPENOLT_LOG(INFO, openolt_log_id, "update acl interface received for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");

    acl_key.id = access_control_id;

    /* Initialize the API struct. */
    BCMOLT_OPER_INIT(&oper, access_control, interfaces_update, acl_key);

    bcmolt_members_update_command command;
    command = acl_cmd;
    BCMOLT_FIELD_SET(&oper.data, access_control_interfaces_update_data, command, command);

    interface_ref_list_elem_intf_type = intf_type;
    BCMOLT_FIELD_SET(&interface_ref_list_elem, intf_ref, intf_type, interface_ref_list_elem_intf_type);

    interface_ref_list_elem_intf_id = intf_id;
    BCMOLT_FIELD_SET(&interface_ref_list_elem, intf_ref, intf_id, interface_ref_list_elem_intf_id);

    interface_ref_list.len = 1;
    BCMOLT_ARRAY_ELEM_SET(&interface_ref_list, 0, interface_ref_list_elem);

    BCMOLT_FIELD_SET(&oper.data, access_control_interfaces_update_data, interface_ref_list, interface_ref_list);

    err =  bcmolt_oper_submit(dev_id, &oper.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "update acl interface fail for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");
        return bcm_to_grpc_err(err, "Access_Control submit interface failed");
    }

    bcmos_free(interface_ref_list.arr);
    OPENOLT_LOG(INFO, openolt_log_id, "update acl interface success for intf_id = %d, intf_type = %s, acl_id = %d, acl_cmd = %s\n",
            intf_id, intf_type == BCMOLT_INTERFACE_TYPE_PON? "pon": "nni", access_control_id,
            acl_cmd == BCMOLT_MEMBERS_UPDATE_COMMAND_ADD? "add": "remove");

    return Status::OK;
}

Status install_acl(const acl_classifier_key acl_key) {
    bcmos_errno err;
    bcmolt_access_control_cfg cfg;
    bcmolt_access_control_key key = { };
    bcmolt_classifier c_val = { };
    // hardcode the action for now.
    bcmolt_access_control_fwd_action_type action_type = BCMOLT_ACCESS_CONTROL_FWD_ACTION_TYPE_TRAP_TO_HOST;
    int acl_id = get_acl_id();
    if (acl_id < 0) {
        OPENOLT_LOG(ERROR, openolt_log_id, "exhausted acl_id for eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d o_vid = %d, max_acl_hit=%d\n",
                acl_key.ether_type, acl_key.ip_proto, acl_key.src_port, acl_key.dst_port, acl_key.o_vid, max_acls_with_vlan_classifiers_hit);
        return bcm_to_grpc_err(BCM_ERR_INTERNAL, "exhausted acl id");
    }

    key.id = acl_id;
    /* config access control instance */
    BCMOLT_CFG_INIT(&cfg, access_control, key);
    if (acl_key.ether_type > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify ether_type 0x%04x\n", acl_key.ether_type);
        BCMOLT_FIELD_SET(&c_val, classifier, ether_type, acl_key.ether_type);
    }

    if (acl_key.ip_proto > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify ip_proto %d\n", acl_key.ip_proto);
        BCMOLT_FIELD_SET(&c_val, classifier, ip_proto, acl_key.ip_proto);
    }

    if (acl_key.dst_port > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify dst_port %d\n", acl_key.dst_port);
        BCMOLT_FIELD_SET(&c_val, classifier, dst_port, acl_key.dst_port);
    }

    if (acl_key.src_port > 0) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify src_port %d\n", acl_key.src_port);
        BCMOLT_FIELD_SET(&c_val, classifier, src_port, acl_key.src_port);
    }

    // Make sure that max_acls_with_vlan_classifiers_hit is not true to consider o_vid for ACL classification.
    if (acl_key.o_vid > 0 && acl_key.o_vid != ANY_VLAN && !max_acls_with_vlan_classifiers_hit) {
        OPENOLT_LOG(DEBUG, openolt_log_id, "Access_Control classify o_vid %d\n", acl_key.o_vid);
        BCMOLT_FIELD_SET(&c_val, classifier, o_vid, acl_key.o_vid);
    }

    BCMOLT_MSG_FIELD_SET(&cfg, classifier, c_val);
    BCMOLT_MSG_FIELD_SET(&cfg, priority, 10000);
    BCMOLT_MSG_FIELD_SET(&cfg, statistics_control, BCMOLT_CONTROL_STATE_ENABLE);

    BCMOLT_MSG_FIELD_SET(&cfg, forwarding_action.action, action_type);

    err = bcmolt_cfg_set(dev_id, &cfg.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Access_Control set configuration failed, err = %s (%d)\n", cfg.hdr.hdr.err_text, err);
        // Free the acl_id
        free_acl_id(acl_id);
        return bcm_to_grpc_err(err, "Access_Control set configuration failed");
    }

    ACL_LOG(INFO, "ACL add ok", err);

    // Update the map that we have installed an acl for the given classfier.
    acl_classifier_to_acl_id_map[acl_key] = acl_id;
    // If there was a valid vlan classifier in the ACL and the ACL ID hit the ceiling, set max_acls_with_vlan_classifiers_hit to true
    // After max_acls_with_vlan_classifiers_hit is set to true no more ACLs can have vlan as an ACL classifier.
    if (acl_key.o_vid > 0 && acl_key.o_vid != ANY_VLAN && acl_id >= MAX_ACL_WITH_VLAN_CLASSIFIER) {
        max_acls_with_vlan_classifiers_hit = true;
    }
    return Status::OK;
}

Status remove_acl(int acl_id) {
    bcmos_errno err;
    bcmolt_access_control_cfg cfg; /* declare main API struct */
    bcmolt_access_control_key key = {}; /* declare key */

    key.id = acl_id;

    /* Initialize the API struct. */
    BCMOLT_CFG_INIT(&cfg, access_control, key);
    BCMOLT_FIELD_SET_PRESENT(&cfg.data, access_control_cfg_data, state);
    err = bcmolt_cfg_get(dev_id, &cfg.hdr);
    if (err != BCM_ERR_OK) {
        OPENOLT_LOG(ERROR, openolt_log_id, "Access_Control get state failed, err = %s (%d)\n", cfg.hdr.hdr.err_text, err);
        return bcm_to_grpc_err(err, "Access_Control get state failed");
    }

    if (cfg.data.state == BCMOLT_CONFIG_STATE_CONFIGURED) {
        key.id = acl_id;
        /* Initialize the API struct. */
        BCMOLT_CFG_INIT(&cfg, access_control, key);

        err = bcmolt_cfg_clear(dev_id, &cfg.hdr);
        if (err != BCM_ERR_OK) {
            // Should we free acl_id here ? We should ideally never land here..
            OPENOLT_LOG(ERROR, openolt_log_id, "Error %d while removing Access_Control rule ID err = %s (%d)\n",
                acl_id, cfg.hdr.hdr.err_text, err);
            return Status(grpc::StatusCode::INTERNAL, "Failed to remove Access_Control");
        }
    }

    // Free up acl_id
    free_acl_id(acl_id);

    OPENOLT_LOG(INFO, openolt_log_id, "acl removed successfully %d\n", acl_id);

    return Status::OK;
}

// Formulates ACL Classifier Key based on the following fields
// a. ether_type b. ip_proto c. src_port d. dst_port
// If any of the field is not available it is populated as -1.
void formulate_acl_classifier_key(acl_classifier_key *key, const ::openolt::Classifier& classifier) {

        // TODO: Is 0 a valid value for any of the following classifiers?
        // because in the that case, the 'if' check would fail and -1 would be filled as value.
        //
        if (classifier.eth_type()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ether_type 0x%04x\n", classifier.eth_type());
            key->ether_type = classifier.eth_type();
        } else key->ether_type = -1;

        if (classifier.ip_proto()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify ip_proto %d\n", classifier.ip_proto());
            key->ip_proto = classifier.ip_proto();
        } else key->ip_proto = -1;


        if (classifier.src_port()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify src_port %d\n", classifier.src_port());
            key->src_port = classifier.src_port();
        } else key->src_port = -1;


        if (classifier.dst_port()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify dst_port %d\n", classifier.dst_port());
            key->dst_port = classifier.dst_port();
        } else key->dst_port = -1;

        // We should also check the max_acls_with_vlan_classifiers_hit flag is not false to consider the vlan for flow classifier key
        if (classifier.o_vid() && !max_acls_with_vlan_classifiers_hit) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "classify o_vid %d\n", classifier.o_vid());
            key->o_vid = classifier.o_vid();
        } else key->o_vid = ANY_VLAN;

}

Status handle_acl_rule_install(int32_t onu_id, uint64_t flow_id, int32_t gemport_id,
                               const std::string flow_type, int32_t access_intf_id,
                               int32_t network_intf_id,
                               const ::openolt::Classifier& classifier) {
    int acl_id;
    uint32_t intf_id = flow_type == upstream? access_intf_id: network_intf_id;
    const std::string intf_type = flow_type == upstream? "pon": "nni";
    bcmolt_interface_type olt_if_type = intf_type == "pon"? BCMOLT_INTERFACE_TYPE_PON: BCMOLT_INTERFACE_TYPE_NNI;

    Status resp;
    trap_to_host_packet_type pkt_type = get_trap_to_host_packet_type(classifier);
    if (pkt_type == unsupported_trap_to_host_pkt_type) {
        OPENOLT_LOG(ERROR, openolt_log_id, "unsupported pkt trap type");
        return Status(grpc::StatusCode::UNIMPLEMENTED, "unsupported pkt trap type");
    }

    // few map keys we are going to use later.
    flow_id_flow_direction fl_id_fl_dir(flow_id, flow_type);

    acl_classifier_key acl_key;
    formulate_acl_classifier_key(&acl_key, classifier);
    const acl_classifier_key acl_key_const = {.ether_type=acl_key.ether_type, .ip_proto=acl_key.ip_proto,
        .src_port=acl_key.src_port, .dst_port=acl_key.dst_port, .o_vid=acl_key.o_vid};
    bcmos_fastlock_lock(&acl_packet_trap_handler_lock);

    // Check if the acl is already installed
    if (acl_classifier_to_acl_id_map.count(acl_key_const) > 0) {
        // retreive the acl_id
        acl_id = acl_classifier_to_acl_id_map[acl_key_const];


        if (flow_to_acl_map.count(fl_id_fl_dir)) {
            // could happen if same trap flow is received again
            OPENOLT_LOG(INFO, openolt_log_id, "flow and related acl already handled, nothing more to do\n");
            bcmos_fastlock_unlock(&acl_packet_trap_handler_lock, 0);
            return Status::OK;
        }

        OPENOLT_LOG(INFO, openolt_log_id, "Acl for flow_id=%lu with eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d o_vid = %d already installed with acl id = %u\n",
                flow_id, acl_key.ether_type, acl_key.ip_proto, acl_key.src_port, acl_key.dst_port, acl_key.o_vid, acl_id);

        // The acl_ref_cnt is needed to know how many flows refer an ACL.
        // When the flow is removed, we decrement the reference count.
        // When the reference count becomes 0, we remove the ACL.
        if (acl_ref_cnt.count(acl_id) > 0) {
            acl_ref_cnt[acl_id] ++;
        } else {
            // We should ideally not land here. The acl_ref_cnt should have been
            // initialized the first time acl was installed.
            acl_ref_cnt[acl_id] = 1;
        }

    } else {
        resp = install_acl(acl_key_const);
        if (!resp.ok()) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Acl for flow_id=%lu with eth_type = %d, ip_proto = %d, src_port = %d, dst_port = %d o_vid = %d failed\n",
                    flow_id, acl_key_const.ether_type, acl_key_const.ip_proto, acl_key_const.src_port, acl_key_const.dst_port, acl_key_const.o_vid);
            bcmos_fastlock_unlock(&acl_packet_trap_handler_lock, 0);
            return resp;
        }

        acl_id = acl_classifier_to_acl_id_map[acl_key_const];

        // Initialize the acl reference count
        acl_ref_cnt[acl_id] = 1;

        OPENOLT_LOG(INFO, openolt_log_id, "acl add success for flow_id=%lu with acl_id=%d\n", flow_id, acl_id);
    }

    // Register the interface for the given acl
    acl_id_intf_id_intf_type ac_id_inf_id_inf_type(acl_id, intf_id, intf_type);
    // This is needed to keep a track of which interface (pon/nni) has registered for an ACL.
    // If it is registered, how many flows refer to it.
    if (intf_acl_registration_ref_cnt.count(ac_id_inf_id_inf_type) > 0) {
        intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type]++;
    } else {
        // The given interface is not registered for the ACL. We need to do it now.
        resp = update_acl_interface(intf_id, olt_if_type, acl_id, BCMOLT_MEMBERS_UPDATE_COMMAND_ADD);
        if (!resp.ok()){
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to update acl interfaces intf_id=%d, intf_type=%s, acl_id=%d", intf_id, intf_type.c_str(), acl_id);
            // TODO: Ideally we should return error from hear and clean up other other stateful
            // counters we creaed earlier. Will leave it out for now.
        }
        intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type] = 1;
    }

    acl_id_intf_id ac_id_if_id(acl_id, intf_id);
    flow_to_acl_map[fl_id_fl_dir] = ac_id_if_id;

    // Populate the trap_to_host_pkt_info_with_vlan corresponding to the trap-to-host voltha flow_id key.
    // When the trap-to-host voltha flow-id is being removed, this entry is cleared too from the map.
    trap_to_host_pkt_info_with_vlan pkt_info_with_vlan((int32_t)olt_if_type, intf_id, (int32_t)pkt_type, gemport_id, (short unsigned int)classifier.o_vid());
    trap_to_host_pkt_info_with_vlan_for_flow_id[flow_id] = pkt_info_with_vlan;
    trap_to_host_pkt_info pkt_info((int32_t)olt_if_type, intf_id, (int32_t)pkt_type, gemport_id);
    bool duplicate = false;
    // Check if the vlan_id corresponding to the trap_to_host_pkt_info key is found. Set the 'duplicate' flag accordingly.
    if (trap_to_host_vlan_ids_for_trap_to_host_pkt_info.count(pkt_info) > 0) {
        auto& vlan_id_list = trap_to_host_vlan_ids_for_trap_to_host_pkt_info[pkt_info];
        auto it = std::find(vlan_id_list.begin(), vlan_id_list.end(), acl_key.o_vid);
        if (it != vlan_id_list.end()) {
            OPENOLT_LOG(DEBUG, openolt_log_id, "cvid = %d exists already in list", acl_key.o_vid);
            duplicate = true;
        }
    }
    // If the vlan_id is not found corresponding to the trap_to_host_pkt_info key, update it.
    // This will be used to validate the vlan_id in the trapped packet. If vlan_id in the
    // trapped packet is not match with the stored value, packet is dropped.
    if (!duplicate) {
        trap_to_host_vlan_ids_for_trap_to_host_pkt_info[pkt_info].push_back(acl_key.o_vid);
    }

    bcmos_fastlock_unlock(&acl_packet_trap_handler_lock, 0);

    return Status::OK;
}

Status handle_acl_rule_cleanup(int16_t acl_id, int32_t intf_id, const std::string flow_type) {
    const std::string intf_type= flow_type == upstream? "pon": "nni";
    acl_id_intf_id_intf_type ac_id_inf_id_inf_type(acl_id, intf_id, intf_type);
    intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type]--;
    if (intf_acl_registration_ref_cnt[ac_id_inf_id_inf_type] == 0) {
        bcmolt_interface_type olt_if_type = intf_type == "pon"? BCMOLT_INTERFACE_TYPE_PON: BCMOLT_INTERFACE_TYPE_NNI;
        Status resp = update_acl_interface(intf_id, olt_if_type, acl_id, BCMOLT_MEMBERS_UPDATE_COMMAND_REMOVE);
        if (!resp.ok()){
            OPENOLT_LOG(ERROR, openolt_log_id, "failed to update acl interfaces intf_id=%d, intf_type=%s, acl_id=%d", intf_id, intf_type.c_str(), acl_id);
        }
        intf_acl_registration_ref_cnt.erase(ac_id_inf_id_inf_type);
    }

    acl_ref_cnt[acl_id]--;
    if (acl_ref_cnt[acl_id] == 0) {
        remove_acl(acl_id);
        acl_ref_cnt.erase(acl_id);
        // Iterate acl_classifier_to_acl_id_map and delete classifier the key corresponding to acl_id
        std::map<acl_classifier_key, uint16_t>::iterator it;
        for (it=acl_classifier_to_acl_id_map.begin(); it!=acl_classifier_to_acl_id_map.end(); ++it)  {
            if (it->second == acl_id) {
                OPENOLT_LOG(INFO, openolt_log_id, "cleared classifier key corresponding to acl_id = %d\n", acl_id);
                acl_classifier_to_acl_id_map.erase(it->first);
                break;
            }
        }
    }

    return Status::OK;
}

Status check_bal_ready() {
    bcmos_errno err;
    int maxTrials = 30;
    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };

    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    BCMOLT_MSG_FIELD_GET(&olt_cfg, bal_state);

    while (olt_cfg.data.bal_state != BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY) {
        if (--maxTrials == 0)
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "check bal ready failed");
        sleep(5);
        #ifdef TEST_MODE
        // It is impossible to mock the setting of olt_cfg.data.bal_state because
        // the actual bcmolt_cfg_get passes the address of olt_cfg.hdr and we cannot
        // set the olt_cfg.data.bal_state. So a new stub function is created and address
        // of olt_cfg is passed. This is one-of case where we need to add test specific
        // code in production code.
        if (bcmolt_cfg_get__bal_state_stub(dev_id, &olt_cfg)) {
        #else
        if (bcmolt_cfg_get(dev_id, &olt_cfg.hdr)) {
        #endif
            continue;
        }
        else
            OPENOLT_LOG(INFO, openolt_log_id, "waiting for BAL ready ...\n");
    }

    OPENOLT_LOG(INFO, openolt_log_id, "BAL is ready\n");
    return Status::OK;
}

Status check_connection() {
    int maxTrials = 60;
    while (!bcmolt_api_conn_mgr_is_connected(dev_id)) {
        sleep(1);
        if (--maxTrials == 0)
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "check connection failed");
        else
            OPENOLT_LOG(INFO, openolt_log_id, "waiting for daemon connection ...\n");
    }
    OPENOLT_LOG(INFO, openolt_log_id, "daemon is connected\n");
    return Status::OK;
}

std::string get_ip_address(const char* nw_intf){
    std::string ipAddress = "0.0.0.0";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
   /* retrieve the current interfaces - returns 0 on success */
    success = getifaddrs(&interfaces);
    if (success == 0) {
        /* Loop through linked list of interfaces */
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                /* Check if interface given present in OLT, if yes return its IP Address */
                if(strcmp(temp_addr->ifa_name, nw_intf) == 0){
                    ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                    break;
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    /* Free memory */
    freeifaddrs(interfaces);
    return ipAddress;
}

bcmos_errno getOnuMaxLogicalDistance(uint32_t intf_id, uint32_t *mld) {
    bcmos_errno err = BCM_ERR_OK;
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg; /* declare main API struct */
    bcmolt_pon_interface_key key = {}; /* declare key */

    key.pon_ni = intf_id;

    if (!state.is_activated()) {
        OPENOLT_LOG(ERROR, openolt_log_id, "ONU maximum logical distance is not available since OLT is not activated yet\n");
        return BCM_ERR_STATE;
    }

    /* Initialize the API struct. */
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);
        BCMOLT_FIELD_SET_PRESENT(&pon_distance, pon_distance, max_log_distance);
    BCMOLT_FIELD_SET(&pon_cfg.data, pon_interface_cfg_data, pon_distance, pon_distance);
    #ifdef TEST_MODE
    // It is impossible to mock the setting of pon_cfg.data.state because
    // the actual bcmolt_cfg_get passes the address of pon_cfg.hdr and we cannot
    // set the pon_cfg.data.state. So a new stub function is created and address
    // of pon_cfg is passed. This is one-of case where we need to add test specific
    // code in production code.
    err = bcmolt_cfg_get__pon_intf_stub(dev_id, &pon_cfg);
    #else
    err = bcmolt_cfg_get(dev_id, &pon_cfg.hdr);
    #endif
        if (err != BCM_ERR_OK) {
            OPENOLT_LOG(ERROR, openolt_log_id, "Failed to retrieve ONU maximum logical distance for PON %d, err = %s (%d)\n", intf_id, pon_cfg.hdr.hdr.err_text, err);
            return err;
        }
        *mld = pon_distance.max_log_distance;

    return BCM_ERR_OK;
}

/**
* Gets mac address based on interface name.
*
* @param intf_name interface name
* @param mac_address  mac address field
* @param max_size_of_mac_address max sixe of the mac_address
* @return mac_address value in case of success or return NULL in case of failure.
*/

char* get_intf_mac(const char* intf_name, char* mac_address,  unsigned int max_size_of_mac_address){
    int fd;
    struct ifreq ifr;
    char *mac;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( fd == -1) {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to get mac, could not create file descriptor");
        return NULL;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy((char *)ifr.ifr_name , (const char *)intf_name , IFNAMSIZ-1);
    if( ioctl(fd, SIOCGIFHWADDR, &ifr) == -1)
    {
        OPENOLT_LOG(ERROR, openolt_log_id, "failed to get mac, ioctl failed and returned err");
        close(fd);
        return NULL;
    }

    close(fd);
    mac = (char *)ifr.ifr_hwaddr.sa_data;

    // formatted mac address
    snprintf(mac_address, max_size_of_mac_address, (const char *)"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", (unsigned char)mac[0], (unsigned char)mac[1], (unsigned char)mac[2], (unsigned char)mac[3], (unsigned char)mac[4], (unsigned char)mac[5]);

    return mac_address;
}

void update_voltha_flow_to_cache(uint64_t voltha_flow_id, device_flow dev_flow) {
    OPENOLT_LOG(DEBUG, openolt_log_id, "updating voltha flow=%lu to cache\n", voltha_flow_id)
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    voltha_flow_to_device_flow[voltha_flow_id] = dev_flow;
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
}

void remove_voltha_flow_from_cache(uint64_t voltha_flow_id) {
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    std::map<uint64_t, device_flow>::const_iterator it = voltha_flow_to_device_flow.find(voltha_flow_id);
    if (it != voltha_flow_to_device_flow.end()) {
        voltha_flow_to_device_flow.erase(it);
    }
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
}

bool is_voltha_flow_installed(uint64_t voltha_flow_id ) {
    int count;
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    count = voltha_flow_to_device_flow.count(voltha_flow_id);
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);

    return count > 0 ? true : false;
}

const device_flow_params* get_device_flow_params(uint64_t voltha_flow_id) {
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    std::map<uint64_t, device_flow>::const_iterator it = voltha_flow_to_device_flow.find(voltha_flow_id);
    if (it != voltha_flow_to_device_flow.end()) {
        bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
        return it->second.params;
    }
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);

    return NULL;
}

const device_flow* get_device_flow(uint64_t voltha_flow_id) {
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    std::map<uint64_t, device_flow>::const_iterator it = voltha_flow_to_device_flow.find(voltha_flow_id);
    if (it != voltha_flow_to_device_flow.end()) {
        bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
        return &it->second;
    }
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);

    return NULL;
}

const device_flow* get_device_flow_by_id_and_type(uint16_t flow_id, std::string flow_type) {
    bcmos_fastlock_lock(&voltha_flow_to_device_flow_lock);
    std::map<uint64_t, device_flow>::const_iterator it = voltha_flow_to_device_flow.begin();
    while (it != voltha_flow_to_device_flow.end()) {
	if(it->second.flow_type == flow_type) {
            for(int i = 0; i < MAX_NUMBER_OF_REPLICATED_FLOWS; i++) {
                if (it->second.params[i].flow_id == flow_id) {
                    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
                    return &it->second;
                }
            }
	}
        it++;
    }
    bcmos_fastlock_unlock(&voltha_flow_to_device_flow_lock, 0);
    return nullptr;
}

trap_to_host_packet_type get_trap_to_host_packet_type(const ::openolt::Classifier& classifier) {
    trap_to_host_packet_type type = unsupported_trap_to_host_pkt_type;
    if (classifier.eth_type() == EAP_ETH_TYPE) {
        type = eap;
    } else if (classifier.src_port() == DHCP_SERVER_SRC_PORT || classifier.src_port() == DHCP_CLIENT_SRC_PORT) {
        type = dhcpv4;
    } else if (classifier.eth_type() == LLDP_ETH_TYPE) {
        type = lldp;
    } else if (classifier.ip_proto() == IGMPv4_PROTOCOL) {
        type = igmpv4;
    } else if (classifier.eth_type() == PPPoED_ETH_TYPE) {
        type = pppoed;
    }

    return type;
}

// is_packet_allowed extracts the VLAN, packet-type, interface-type, interface-id from incoming trap-to-host packet.
// Then it verifies if this packet can be allowed upstream to host. It does this by checking if the vlan in the incoming packet
//exists in trap_to_host_vlan_ids_for_trap_to_host_pkt_info map for (interface-type, interface-id, packet-type) key.
bool is_packet_allowed(bcmolt_access_control_receive_eth_packet_data *data, int32_t gemport_id) {
    bcmolt_interface_type intf_type = data->interface_ref.intf_type;
    uint32_t intf_id = data->interface_ref.intf_id;
    trap_to_host_packet_type pkt_type = unsupported_trap_to_host_pkt_type;
    uint16_t vlan_id = 0;
    int ethType;

    struct timeval dummy_tv = {0, 0};
    bool free_memory_of_raw_packet = false; // This indicates the pcap library to not free the message buffer. It will freed by the caller.

    pcpp::RawPacket rawPacket(data->buffer.arr, data->buffer.len, dummy_tv, free_memory_of_raw_packet, pcpp::LINKTYPE_ETHERNET);
    pcpp::Packet parsedPacket(&rawPacket);
    pcpp::EthLayer* ethernetLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
    if (ethernetLayer == NULL)
    {
        OPENOLT_LOG(ERROR, openolt_log_id, "Something went wrong, couldn't find Ethernet layer\n");
        return false;
    }

    // Getting Vlan layer
    pcpp::VlanLayer* vlanLayer = parsedPacket.getLayerOfType<pcpp::VlanLayer>();
    if (vlanLayer == NULL)
    {
        // Allow Untagged LLDP Ether type packet to trap from NNI
        if (ntohs(ethernetLayer->getEthHeader()->etherType) == LLDP_ETH_TYPE && intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
           return true;
        } else {
            OPENOLT_LOG(WARNING, openolt_log_id, "untagged packets other than lldp packets are dropped. ethertype=%d, intftype=%d, intf_id=%d\n",
                        ntohs(ethernetLayer->getEthHeader()->etherType), intf_type, intf_id);
            return false;
        }
    } else {
        ethType = ntohs(vlanLayer->getVlanHeader()->etherType);
        if (ethType == EAP_ETH_TYPE) { // single tagged packet with EAPoL payload
            vlan_id = vlanLayer->getVlanID();
            pkt_type = eap;
        } else if (ethType == PPPoED_ETH_TYPE) { // single tagged packet with PPPOeD payload
            vlan_id = vlanLayer->getVlanID();
            pkt_type = pppoed;
        } else if (ethType == IPV4_ETH_TYPE) { // single tagged packet with IPv4 payload
            vlan_id = vlanLayer->getVlanID();
            vlanLayer->parseNextLayer();
            pcpp::IPv4Layer *ipv4Layer = (pcpp::IPv4Layer*)vlanLayer->getNextLayer();
            if(ipv4Layer->getIPv4Header()->protocol == UDP_PROTOCOL) { // UDP payload
                // Check the UDP Ports to see if it is a DHCPv4 packet
                ipv4Layer->parseNextLayer();
                pcpp::UdpLayer *udpLayer = (pcpp::UdpLayer*)ipv4Layer->getNextLayer();
                if (ntohs(udpLayer->getUdpHeader()->portSrc) ==  DHCP_SERVER_SRC_PORT|| ntohs(udpLayer->getUdpHeader()->portSrc) == DHCP_CLIENT_SRC_PORT) {
                    pkt_type = dhcpv4;
                } else {
                    OPENOLT_LOG(ERROR, openolt_log_id, "unsupported udp source port = %d\n", ntohs(udpLayer->getUdpHeader()->portSrc));
                    return false;
                }
            } else if (ipv4Layer->getIPv4Header()->protocol == IGMPv4_PROTOCOL) { // Igmpv4 payload
                pkt_type = igmpv4;
            } else {
                OPENOLT_LOG(ERROR, openolt_log_id, "unsupported ip protocol = %d\n", ipv4Layer->getIPv4Header()->protocol);
                return false;
            }
        } else if (ethType == VLAN_ETH_TYPE) { // double tagged packet

            // Trap-to-host from NNI flows do not specify the VLANs, so no vlan validation is necessary.
            if (intf_type == BCMOLT_INTERFACE_TYPE_NNI) {
                return true;
            }

            // Here we parse the inner vlan payload and currently support only IPv4 packets

            // Extract the vlan_id for trap-to-host packets arriving from the PON
            // trap-to-host ACLs from the NNI do not care about VLAN.
            if (intf_type == BCMOLT_INTERFACE_TYPE_PON) {
                vlan_id = vlanLayer->getVlanID(); // This is the outer vlan id
            }
            vlanLayer->parseNextLayer();
            vlanLayer = (pcpp::VlanLayer*)vlanLayer->getNextLayer(); // Here we extract the inner vlan layer
            ethType = ntohs(vlanLayer->getVlanHeader()->etherType);
            if (ethType == IPV4_ETH_TYPE) { // IPv4
                uint16_t _inner_vlan_id = vlanLayer->getVlanID();
                vlanLayer->parseNextLayer();
                pcpp::IPv4Layer *ipv4Layer = (pcpp::IPv4Layer*)vlanLayer->getNextLayer(); // here we extract the inner vlan IPv4 payload
                if(ipv4Layer->getIPv4Header()->protocol == UDP_PROTOCOL) { // UDP payload
                    // Check the UDP Ports to see if it is a DHCPv4 packet
                    ipv4Layer->parseNextLayer();
                    pcpp::UdpLayer *udpLayer = (pcpp::UdpLayer*)ipv4Layer->getNextLayer();
                    if (ntohs(udpLayer->getUdpHeader()->portSrc) == DHCP_SERVER_SRC_PORT || ntohs(udpLayer->getUdpHeader()->portSrc) == DHCP_CLIENT_SRC_PORT) {
                        pkt_type = dhcpv4;
                    } else {
                        OPENOLT_LOG(ERROR, openolt_log_id, "unsupported udp source port = %d\n", ntohs(udpLayer->getUdpHeader()->portSrc));
                        return false;
                    }
                } else if (ipv4Layer->getIPv4Header()->protocol == IGMPv4_PROTOCOL) { // Igmpv4 payload
                    pkt_type = igmpv4;
                } else {
                    OPENOLT_LOG(ERROR, openolt_log_id, "unsupported ip protocol = %d\n", ipv4Layer->getIPv4Header()->protocol)
                    return false;
                }
            }
        } else {
            OPENOLT_LOG(ERROR, openolt_log_id, "unsupported ether type = 0x%x\n", ntohs((vlanLayer->getVlanHeader()->etherType)));
            return false;
        }
    }

#if 0 // Debug logs for test purpose only
    std::cout << "vlan of received packet " << vlan_id << " intf_type " << intf_type << " intf_id " <<intf_id << " pkt_type " <<pkt_type << " gem_port_id" << gemport_id << "\n";
    for(std::map<trap_to_host_pkt_info, std::list<uint16_t> >::const_iterator it = trap_to_host_vlan_ids_for_trap_to_host_pkt_info.begin();
        it != trap_to_host_vlan_ids_for_trap_to_host_pkt_info.end(); ++it)
    {
        std::cout << "value entries" << " " << std::get<0>(it->first) << " "<< std::get<1>(it->first) << " "<< std::get<2>(it->first) << " "<< std::get<3>(it->first) << "\n\n";
        std::cout << "vlans for the above key are => ";
        for (std::list<uint16_t>::const_iterator _it=it->second.begin();
             _it != it->second.end();
             ++_it) {
            std::cout << *_it << " ";
        }
        std::cout << "\n\n";
    }
#endif

    trap_to_host_pkt_info pkt_info(intf_type, intf_id, pkt_type, gemport_id);
    // Check for matching vlan only if the trap_to_host_pkt_info exists in the trap_to_host_vlan_ids_for_trap_to_host_pkt_info map
    if (trap_to_host_vlan_ids_for_trap_to_host_pkt_info.count(pkt_info) > 0) {
        // Iterate throught the vlan list to find matching vlan
        auto& vlan_id_list = trap_to_host_vlan_ids_for_trap_to_host_pkt_info[pkt_info];
        for (auto allowed_vlan_id : vlan_id_list) {
            // Found exact matching vlan in the allowed list of vlans for the trap_to_host_pkt_info key or
            // there is generic match ANY_VLAN in the list in the allowed vlan list.
            if (allowed_vlan_id == vlan_id || allowed_vlan_id == ANY_VLAN) {
                return true;
            }
        }
    }
    return false;
}

std::pair<grpc_ssl_client_certificate_request_type, bool> get_grpc_tls_option(const char* tls_option) {
    static const std::map<std::string,grpc_ssl_client_certificate_request_type> grpc_security_option_map = {{"GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE",
                                                                                                        grpc_ssl_client_certificate_request_type::GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE},
                                                                                                      {"GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_BUT_DONT_VERIFY",
                                                                                                        grpc_ssl_client_certificate_request_type::GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_BUT_DONT_VERIFY},
                                                                                                      {"GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_AND_VERIFY",
                                                                                                        grpc_ssl_client_certificate_request_type::GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_AND_VERIFY},
                                                                                                      {"GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_BUT_DONT_VERIFY",
                                                                                                        grpc_ssl_client_certificate_request_type::GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_BUT_DONT_VERIFY},
                                                                                                      {"GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY",
                                                                                                        grpc_ssl_client_certificate_request_type::GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY}};

    auto it = grpc_security_option_map.find(tls_option);
    if (it == grpc_security_option_map.end()) {
        OPENOLT_LOG(ERROR, openolt_log_id, "invalid gRPC Server security option: %s\n", tls_option);
        return {grpc_ssl_client_certificate_request_type::GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE, false};
    } else {
        OPENOLT_LOG(INFO, openolt_log_id, "valid gRPC Server security option: %s\n", tls_option);
        tls_option_arg = std::string{tls_option};
        return {it->second, true};
    }
}

const std::string &get_grpc_tls_option() {
    return tls_option_arg;
}

bool is_grpc_secure() {
    return !tls_option_arg.empty();
}

std::pair<std::string, bool> read_from_txt_file(const std::string& file_name) {
    std::ifstream in_file(file_name);

    if (!in_file.is_open()) {
        OPENOLT_LOG(ERROR, openolt_log_id, "error opening file '%s'\n", file_name.c_str());
        return {"", false};
    }

    std::stringstream buffer;
    buffer << in_file.rdbuf();

    return {buffer.str(), in_file.good()};
}

bool save_to_txt_file(const std::string& file_name, const std::string& content) {
    std::ofstream out_file;
    out_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    try {
        out_file.open(file_name, std::ios::out | std::ios::trunc);

        if (!out_file.is_open()) {
            std::cerr << "error while opening file '" << file_name << "'\n";
            return false;
        }

        out_file << content;
        out_file.close();

        return true;
    } catch (const std::ofstream::failure& e) {
        std::cerr << "exception while writing to file '" << file_name << "' | err: " << e.what() << '\n';
        return false;
    }
}

pair<string, bool> hex_to_ascii_string(unsigned char* ptr, int length) {
    // initialize the ASCII code string as empty.
    string ascii = "";
    for (size_t i = 0; i < length; i ++)
    {
        string part = string(1,ptr[i]);
        ascii += part;
    }
    return {ascii, true};
}

pair<uint32_t, bool> hex_to_uinteger(unsigned char *ptr, int length) {
    if (length > 8) {
        perror("invalid length of bytes for conversion to uint\n");
        return {0, false};
    }
    uint32_t res = 0;
    for (int i = 0; i < length; i++)
    {
        res = uint32_t(ptr[i]) * pow(2, (length - 1 - i) * 8) + res;
    }
    return {res, true};
}

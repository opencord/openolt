/*
 * Copyright 2018-present Open Networking Foundation
 *
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

#ifndef BCMOLT_API_MODEL_API_STRUCTS_TEST_H_
#define BCMOLT_API_MODEL_API_STRUCTS_TEST_H_

#include <bcmos_system.h>
#include <bcmolt_system_types.h>
#include "bcmolt_msg.h"
#include "bcmolt_api_model_supporting_structs.h"
#include "bcmolt_api_model_supporting_enums.h"
#include "bcmolt_api_model_supporting_typedefs.h"

/**
This header file provides STUB definitions of missing BAL structs for unit test compilation
*/


// ITU PON Statistics
typedef struct
{
    uint64_t presence_mask;
    uint64_t rdi_errors;
} bcmolt_onu_itu_pon_stats_data;

typedef struct
{
    bcmolt_stat hdr;
    bcmolt_onu_key key;
    bcmolt_onu_itu_pon_stats_data data;
} bcmolt_onu_itu_pon_stats;



// onu_itu_pon_stats_data'
typedef enum
{
    BCMOLT_ONU_ITU_PON_STATS_DATA_ID_RDI_ERRORS = 1,
    BCMOLT_ONU_ITU_PON_STATS_CFG_DATA_ID_BIP_ERRORS = 2

    #define bcmolt_onu_itu_pon_stats_data_id_rdi_errors BCMOLT_ONU_ITU_PON_STATS_DATA_ID_RDI_ERRORS
    #define bcmolt_onu_itu_pon_stats_cfg_data_id_bip_errors BCMOLT_ONU_ITU_PON_STATS_CFG_DATA_ID_BIP_ERRORS

} bcmolt_onu_itu_pon_stats_data_id;

// onu_itu_pon_stats_cfg_data
typedef enum
{

    BCMOLT_ONU_ITU_PON_STATS_CFG_DATA_ID_RDI_ERRORS = 1

    #define bcmolt_onu_itu_pon_stats_cfg_data_id_rdi_errors BCMOLT_ONU_ITU_PON_STATS_CFG_DATA_ID_RDI_ERRORS

} bcmolt_onu_itu_pon_stats_cfg_data_id;

//onu_itu_pon_stats_alarm_raised_data
typedef enum
{
    BCMOLT_ONU_ITU_PON_STATS_ALARM_RAISED_DATA_ID_STAT = 0
    #define bcmolt_onu_itu_pon_stats_alarm_raised_data_id_stat BCMOLT_ONU_ITU_PON_STATS_ALARM_RAISED_DATA_ID_STAT

} bcmolt_onu_itu_pon_stats_alarm_raised_data_id;

// onu_itu_pon_stats_alarm_cleared_data
typedef enum
{
    BCMOLT_ONU_ITU_PON_STATS_ALARM_CLEARED_DATA_ID_STAT = 0
    #define bcmolt_onu_itu_pon_stats_alarm_cleared_data_id_stat BCMOLT_ONU_ITU_PON_STATS_ALARM_CLEARED_DATA_ID_STAT

} bcmolt_onu_itu_pon_stats_alarm_cleared_data_id;


/** ONU: ITU PON Statistics Alarm Raised */
typedef struct
{
    bcmolt_presence_mask presence_mask;
    bcmolt_onu_itu_pon_stats_data_id stat;
} bcmolt_onu_itu_pon_stats_alarm_raised_data;

// itu_pon_stats_alarm_raised" group of "onu" object
typedef struct
{
    bcmolt_auto hdr;
    bcmolt_onu_key key;
    bcmolt_onu_itu_pon_stats_alarm_raised_data data;
} bcmolt_onu_itu_pon_stats_alarm_raised;

// ITU PON Statistics Alarm Cleared
typedef struct
{
    uint64_t presence_mask;
    bcmolt_onu_itu_pon_stats_data_id stat;
} bcmolt_onu_itu_pon_stats_alarm_cleared_data;

// itu_pon_stats_alarm_cleared" group of "onu" object
typedef struct
{
    bcmolt_auto hdr;
    bcmolt_onu_key key;
    bcmolt_onu_itu_pon_stats_alarm_cleared_data data;
} bcmolt_onu_itu_pon_stats_alarm_cleared;


// ITU PON Statistics Configuration
typedef struct
{
    uint64_t presence_mask;
    bcmolt_stat_alarm_config rdi_errors;
    bcmolt_stat_alarm_config bip_errors;

} bcmolt_onu_itu_pon_stats_cfg_data;

// itu_pon_stats_cfg" group of "onu" object
typedef struct
{
    bcmolt_stat_cfg hdr;
    bcmolt_onu_key key;
    bcmolt_onu_itu_pon_stats_cfg_data data;
} bcmolt_onu_itu_pon_stats_cfg;

#define BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_CLEARED 1
#define BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_RAISED 2
#define BCMOLT_ONU_STAT_SUBGROUP_ITU_PON_STATS 3
#define BCMOLT_ONU_STAT_CFG_SUBGROUP_ITU_PON_STATS_CFG 4

#define bcmolt_onu_auto_subgroup_itu_pon_stats_alarm_cleared BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_CLEARED
#define bcmolt_onu_auto_subgroup_itu_pon_stats_alarm_raised BCMOLT_ONU_AUTO_SUBGROUP_ITU_PON_STATS_ALARM_RAISED
#define bcmolt_onu_stat_subgroup_itu_pon_stats BCMOLT_ONU_STAT_SUBGROUP_ITU_PON_STATS
#define bcmolt_onu_stat_cfg_subgroup_itu_pon_stats_cfg BCMOLT_ONU_STAT_CFG_SUBGROUP_ITU_PON_STATS_CFG

#endif // BCMOLT_API_MODEL_API_STRUCTS_TEST_H_


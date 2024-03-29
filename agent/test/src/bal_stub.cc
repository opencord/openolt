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

// This file stub definitions for some BAL APIs that are unavailable
// in TEST_MODE
//
extern "C" {
#include <test_stub.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <bcmos_system.h>
#include <bcmolt_msg.h>
#include <bcmolt_host_api.h>

char log_string[500];
dev_log_id def_log_id=0;

void bcmos_usleep(uint32_t us) {
    // let always sleep for 10ms irrespective of the value passed.
    usleep (10000);
}

void bcmos_free(void *ptr) {
    if (ptr) {
       free(ptr);
    }
}

/* Initialize API layer */
bcmos_errno bcmolt_api_init(void)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Set configuration */
/*bcmos_errno bcmolt_cfg_set(bcmolt_oltid olt, bcmolt_cfg *cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}*/

/* Get configuration */
/*
bcmos_errno bcmolt_cfg_get(bcmolt_oltid olt, bcmolt_cfg *cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}
*/

/* Clear configuration */
/*
bcmos_errno bcmolt_cfg_clear(bcmolt_oltid olt, bcmolt_cfg *cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}
*/

/* Get statistics */
bcmos_errno bcmolt_stat_get(bcmolt_oltid olt, bcmolt_stat *stat, bcmolt_stat_flags flags)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Get statistics configuration */
bcmos_errno bcmolt_stat_cfg_get(bcmolt_oltid olt, bcmolt_stat_cfg *cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Register Autonomous Indication Message Handler */
bcmos_errno bcmolt_ind_subscribe(bcmolt_oltid olt, bcmolt_rx_cfg *rx_cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Un-register Autonomous Indication Message Handler registered by bcmolt_ind_subscribe() */
bcmos_errno bcmolt_ind_unsubscribe(bcmolt_oltid olt, bcmolt_rx_cfg *rx_cfg)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Submit Operation */
/*
bcmos_errno bcmolt_oper_submit(bcmolt_oltid olt, bcmolt_oper *oper)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}
*/

/* Get configuration of multiple objects */
bcmos_errno bcmolt_multi_cfg_get(
    bcmolt_oltid olt,
    bcmolt_multi_cfg *cfg,
    bcmolt_filter_flags filter_flags)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Get statistics of multiple objects */
bcmos_errno bcmolt_multi_stat_get(
    bcmolt_oltid olt,
    bcmolt_multi_stat *stat,
    bcmolt_stat_flags stat_flags,
    bcmolt_filter_flags filter_flags)
{
    printf("-- entering :stubbed %s\n", __FUNCTION__);
    return BCM_ERR_OK;
}

/* Map error code to error string */
const char *bcmos_strerror(bcmos_errno err)
{
    static const char *errstr[] = {
        [-BCM_ERR_OK]                        = "OK",
        [-BCM_ERR_IN_PROGRESS]               = "In progress",
        [-BCM_ERR_PARM]                      = "Error in parameters",
        [-BCM_ERR_NOMEM]                     = "No memory",
        [-BCM_ERR_NORES]                     = "No resources",
        [-BCM_ERR_INTERNAL]                  = "Internal error",
        [-BCM_ERR_NOENT]                     = "Entry doesn't exist",
        [-BCM_ERR_NODEV]                     = "Device doesn't exist",
        [-BCM_ERR_ALREADY]                   = "Entry already exists/already in requested state",
        [-BCM_ERR_RANGE]                     = "Out of range",
        [-BCM_ERR_PERM]                      = "No permission to perform an operation",
        [-BCM_ERR_NOT_SUPPORTED]             = "Operation is not supported",
        [-BCM_ERR_PARSE]                     = "Parsing error",
        [-BCM_ERR_INVALID_OP]                = "Invalid operation",
        [-BCM_ERR_IO]                        = "I/O error",
        [-BCM_ERR_STATE]                     = "Object is in bad state",
        [-BCM_ERR_DELETED]                   = "Object is deleted",
        [-BCM_ERR_TOO_MANY]                  = "Too many objects",
        [-BCM_ERR_NO_MORE]                   = "No more entries",
        [-BCM_ERR_OVERFLOW]                  = "Buffer overflow",
        [-BCM_ERR_COMM_FAIL]                 = "Communication failure",
        [-BCM_ERR_NOT_CONNECTED]             = "No connection with the target system",
        [-BCM_ERR_SYSCALL_ERR]               = "System call returned error",
        [-BCM_ERR_MSG_ERROR]                 = "Received message is insane",
        [-BCM_ERR_TOO_MANY_REQS]             = "Too many outstanding requests",
        [-BCM_ERR_TIMEOUT]                   = "Operation timed out",
        [-BCM_ERR_TOO_MANY_FRAGS]            = "Too many fragments",
        [-BCM_ERR_NULL]                      = "Got NULL pointer",
        [-BCM_ERR_READ_ONLY]                 = "Attempt to set read-only parameter",
        [-BCM_ERR_ONU_ERR_RESP]              = "ONU returned an error response",
        [-BCM_ERR_MANDATORY_PARM_IS_MISSING] = "Mandatory parameter is missing",
        [-BCM_ERR_KEY_RANGE]                 = "Key field out of range",
        [-BCM_ERR_QUEUE_EMPTY]               = "Rx of PCIe empty",
        [-BCM_ERR_QUEUE_FULL]                = "Tx of PCIe full",
        [-BCM_ERR_TOO_LONG]                  = "Processing is taking too long, but will finish eventually",
        [-BCM_ERR_INSUFFICIENT_LIST_MEM]     = "Insufficient list memory provided",
        [-BCM_ERR_OUT_OF_SYNC]               = "Sequence number or operation step was out of sync",
        [-BCM_ERR_CHECKSUM]                  = "Checksum error",
        [-BCM_ERR_IMAGE_TYPE]                = "Unsupported file/image type",
        [-BCM_ERR_INCOMPLETE_TERMINATION]    = "Incomplete premature termination",
        [-BCM_ERR_MISMATCH]                  = "Parameters mismatch",
    };
    static const char *unknown = "*unknown*";

    if ((unsigned)(-err) >= sizeof(errstr)/sizeof(errstr[0]) || !errstr[-err])
        return unknown;
    return errstr[-err];
}

void bcmolt_msg_free(bcmolt_msg *msg)
{
    return;
}

void bcmolt_api_set_prop_present(bcmolt_msg *msg, const void *prop_ptr)
{
    return;
}

const bcmolt_enum_val bcmolt_obj_id_string_table[] = {"dummy string, never used"};
const bcmolt_enum_val bcmolt_interface_state_string_table[] = {"dummy string, never used"};

dev_log_id bcm_dev_log_id_register(const char *xi_name,
     bcm_dev_log_level xi_default_log_level,
     bcm_dev_log_id_type xi_default_log_type) {
    return 0;
}
bool bcmcli_is_stopped(bcmcli_session *sess) {
    printf("-- stub bcmcli_is_stopped called --\n");
    return true;
}

bool bcmcli_parse(bcmcli_session *sess, char *s) {
    printf("--  stub bcmcli_parse called --\n");
    return true;
}

bool bcmcli_driver(bcmcli_session *sess) {
    printf("--  stub bcmcli_driver called --\n");
    return true;
}
void bcmcli_token_destroy(void *ptr) {
    printf("--  stub  bcmcli_token_destroy called --\n");
    return;
}

void bcmcli_session_close( bcmcli_session*ptr) {
    printf("--  stub bcmcli_session_close called --\n");
    return;
}

bcmos_errno bcm_api_cli_set_commands(bcmcli_session *sess) {
    printf("-- stub bcm_api_cli_set_commands called --\n");
    return BCM_ERR_OK;
}

void bcmcli_stop(bcmcli_session *sess) {
    printf("-- stub bcmcli_stop called --\n");
    return;
}

void bcmcli_session_print(bcmcli_session *sess, const char *s) {
    printf("-- stub bcmcli_session_print called --\n");
    return;
}

bcmos_errno bcmcli_session_open(bcmcli_session_parm *mon_sess, bcmcli_session **curr_sess) {
    printf("-- stub bcmcli_session_open called --\n");
    return BCM_ERR_OK;
}

void bcm_dev_log_log(dev_log_id xi_id,
    bcm_dev_log_level xi_log_level,
    uint32_t xi_flags,
    const char *fmt,
    ...) {
    memset(log_string, '\0', sizeof(log_string));
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_string, 490, fmt, args);
    switch (xi_log_level) {
        case DEV_LOG_LEVEL_FATAL:
            printf("FATAL: %s\n", log_string);
            // exit (0);
            break;
        case DEV_LOG_LEVEL_ERROR:
            printf("ERROR : %s\n", log_string);
            break;
        case DEV_LOG_LEVEL_WARNING:
            printf("WARNING : %s\n", log_string);
            break;
        case DEV_LOG_LEVEL_INFO:
            printf("INFO : %s\n", log_string);
            break;
        case DEV_LOG_LEVEL_DEBUG:
            printf("DEBUG : %s\n", log_string);
            break;
        default:
            printf("%s\n", log_string);
    }
    va_end(args);
}

bcmos_errno bcmos_task_query(const bcmos_task *task, bcmos_task_parm *parm) {
    printf (" -- stub bcmos_task_query called --\n");
    return BCM_ERR_ALREADY;
}

bcmos_errno bcmos_task_create(bcmos_task *task, const bcmos_task_parm *parm) {
    printf (" -- stub bcmos_task_create called --\n");
    return BCM_ERR_OK;
}

int bcmos_printf(const char *fmt, ...) {
    memset(log_string, '\0', sizeof(log_string));
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_string, 490, fmt, args);
    printf("%s\n", log_string);
    va_end(args);

}

bcmos_bool bcmolt_api_conn_mgr_is_connected(bcmolt_goid olt) {
    printf ("-- stub bcmolt_api_conn_mgr_is_connected called --\n");
    return true;
}

bcmos_errno bcmos_mutex_create(bcmos_mutex *mutex, uint32_t flags, const char *name)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
      printf("pthread_mutex_attr_init() error");
      exit(1);
    }
    pthread_mutex_init(&mutex->m, &attr);
    return BCM_ERR_OK;
}

/* Destroy mutex */
void bcmos_mutex_destroy(bcmos_mutex *mutex)
{
    pthread_mutex_destroy(&mutex->m);
}

/* Lock mutex */
void bcmos_mutex_lock(bcmos_mutex *mutex)
{
    pthread_mutex_lock(&mutex->m);
}

/* Release mutex */
void bcmos_mutex_unlock(bcmos_mutex *mutex)
{
    pthread_mutex_unlock(&mutex->m);
}

void *bcmos_alloc(uint32_t size)
{
    return malloc(size);
}

bcmos_errno bcmcli_cmd_add(bcmcli_entry *dir, const char *name, bcmcli_cmd_cb cmd_cb,
                  const char *help, bcmcli_access_right access_right,
                  const bcmcli_cmd_extra_parm *extras, bcmcli_cmd_parm parms[]) {
    return BCM_ERR_OK;
}

}

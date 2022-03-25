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
#ifndef OPENOLT_CORE_UTILS_H_
#define OPENOLT_CORE_UTILS_H_
#include <string>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "core.h"
#include "core_data.h"
#include "error_format.h"
#include <grpc/grpc_security_constants.h>

extern "C"
{
#include <bcmolt_api.h>
#include <bcmolt_host_api.h>
#include <bcmolt_api_model_supporting_enums.h>
#include <bcmolt_api_model_supporting_structs.h>

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


std::string serial_number_to_str(bcmolt_serial_number* serial_number);
std::string vendor_specific_to_str(const char* const serial_number);
uint16_t get_dev_id(void);
int get_default_tm_sched_id(int intf_id, std::string direction);
uint32_t get_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id);
void free_tm_sched_id(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id);
bool is_tm_sched_id_present(int pon_intf_id, int onu_id, int uni_id, std::string direction, int tech_profile_id);
bool check_tm_qmp_equality(std::vector<uint32_t> tmq_map_profileA, std::vector<uint32_t> tmq_map_profileB);
std::string* get_valid_queues_pbit_map(std::string *queues_pbit_map, uint32_t size);
std::vector<uint32_t> get_tmq_map_profile(std::string *queues_pbit_map, uint32_t *queues_priority_q, uint32_t size);
int get_tm_qmp_id(std::vector<uint32_t> tmq_map_profile);
void update_sched_qmp_id_map(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id,
                             uint32_t uni_id, int tm_qmp_id);
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id);
int get_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id,
                  std::vector<uint32_t> tmq_map_profile);
bool free_tm_qmp_id(uint32_t sched_id,uint32_t pon_intf_id, uint32_t onu_id,
                    uint32_t uni_id, int tm_qmp_id);
int get_acl_id();
void free_acl_id (int acl_id);
uint16_t get_flow_id();
bool get_flow_ids(int num_of_flow_ids, uint16_t *flow_ids);
void free_flow_id (uint16_t flow_id);
void free_flow_ids(uint8_t num_flows, uint16_t *flow_ids);
std::string get_qos_type_as_string(bcmolt_egress_qos_type qos_type);
bcmolt_egress_qos_type get_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id, uint32_t queue_size=0);
void clear_qos_type(uint32_t pon_intf_id, uint32_t onu_id, uint32_t uni_id);
std::string GetDirection(int direction);
bcmos_errno wait_for_alloc_action(uint32_t intf_id, uint32_t alloc_id, AllocCfgAction action);
bcmos_errno wait_for_gem_action(uint32_t intf_id, uint32_t gem_port_id, GemCfgAction action);
bcmos_errno wait_for_onu_deactivate_complete(uint32_t intf_id, uint32_t onu_id);
char* openolt_read_sysinfo(const char* field_name, char* field_val);
Status pushOltOperInd(uint32_t intf_id, const char *type, const char *state);
void openolt_cli_get_prompt_cb(bcmcli_session *session, char *buf, uint32_t max_len);
int _bal_apiend_cli_thread_handler(long data);
bcmos_errno bcm_openolt_api_cli_init(bcmcli_entry *parent_dir, bcmcli_session *session);
bcmos_errno bcm_cli_quit(bcmcli_session *session, const bcmcli_cmd_parm parm[], uint16_t n_parms);
int get_status_bcm_cli_quit(void);
bcmos_errno bcmolt_apiend_cli_init();
bcmos_errno get_pon_interface_status(bcmolt_interface pon_ni, bcmolt_interface_state *state, bcmolt_status *los_status);
bcmos_errno get_onu_state(bcmolt_interface pon_ni, int onu_id, bcmolt_onu_state *onu_state);
bcmos_errno bcmolt_cfg_get_mult_retry(bcmolt_oltid olt, bcmolt_cfg *cfg);
unsigned NumNniIf_();
unsigned NumPonIf_();
bcmos_errno get_nni_interface_status(bcmolt_interface id, bcmolt_interface_state *state);
bcmos_errno get_nni_interface_speed(bcmolt_interface id, uint32_t *speed);
Status install_gem_port(int32_t intf_id, int32_t onu_id, int32_t uni_id, int32_t gemport_id, std::string board_technology);
Status remove_gem_port(int32_t intf_id, int32_t onu_id, int32_t uni_id, int32_t gemport_id, std::string board_technology);
Status enable_encryption_for_gem_port(int32_t intf_id, int32_t gemport_id, std::string board_technology);
Status update_acl_interface(int32_t intf_id, bcmolt_interface_type intf_type, uint32_t access_control_id,
                bcmolt_members_update_command acl_cmd);
Status install_acl(const acl_classifier_key acl_key);
Status remove_acl(int acl_id);
void formulate_acl_classifier_key(acl_classifier_key *key, const ::openolt::Classifier& classifier);
Status handle_acl_rule_install(int32_t onu_id, uint64_t flow_id, int32_t gemport_id,
                               const std::string flow_type, int32_t access_intf_id,
                               int32_t network_intf_id,
                               const ::openolt::Classifier& classifier);
void clear_gem_port(int gemport_id, int access_intf_id);
Status handle_acl_rule_cleanup(int16_t acl_id, int32_t intf_id, const std::string flow_type);
Status check_bal_ready();
Status check_connection();
std::string get_ip_address(const char* nw_intf);
bcmos_errno getOnuMaxLogicalDistance(uint32_t intf_id, uint32_t *mld);
char* get_intf_mac(const char* intf_name, char* mac_address, unsigned int max_size_of_mac_address);
void update_voltha_flow_to_cache(uint64_t voltha_flow_id, device_flow dev_flow);
void remove_voltha_flow_from_cache(uint64_t voltha_flow_id);
bool is_voltha_flow_installed(uint64_t voltha_flow_id );
const device_flow* get_device_flow(uint64_t voltha_flow_id);
const device_flow_params* get_device_flow_params(uint64_t voltha_flow_id);
trap_to_host_packet_type get_trap_to_host_packet_type(const ::openolt::Classifier& classifier);
bool is_packet_allowed(bcmolt_access_control_receive_eth_packet_data *data, int32_t gemport_id);
std::pair<grpc_ssl_client_certificate_request_type, bool> get_grpc_tls_option(const char* tls_option);
const std::string &get_grpc_tls_option();
bool is_grpc_secure();
std::pair<std::string, bool> read_from_txt_file(const std::string& file_name);
bool save_to_txt_file(const std::string& file_name, const std::string& content);
bcmos_errno get_gem_obj_state(bcmolt_interface pon_ni, bcmolt_gem_port_id id, bcmolt_activation_state *state);
bcmos_errno get_alloc_obj_state(bcmolt_interface pon_ni, bcmolt_alloc_id id, bcmolt_activation_state *state);
pair<string, bool> hex_to_ascii_string(unsigned char* ptr, int length);
pair<uint32_t, bool> hex_to_uinteger(unsigned char *ptr, int length);
#endif // OPENOLT_CORE_UTILS_H_

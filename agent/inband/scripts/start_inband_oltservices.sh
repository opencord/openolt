#!/bin/bash

#Copyright 2018-present Open Networking Foundation
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

### BEGIN INIT INFO
# Provides:          start_inband_oltservices.sh
# Required-Start:    $all
# Required-Stop:     $network $local_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Sets up inband management channel and starts device management and Openolt services
# Description:
#                    This script does the following things
#                    1) Extracts bal package and keep the files in appropriate paths.
#                    2) Starts svk_init which installs required drivers needed by the MAC devices
#                    3) Starts device management daemon which initializes all maple, qumran chip sets
#                       and brings up access terminal.
#                    4) Sets up inband management channel
#                    5) Creates inband vlan tagged interface which will be used to communicate to
#                       Openolt service from vOLTHA via NNI
#                    6) Sets up dhcp-client-identifier in "/etc/dhcp/dhclient.conf" for each inband
#                       tagged interface so that it can obtain IP address from DHCP server
#                    7) Sets up DHCP IP configuration in "/etc/network/interfaces" for inband tagged
#                       interface so that it can initiate DHCPREQUEST to DHCP server once network
#                       restart triggered.
#                    8) Starts openolt service which enables all PON and NNI interfaces and creates
#                       respective PON and NNI schedulers and Queues.
### END INIT INFO

#------------------------------------------------------------------------------
# GLOBAL VARIABLES
#------------------------------------------------------------------------------

# Root path where required bal directories are located
BRCM_OPT_DIR='/opt/bcm68620'
BRCM_DIR='/broadcom'

# olt service files
SVK_INIT_FILE="${BRCM_OPT_DIR}/svk_init.sh"

# inband config file
INBAND_CONFIG_FILE="${BRCM_DIR}/inband.config"
DHCLIENT_CONF="/etc/dhcp/dhclient.conf"

# olt serial number
SERIAL_NUMBER="/sys/devices/virtual/dmi/id/product_serial"

OLT_MODEL=$(cat /sys/devices/virtual/dmi/id/board_name)
NETWORK_INTERFACE="/etc/network/interfaces"
BAL_OPENOLT_DEB_16="/openolt_asfvolt16.deb"
BAL_OPENOLT_DEB_64="/openolt_asgvolt64.deb"

DEV_MGMT_DAEMON="dev_mgmt_daemon -d -pcie"
OPENOLT="openolt"
ASFVOLT16="asfvolt16"
ASGVOLT64="asgvolt64"
ASF16_MODEL="ASXvOLT16"
EDGECORE="edgecore"

# vlan id for asfvolt16
ASFVOLT16_VLAN_ID_ETH2=

# vlan id for asgvolt64
ASGVOLT64_VLAN_ID_ETH1=

# File used to set/get argument to openolt service
OPENOLT_ARG_INPUT_FILE=/etc/default/openolt

#------------------------------------------------------------------------------
# Function Name: does_logger_exist
# Description:
#    This function check if logger exist and executable.
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    returns 0 if exist and executable else 1
#------------------------------------------------------------------------------
does_logger_exist() {
    cmd=/usr/bin/logger
    if [ -x ${cmd} ]; then
        return 0
    else
        return 1
    fi
}

#------------------------------------------------------------------------------
# Function Name: info_message
# Description:
#    This function print the info message information to the console
#
# Globals:
#    None
#
# Arguments:
#    string message
#
# Returns:
#    None
#------------------------------------------------------------------------------
info_message() {
    echo "INFO: $1"
    logger -p user.info "$1"
}

#------------------------------------------------------------------------------
# Function Name: error_message
# Description:
#    This function print the error message information to the console
#
# Globals:
#    None
#
# Arguments:
#    string message
#
# Returns:
#    returns 1
#------------------------------------------------------------------------------
error_message() {
    echo "ERROR: $1"
    logger -p user.err "$1"
    return 1
}

#------------------------------------------------------------------------------
# Function Name: get_vlan_ids
# Description:
#    This function facilitates to fetch vlan id from inband configuration file
#    located at /broadcom/inband.config
#
# Globals:
#    INBAND_CONFIG_FILE, ASFVOLT16_VLAN_ID_ETH2, ASFVOLT16_VLAN_ID_ETH3,
#    ASGVOLT64_VLAN_ID_ETH1, ASGVOLT64_VLAN_ID_ETH2
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
get_vlan_ids() {
    # Read inband.config file to fetch vlan id information
    if [ -f ${INBAND_CONFIG_FILE} ]; then
        ASFVOLT16_VLAN_ID_ETH2=$(awk '/asfvolt16_vlan_id_eth2/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')
        ASGVOLT64_VLAN_ID_ETH1=$(awk '/asgvolt64_vlan_id_eth1/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')
        if [ -z ${ASFVOLT16_VLAN_ID_ETH2} ] || [ -z ${ASGVOLT64_VLAN_ID_ETH1} ]; then
            error_message "ERROR: vlan ids not valid"
            exit 1
        fi
    else
        error_message "ERROR: ${INBAND_CONFIG_FILE} not found, using default value 4093"
    fi
}

#------------------------------------------------------------------------------
# Function Name: is_out_band_connection_enabled
# Description:
#    This function checks if out-of-band connection is enabled by reading
#    the enable_out_of_band_connection value in file /broadcom/inband.config
#
# Globals:
#
# Arguments:
#    None
#
# Returns:
#    true if out-of-band connection enabled, else false
#------------------------------------------------------------------------------
is_out_band_connection_enabled() {
    # Read inband.config file to fetch configurtion to enable or not the out-of-band connection to the OLT.
    if [ -f ${INBAND_CONFIG_FILE} ]; then
        ob_cfg=$(awk '/enable_out_of_band_connection/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')
        if [ -z ${ob_cfg} ]; then
            error_message "ERROR: missing configuration to enable out-of-band connection to OLT. Default to false"
            echo "no"
        elif [ "${ob_cfg}" = "yes" ]; then
            echo "yes"
        elif [ "${ob_cfg}" = "no" ]; then
            echo "no"
        else
            error_message "ERROR: Invalid configuration to enable out-of-band connection -> ${ob_cfg}"
            echo "no"
        fi
    else
        echo "no"
    fi
}

#------------------------------------------------------------------------------
# Function Name: setup_nw_configuration
# Description:
#    This function read the "/broadcom/inband.config" file to get VLAND IDs
#    for the interface eth1 and eth2 based on the OLT model  and update
#    these VLAN ID to /etc/network/interfaces file for dhcp request.
# Globals:
#    INBAND_CONFIG_FILE, ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
setup_nw_configuration() {
    # Dynamic vlan entry in /etc/network/interfaces file
    local is_out_band=$(is_out_band_connection_enabled)
    if [ "${is_out_band}" = "yes" ]; then
        # This interface is used for out-of-band connection for the OLT
        # This is not a mandatory requirement for in-band management of the OLT
        set_dhcp_ip_configuration ma1
    fi

    # These interfaces are used for in-band management of the OLT
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        set_dhcp_ip_configuration eth2 ${ASFVOLT16_VLAN_ID_ETH2}
    else
        set_dhcp_ip_configuration eth1 ${ASGVOLT64_VLAN_ID_ETH1}
    fi
    # Restart the networking services
    /etc/init.d/networking restart
}

#------------------------------------------------------------------------------
# Function Name: set_dhcp_ip_configuration
# Description:
#    This function facilitates setup_nw_configuration function and accepts interface
#    vlan id as an argumnet to modify network interfaces file
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
set_dhcp_ip_configuration() {
    interface=$1
    vlan_id=$2
    grep -q "iface ${interface}.${vlan_id}" $NETWORK_INTERFACE
    if [ $? -ne 0 ]; then
        if [ -z ${vlan_id} ]; then
            echo "auto ${interface}" >>${NETWORK_INTERFACE}
            echo "iface ${interface} inet dhcp" >>${NETWORK_INTERFACE}
        else
            echo "auto ${interface}.${vlan_id}" >>${NETWORK_INTERFACE}
            echo "iface ${interface}.${vlan_id} inet dhcp" >>${NETWORK_INTERFACE}
        fi
    fi
}

#------------------------------------------------------------------------------
# Function Name: disable_autostart_of_openolt_and_dev_mgmt_daemon_processes
# Description:
#    Disables autostart of openolt processes (openolt and dev_mgmt_daemon).
#    The start of these openolt processes is now controlled through this script
#
# Globals:
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
disable_autostart_of_openolt_and_dev_mgmt_daemon_processes() {
    update-rc.d dev_mgmt_daemon disable
    update-rc.d openolt disable
}

#------------------------------------------------------------------------------
# Function Name: stop_openolt_and_dev_mgmt_daemon_processes
# Description:
#    Stop openolt processes (openolt and dev_mgmt_daemon) if they were running
#    before
#
# Globals:
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
stop_openolt_and_dev_mgmt_daemon_processes() {
    service dev_mgmt_daemon stop
    service openolt stop
}

#------------------------------------------------------------------------------
# Function Name: start_openolt_dev_mgmt_daemon_process_watchdog
# Description:
#    Start openolt and dev_mgmt_daemon process watchdog
#
# Globals:
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
start_openolt_dev_mgmt_daemon_process_watchdog() {
    nohup bash /opt/openolt/openolt_dev_mgmt_daemon_process_watchdog &
}

#------------------------------------------------------------------------------
# Function Name: start_dev_mgmt_service
# Description:
#    This function starts svk_init.sh script and device management service.
#    Device management service initializes all maple, qumran chip sets and
#    brings up access terminal.
#
# Globals:
#    SVK_INIT_FILE, BRCM_DIR, DEV_MGMT_DAEMON
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
start_dev_mgmt_service() {
    info_message "Starting device management service"
    chmod +x ${SVK_INIT_FILE}
    ${SVK_INIT_FILE}

    # starts the device_management deamon and openolt services
    service dev_mgmt_daemon start
    service dev_mgmt_daemon status
    if [ $? -eq 0 ]; then
        info_message "${DEV_MGMT_DAEMON} service is running"
        setup_inband_mgmt_channel
    else
        error_message "${DEV_MGMT_DAEMON} is not running"
        exit 1
    fi
}

#------------------------------------------------------------------------------
# Function Name: setup_inband_mgmt_channel
# Description:
#    This function sets up inband management channel.
#
# Globals:
#    BRCM_DIR, ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
setup_inband_mgmt_channel() {
    local interface_type="inband"

    # Extracting vlan ids from file
    get_vlan_ids

    cd ${BRCM_DIR}
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        # Waiting till interface eth2 get initialized & RUNNING state
        check_interface_is_up eth2 $interface_type

        # wait for BAL to get ready
        sleep $WAIT_TIME_BAL_READY

        # enabling in-band communication through broadcom API on NIC interface id 14 for eth2 interface
        echo "/Api/Set object=inband_mgmt_channel id=0 nni_intf={intf_type=nni intf_id=0} nic_intf_id=14 \
        vlan_id=${ASFVOLT16_VLAN_ID_ETH2} action=none" | ./example_user_appl
        if [ $? -ne 0 ]; then
            error_message "Failed to enable in-band channel on NIC interface id 14 with vlan ${ASFVOLT16_VLAN_ID_ETH2}"
        fi
    else
        # Waiting till interface eth1 get initialized & RUNNING state
        check_interface_is_up eth1 $interface_type
        # wait for BAL to get ready
        sleep $WAIT_TIME_BAL_READY
        # enabling in-band communication through broadcom API on NIC interface id 14 for eth1 interface
        echo "/Api/Set object=inband_mgmt_channel id=0 nni_intf={intf_type=nni intf_id=0} nic_intf_id=14 \
        vlan_id=${ASGVOLT64_VLAN_ID_ETH1} action=none" | ./example_user_appl
        if [ $? -ne 0 ]; then
            error_message "Failed to enable in-band channel on NIC interface id 14 with vlan ${ASGVOLT64_VLAN_ID_ETH1}"
        fi
    fi
}

#------------------------------------------------------------------------------
# Function Name: create_vlan_tagged_Iface
# Description:
#    This function create the VLAN interfaces and brings them UP.
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
create_vlan_tagged_Iface() {
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        # Adding vlan to the interface eth2
        ip link add link eth2 name eth2.${ASFVOLT16_VLAN_ID_ETH2} type vlan id ${ASFVOLT16_VLAN_ID_ETH2}
        ifconfig eth2.${ASFVOLT16_VLAN_ID_ETH2} up
    else
        # Adding vlan to the interface eth1
        ip link add link eth1 name eth1.${ASGVOLT64_VLAN_ID_ETH1} type vlan id ${ASGVOLT64_VLAN_ID_ETH1}
        ifconfig eth1.${ASGVOLT64_VLAN_ID_ETH1} up
    fi
    info_message "Inband tagged interfaces created succesfully"
}

#------------------------------------------------------------------------------
# Function Name: setup_dhcpd_configuration
# Description:
#    This function updates /etc/dhcp/dhclient.conf file for dhcp
#    request for eth2.ASFVOLT16_VLAN_ID_ETH2, eth3.ASFVOLT16_VLAN_ID_ETH3,
#    eth1.ASGVOLT64_VLAN_ID_ETH1 and eth2.ASGVOLT64_VLAN_ID_ETH2 interfaces
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASFVOLT16_VLAN_ID_ETH3, ASGVOLT64_VLAN_ID_ETH1,
#    ASGVOLT64_VLAN_ID_ETH2,  DHCLIENT_CONF
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
setup_dhcpd_configuration() {
    if [ -f ${DHCLIENT_CONF} ]; then
        if [ -f ${SERIAL_NUMBER} ]; then
            serial_num=$(cat ${SERIAL_NUMBER})
            if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
                # dhcient.conf file should have only one entry for eth2.<ASFVOLT16_VLAN_ID_ETH2> interface
                set_dhclient_configuration eth2 ${ASFVOLT16_VLAN_ID_ETH2}
            else
                # dhcient.conf file should have only one entry for eth1.<ASGVOLT64_VLAN_ID_ETH1> interface
                set_dhclient_configuration eth1 ${ASGVOLT64_VLAN_ID_ETH1}
            fi
        else
            error_message "ERROR: serial number of olt not found"
        fi
    else
        error_message "ERROR: DHCP config file not found"
    fi
}

#------------------------------------------------------------------------------
# Function Name: set_dhclient_configuration
# Description:
#    This function updates dhclient conf file with in-band interfaces.
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
set_dhclient_configuration() {
    interface=$1
    vlan_id=$2
    grep -q "interface \"${interface}.${vlan_id}\"" $DHCLIENT_CONF
    if [ $? -ne 0 ]; then
        echo "interface \"${interface}.${vlan_id}\" {" >>$DHCLIENT_CONF
        echo "    send dhcp-client-identifier \"${serial_num}.${vlan_id}\";" >>$DHCLIENT_CONF
        echo "    send vendor-class-identifier \"${EDGECORE}\";" >>$DHCLIENT_CONF
        echo "}" >>$DHCLIENT_CONF
    fi
}

#------------------------------------------------------------------------------
# Function Name: check_interface_is_up
# Description:
#    This function checks if provided inband interface is up and running
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
check_interface_is_up()
{
    interface=$1
    interface_type=$2
    count=0

    while true; do
        if [ $interface_type = "inband" ]; then
            ifconfig ${interface} | grep 'RUNNING' &>/dev/null
            if [ $? -eq 0 ]; then
                info_message "${interface} is up and running"
                break
            fi
        elif [ $interface_type = "inband-tagged" ]; then
            while true; do
                ifconfig ${interface} | grep 'RUNNING' &>/dev/null
                if [ $? -eq 0 ]; then
                    ip=$(ifconfig $interface | grep "inet addr" | cut -d ':' -f 2 | cut -d ' ' -f 1)
                    if [ ! -z "$ip" ]; then
                        info_message "${interface} is up and running with valid IP address"
                        return 0
                    else
                        info_message "Inband interface ${interface} is not up, continuously retrying for DHCP IP assignment"
                        info_message "Inband interface ${interface} is not up with valid IP hence not starting openolt service"
                        sleep 5
                        continue
                    fi
                fi
            done
        fi
    done
}

#------------------------------------------------------------------------------
# Function Name: stop_olt_services
# Description:
#    This function informs about inband interface error status and stops device management service.
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
stop_olt_services()
{
    error_message "Inband tagged interface $1 is not up with valid IP address"
    info_message "Not starting openolt service and stopping device menagement service"
    service dev_mgmt_daemon stop
    exit 1
}

#------------------------------------------------------------------------------
# Function Name: start_openolt_service
# Description:
#    This function checks whether inband interfaces are up with IP address, then
#    appends inband interface name to a file which later will be used by openolt
#    service as command line option then starts openolt service.
#    Openolt service enables all PON and NNI interfaces and creates respective PON
#    and NNI schedulers and Queues.
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
start_openolt_service()
{
    info_message "Starting openolt service"
    local interface_type="inband-tagged"

    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        check_interface_is_up eth2.${ASFVOLT16_VLAN_ID_ETH2} $interface_type
        if [ $? -ne 0 ]; then stop_olt_services eth2.${ASFVOLT16_VLAN_ID_ETH2}; fi
        openolt_grpc_interface=eth2.${ASFVOLT16_VLAN_ID_ETH2}
    else
        check_interface_is_up eth1.${ASGVOLT64_VLAN_ID_ETH1} $interface_type
        if [ $? -ne 0 ]; then stop_olt_services eth1.${ASGVOLT64_VLAN_ID_ETH1}; fi
        openolt_grpc_interface=eth1.${ASGVOLT64_VLAN_ID_ETH1}
    fi

    # Inband interface name appended to this file. Interface name will be
    # used as command line argument by openolt service while running the binary.
    if [ -f "$OPENOLT_ARG_INPUT_FILE" ]; then
        info_message "$OPENOLT_ARG_INPUT_FILE exist"
        echo "gRPC_interface=$openolt_grpc_interface" > $OPENOLT_ARG_INPUT_FILE
    else
        info_message "$OPENOLT_ARG_INPUT_FILE does not exist, creating it"
        touch $OPENOLT_ARG_INPUT_FILE
        echo "gRPC_interface=$openolt_grpc_interface" > $OPENOLT_ARG_INPUT_FILE
    fi

    service openolt start
    service openolt status
    if [ $? -eq 0 ]; then
        info_message "${OPENOLT} service is running"
    else
        error_message "${OPENOLT} is not running"
        exit 1
    fi
}

#------------------------------------------------------------------------------
# Function Name: start_olt_services
# Description:
#    This function triggers services to create vlan interfaces, to run BAL services
#    and to trigger DHCP requets on eth2.VLAN_ID_1 and eth3.VLAN_ID_2 interfaces.
#
# Globals:
#    None
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
start_olt_services() {
    # First stop any openolt processes if they were running before
    # and also disable auto start of these processes (since they are in /etc/init.d)
    stop_openolt_and_dev_mgmt_daemon_processes
    disable_autostart_of_openolt_and_dev_mgmt_daemon_processes

    start_dev_mgmt_service
    start_openolt_dev_mgmt_daemon_process_watchdog
    create_vlan_tagged_Iface
    setup_dhcpd_configuration
    setup_nw_configuration
    start_openolt_service
}

#------------------------------------------------------------------------------
# Function Name: copy_config_files
# Description:
#    This function copy config files to /broadcom directory
#
# Globals:
#     BRCM_DIR
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
copy_config_files() {
    info_message "copying config files to ${BRCM_DIR}"
    # if [ "${OLT_MODEL}" != ${ASF16_MODEL} ]; then
    #     [ -f /qax.soc ] && cp "/qax.soc" "${BRCM_DIR}/"
    # fi
    # [ -f /config.bcm ] && cp "/config.bcm" "${BRCM_DIR}/"
    [ -f /inband.config ] && cp "/inband.config" "${BRCM_DIR}/"
}

# Execution starts from here
does_logger_exist
if [ $? -eq 0 ]; then
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        # check if debian package is already installed.
        dpkg -l | grep ${ASFVOLT16}
        if [ $? -ne 0 ]; then
            # installs openolt debian package for asfvolt16 model
            dpkg -i ${BAL_OPENOLT_DEB_16}
        fi
    else
        dpkg -l | grep ${ASGVOLT64}
        if [ $? -ne 0 ]; then
            # installs openolt debian package for asgvolt64 model
            dpkg -i ${BAL_OPENOLT_DEB_64}
        fi
    fi
    copy_config_files
    # Wait time for BAL to get ready
    WAIT_TIME_BAL_READY=$(awk '/wait_time_bal_ready/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')

    start_olt_services
else
    error_message "logger does not exist"
    exit 1
fi

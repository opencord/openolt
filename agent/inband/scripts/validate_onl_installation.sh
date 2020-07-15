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
# Provides:          validate_onl_installation.sh
# Required-Start:    $all
# Required-Stop:     $network $local_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: This script validates ONL installation by checking if expected interfaces and
#                    services are UP and RUNNING.
# Description:
#                    Following are the interafces and services considered for validation:
#                    1. Based on the device type(asfvolt16/asgvolt64) this script validates whether
#                       eth2.<VLAN_ID>/eth1.<VLAN_ID> is up and running
#                    3. Validate system services
#                       a. openolt driver
#                       b. dev_mgmt_daemon
#                       c. onlp
#                       d. sshd
#                       e. dhclient
#                       returns the current status if service is up or not.
### END INIT INFO

#------------------------------------------------------------------------------
# GLOBAL VARIABLES
#------------------------------------------------------------------------------

# Time interval in seconds to execute each function call for validation.
TIME_INTERVAL=2

# Total time required to validate all services.
TOTAL_VALIDATION_TIME=20

# Root path where required bal directories are located
BRCM_DIR='/broadcom'

# Path to inband config file
INBAND_CONFIG_FILE="${BRCM_DIR}/inband.config"

# vlan id for asfvolt16
ASFVOLT16_VLAN_ID_ETH2=

# vlan id for asgvolt64
ASGVOLT64_VLAN_ID_ETH1=

OLT_MODEL=$(cat /sys/devices/virtual/dmi/id/board_name)
ASF16_MODEL="ASXvOLT16"
# interfaces
ETH1=eth1
ETH2=eth2

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
does_logger_exist()
{
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
info_message()
{
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
error_message()
{
    echo "ERROR: $1"
    logger -p user.err "$1"
    return 1
}

#------------------------------------------------------------------------------
# Function Name: is_interface_up_running
# Description:
#    This function validate the interface whether it is UP and RUNNING or DOWN.
#
# Globals:
#    None
#
# Arguments:
#    interface name
#
# Returns:
#    returns 0 if interface is up and running and returns 1 if interface is down
#------------------------------------------------------------------------------
is_interface_up_running()
{
    info_message "Validating interface - $1"
    interface_name=$1
    ifconfig ${interface_name} | grep -q "UP BROADCAST RUNNING MULTICAST"
    if [ $? -eq 0 ]; then
        info_message "${interface_name} UP & RUNNING"
        echo "---------------------------------------------------------------------"
        return 0
    else
        error_message "${interface_name} is DOWN"
        echo "---------------------------------------------------------------------"
        return 1
    fi
}

#------------------------------------------------------------------------------
# Function Name : validate_interfaces
# Description:
#    This function validate interfaces ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2} and
#    ${ETH3}.${ASFVOLT16_VLAN_ID_ETH3} or ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1} and
#    ${ETH2}.${ASGVOLT64_VLAN_ID_ETH2} based on OLT model.
#    Basically it validates if these interfaces are UP and RUNNING or not
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASFVOLT16_VLAN_ID_ETH3, ASGVOLT64_VLAN_ID_ETH1,
#    ASGVOLT64_VLAN_ID_ETH2, ETH1, ETH2, ETH3
#
#
# Arguments:
#    None
#
# Returns:
#    return 1 if interface are down else returns 0
#------------------------------------------------------------------------------
validate_interfaces()
{
    # Validating interfaces whether they are UP and RUNNING or not
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
        is_interface_up_running ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2}
        if [ $? -eq 0 ]; then
            return 0
        else
            return 1
        fi
    else
       is_interface_up_running ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1}
       if [ $? -eq 0 ]; then
           return 0
       else
           return 1
       fi
    fi
}

#------------------------------------------------------------------------------
# Function Name: validate_system_services
# Description:
#    This function checks if the below services/processes are running or not.
#    1. dev_mgmt_daemon
#    2. openolt
#    3. onlp
#    4. sshd
#    5. dhclient
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASFVOLT16_VLAN_ID_ETH3, ASGVOLT64_VLAN_ID_ETH1,
#    ASGVOLT64_VLAN_ID_ETH2, ETH1, ETH2, ETH3
#
#
# Arguments:
#    None
#
# Returns:
#    returns 0
#------------------------------------------------------------------------------
validate_system_services()
{
    echo "---------------------------------------------------------------------"
    echo "Validating Services"
    echo "---------------------------------------------------------------------"

    dhclient_interface dhclient_val1
    for service_name in dev_mgmt_daemon openolt onlpd ssh ${dhclient_val1}
    do
        echo "---------------------------------------------------------------------"
        ps -ef | grep -v grep | grep ${service_name}
        #service ${service_name} status
        if [ $? -eq 0 ]; then
          info_message "${service_name} service is running"
        else
          error_message "${service_name} is not running"
          return 1
        fi
        echo "---------------------------------------------------------------------"
    done
    return 0
}

#------------------------------------------------------------------------------
# Function Name: dhclient_interface
# Description:
#    This function sets values to which tagged interface dhclient service need
#    to be tested based on the OLT model package.
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASFVOLT16
#    ASGVOLT64_VLAN_ID_ETH1, ETH1, ETH2
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
dhclient_interface()
{
    local value1=$1
    if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
       dhclient1=dhclient.${ETH2}.${ASFVOLT16_VLAN_ID_ETH2}
       eval $value1="'$dhclient1'"
    else
       dhclient2=dhclient.${ETH1}.${ASGVOLT64_VLAN_ID_ETH1}
       eval $value1="'$dhclient2'"
    fi

}

#------------------------------------------------------------------------------
# Function Name: check_services
# Description:
#    This function checks if the expected services like onlp, sshd, dev_mgmt_daemon, dhclient
#    and also checks that physical and vlan interfaces are up.
#    This script times out and returns an error code of 1 if all vaildations don't turn out to be successful
#    in counter a number is equal to 20
#
# Globals:
#     INBAND_CONFIG_FILE, ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1,
#     TIME_INTERVAL, ASFVOLT16
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
check_services()
{
    if [ -f ${INBAND_CONFIG_FILE} ]; then
        ASFVOLT16_VLAN_ID_ETH2=$(awk '/asfvolt16_vlan_id_eth2/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')
        ASGVOLT64_VLAN_ID_ETH1=$(awk '/asgvolt64_vlan_id_eth1/{print $0}' ${INBAND_CONFIG_FILE} | awk -F "=" '{print $2}')
        if [ "${OLT_MODEL}" = ${ASF16_MODEL} ]; then
            if [ ${ASFVOLT16_VLAN_ID_ETH2} -gt 4094 ] || [ ${ASFVOLT16_VLAN_ID_ETH2} -lt 1 ]; then
                error_message "vlan ids not in range"
                exit 1
            fi
        else
            if [ ${ASGVOLT64_VLAN_ID_ETH1} -gt 4094 ] || [ ${ASGVOLT64_VLAN_ID_ETH1} -lt 1 ]; then
                error_message "vlan ids not in range"
                exit 1
           fi
       fi
    else
        error_message "${INBAND_CONFIG_FILE} does not exist"
        exit 1
    fi
    echo "*********************************************************************"
    info_message "Validating ONL Installation"
    echo "*********************************************************************"
    for func_name in validate_interfaces validate_system_services
    do
        counter=0
        while true; do
            sleep ${TIME_INTERVAL}
            $func_name
            if [ $? -eq 0 ]; then
                break
            fi
            if [ $counter -eq $TOTAL_VALIDATION_TIME ]; then
                error_message "Time out ,vlan interfaces or all services may not be up"
                return 1
            fi
            counter=$((counter+${TIME_INTERVAL}))
        done
    done
    return 0
}

does_logger_exist
if [ $? -eq 0 ]; then
   check_services
else
   error_message "logger does not exist"
   exit 1
fi

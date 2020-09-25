#!/bin/bash

#Copyright 2020-present Open Networking Foundation
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
#

### BEGIN INIT INFO
# Description:
#                   This script swaps the block id labels if expected services 
#                   like system services onlp, sshd, bal_core_dist, dhclient, physical
#                   ,vlan interfaces status and olt services dev_mgmt_daemon, openolt services
#                   are UP and RUNNING. If all services are not UP and RUNNING, then roll back 
#                   to previous state.
### END INIT INFO

# Labels names
ONL_ACTIVE_BOOT="ONL-ACTIVE-BOOT"
ONL_ACTIVE_DATA="ONL-ACTIVE-DATA"
ONL_STANDBY_BOOT="ONL-STANDBY-BOOT"
ONL_STANDBY_DATA="ONL-STANDBY-DATA"

RUNNING_ROOT_DEV=
RUNNING_ROOT_LABEL=
RUNNING_BOOT_LABEL=
RUNNING_BOOT_DEV=

STANDBY_BOOT_DEV=
STANDBY_BOOT_LABEL=
STANDBY_ROOT_LABEL=
STANDBY_ROOT_DEV=

# Configuration file about upgrade
ONL_ROOT="/mnt/onl"
IMAGES="${ONL_ROOT}/images"
UPGRADE_PATH="${IMAGES}/upgrade"
ACTIVE_GRUB_ENV_FILE="${ONL_ROOT}/active_boot/grub/grubenv"
STANDBY_GRUB_ENV_FILE="${ONL_ROOT}/standby_boot/grub/grubenv"
GRUB_STAND_PATH="${ONL_ROOT}/standby_boot/grub"
GRUB_ACTIVE_PATH="${ONL_ROOT}/active_boot/grub"
STANDBY_GRUB_ACTIVE_PATH="${ONL_ROOT}/standby_boot/grub"
GRUB_CFG="grub.cfg"
GRUB_CFG_BACKUP="grub.cfg.back"
GRUB_CONFIG_FILE="${ONL_ROOT}/config/grub.cfg"
# Time interval in seconds
TIME_INTERVAL=5

#in band interface IP
ASFVOLT16_ETH2_VLAN_INF_IP=

ASGVOLT64_ETH1_VLAN_INF_IP=

ETH3_VLAN_IP=

BRCM_DIR='/broadcom'

# vlan config file
VLAN_CONFIG_FILE="${BRCM_DIR}/inband.config"

# vlan id for asfvolt16
ASFVOLT16_VLAN_ID_ETH2=

#vlan id for asgvolt64
ASGVOLT64_VLAN_ID_ETH1=

OLT_MODEL=$(cat /sys/devices/virtual/dmi/id/board_name)
ASXvOLT16="ASXvOLT16"
ASGvOLT64="ASGvOLT64"

VLAN_ID_1=
VLAN_ID_2=

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
display_services()
{
    echo "Sanity result: 
          Service Name       - Status
          --------------------------------------
          1. eth2                     - UP & RUNNING
          2. eth1                     - UP & RUNNING
          3. bal_service              - RUNNING
          4. system services          - RUNNING"

    if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
       echo "
          5. eth2.${ASFVOLT16_VLAN_ID_ETH2}        - UP & RUNNING
          6. eth2.${ASFVOLT16_VLAN_ID_ETH2} IP     - ${ASFVOLT16_ETH2_VLAN_INF_IP}"
    elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
       echo"
          5. eth1.${ASGVOLT64_VLAN_ID_ETH1}        - UP & RUNNING
          6. eth1.${ASGVOLT64_VLAN_ID_ETH1} IP     - ${ASGVOLT64_ETH1_VLAN_INF_IP}"
    fi
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
# Function Name: validate_ip
# Description:
#    This function get the ip from the interface and validates it.
#
# Globals:
#    OLT_MODEL, ASXvOLT16, ASGvOLT64, ASFVOLT16_VLAN_ID_ETH2,
#     ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1
#
#
# Arguments:
#    interface name
#
# Returns:
#    ip address
#------------------------------------------------------------------------------
validate_ip()
{
    interface_name=$1
    ip_address=$(ifconfig $interface_name | grep "inet addr" | awk  'BEGIN {FS =":"} {print $2}' | awk '{print $1}')

    if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
        if [ ! -z $ip_address ] && [ $interface_name = ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2} ]; then
           ASFVOLT16_ETH2_VLAN_INF_IP=$ip_address
        fi
    elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
        if [ ! -z $ip_address ] && [ $interface_name = ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1} ]; then
           ASGVOLT64_ETH1_VLAN_INF_IP=$ip_address
        fi
    fi

    info_message "Validating $1 ip address - $ip_address"
    if expr "$ip_address" : '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$' >/dev/null; then
        for i in 1 2 3 4; do
            if [ $(echo "$ip_address" | cut -d. -f$i) -gt 255 ]; then
                return 1
            fi
        done
        return 0
    else
        return 1
    fi
}

#------------------------------------------------------------------------------
# Function Name : validate_interfaces
# Description:
#    This function validate interfaces ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2} and
#    or ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1} and based on OLT model.
#    Basically it validates if these interfaces are UP and RUNNING or not
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1,
#    ETH1, ETH2, ASFVOLT16_BAL_PACKAGE
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
    if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
        is_interface_up_running ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2}
        if [ $? -eq 0 ]; then
            return 0
        else
            return 1
        fi
    elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
       is_interface_up_running ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1}
       if [ $? -eq 0 ]; then
           return 0
       else
           return 1
       fi
    fi
}

#------------------------------------------------------------------------------
# Function Name: validate_vlan_intf_ip
# Description:
#    This function validate the inband vlan interfaces ip, checks if DHCP server has
#    assigned proper IP addresses.
#
#
# Globals:
#    ASFVOLT16_VLAN_ID_ETH2,  ASGVOLT64_VLAN_ID_ETH1,
#    ETH1, ETH2, OLT_MODEL, ASXvOLT16 and ASGvOLT64
#
#
# Arguments:
#    None
#
# Returns:
#    returns 0 if ip adresses are valid else return 1
#------------------------------------------------------------------------------
validate_vlan_intf_ip()
{
    # Validating vlan interfaces and their IP
    if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
        validate_ip ${ETH2}.${ASFVOLT16_VLAN_ID_ETH2}
        if [ $? -eq 0 ]; then
           return 0
        else
           return 1
        fi
    elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
        validate_ip ${ETH1}.${ASGVOLT64_VLAN_ID_ETH1}
        if [ $? -eq 0 ]; then
           return 0
        else
           return 1
        fi

    fi
    return 0
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
#    None
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

    dhclient_interface dhclient_val1 dhclient_val2
    for service_name in dev_mgmt_daemon openolt onlp sshd ${dhclient_val1} ${dhclient_val2}
    do
        echo "---------------------------------------------------------------------"
        ps -ef | grep -v grep | grep ${service_name}
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
#    OLT_MODEL, ASFVOLT16_VLAN_ID_ETH2,
#    ASGVOLT64_VLAN_ID_ETH1, ETH1, ETH2,
#    ASGvOLT64, ASXvOLT16.
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
    local value2=$2
    if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
       dhclient1=dhclient.${ETH2}.${ASFVOLT16_VLAN_ID_ETH2}
       eval $value1="'$dhclient1'"
    elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
       dhclient3=dhclient.${ETH1}.${ASGVOLT64_VLAN_ID_ETH1}
       eval $value1="'$dhclient3'"
    fi

}

#------------------------------------------------------------------------------
# Function Name: check_services
# Description:
#    This function check the expected services, like system services for ex:
#    onlp, sshd, bal_core_dist, dhclient and physical and vlan interfaces status.
#
# Globals:
#   VLAN_CONFIG_FILE, ASFVOLT16_VLAN_ID_ETH2, ASGVOLT64_VLAN_ID_ETH1
#   TIME_INTERVAL, OLT_MODEL, ASXvOLT16, ASGvOLT64
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
check_services()
{
    # Let bal services are up and running
    waiting_time=0
    while true; do
        sleep ${TIME_INTERVAL}
        if [ $waiting_time -eq 70 ]; then
            break
        fi
        waiting_time=$((waiting_time+${TIME_INTERVAL}))
    done

    if [ -f ${VLAN_CONFIG_FILE} ]; then
        ASFVOLT16_VLAN_ID_ETH2=$(awk '/asfvolt16_vlan_id_eth2/{print $0}' \
        ${VLAN_CONFIG_FILE} | awk -F "=" '{print $2}')
        ASGVOLT64_VLAN_ID_ETH1=$(awk '/asgvolt64_vlan_id_eth1/{print $0}' \
        ${VLAN_CONFIG_FILE} | awk -F "=" '{print $2}')
        if [ "${OLT_MODEL}" = ${ASXvOLT16} ];then
            if [ ${ASFVOLT16_VLAN_ID_ETH2} -gt 4094 ] || [ ${ASFVOLT16_VLAN_ID_ETH2} -lt 1 ]; then
                error_message "vlan ids not in range"
                exit 1
            fi
        elif [ "${OLT_MODEL}" = ${ASGvOLT64} ];then
            if [ ${ASGVOLT64_VLAN_ID_ETH1} -gt 4094 ] || [ ${ASGVOLT64_VLAN_ID_ETH1} -lt 1 ]; then
                error_message "vlan ids not in range"
                exit 1
           fi
       fi
    else
        error_message "${VLAN_CONFIG_FILE} does not exist"
        exit 1
    fi

    info_message "Sanity check"
    for func_name in validate_interfaces validate_vlan_intf_ip validate_system_services
    do
        counter=0
        while true; do
            sleep ${TIME_INTERVAL}
            $func_name
            if [ $? -eq 0 ]; then
                break
            fi
            if [ $counter -eq 100 ]; then
                error_message "Time out ,all services are not up"
                return 1
            fi
            counter=$((counter+${TIME_INTERVAL}))
        done
    done
    display_services
    return 0
}

#------------------------------------------------------------------------------
# Function Name: change_labels
# Description:
#    This function does the following functions
#             1) After the upgrade procedure installs the image in stand_by_partition,
#                This function checks for the system services after upgrade,
#                if all ther services are up and running then following steps takes place
#                a) It changes the device labels from Active to Standby and
#                Standby to Active.
#                b) It sets the grub menu entry from 1 to 0 which is default
#                grub entry. After reboot in the grub menu entry will 
#                have one entry.
#                c) It will swap the upgrade image to Image directory and vice
#                versa.
#             2) If system services fails to start after image upgrade,
#                roll-back to previous state of OLT i.e
#                boot the OLT back from the active partition.
#
# Globals:
#    RUNNING_BOOT_DEV, STANDBY_BOOT_LABEL, RUNNING_ROOT_DEV, STANDBY_ROOT_LABEL
#    STANDBY_BOOT_DEV, RUNNING_BOOT_LABEL, STANDBY_ROOT_DEV, RUNNING_ROOT_LABEL
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
change_labels()
{
    grep "data-standby" /proc/cmdline > /dev/null
    if [ $? -eq 0 ]; then
        check_services
        # if the OLT system services fail to start, fall back to previous state
        if [ $? -eq 1 ]; then
            reboot -f
        fi
        # Change running boot device label to standby boot label
        e2label $RUNNING_BOOT_DEV $STANDBY_BOOT_LABEL
        # Change running root device label to standby root label
        e2label $RUNNING_ROOT_DEV $STANDBY_ROOT_LABEL
        # Change standby boot device label to active boot label
        e2label $STANDBY_BOOT_DEV $RUNNING_BOOT_LABEL
        # Change standby root device label to active root label
        e2label $STANDBY_ROOT_DEV $RUNNING_ROOT_LABEL
        partprobe

        if [ -f ${ACTIVE_GRUB_ENV_FILE} ]; then
            next_entry=$(grub-editenv  ${ACTIVE_GRUB_ENV_FILE} list | sed -n 's/^next_entry=//p')
            saved_entry=$(grub-editenv  ${ACTIVE_GRUB_ENV_FILE} list | sed -n 's/^saved_entry=//p')
            if [ ${next_entry} -eq 1 ] || [ ${saved_entry} -eq 1 ]; then
                grub-editenv ${ACTIVE_GRUB_ENV_FILE} set next_entry=0
                grub-editenv ${ACTIVE_GRUB_ENV_FILE} set saved_entry=0
            fi
        fi

        if [ -f ${STANDBY_GRUB_ENV_FILE} ]; then
            next_entry=$(grub-editenv  ${STANDBY_GRUB_ENV_FILE} list | sed -n 's/^next_entry=//p')
            saved_entry=$(grub-editenv  ${STANDBY_GRUB_ENV_FILE} list | sed -n 's/^saved_entry=//p')
            if [ ${next_entry} -eq 1 ] || [ ${saved_entry} -eq 1 ]; then
                grub-editenv ${STANDBY_GRUB_ENV_FILE} set next_entry=0
                grub-editenv ${STANDBY_GRUB_ENV_FILE} set saved_entry=0
            fi
        fi

        if [ -f ${GRUB_CONFIG_FILE} ]; then
            if [ -f "${GRUB_ACTIVE_PATH}/${GRUB_CFG}" ]; then
                cp ${GRUB_CONFIG_FILE}  ${GRUB_ACTIVE_PATH}/${GRUB_CFG}
            fi
            if [ -f "${GRUB_STAND_PATH}/${GRUB_CFG}" ]; then
                cp ${GRUB_CONFIG_FILE} ${GRUB_STAND_PATH}/${GRUB_CFG}
            fi
        fi

        if [ -d ${UPGRADE_PATH} ]; then
            cd ${UPGRADE_PATH}
            upgrade_image=$(ls *.swi)
            mv $upgrade_image "$upgrade_image.bak"
            cd ${IMAGES}
            active_image=$(ls *.swi)
            mv $active_image "$active_image.bak"
            mv ${UPGRADE_PATH}/"$upgrade_image.bak" ${IMAGES}/$upgrade_image
            mv ${IMAGES}/"$active_image.bak" ${UPGRADE_PATH}/$active_image
        fi
    fi

}

#------------------------------------------------------------------------------
# Function Name: get_labels
# Description:
#    This function get the labels of the devices and also find out the 
#    Running and Standby devices.
#
# Globals:
#    RUNNING_ROOT_DEV, RUNNING_ROOT_LABEL, RUNNING_BOOT_LABEL, STANDBY_BOOT_LABEL
#    RUNNING_ROOT_LABEL, STANDBY_ROOT_LABEL
#
# Arguments:
#    None
#
# Returns:
#    None
#------------------------------------------------------------------------------
get_labels()
{
    RUNNING_ROOT_DEV=$(cat /proc/mounts | grep  " \/ ext4" | awk '{print $1}')
    RUNNING_ROOT_LABEL=$(blkid | grep ${RUNNING_ROOT_DEV} | awk '{print $2}' | cut -c 7- | tr -d \")
    if [ ${RUNNING_ROOT_LABEL} = "ONL-ACTIVE-DATA" ]; then
        RUNNING_BOOT_LABEL=${ONL_ACTIVE_BOOT}
        STANDBY_BOOT_LABEL=${ONL_STANDBY_BOOT}
        RUNNING_ROOT_LABEL=${ONL_ACTIVE_DATA}
        STANDBY_ROOT_LABEL=${ONL_STANDBY_DATA}
    else
        RUNNING_BOOT_LABEL=${ONL_STANDBY_BOOT}
        RUNNING_ROOT_LABEL=${ONL_STANDBY_DATA}
        STANDBY_BOOT_LABEL=${ONL_ACTIVE_BOOT}
        STANDBY_ROOT_LABEL=${ONL_ACTIVE_DATA}
    fi
    STANDBY_ROOT_DEV=$(blkid | grep " LABEL=\"$STANDBY_ROOT_LABEL\"" | awk '{print $1}' | tr -d :)
    RUNNING_BOOT_DEV=$(blkid | grep " LABEL=\"$RUNNING_BOOT_LABEL\"" | awk '{print $1}' | tr -d :)
    STANDBY_BOOT_DEV=$(blkid | grep " LABEL=\"$STANDBY_BOOT_LABEL\"" | awk '{print $1}' | tr -d :)
}

# Starts from here
does_logger_exist
if [ $? -eq 0 ]; then
   get_labels
   change_labels
else
   error_message "logger does not exist"
   exit 1
fi

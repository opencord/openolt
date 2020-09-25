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

### BEGIN INIT INFO
# Description:
#             Upgrade the NOS from Active to Standby partition, 
#             This script checks if installed ONL on OLT has active and standby partions
#             if yes, it extracts the ONL package and keep the
#             required files to the Standby partition and start the NOS from Standby partition.
#
### END INIT INFO


WORKING_DIR=$(pwd)
# Name of the script
SCRIPT_NAME="$(basename ${0})"
UPGRADE=
ONL_FILE=${1}
ONL_ROOT="/mnt/onl"
ACTIVE_BOOTDIR="${ONL_ROOT}/active_boot/"
STANDBY_BOOTDIR="${ONL_ROOT}/standby_boot/"

# Partition labels
ONL_ACTIVE_BOOT="ONL-ACTIVE-BOOT"
ONL_ACTIVE_DATA="ONL-ACTIVE-DATA"
ONL_STANDBY_BOOT="ONL-STANDBY-BOOT"
ONL_STANDBY_DATA="ONL-STANDBY-DATA"
ONL_CONFIG="ONL-CONFIG"
ONL_IMAGES="ONL-IMAGES"

# Mount paths
CONFIG="${ONL_ROOT}/config"
IMAGES="${ONL_ROOT}/images"
ACTIVE_DATA="${ONL_ROOT}/active_data"
ACTIVE_BOOT="${ONL_ROOT}/active_boot"
STANDBY_DATA="${ONL_ROOT}/standby_data"
STANDBY_BOOT="${ONL_ROOT}/standby_boot"
GRUB="grub"
ONL_SOURCE="onl_source"

ACTIVE_DATA_DEV_NAME=
ACTIVE_DATA_MNT_PATH=
ACTIVE_BOOT_DEV_NAME=
ACTIVE_BOOT_MNT_PATH=
STANDBY_DATA_DEV_NAME=
STANDBY_DATA_MNT_PATH=
STANDBY_BOOT_DEV_NAME=
STANDBY_BOOT_MNT_PATH=

# Configuration file about upgrade
IMAGES="${ONL_ROOT}/images"
ACTIVE_GRUB_ENV_FILE="${ONL_ROOT}/active_boot/grub/grubenv"
STANDBY_GRUB_ENV_FILE="${ONL_ROOT}/active_boot/grub/grubenv"
GRUB_CONFIG_FILE="${ONL_ROOT}/config/grub.cfg"
GRUB_CONFIG_FILE_CHANGED="${ONL_ROOT}/config/grub.cfg.changed"
ASF_VOLT16_VOLTHA_BAL=$(ls /*asfvolt16*.tar.gz 2>/dev/null)
BROADCOM="/broadcom"
OPT="/opt/bcm68620"
DEV_MGMT_DAEMON="dev_mgmt_daemon -d -pcie"
OPENOLT="openolt"
#------------------------------------------------------------------------------
# Function : usage
#
# Description: 
#    This function says how to use the script
#                                                                               
# Globals:                                                                 
#    SCRIPT_NAME
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                 
#    None        
#------------------------------------------------------------------------------

usage()
{
    echo "${SCRIPT_NAME}:usage:
        ${SCRIPT_NAME} <ONL_IMAGE_WITH_ABSOLUTE_PATH>
    "
    exit 1
}

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
# Function: clean_up 
#
# Description: 
#    This function delete the existing image in the standby boot partition. 
#    In order to copy the the new image. also delete the onl source in the 
#    working directory if it is there.
#
# Globals:                                                                 
#    WORKING_DIR, ONL_SOURCE, STANDBY_BOOT_MNT_PATH, ACTIVE_DATA_MNT_PATh
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                 
#    None        
#------------------------------------------------------------------------------
clean_up()
{
     info_message "Deleting onl source"
     rm -rf ${WORKING_DIR}/${ONL_SOURCE}
     info_message "Deleting kernel and initrd files"
     rm -rf ${STANDBY_BOOT_MNT_PATH}/kernel-*
     rm -rf ${STANDBY_BOOT_MNT_PATH}/x86-64-accton-*
     echo "INFO: Deleting configuration files"
     rm -rf /etc/onl/*
     rm -rf /lib/vendor-config/
     info_message "Deleting standby rootfs"
     rm -rf ${STANDBY_DATA_MNT_PATH}/*
}

#------------------------------------------------------------------------------
# Function: update_grub_cfg_for_temp_reboot
#
# Description: 
#    This function updates the grub.cfg file in order to achieve the temporary 
#    reboot. by making standby entry in the temporary reboot. once temporary 
#    reboot happens successfully grub.cfg file will have only active entry.
#                                                                               
# Globals:                                                                 
#    GRUB_CONFIG_FILE_CHANGED, ACTIVE_KERNEL_NAME, STANDBY_KERNEL_NAME
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                 
#    None        
#------------------------------------------------------------------------------
update_grub_cfg_for_temp_reboot()
{
     cat > ${GRUB_CONFIG_FILE_CHANGED} <<- EOF
serial --port=0x3f8 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial
terminal_output serial
set timeout=5

# Always boot the saved_entry value
load_env
if [ \$saved_entry ] ; then
   set default=\$saved_entry
fi

menuentry "Open Network Linux" {
  search --no-floppy --label --set=root ONL-ACTIVE-BOOT
  # Always return to this entry by default.
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading Open Network Linux ...'
  insmod gzio
  insmod part_msdos
  linux /${ACTIVE_KERNEL_NAME} nopat console=ttyS0,115200n8 tg3.short_preamble=1 tg3.bcm5718s_reset=1 intel_iommu=off onl_platform=x86-64-accton-asxvolt16-r0 data-active
  initrd /x86-64-accton-asxvolt16-r0.cpio.gz
}

menuentry "Open Network Linux Standby" {
  search --no-floppy --label --set=root ONL-STANDBY-BOOT
  # Always return to this entry by default.
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading Open Network Linux Standby...'
  insmod gzio
  insmod part_msdos
  linux /${STANDBY_KERNEL_NAME} nopat console=ttyS0,115200n8 tg3.short_preamble=1 tg3.bcm5718s_reset=1 intel_iommu=off onl_platform=x86-64-accton-asxvolt16-r0 data-standby
  initrd /x86-64-accton-asxvolt16-r0.cpio.gz
}

# Menu entry to chainload ONIE
menuentry ONIE {
  search --no-floppy --label --set=root ONIE-BOOT
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading ONIE ...'
  chainloader +1
}


EOF
}

#------------------------------------------------------------------------------
# Function: revert_grub_cfg 
#
# Description: 
#    This function revert the original grub.cfg file which was affected
#    by temporary reboot.
#                                                                               
# Globals:                                                                 
#    GRUB_CONFIG_FILE, ACTIVE_KERNEL_NAME
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                 
#    None        
#------------------------------------------------------------------------------
revert_grub_cfg()
{
     cat > ${GRUB_CONFIG_FILE}  <<- EOF
serial --port=0x3f8 --speed=115200 --word=8 --parity=no --stop=1
terminal_input serial
terminal_output serial
set timeout=5

# Always boot the saved_entry value
load_env
if [ \$saved_entry ] ; then
   set default=\$saved_entry
fi

menuentry "Open Network Linux" {
  search --no-floppy --label --set=root ONL-ACTIVE-BOOT
  # Always return to this entry by default.
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading Open Network Linux ...'
  insmod gzio
  insmod part_msdos
  linux /${ACTIVE_KERNEL_NAME} nopat console=ttyS0,115200n8 tg3.short_preamble=1 tg3.bcm5718s_reset=1 intel_iommu=off onl_platform=x86-64-accton-asxvolt16-r0 data-active
  initrd /x86-64-accton-asxvolt16-r0.cpio.gz
}

menuentry "Open Network Linux Standby" {
  search --no-floppy --label --set=root ONL-STANDBY-BOOT
  # Always return to this entry by default.
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading Open Network Linux Standby...'
  insmod gzio
  insmod part_msdos
  linux /${STANDBY_KERNEL_NAME} nopat console=ttyS0,115200n8 tg3.short_preamble=1 tg3.bcm5718s_reset=1 intel_iommu=off onl_platform=x86-64-accton-asxvolt16-r0 data-standby
  initrd /x86-64-accton-asxvolt16-r0.cpio.gz
}

# Menu entry to chainload ONIE
menuentry ONIE {
  search --no-floppy --label --set=root ONIE-BOOT
  set saved_entry="0"
  save_env saved_entry
  echo 'Loading ONIE ...'
  chainloader +1
}

EOF
}
#------------------------------------------------------------------------------
# Function: grub_reboot 
#
# Description: 
#    This function gives a temporary reboot using grub-reboot command, this will 
#    set the boot menu entry from active default one to Standby. Now OS will 
#    boot up in Standby mode.
#                                                                               
# Globals:
#    ACTIVE_BOOTDIR, STANDBY_BOOTDIR, GRUB_CONFIG_FILE_CHANGED, ACTIVE_BOOT
#    GRUB
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                
#    None        
#------------------------------------------------------------------------------
grub_reboot()
{
    if [ -f "${ACTIVE_BOOTDIR}/grub/grubenv" ]; then
        saved_entry=$(grub-editenv  ${ACTIVE_BOOTDIR}/grub/grubenv list | sed -n 's/^saved_entry=//p')
        if [ ${saved_entry} -eq 0 ]; then
            grub-editenv ${ACTIVE_BOOTDIR}/grub/grubenv set saved_entry=1
            sed  -i '/next_entry=/a\prev_saved_entry=1' ${ACTIVE_BOOTDIR}/grub/grubenv
        fi
    fi

    if [ -f "${STANDBY_BOOTDIR}/grub/grubenv" ]; then
        saved_entry=$(grub-editenv  ${STANDBY_BOOTDIR}/grub/grubenv list | sed -n 's/^saved_entry=//p')
        if [ ${saved_entry} -eq 0 ]; then
            grub-editenv ${STANDBY_BOOTDIR}/grub/grubenv set saved_entry=1
            sed  -i '/next_entry=/a\prev_saved_entry=1' ${STANDBY_BOOTDIR}/grub/grubenv
        fi
    fi

    if [ -f ${GRUB_CONFIG_FILE_CHANGED} ]; then
        if [ -f "${ACTIVE_BOOT}/${GRUB}/grub.cfg" ]; then
            cp ${GRUB_CONFIG_FILE_CHANGED} ${ACTIVE_BOOT}/${GRUB}/grub.cfg
        fi
        if [ -f "${STANDBY_BOOT}/${GRUB}/grub.cfg" ]; then
            cp ${GRUB_CONFIG_FILE_CHANGED} ${STANDBY_BOOT}/${GRUB}/grub.cfg
        fi
    fi
    grub-reboot --boot-directory=${ACTIVE_BOOTDIR} 1
    reboot
}

#------------------------------------------------------------------------------
# Function: install_onl
#
# Description: 
#    This function copy the upgrade image.
#
# Globals:
#    WORKING_DIR, ONL_SOURCE, STANDBY_BOOT_MNT_PATH, IMAGES, ACTIVE_BOOT_MNT_PATH 
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                
#    None        
#------------------------------------------------------------------------------
install_onl()
{
    if [ -f ${ONL_FILE} ]; then
        clean_up
        mkdir ${WORKING_DIR}/${ONL_SOURCE}
        cd ${WORKING_DIR}/${ONL_SOURCE}
        chmod 777 ${ONL_FILE}
        # Extract onl package
        unzip ${ONL_FILE}
        # Copy kernel files to standby boot partition
        cp kernel-* ${STANDBY_BOOT_MNT_PATH}/
        # Copy initrd file to standby boot partition
        cp onl-loader-initrd-amd64.cpio.gz ${STANDBY_BOOT_MNT_PATH}/x86-64-accton-asxvolt16-r0.cpio.gz
        # Create upgrade directory in the images partition to copy upgrade image
        if [ -d ${IMAGES}/upgrade ]; then
            rm -rf ${IMAGES}/upgrade
        fi
        info_message "Creating Upgrade Directory"
        mkdir ${IMAGES}/upgrade
        info_message "Copying SWI file to Upgrade Directory"
        cp *.swi ${IMAGES}/upgrade

        # Update the kernel version
        STANDBY_KERNEL_NAME=$(ls ${STANDBY_BOOT_MNT_PATH} | grep kernel-4.14)
        ACTIVE_KERNEL_NAME=$(ls ${ACTIVE_BOOT_MNT_PATH} | grep kernel-4.14 )
        update_grub_cfg_for_temp_reboot
        revert_grub_cfg
    else
        error_message "ONL file not found"
        exit 1
    fi
}

#------------------------------------------------------------------------------
# Function: upgrade_onl 
#
# Description: 
#    This function get the labels, and install the upgrade NOS.
#
# Globals       :
#    ACTIVE_DATA_DEV_NAME, ACTIVE_BOOT_DEV_NAME, ACTIVE_BOOT_MNT_PATH, 
#    STANDBY_DATA_DEV_NAME, STANDBY_BOOT_MNT_PATH 
#                                                                               
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                
#    None        
#------------------------------------------------------------------------------
upgrade_onl()
{

    ACTIVE_DATA_DEV_NAME=$(blkid | grep " LABEL=\"ONL-ACTIVE-DATA\"" | awk '{print $1}' | sed 's/://')
    echo "active data dev - $ACTIVE_DATA_DEV_NAME"
    ACTIVE_DATA_MNT_PATH=$(mount | grep $ACTIVE_DATA_DEV_NAME | awk '{print $3}')
    echo $"active data mnt - $ACTIVE_DATA_MNT_PATH"
    ACTIVE_BOOT_DEV_NAME=$(blkid | grep " LABEL=\"ONL-ACTIVE-BOOT\"" | awk '{print $1}' | sed 's/://')
    echo "active boot dev $ACTIVE_BOOT_DEV_NAME"
    ACTIVE_BOOT_MNT_PATH=$(mount | grep $ACTIVE_BOOT_DEV_NAME | awk '{print $3}')
    echo "active boot mnt - $ACTIVE_BOOT_MNT_PATH"
    STANDBY_DATA_DEV_NAME=$(blkid | grep " LABEL=\"ONL-STANDBY-DATA\"" | awk '{print $1}' | sed 's/://')
    echo "standby data dev - $STANDBY_DATA_DEV_NAME"
    STANDBY_DATA_MNT_PATH=$(mount | grep $STANDBY_DATA_DEV_NAME | awk '{print $3}')
    echo "standby data mnt - $STANDBY_DATA_MNT_PATH"
    STANDBY_BOOT_DEV_NAME=$(blkid | grep " LABEL=\"ONL-STANDBY-BOOT\"" | awk '{print $1}' | sed 's/://')
    echo "standby boot dev - $STANDBY_BOOT_DEV_NAME"
    STANDBY_BOOT_MNT_PATH=$(mount | grep $STANDBY_BOOT_DEV_NAME | awk '{print $3}')
    echo "standby boot mnt -$STANDBY_BOOT_MNT_PATH"

    install_onl
    grub_reboot
}

#------------------------------------------------------------------------------
# Function: command_result 
#
# Description: 
#    This function executes the blkid command.
#
# Globals:
#    None
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                
#    None        
#------------------------------------------------------------------------------
command_result(){
    blkid | grep $1 > /dev/null
    if [ $? -eq 0 ]; then
        return 0
    else
        return 1
    fi
}

#------------------------------------------------------------------------------
# Function: is_onl_running 
#
# Description: 
#    This function check the running NOS and  also check the Upgrade is required 
#    or not.
#
# Globals:
#    ONL_ACTIVE_BOOT, ONL_ACTIVE_DATA, ONL_STANDBY_BOOT, ONL_STANDBY_DATA
#    ONL_CONFIG, ONL_IMAGES, IMAGES, ACTIVE_DATA, ACTIVE_BOOT, STANDBY_DATA
#    STANDBY_BOOT
#
# Arguments:                                                                 
#    None                                                                       
#                                                                               
# Returns:                                                                
#    None        
#------------------------------------------------------------------------------
is_onl_running()
{
    info_message "Check all directories exist or not"
    if command_result "$ONL_ACTIVE_BOOT" && command_result "$ONL_ACTIVE_DATA" && command_result "$ONL_STANDBY_BOOT" && command_result "$ONL_STANDBY_DATA" && command_result "$ONL_CONFIG" && command_result "$ONL_IMAGES" ; then
        if [ \( -d "$CONFIG" -a -d "$IMAGES" \) -a \( -d "$ACTIVE_DATA" -a -d "$ACTIVE_BOOT" \) -a \( -d "$STANDBY_DATA" -a -d "$STANDBY_BOOT" \) ]; then 
            info_message "All directories exist. Need Upgrade"
            UPGRADE=1
        fi
    else
        info_message "It is new installation"
        UPGRADE=0
    fi
}

#------------------------------------------------------------------------------
# Function: main 
#
# Description: 
#    This is the main function. which decides fresh installation is required or 
#    upgrade is required.
#
# Globals       :
#    UPGRADE, ONL_FILE
# Arguments     :                                                                 
#    None                                                                       
#                                                                               
# Returns       :                                                                
#    None        
#------------------------------------------------------------------------------
main()
{
    echo "INFO: Check if image upgrade or new installation"
    is_onl_running
    if [ $UPGRADE -eq 0 ]; then
        error_message "ONL does not have active and stand by partitions \
        install ONL image with active and standby partitions"
        exit 1    
    else
        info_message "Upgrade started"
        upgrade_onl
    fi
}

# main function call
does_logger_exist
if [ $? -eq 0 ]; then
    main
else
   error_message "logger does not exist"
   exit 1
fi

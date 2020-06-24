#!/usr/bin/python

"""
Copyright 2018-present Open Networking Foundation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

"""
:Description:  Module for fetching OLT information.

               This module is used to fetch OLT software and hardware information.
               The OLT software information includes Operating system, OS Version,
               and ONIE version.
               The hardware information includes OLT Serial Number, OLT Hardware Model,
               Vendor ID, OLT Hardware Part Number and In-band interfaces MAC addresses.

               The gathered information will be output to the screen as a json string.
               When passed through a JSON processor (like jq) the output would look
               like below(sample output, the keys remain same and values change based on
               OLT model and binary versions)
               {
                   "hardware_information": {
                   "vendor": "Accton",
                   "hw_part_number": "FN1EC0816400Z",
                   "hw_version": "0.0.0.3",
                   "inband_interface": "34:12:78:56:01:00",
                   "hw_model": "ASXvOLT16-O-AC-F",
                   "serial_number": "EC1729003537"
                   },
                   "software_information": {
                   "onie_version": "2017.02.00.06",
                   "os_version": "3.7.10",
                   "os_name": "OpenNetworkLinux",
                   "openolt_version": "2.0.0",
                   "bal_version": "2.6.0.1"
                   }
               }

               This script exits with exit code 0 in case of success and 1 in case of failure.
"""

import os
import re
import json
import syslog
import yaml

# dictionary containing OLT information with hardware_information and
# software_information as keys and holds the respective data values
OLT_INFORMATION = {}

# Path where BAL libraries, configuration files, bal_core_dist
# and openolt binaries are located.
BRCM_DIR = '/broadcom'

# Path to vlan config file
VLAN_CONFIG_FILE = BRCM_DIR+"/vlan.config"

# Operating system name which is running at OLT.
# By default Open Network Linux (ONL) is used as operating
# system at OLT
OS_NAME = 'OpenNetworkLinux'

# BAL library version running at OLT

# openOLT version from OLT when available
OPENOLT_VERSION = " "

# in-band interface incase of device asgvolt64
ETH_1 = 'eth1'

# in-band interface incase of device asfvolt16
ETH_2 = 'eth2'

# Constants for the name of the keys in the JSON being printed
VENDOR = "vendor"
SERIAL_NUMBER = "serial_number"
HARDWARE_MODEL = "hw_model"
HARDWARE_PART_NUMBER = "hw_part_number"
HARDWARE_VERSION = "hw_version"
OPERATING_SYSTEM = "os_name"
OPERATING_SYSTEM_VERSION = "os_version"
ONIE_VERSION = "onie_version"
SOFTWARE_INFORMATION = "software_information"
HARDWARE_INFORMATION = "hardware_information"
INBAND_INTERFACE = "inband_interface"
BAL_VER = "bal_version"
OPENOLT_VER = "openolt_version"
OLT_MODEL=None
ASX_16 = "ASXvOLT16"
ASG_64 = "ASGvOLT64"

# log to syslog
syslog.openlog(facility=syslog.LOG_SYSLOG)

def get_olt_board_name():
    """
    Reads the bal package name

    This function fetchs bal package whether asfvolt16 or asgvolt64

    :return :  packge names if successful and None in case of failure.
    :rtype : tuple of package name in case of success, else None in case of failure.
    """

    try:
       OLT_MODEL = os.popen("cat /sys/devices/virtual/dmi/id/board_name").read().strip("\n")
       syslog.syslog(syslog.LOG_INFO, "successfully-read-olt-board-name")
       return OLT_MODEL
    except (IOError, NameError) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-executing-command-{}".format(exception))
        return None


def retrieve_vlan_id():
    """
    Retrieves VLAN ids of in-band interfaces.

    This function fetchs vlan ids from OLT /broadcom/bal_config.ini file.

    :return : vlan id of in-band interface if successfull and None in case of failure.
    :rtype : integer(valid vlan_id) in case of success, else None in case of failure.
    """

    eth_vlan = None

    # retrieving vlan ids based on the below two keys in bal_config.ini
    asf16_vlan = 'asfvolt16_vlan_id_eth2'
    asg64_vlan = 'asgvolt64_vlan_id_eth1'
    olt_model=get_olt_board_name()
    try:
        if os.path.exists(VLAN_CONFIG_FILE):
            with open(VLAN_CONFIG_FILE, "r") as file_descriptor:
                lines = file_descriptor.readlines()
                for line in lines:
                    if olt_model == ASX_16:
                        if re.search(asf16_vlan, line):
                            eth_vlan = int(line.split('=')[1].strip())
                    elif olt_model == ASG_64:
                        if re.search(asg64_vlan, line):
                            eth_vlan = int(line.split('=')[1].strip())
        else:
            syslog.syslog(syslog.LOG_ERR, "{}-file-does-not-exist".format(VLAN_CONFIG_FILE))
            return None, None
    except(EnvironmentError, re.error) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-retreving-vlan-ids-{}".format(exception))
        return None, None

    if eth_vlan > 4094 or eth_vlan < 1:
        syslog.syslog(syslog.LOG_ERR, "vlan-id-not-in-range-{}-{}".format(eth_vlan, eth_vlan_2))
        return None, None

    return eth_vlan


def get_olt_basic_info():
    """
     Fetch OLT basic information

     This function retireves OLT's basic information using the command 'onlpdump -s'

    :return: OLT's basic info in case of success and None in case of failure
    :rtype: json if success and None if failure
    """
    # fetch OLT basic information
    try:
        olt_info = os.popen('onlpdump -s').read()
        out = yaml.dump(yaml.load(olt_info)['System Information'])
        json_data = json.dumps(yaml.load(out))
        data = json.loads(json_data)
    except (IOError, TypeError, ValueError, yaml.YAMLError, yaml.MarkedYAMLError) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-fetching-olt-information-{}".format(exception))
        return None
    return data

def get_bal_openolt_version():
    """
     Fetch bal and openolt version

     This function retireves OLT's basic information using the command 'onlpdump -s'

     :return : bal and openolt version if successfull and None in case of failure.
     :rtype : tuple of bal and opneolt version in case of success, else None in case of failure.
    """
    try:
        os.chdir('/')
        os.environ["LD_LIBRARY_PATH"] = "."
        version_string = os.popen("./broadcom/openolt --version").read()
        version_tuple = tuple(version_string.split("\n"))
        openolt_v = version_tuple[0].split(":")
        OPENOLT_VERSION = openolt_v[1].split(" ")
        bal_v = version_tuple[1].split(":")
        BAL_VERSION = bal_v[1].split(" ")
        return OPENOLT_VERSION[1], BAL_VERSION[1]
    except (IOError, NameError) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-executing-command-{}".format(exception))
        return None, None


def fetch_olt_info():
    """
    Gather OLT software and hardware information.

    This function gather OLT information and returns  OLT
    software and hardware information.

    :return: OLT's software and hardware information in case of successful execution
             and returns None in case of failure
    :rtype: JSON string in case of success and None in case of failure.
    """

    hw_facts = {}
    sw_facts = {}

    # fetch olt basic information
    data = get_olt_basic_info()
    if data is None:
        return None

    olt_model=get_olt_board_name()
    # retrieving VLAN ids of in-band interfaces
    vlan = retrieve_vlan_id()
    try:
        if vlan is not None:
            # Retreiving MAC address for in-band interfaces
            if olt_model == ASX_16:
                macaddr = os.popen("ifconfig " + ETH_2 + "." + str(vlan) +
                                     " | grep -Po 'HWaddr " + r"\K.*$'").read().strip()
            elif olt_model == ASG_64:
                macaddr = os.popen("ifconfig " + ETH_1 + "." + str(vlan) +
                                     " | grep -Po 'HWaddr " + r"\K.*$'").read().strip()
        else:
            return None

        # get the operating system version details from olt
        operating_sys = os.popen("uname -a").read()
        os_tuple = tuple(operating_sys.split(" "))
        os_and_version = os_tuple[2].split("-")
        os_version = os_and_version[0]
    except (IOError, NameError) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-executing-command-{}".format(exception))
        return None

    openolt_v, bal_v = get_bal_openolt_version()

    # adding OLT hardware details to dictionary
    try:
        # adding OLT's in-band interfaces mac addresses
        hw_facts[INBAND_INTERFACE] = macaddr
        hw_facts[VENDOR] = data.get("Manufacturer", "")
        hw_facts[SERIAL_NUMBER] = data.get("Serial Number", "")
        hw_facts[HARDWARE_MODEL] = data.get("Product Name", "")
        hw_facts[HARDWARE_PART_NUMBER] = data.get("Part Number", "")
        hw_facts[HARDWARE_VERSION] = data.get("Diag Version", "")

        # adding OLT software details to dictionary
        sw_facts[OPERATING_SYSTEM] = OS_NAME
        sw_facts[OPERATING_SYSTEM_VERSION] = os_version
        sw_facts[ONIE_VERSION] = data.get('ONIE Version', "")
        sw_facts[BAL_VER] = bal_v
        sw_facts[OPENOLT_VER] = openolt_v

        # adding OLT hardware and software information to OLT_INFORMATION dictionary
        OLT_INFORMATION[SOFTWARE_INFORMATION] = sw_facts
        OLT_INFORMATION[HARDWARE_INFORMATION] = hw_facts

        # converting OLT information to json object
        olt_json_string = json.dumps(OLT_INFORMATION)

    except (TypeError, NameError) as exception:
        syslog.syslog(syslog.LOG_ERR, "error-serializing-to-json-data-{}".format(exception))
        return None

    return olt_json_string


if __name__ == "__main__":
    str_data = fetch_olt_info()
    if str_data is not None:
        print(str_data)
        syslog.syslog(syslog.LOG_INFO, "successfull-execution-printing-OLT-information")
        exit(0)
    else:
        print("error-occurred-exiting")
        syslog.syslog(syslog.LOG_ERR, "error-occurred-exiting")
        exit(1)

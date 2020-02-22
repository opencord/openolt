# OpenOLT agent

The *OpenOLT agent* runs on white box Optical Line Terminals (OLTs) and
provides a gRPC-based management and control interface to OLTs.

The OpenOLT agent is used by [VOLTHA](https://github.com/opencord/voltha)
through the [OpenOLT
adapter](https://github.com/opencord/voltha/tree/master/voltha/adapters/openolt).

OpenOLT agent uses Broadcom's BAL (Broadband Access Layer) software for Maple/Qumran chipsets based OLT such as the Edgecore/Accton ASXvOLT16.

```text

  +---------------------------------+
  |             VOLTHA              |
  |                                 |
  |       +------------------+      |
  |       | OpenOLT adapter  |      |
  +-------+--------+---------+------+
                   |
  OpenOLT gRPC API |
                   |
+--------------------- ---------------+
|                  |                  |
|         +------------------+        |
|         |   OpenOLT agent  |        |
|         +--------+---------+        |
|                  |                  |
| Vendor specific  | (e.g. Broadcom   |
|       API        |     BAL API      |
|                  |                  |
|         +------------------+        |
|         | Vendor SoC/FPGA  |        |
|         |(e.g Maple/Qumran)|        |
|         +------------------+        |
|                                     |
|           White box OLT             |
+-------------------------------------+

```

## Hardware requirements

A list of tested devices and optics can be found in the [CORD hardware
requirements](https://guide.opencord.org/prereqs/hardware.html#recommended-hardware)
guide, in the *R-CORD access equipment and optics* section.

## Pre-built debian packages of OpenOLT agent for Accton/Edgecore ASFVOLT16

Accton/Edgecore makes available pre-built debian packages of OpenOLT agent to their customers.
Get access credentials for https://edgecore.quickconnect.to and then login and navigate to
File_Station -> EdgecoreNAS, and then the folder /ASXvOLT16/OpenOLT_Agent/From_ONF_Distribution/
and pick the right version of deb package required for your testing.
voltha-2.3/openolt-2.3.0.deb is the latest version of package with support for BAL3.2.3.2
The pre-built debian packages have been tested on ONL-4.14. The ONL Installer required for
voltha-2.3/openolt-2.3.0.deb is also available at in the same path, i.e., voltha-2.3/.

More info on how to install ONL can be found on the official [ONL
website](https://opennetlinux.org/).

## Install OpenOLT

Copy the debian package to the OLT. For example:

```shell
scp openolt.deb root@10.6.0.201:~/.
```

Install the *openolt.deb* package using *dpkg*:

```shell
dpkg -i openolt.deb
```

Note that ONL required for BAL3.2.3.2 is ONL `4.14.109-OpenNetworkLinux`. This will be built as part of build procedure described `Build OpenOLT` section.

## Run OpenOLT as a Linux service

Rebooting the OLT (after the installation) will start dev_mgmt_daemon and openolt as init.d services:

Rebooting the OLT will start the dev_mgmt_daemon and openolt services:
```shell
reboot
```
The services can also be stopped/started manually:
```shell
service dev_mgmt_daemon stop
service openolt stop
service dev_mgmt_daemon start
service openolt start
```

Check the status of the services:
```shell
service dev_mgmt_daemon status
service openolt status
```

## Run OpenOLT in foreground

Running the dev_mgmt_daemon and/or openolt services in the forground is useful for development and debugging. Make sure to first stop the services if they are running in background.

Open a terminal and run the Broadcom BAL software (*dev_mgmt_daemon*):

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie
```
The `dev_mgmt_daemon` executable, when run in foreground, presents the CLI for Broadcom's BAL - Broadband Access Layer which is useful for debugging.

While the first executable still runs (even in background), open another
terminal and run *openolt*:

```shell
cd /broadcom
./openolt
```

> **NOTE**: the two executables will remain open in the terminals, unless they
> are put in background.

### Connect from VOLTHA

At the VOLTHA CLI, preprovision and enable the OLT:

```shell
(voltha) preprovision_olt -t openolt -H YOUR_OLT_MGMT_IP:9191
(voltha) enable
```

### Additional notes

* *9191* is the TCP port that the *OpenOLT* agent uses for its gRPC channel
* In the commands above, you can either use the loopback IP address (127.0.0.1)
  or substitute all its occurrences with the management IP of your OLT

## Build OpenOLT

### Supported BAL API versions

Currently, OpenOLT supports Broadcom's BAL API, version *3.2.3.2*.

### Proprietary software requirements

The following proprietary source code is required to build the OpenOLT agent.

* `SW-BCM686OLT_<BAL_VER>.tgz` - Broadcom BAL source and Maple SDK
* `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK
* `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch

The versions currently supported by the OpenOLT agent are:

* SW-BCM686OLT_3_2_3_2.tgz
* sdk-all-6.5.13.tar.gz
* ACCTON_BAL_3.2.3.2-V201912230101.patch

> NOTE: the repository does not contain the above three source packages.  These
> are needed to build the OpenOLT agent executable. Contact
> [Broadcom](mailto:dave.baron@broadcom.com) to access the source packages.

### System Requirements

**Hardware** :

CPU: Dual-core (4 Threads) up.

Memory: 6G bytes.

Hard Disk: 40G of disk free space.

**Software** :

1. OpenOLT agent builds on *Ubuntu 16.04 LTS*.

2. At least 4G of ram and 4G of swap -  compilation is memory intensive

3. Essential tools for building packages

       `sudo apt-get install build-essential ccache`

4. Install cmake version 3.5.0 or above. Download cmake-3.5.0.tar.gz and install it by untaring cmake tar file.
   Go to cmake-3.5.0 directory after untaring and execute

       `./bootstrap && make && make install`

### Build procedure

Clone the *openolt* repository either from GitHub or from OpenCORD Gerrit:

```shell
git clone git@github.com:opencord/openolt.git
or
git clone https://gerrit.opencord.org/openolt
```

Copy the Broadcom source and patch files to the openolt/agent/download directory:

```shell
cd <dir containing Broadcom source and patch files>
cp ACCTON_BAL_3.2.3.2-V201912230101.patch SW-BCM686OLT_3_2_3_2.tgz sdk-all-6.5.13.tar.gz <cloned openolt repo path>/agent/download
```

Run Autoconfig to generate the appropriate makefile scaffolding for the desired target

```shell
cd openolt/agent
./configure
```

Run *make prereq* to install the package dependencies.
This is usually a one-time thing, unless there is a change in the dependencies.

```shell
make OPENOLTDEVICE=asfvolt16 prereq
```

Run *make*. This can take a while to complete the first time, since it builds
ONL and the Broadcom SDKs. Following runs will be much faster, as they only
build the OpenOLT agent source.

```shell
make OPENOLTDEVICE=asfvolt16
```

Note that the required ONL vesion `4.14` is built as part of the above build procedure and is available at path `build/onl/OpenNetworkLinux/RELEASE/jessie/amd64/ONL-onl-4.14_ONL-OS8_2019-09-24.0446 b4af32e_AMD64_INSTALLED_INSTALLER`. This ONL Installer should be used to flash the OS on the OLT. 

If you need to use a specific version of voltha-protos, then specify the git tag/branch corresponding to
tht specific version as below. For ex:

```shell
make OPENOLTDEVICE=asfvolt16 OPENOLT_PROTO_VER=master
```

By default, the OPENOLT_PROTO_VER defaults to git tag *v3.1.0* of https://github.com/opencord/voltha-protos repo.

If the build process succeeds, libraries and executables will be created in the
*openolt/agent/build* directory.

Optionally, build the debian package that will be installed on the OLT.

```shell
make OPENOLTDEVICE=asfvolt16 deb
```

If the build process succeeds, the *openolt.deb* package will be created as
well in the *openolt/agent/build* directory.

NOTE: To compile for ASGvOLT 64 port GPON OLT, set `OPENOLTDEVICE` to `asgvolt64` during build procedure like below.

```shell
make OPENOLTDEVICE=asgvolt64
```

### Cleanup

To cleanup the repository and start the build procedure again, run:

```shell
# cleans up the agent objects, protos compiled artificats and openolt deb packages
make OPENOLTDEVICE=asfvolt16 clean

# cleans up the agent objects, protos compiled artificats, openolt deb packages and bal sources
make OPENOLTDEVICE=asfvolt16 distclean
```

## FAQ

The information here may be specific to specific OLT and ONU hardware such as Edgecore ASFVOLT16 OLT and Broadcom based ONUs.

### How to change speed of ASFVOLT16 NNI interface?

Auto-negotiation on the NNI (uplink) interfaces is not tested. By default, the OpenOLT agent sets the speed of the NNI interfaces to 100G. To downgrade the network interface speed to 40G, add the following lines at the end of the qax.soc (/broadcom/qax.soc) configuration file. A restart of the dev_mgmt_daemon and openolt executables is required after the change.

```shell
port ce128 sp=40000
```

### Why does the Broadcom ONU not forward eapol packets?

The firmware on the ONU is likely not setup to forward 802.1x on the linux bridge. Drop down to the shell in the Broadcom ONU's console and configure the Linux bridge to forward 802.1x.

```shell
> sh
# echo 8 > /sys/class/net/bronu513/bridge/group_fwd_mask
```

Version 1.7 of VOLTHA has a known issue where the ONU is only able to pass EAPOL packets from a specific LAN port (e.g. LAN port 1 on ALPHA ONUs)

### How do I check packet counters on the ONU?

LAN port packet counters:

```shell
bs /b/e port/index=lan{0,1,2,3,4,5,6}
```

WAN port packt counters:
```shell
bs /b/e port/index=wan0
```
### How to get access to MAPLE CLI on OLT box
To get access to the `BCM.0>` maple console please ssh into the OLT and then execute:
```shell
cd /broadcom
./example_user_appl
```

### How do I check packet counters on the OLT's PON interface?

Following is an example of retrieving the interface description for PON intf_id 0 (TODO: document PON interface numbering for Edgecore OLT).

```shell
BCM.0> a/t clear=no object=pon_interface sub=itu_pon_stats pon_ni=0
[-- API Start: bcmolt_stat_get --]
[-- API Message Data --]
object: pon_interface- PON Network Interface.
get stat subgroup: itu_pon_stats-ITU PON Statistics. loid: 0 response: OK
key={pon_ni=0}
data={
  fec_codewords=0
  bip_units=0
  bip_errors=0
  rx_gem=0
  rx_gem_dropped=0
  rx_gem_idle=0
  rx_gem_corrected=0
  rx_crc_error=0
  rx_fragment_error=0
  rx_packets_dropped=0
  rx_dropped_too_short=0
  rx_dropped_too_long=0
  rx_key_error=0
  rx_cpu_omci_packets_dropped=0
  tx_ploams=157
  rx_ploams_dropped=0
  rx_allocations_valid=0
  rx_allocations_invalid=0
  rx_allocations_disabled=0
  rx_ploams=39
  rx_ploams_non_idle=39
  rx_ploams_error=0
  rx_cpu=0
  rx_omci=0
  rx_omci_packets_crc_error=0
  tx_packets=0
  tx_gem=0
  tx_cpu=0
  tx_omci=0
  tx_dropped_illegal_length=0
  tx_dropped_tpid_miss=0
  tx_dropped_vid_miss=0
  tx_dropped_total=0
  rx_xgtc_headers=39
  rx_xgtc_corrected=0
  rx_xgtc_uncorrected=0
  fec_codewords_uncorrected=0
  rx_fragments_errors=0
}
[-- API Complete: 0 (OK) --]
```

### How do I check packet counters on the OLT's NNI interface?

Following command retrieves NNI intf_id 0:

```shell
BCM.0> a/t clear=no object=nni_interface sub=stats id=0
[-- API Start: bcmolt_stat_get --]
[-- API Message Data --]
object: nni_interface- nni_interface.
get stat subgroup: stats-NNI Interface Statistics. loid: 0 response: OK
key={id=0}
data={
  rx_bytes=40023
  rx_packets=283
  rx_ucast_packets=0
  rx_mcast_packets=283
  rx_bcast_packets=0
  rx_error_packets=0
  rx_unknown_protos=0
  tx_bytes=0
  tx_packets=0
  tx_ucast_packets=0
  tx_mcast_packets=0
  tx_bcast_packets=0
  tx_error_packets=0
}
[-- API Complete: 0 (OK) --]
```

### How do I list flows installed in the OLT?

Following command lists the flows installed on the OLT.

```shell
BCM.0> a/m max_msgs=100 filter_invert=no object=flow
```

### How do I access device CLI when running dev_mgmt_daemon and openolt agent binary in background?

Navigate to `/opt/bcm68620` and execute the command `/broadcom/dev_mgmt_attach` to access the QAX
diag shell. Note that you will not be able to access the QAX diag shell, you need to recompile the
BAL SDK with `SW_UTIL_SHELL=y` option.

### DBA Scheduler fail to create on the OLT with errors related to Bandwidth configuration

Ensure that `additional_bw` indicator in the Technology Profile and the `bandwidthprofile` configured in ONOS netcfg for the subscriber are following the below guideline.

* When `additional_bw` is configured as `AdditionalBW_BestEffort`, ensure `cir` + `eir`
  values in ONOS netcfg BW profile for the subscriber are greater than or equal to *16000*
  for XGSPON, and it is greater than equal to *32000* for GPON. Also ensure that `eir`
  is a non-zero value.
* When `additional_bw` is configured as `AdditionalBW_NA`, ensure that both `cir` and `eir`
  are non-zero values, and `cir` + `eir` is greater than or equal to *16000* for XGSPON and
  *32000* for GPON.
* When `additional_bw` is configured as `AdditionalBW_None`, ensure that `eir` is 0 and `cir`
  is non-zero and `cir` is greater than or equal to *16000* for XGSPON, and it is greater than
  equal to *32000* for GPON.

For more details about BW profile parameters, please refer below links.

[MEF Whitepaper - Bandwidth-Profiles-for-Ethernet-Services](https://www.mef.net/Assets/White_Papers/Bandwidth-Profiles-for-Ethernet-Services.pdf)
[Technology Profile Implementation Note](https://www.opennetworking.org/wp-content/uploads/2019/09/2pm-Shaun-Missett-Technology-Profile-and-Speed-Profile-Implementation.pdf)


## Known Issues

* The Minimum BW that should be configured for ITU PON Alloc Object has changed from 16Kbps
  to 32Kbps from BAL3.1 to BAL3.2 release if you are using ALPHA ONUs.
  As per BAL3.x documents, when FEC is disabled, the minimum BW is 16Kbps on the ITU PON Alloc Object.
  This seems to be a discrepency on the ALPHA ONU. So, ensure that `cir` + `eir` value is greater than
  equal to *32000* for XGSPON use case for ALPHA ONU.


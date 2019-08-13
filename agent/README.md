# OpenOLT agent

The *OpenOLT agent* runs on white box Optical Line Terminals (OLTs) and
provides a gRPC-based management and control interface to OLTs.

The OpenOLT agent is used by [VOLTHA](https://github.com/opencord/voltha)
through the [OpenOLT
adapter](https://github.com/opencord/voltha/tree/master/voltha/adapters/openolt).

OpenOLT agent currently supports Broadcom's Maple/Qumran chipsets.

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
requirements](https://github.com/opencord/docs/blob/master/prereqs/hardware.md#suggested-hardware)
guide, in the *R-CORD access equipment and optics* section.

## Pre-built debian packages of OpenOLT agent for Accton/Edgecore ASFVOLT16

Accton/Edgecore makes available pre-built debian packages of OpenOLT agent to their customers.
Contact your Accton/Edgecore representative for more information.

The pre-built debian packages have been tested on [this specific version of
OpenNetworkingLinux
(ONL)](https://github.com/opencord/OpenNetworkLinux/releases/download/20180124-olt-kernel-3.7.10/ONL-2.0.0_ONL-OS_2018-01-24.0118-1303f20_AMD64_INSTALLED_INSTALLER).

More info on how to install ONL can be found on the official [ONL
website](https://opennetlinux.org/docs/deploy).

## Install OpenOLT

Copy the debian package to the OLT. For example:

```shell
scp openolt.deb root@10.6.0.201:~/.
```

Install the *openolt.deb* package using *dpkg*:

```shell
dpkg -i openolt.deb
```

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

Currently, OpenOLT support the Broadcom BAL APIs, version *3.1.1.1*.

### Proprietary software requirements

The following proprietary source code is required to build the OpenOLT agent.

* `SW-BCM68620_<BAL_VER>.zip` - Broadcom BAL source and Maple SDK
* `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK
* `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch

The versions currently supported by the OpenOLT agent are:

* SW-BCM68620_3_1_1_1.tgz
* sdk-all-6.5.13.tar.gz
* ACCTON_BAL_3.1.1.1-V201908010203.patch

> NOTE: the repository does not contain the above four source packages.  These
> are needed to build the OpenOLT agent executable. Contact
> [Broadcom](mailto:dave.baron@broadcom.com) to access the source packages.

### System Requirements

OpenOLT agent builds on *Ubuntu 16.04 LTS*.

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
cp SW-BCM68620_3_1_1_1.tgz sdk-all-6.5.13.tar.gz ACCTON_BAL_3.1.1.1-V201908010203.patch <cloned openolt repo path>/agent/download
```

Run Autoconfig to generate the appropriate makefile scaffolding for the desired target

```shell
cd openolt
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

If the build process succeeds, libraries and executables will be created in the
*openolt/agent/build* directory.

Optionally, build the debian package that will be installed on the OLT.

```shell
make OPENOLTDEVICE=asfvolt16 deb
```

If the build process succeeds, the *openolt.deb* package will be created as
well in the *openolt/agent/build* directory.

### Cleanup

To cleanup the repository and start the build procedure again, run:

```shell
make OPENOLTDEVICE=asfvolt16 clean-all
```

## FAQ

### How to change speed of ASFVOLT16 NNI interface?

Auto-negotiation on the NNI (uplink) interfaces is not tested. By default, the OpenOLT agent sets the speed of the NNI interfaces to 100G. To downgrade the network interface speed to 40G, add the following lines at the end of the qax.soc (/broadcom/qax.soc) configuration file. A restart of the dev_mgmt_daemon and openolt executables is required after the change.

```shell
port ce128 sp=40000
```

This change can also be made at run-time from the CLI of the dev_mgmt_daemon:

```shell
d/s/shell
port ce128 speed=40000
```
(It is safe to ignore the error msgs.)

### How do I configure rate limiting?

By default, the full 1G xgs-pon bandwidth is available to an ONU when no rate-limiting is applied. There is experimental support available to change the default bandwith and also rate-limit specific ONUs from the voltha CLI.


Configure default rate limit (PIR = 5Mbps):

```shell
(voltha) xpon
(voltha-xpon )traffic_descriptor_profile create -n "default" -f 1000 -a 1000 -m 5000
```


Configure ONU BRCM12345678 to be rate-limited to 100Mbps:
```shell
(voltha) xpon
(voltha-xpon )traffic_descriptor_profile create -n "default" -f 50000 -a 70000 -m 100000
```

### Why does the Broadcom ONU not forward eapol packets?

The firmware on the ONU is likely not setup to forward 802.1x on the linux bridge. Drop down to the shell in the Broadcom ONU's console and configure the Linux bridge to forward 802.1x.

```shell
> sh
# echo 8 > /sys/class/net/bronu513/bridge/group_fwd_mask
```

### How do I check packet counters on the ONU?

LAN port packet counters:

```shell
bs /b/e port/index=lan{0,1,2,3,4,5,6}
```

WAN port packt counters:
```shell
bs /b/e port/index=wan0
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

#### How do I check packet counters on the OLT's NNI interface?

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

In the dev_mgmt_daemon CLI:

```shell
> ~, Debug/, Maple/, Board/, Cld/, Transport/, Logger/, Quit
> Debug
.../Debug> Rsc_mgr/, Mac_util/, Switch/, sLeep, sHow_config, os/
> Mac_util
.../Mac_util> Print_flows, pRint_flows_for_gem, prInt_flows_for_alloc_id, Epon_helper
> Print_flow
```

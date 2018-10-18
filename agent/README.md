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

Rebooting the OLT (after the installation) will start bal_core_dist and openolt as init.d services:

Rebooting the OLT will start the bal_core_dist and openolt services:
```shell
reboot
```
The services can also be stopped/started manually:
```shell 
service bal_core_dist stop
service openolt stop
service bal_core_dist start
service openolt start
```

Check the status of the services:
```shell
service bal_core_dist status
service openolt status
```

## Run OpenOLT in foreground

Running the bal_core_dist and/or openolt services in the forground is useful for development and debugging. Make sure to first stop the services if they are running in background.

Open a terminal and run the Broadcom BAL software (*bal_core_dist*):

```shell
cd /broadcom
./bal_core_dist -C :55001
```

While the first executable still runs (even in background), open another
terminal and run *openolt*:

```shell
cd /broadcom
./openolt -C 127.0.0.1:55001
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

Currently, OpenOLT support the Broadcom BAL APIs, version *2.6.0.1*.

### Proprietary software requirements

The following proprietary source code is required to build the OpenOLT agent.

* `SW-BCM68620_<BAL_VER>.zip` - Broadcom BAL source and Maple SDK
* `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK
* `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch
* `OPENOLT_BAL_<BAL_VER>.patch` - A patch to Broadcom software to allow
  compilation with C++ based openolt

The versions currently supported by the OpenOLT agent are:

* SW-BCM68620_2_6_0_1.zip
* sdk-all-6.5.7.tar.gz
* ACCTON_BAL_2.6.0.1-V201804301043.patch
* OPENOLT_BAL_2.6.0.1.patch

> NOTE: the repository does not contain the above four source packages.  These
> are needed to build the OpenOLT agent executable. Contact
> [Broadcom](mailto:dave.baron@broadcom.com) to access the source packages.

### System Requirements

OpenOLT agent builds on *Ubuntu 14.04 LTS*.

### Build procedure

Clone the *openolt* repository either from GitHub or from OpenCORD Gerrit:

```shell
git clone git@github.com:opencord/openolt.git
or
git clone https://gerrit.opencord.org/openolt
```

Copy the Broadcom source and patch files to the openolt/download directory:

```shell
cd openolt/download
cp SW-BCM68620_2_6_0_1.zip sdk-all-6.5.7.tar.gz ACCTON_BAL_2.6.0.1-V201804301043.patch OPENOLT_BAL_2.6.0.1.patch ./download
```

Run Autoconfig to generate the appropriate makefile scaffolding for the desired target

```shell
cd openolt
./configure
```

Run *make prereq* to install the package dependencies.
This is usually a one-time thing, unless there is a change in the dependencies.

```shell
make prereq
```

Run *make*. This can take a while to complete the first time, since it builds
ONL and the Broadcom SDKs. Following runs will be much faster, as they only
build the OpenOLT agent source.

```shell
make
```

If the build process succeeds, libraries and executables will be created in the
*openolt/build* directory.

Optionally, build the debian package that will be installed on the OLT.

```shell
make deb
```

If the build process succeeds, the *openolt.deb* package will be created as
well in the *openolt/build* directory.

### Cleanup

To cleanup the repository and start the build procedure again, run:

```shell
make clean-all
```

## FAQ

### How to change speed of ASFVOLT16 NNI interface?

Auto-negotiation on the NNI (uplink) interfaces is not tested. By default, the OpenOLT agent sets the speed of the NNI interfaces to 100G. To downgrade the network interface speed to 40G, add the following lines at the end of the qax.soc (/broadcom/qax.soc) configuration file. A restart of the bal_core_dist and openolt executables is required after the change.

```shell
port ce128 sp=40000
```

This change can also be made at run-time from the CLI of the bal_core_dist:

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
ACC.0>b/t clear=no object=interface intf_id=0 intf_type=pon
[-- API Start: bcmbal_stat_get --]
[-- API Message Data --]
object: interface - BAL interface object
get stat: response: OK
key:
   intf_id=0
   intf_type=pon
data:
   rx_bytes=18473516
   rx_packets=176416
   rx_ucast_packets=30627
   rx_mcast_packets=2230
   rx_bcast_packets=143559
   rx_error_packets=0
   rx_unknown_protos=0
   rx_crc_errors=0
   bip_errors=0
   tx_bytes=5261350
   tx_packets=39164
   tx_ucast_packets=30583
   tx_mcast_packets=0
   tx_bcast_packets=8581
   tx_error_packets=0
[-- API Complete: 0 (OK) --]
```

#### How do I check packet counters on the OLT's NNI interface?

Following command retrieves NNI intf_id 0:

```shell
ACC.0>b/t clear=no object=interface intf_id=0 intf_type=nniCollecting statistics
[-- API Start: bcmbal_stat_get --]
[-- API Message Data --]
object: interface - BAL interface object
get stat: response: OK
key:
   intf_id=0
   intf_type=nni
data:
   rx_bytes=8588348
   rx_packets=69774
   rx_ucast_packets=61189
   rx_mcast_packets=0
   rx_bcast_packets=8585
   rx_error_packets=0
   rx_unknown_protos=0
   tx_bytes=35354878
   tx_packets=347167
   tx_ucast_packets=61274
   tx_mcast_packets=4447
   tx_bcast_packets=281446
   tx_error_packets=0
[-- API Complete: 0 (OK) --]
```

### How do I list flows installed in the OLT?

In the bal_core_dist CLI:

```shell
> ~, Debug/, Maple/, Board/, Cld/, Transport/, Logger/, Quit
> Debug
.../Debug> Rsc_mgr/, Mac_util/, Switch/, sLeep, sHow_config, os/
> Mac_util
.../Mac_util> Print_flows, pRint_flows_for_gem, prInt_flows_for_alloc_id, Epon_helper
> Print_flow
```

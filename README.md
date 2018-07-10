# OpenOLT driver

The *OpenOLT driver* runs on white box Optical Line Terminals (OLTs) and
provides a gRPC-based management and control interface to OLTs.

The OpenOLT driver is used by [VOLTHA](https://github.com/opencord/voltha)
through the [OpenOLT
adapter](https://github.com/opencord/voltha/tree/master/voltha/adapters/openolt).

OpenOLT driver currently supports Broadcom's Maple/Qumran chipsets.

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
|         | OpenOLT driver   |        |
|         +--------+---------+        |
|                  |                  |
| Broadcom BAL API |                  |
|                  |                  |
|         +------------------+        |
|         | Maple/Qumran SDK |        |
|         +------------------+        |
|                                     |
|           White box OLT             |
+-------------------------------------+

```

## Hardware requirements

A list of tested devices and optics can be found in the [CORD hardware
requirements](https://github.com/opencord/docs/blob/master/prereqs/hardware.md#suggested-hardware)
guide, in the *R-CORD access equipment and optics* section.

## Get the pre-built debian package

Currently, [EdgeCore](mailto:jeff_catlin@edge-core.com) can privately
distribute the latest OpenOLT debian package to its customers. Contact [Jeff
Catlin](mailto:jeff_catlin@edge-core.com) for more information.

## Prerequisites

The debian package has been tested on [this specific version of
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

Reboot the OLT:

```shell
reboot
```

## Run OpenOLT

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

* *9191* is the TCP port that the *OpenOLT* driver uses for its gRPC channel
* In the commands above, you can either use the loopback IP address (127.0.0.1)
  or substitute all its occurrences with the management IP of your OLT

## Build OpenOLT

### Supported BAL API versions

Currently, OpenOLT support the Broadcom BAL APIs, version *2.6.0.1*.

### Proprietary software requirements

The following proprietary source code is required to build the OpenOLT driver.

* `SW-BCM68620_<BAL_VER>.zip` - Broadcom BAL source and Maple SDK
* `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK
* `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch
* `OPENOLT_BAL_<BAL_VER>.patch` - A patch to Broadcom software to allow
  compilation with C++ based openolt

The versions currently supported by the OpenOLT driver are:

* SW-BCM68620_2_6_0_1.zip
* sdk-all-6.5.7.tar.gz
* ACCTON_BAL_2.6.0.1-V201804301043.patch
* OPENOLT_BAL_2.6.0.1.patch

> NOTE: the repository does not contain the above four source packages.  These
> are needed to build the OpenOLT driver executable. Contact
> [Broadcom](mailto:dave.baron@broadcom.com) to access the source packages.

### System Requirements

OpenOLT driver builds on *Ubuntu 14.04 LTS*.

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

Run *make prereq* to install the package dependencies.
This is usually a one-time thing, unless there is a change in the dependencies.

```shell
make prereq
```

Run *make*. This can take a while to complete the first time, since it builds
ONL and the Broadcom SDKs. Following runs will be much faster, as they only
build the OpenOLT driver source.

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

### Change speed of ASFVOLT16 NNI interface

Auto-negotiation on the NNI (uplink) interfaces is not tested. By default, the OpenOLT driver sets the speed of the NNI interfaces to 100G. To downgrade the network interface speed to 40G, add the following lines at the end of the qax.soc (/broadcom/qax.soc) configuration file. A restart of the bal_core_dist and openolt executables is required after the change.

```shell
port ce128 sp=40000
```

This change can also be made at run-time from the CLI of the bal_core_dist:

```shell
d/s/shell
port ce128 speed=40000
```
(It is safe to ignore the error msgs.)

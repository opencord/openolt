# OpenOLT driver

**OpenOLT driver** runs on white box Optical Line Terminals (OLTs) and provides a gRPC-based management and control interface to the OLT.

The **OpenOLT driver** is used by [VOLTHA](https://github.com/opencord/voltha) through the [OpenOLT adapter](https://github.com/opencord/voltha/tree/master/voltha/adapters/openolt).

**OpenOLT driver** currently supports Broadcom's Maple/Qumran chipsets.

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

## Supported BAL API versions

- 2.4.3.6

## Proprietary software requirements

The following proprietary source code is required to build **OpenOLT driver**.

- `SW-BCM68620_<BAL_VER>.zip` - Broadcom BAL source and Maple SDK.
- `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK.
- `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch.
- `OPENOLT_BAL_<BAL_VER>.patch` - A patch to Broadcom software to allow compilation with C++ based openolt.

The versions currently supported by **OpenOLT driver** are:

- SW-BCM68620_2_4_3_6.zip
- sdk-all-6.5.7.tar.gz
- ACCTON_BAL_2.4.3.6-V201710131639.patch
- OPENOLT_BAL_2.4.3.6.patch

Note - This repo does not contain the above four source packages. These are needed to build the **OpenOLT driver** executable. Contact Broadcom for access to these source packages.

## System Requirements

**OpenOLT driver** builds on Ubuntu 14.04.

## Build

- Clone this repo either from the github mirror or from opencord gerrit:

```shell
git clone git@github.com:opencord/openolt.git
or
git clone https://gerrit.opencord.org/openolt
```

- Copy Broadcom sources (and patches) to the openolt/download directory:

```shell
cd openolt/download
cp SW-BCM68620_2_4_3_6.zip sdk-all-6.5.7.tar.gz ACCTON_BAL_2.4.3.6-V201710131639.patch OPENOLT_BAL_2.4.3.6.zip ./download
```

- Run "make prereq" to install package dependencies. This is usually a one-time thing  (unless there is change in the dependencies).

```shell
make prereq
```

- Run "make" or "make all". This can take a while to complete the first time around since it builds ONL and Broadcom SDKs. Subsequent runs are much faster as they only build the **OpenOLT driver** source.

```shell
make
```

- Finally, build the debian package that will be installed on the OLT.

```shell
make deb
```

- If the build succeeds, the **openolt.deb** package is created in the **openolt/build** directory. Copy **openolt.deb** to the OLT

```shell
scp openolt/build/openolt.deb root@10.6.0.201:~/.
```

## Install

- Install the openolt.deb package on the OLT

```shell
dpkg -i openolt.deb
```

- Reboot the OLT

```shell
reboot
```

## Run

- Run bal_core_dist in one terminal:

```shell
cd /broadcom
./bal_core_dist -C 10.6.0.201:40000 -A 10.6.0.201:50000
```

- Run openolt in another terminal:

```shell
cd /broadcom
./openolt  -C 10.6.0.201:40000 -A 10.6.0.201:50000
```

## Connect from VOLTHA

- In VOLTHA cli, preprovision and enable the OLT:

```shell
(voltha) preprovision_olt -t openolt -H 10.6.0.201:9191
(voltha) enable
```

Note:

- **OpenOLT driver** uses port 9191 for its gRPC channel
- Substitute 10.6.0.201 in above steps with mgmt IP of your OLT

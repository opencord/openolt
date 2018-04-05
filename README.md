# openolt

**openolt** is an SDN Agent for white box Optical Line Terminals (OLTs). It exposes a well-documented, north-bound protobuf/gRPC [API](https://github.com/shadansari/openolt-api) for SDN controllers. [VOLTHA](https://github.com/opencord/voltha) support for **openolt** is under development. 

**openolt** currently supports Broadcom's Maple/Qumran chipsets.

```

               +-------------------------------+
               |         SDN Controller        |
               |    (e.g. VOLTHA, ONOS, ODL)   |
               +---------------+---------------+
                               |
              openolt gRPC API |
                               |
        +---------------------------------------------+
        |                      |                      |
        |      +---------------+---------------+      |
        |      |            openolt            |      |
        |      +---------------+---------------+      |
        |                      |                      |
        |  vendor-specific API |                      |
        |                      |                      |
        |      +---------------+---------------+      |
        |      |   vendor asic/soc/fpga/...    |      |
        |      +-------------------------------+      |
        |                                             |
        |                                             |
        |                White box OLT                |
        +---------------------------------------------+

```
## Software versions

The following vendor proprietary source files are required to build **openolt**. These files can be obtained from Broadcom under NDA. Once an NDA is signed with Broadcom, these files are made available by Broadcom on their Customer Service Portal (CSP) via case CS3233745.

- SW-BCM68620_<BAL_VER>.zip - Broadcom BAL source and Maple SDK.
- sdk-all-<SDK_VER>.tar.gz - Broadcom Qumran SDK.
- ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch - Accton/Edgecore's patch.
- OPENOLT_BAL_<BAL_VER>.patch - A patch to Broadcom software to allow compilation with C++ based openolt.

The versions that have been tested are:

- SW-BCM68620_2_4_3_6.zip
- sdk-all-6.5.7.tar.gz
- ACCTON_BAL_2.4.3.6-V201710131639.patch
- OPENOLT_BAL_2.4.3.6.zip

## Build

Clone this repo, copy above four source code zip/patch files to the openolt/download directory, and run make:
```shell
git clone git@github.com:OpenOLT/openolt.git

mv SW-BCM68620_2_4_3_6.zip sdk-all-6.5.7.tar.gz ACCTON_BAL_2.4.3.6-V201710131639.patch OPENOLT_BAL_2.4.3.6.zip openolt/download

make
```
The **build** directory contains all the build artifacts:

- **release_asfvolt16_V02.04.201710131639.tar.gz** - Broadcom BAL (specifically, this tar ball includes the bal_core_dist)
- **openolt** - the **openolt** agent/driver
- **libgrpc++_reflection.so.1  libgrpc++.so.1  libgrpc.so.5** - gRPC libraries

## Copy files to target OLT

1. Untar **release_asfvolt16_V02.04.201710131639.tar.gz** in the root (top level) directory
2. Copy **openolt** to /broadcom
3. Copy the gRPC libraries to /usr/local/lib


## Run

- Run bal_core_dist:
```shell
cd /broadcom
ldconfig
./bal_core_dist -C 10.6.0.201:40000 -A 10.6.0.201:50000
```
- Run openolt:
```shell
cd /broadcom
ldconfig
openolt  -C 10.6.0.201:40000 -A 10.6.0.201:50000
```
(Note - Substitute 10.6.0.201 with a local IP of the OLT - e.g. mgmt interface IP).

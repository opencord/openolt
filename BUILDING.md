# Build OpenOLT

## Supported BAL API versions

Currently, OpenOLT supports Broadcom's BAL API, version *3.4.9.6*.

## Proprietary software requirements

The following proprietary source code is required to build the OpenOLT agent.

* `SW-BCM686OLT_<BAL_VER>.tgz` - Broadcom BAL source and Maple SDK
* `sdk-all-<SDK_VER>.tar.gz` - Broadcom Qumran SDK
* `ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch` - Accton/Edgecore's patch

The versions currently supported by the OpenOLT agent are:

* `SW-BCM686OLT_3_4_9_6.tgz`
* `sdk-all-6.5.13.tar.gz`
* `ACCTON_BAL_3.4.9.6-V202008310101.patch`. This is downloadable from the common CSP CS00003233745.

> NOTE: the repository does not contain the above three source packages.  These
> are needed to build the OpenOLT agent executable. Contact [Dave Baron at
> Broadcom](mailto:dave.baron@broadcom.com) to access the source packages.

## System Requirements

**Hardware** :

CPU: Dual-core (4 Threads) up.

Memory: 6GB

Storage: 50GB of free space.

**Software** :

1. OpenOLT agent builds on *Debian GNU/Linux 8.11.1 (jessie)* . The ISO installer image is downloadble from [here](https://cdimage.debian.org/cdimage/archive/8.11.1/amd64/iso-cd/debian-8.11.1-amd64-netinst.iso)

2. At least 4G of ram and 4G of swap -  compilation is memory intensive

3. Essential tools for building packages

Install the following packages

   `apt-get install -y git pkg-config build-essential autoconf libgflags-dev clang libc++-dev unzip libssl-dev gawk debhelper debhelper dh-systemd init-system-helpers curl cmake ccache g++-4.9 wget ca-certificates lcov libgoogle-glog-dev libpcap-dev`

Follow the instructions [here](https://docs.docker.com/engine/install/debian/) to install `docker-ce`. It is not necessary to install `docker-ce-cli` and `containerd.io`.

4. Install cmake version 3.5.1 or above.

```shell
cd /tmp
git clone -b v3.5.1 https://gitlab.kitware.com/cmake/cmake.git
cd /tmp/cmake
./bootstrap
make
make install
```

5. Install gRPC version v1.31.1

``` shell
cd /tmp
git clone -b v1.31.1  https://github.com/grpc/grpc
cd /tmp/grpc
git submodule update --init
mkdir -p cmake/build
cd cmake/build
cmake ../.. -DgRPC_INSTALL=ON                \
            -DCMAKE_BUILD_TYPE=Release       \
            -DgRPC_ABSL_PROVIDER=module     \
            -DgRPC_CARES_PROVIDER=module    \
            -DgRPC_PROTOBUF_PROVIDER=module \
            -DgRPC_RE2_PROVIDER=module      \
            -DgRPC_SSL_PROVIDER=module      \
            -DgRPC_ZLIB_PROVIDER=module
make
make install
# copy library and grpc_cpp_plugin to path below.
cp `find . -name "*.a"` /usr/local/lib/
cp `find . -name grpc_cpp_plugin` /usr/local/bin/
```

6. Install PcapPlusPlus library

```shell
git clone -b v20.08 https://github.com/seladb/PcapPlusPlus.git
./configure-linux.sh —default
make all
make install
```

7. Install latest version of Git (optional)

By default the apt-get install an older version of git (2.1.4) on debian jessie 8.11.1. This older version does not support some of the latest git options. You may want to install a later version of git using procedure below.

```shell
apt-get remove --purge git ## Removes older verion of git
apt update
apt install make libssl-dev libghc-zlib-dev libcurl4-gnutls-dev libexpat1-dev gettext unzip
wget https://github.com/git/git/archive/v2.18.0.zip -O git.zip
unzip git.zip
cd git-*
make prefix=/usr/local all
sudo make prefix=/usr/local install
```

Note1: The build environment has been validated with only Jessie 8.11.1 64bit AMD64 OS only.
Note2: Make sure you are using g++-4.9 as the default g++ compiler version on your build system. The grpc libraries and openolt agent code has to be compiled with this g++ version.

## Build procedure

Clone the `openolt` repository either from OpenCORD Gerrit:

```shell
git clone https://gerrit.opencord.org/openolt
```

Copy the Broadcom source and patch files to the openolt/agent/download directory:

```shell
cd <dir containing Broadcom source and patch files>
cp ACCTON_BAL_3.4.9.6-V202008310101.patch SW-BCM686OLT_3_4_9_6.tgz sdk-all-6.5.13.tar.gz <cloned openolt repo path>/agent/download
```

Run the configure script to generate the appropriate Makefile scaffolding for
the desired target:

```shell
cd openolt/agent
./configure
```

Run *make*. This can take a while to complete the first time, since it builds
ONL and the Broadcom SDKs. Following runs will be much faster, as they only
build the OpenOLT agent source.

```shell
make OPENOLTDEVICE=asfvolt16
```

Note that the required ONL version `4.14` is built as part of the above build
procedure and is available at path
`build/onl/OpenNetworkLinux/RELEASE/jessie/amd64/ONL-onl-4.14_ONL-OS8_2020-01-30.0413-72b95a7_AMD64_INSTALLED_INSTALLER`.
This ONL Installer should be used to flash the OS on the OLT.

If you need to use a specific version of voltha-protos, then specify the git
tag/branch corresponding to that specific version:

```shell
make OPENOLTDEVICE=asfvolt16 OPENOLT_PROTO_VER=master
```

By default, the `OPENOLT_PROTO_VER` defaults to git tag *v4.0.3* of the
[voltha-protos](https://gerrit.opencord.org/gitweb?p=voltha-protos.git;a=summary)
repo.

Optionally, for the NNI port default setting, the options could make the default setting of the NNI port speed.

* 100G port change to 40Gbps speed
```shell
make OPENOLTDEVICE=asfvolt16 PORT_100G_SPEED=40000
```
* 100G port change to 10Gbps speed (use Break-out cable)
```shell
make OPENOLTDEVICE=asfvolt16 PORT_100G_SPEED=10000
```
* 25G port change to 20Gbps speed (Accton/Edgecore ASGvOLT64)
```shell
make OPENOLTDEVICE=asgvolt64 PORT_100G_SPEED=100000 PORT_25G_SPEED=20000
```
* 25G port change to 10Gbps speed (Accton/Edgecore ASGvOLT64)
```shell
make OPENOLTDEVICE=asgvolt64 PORT_100G_SPEED=100000 PORT_25G_SPEED=10000
```
* 25G port change to 1Gbps speed (Accton/Edgecore ASGvOLT64)
```shell
make OPENOLTDEVICE=asgvolt64 PORT_100G_SPEED=100000 PORT_25G_SPEED=1000
```

If the build process succeeds, libraries and executables will be created in the
*openolt/agent/build* directory.

Optionally, build the debian package that will be installed on the OLT.

```shell
make OPENOLTDEVICE=asfvolt16 deb
```

If the build process succeeds, a `.deb` package will be created as well in the
`openolt/agent/build` directory.

Run make with inband option as specified below to build all-in-one ONL image
packed with Openolt debian package and Inband OLT startup scripts(Scripts
to enable Inband channel and start dev_mgmt_daemon and openolt services).
This can take a while to complete the first time, since it builds
ONL and the Broadcom SDKs. Following runs will be much faster, as they try to
build OpenOLT agent source and Inband ONL with modified Openolt deb package.

```shell
make OPENOLTDEVICE=asfvolt16 INBAND=y VLAN_ID=<INBAND_VLAN>
e.g:
make OPENOLTDEVICE=asfvolt16 INBAND=y VLAN_ID=5
```

If no VLAN_ID is specified in above command it defaults to 4093.

Note that the required INBAND ONL version `4.14` is built as part of the above
build procedure and is available at path
`build/onl/OpenNetworkLinux/RELEASE/jessie/amd64/ONL-onl-4.14_ONL-OS8_2020-04-22.
2206-b4af32e_AMD64_INSTALLED_INSTALLER\.`
This ONL Installer should be used to flash the OS on the OLT.

NOTE: To compile for ASGvOLT 64 port GPON OLT, set `OPENOLTDEVICE` to
`asgvolt64` during build procedure like below.

```shell
make OPENOLTDEVICE=asgvolt64
```

## Cleanup

To cleanup the repository and start the build procedure again, run:

```shell
# cleans up the agent objects, protos compiled artificats and openolt deb packages
make OPENOLTDEVICE=asfvolt16 clean

# cleans up the agent objects, protos compiled artificats, openolt deb packages and bal sources
make OPENOLTDEVICE=asfvolt16 distclean
```

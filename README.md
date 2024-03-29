# OpenOLT agent

The *OpenOLT agent* runs on white box Optical Line Terminals (OLTs) and
provides a gRPC-based management and control interface to OLTs.

The OpenOLT agent is used by [VOLTHA](https://github.com/opencord/voltha)
through the [OpenOLT
adapter](https://github.com/opencord/voltha/tree/master/voltha/adapters/openolt).

OpenOLT agent uses Broadcom's BAL (Broadband Adaptation Layer) software for
interfacing with the Maple/Qumran chipsets in OLTs such as the Edgecore/Accton
ASXvOLT16 and with Aspen/Qumran chipsets in OLTs such as the Radisys RLT-3200G-W.
The current version of the Broadcom BAL integrated with OpenOLT agent is BAL3.10.2.2.

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
requirements](https://guide.opencord.org/master/prereqs/hardware.html#recommended-hardware)
guide, in the *R-CORD access equipment and optics* section.

## Pre-built debian packages of OpenOLT agent for Accton/Edgecore ASFVOLT16/ASGvOLT64

Accton/Edgecore makes available pre-built debian packages of OpenOLT agent to
their customers.  Get access credentials for
[edgecore.quickconnect.to](https://edgecore.quickconnect.to) and then login and
navigate to `File_Station -> EdgecoreNAS`, and then the folder
`/ASXvOLT16/OpenOLT_Agent/From_ONF_Distribution/` and pick the right version of
`.deb` package required for your testing.

## Install OpenOLT

Copy the debian package to the OLT. For example:

```shell
scp openolt.deb root@10.6.0.201:~/.
```

Install the *openolt.deb* package using *dpkg*:

```shell
dpkg -i openolt_<OPENOLTDEVICE>-<Version>-<GIT Commit ID>.deb
```

## Run OpenOLT as a Linux service

Rebooting the OLT (after the installation) will start `dev_mgmt_daemon` and
`openolt` as system services:

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

## Run OpenOLT in foreground for development and debugging

Running the `dev_mgmt_daemon` and/or `openolt` services in the foreground is
useful for development and debugging. Make sure to first stop the services if
they are running in background.

```shell
service dev_mgmt_daemon stop
service openolt stop
```

Open a terminal and run the Broadcom BAL software (`dev_mgmt_daemon`):

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie
```

Perform `dev_mgmt_daemon` and change NNI port speed:

* 100G port change to 40Gbps speed (Accton/Edgecore ASXvOLT16)

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie -port_100g_speed 40000
```

* 100G port change to 10Gbps speed (use Break-out cable) (Accton/Edgecore ASXvOLT16)

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie -port_100g_speed 10000 topology_config_file ./topology_config_16_ports.ini
```

* 25G port change to 20Gbps speed (Accton/Edgecore ASGvOLT64)

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie -port_25g_speed 20000
```

* 25G port change to 10Gbps speed (Accton/Edgecore ASGvOLT64)

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie -port_25g_speed 10000
```

* 25G port change to 1Gbps speed (Accton/Edgecore ASGvOLT64)

```shell
cd /broadcom
./dev_mgmt_daemon -d -pcie -port_25g_speed 1000
```

* 40G QSFP NNI port change to 10Gbps speed and 10G SFP NNI port to default speed (Phoenix/Radisys RLT-3200G-W, RLT-1600G-W).\
  If no speed specified QSFP port speed defaults to 40G and SFP port speed defaults to 10G

```shell
cd /opt/bcm68650/
./svk_init.sh -clean
./svk_init.sh -qsfp_speed=10g -sfp_speed=10g
cd /broadcom
./dev_mgmt_daemon -d -pcie
```

* 100G QSFP NNI port change to 10Gbps speed and 25G SFP NNI port change to 10Gbps speed (Phoenix/Radisys RLT-1600X-W).\
  If no speed specified QSFP port speed defaults to 100G and SFP port speed defaults to 25G

```shell
cd /opt/bcm68650/
./svk_init.sh -clean
./svk_init.sh -qsfp_speed=10g -sfp_speed=10g
cd /broadcom
./dev_mgmt_daemon -d -pcie
```

* 100G QSFP NNI port change to 40Gbps speed and 25G SFP NNI port change to 10Gbps speed (Phoenix/Radisys RLT-1600X-W).\
  If no speed specified QSFP port speed defaults to 100G and SFP port speed defaults to 25G

```shell
cd /opt/bcm68650/
./svk_init.sh -clean
./svk_init.sh -qsfp_speed=40g -sfp_speed=10g
cd /broadcom
./dev_mgmt_daemon -d -pcie
```

The `dev_mgmt_daemon` executable presents the CLI for Broadcom's BAL when run
in the foreground which is useful for debugging.

While the first executable still runs (even in background), open another
terminal and run `openolt`:

```shell
cd /broadcom
./openolt --interface <interface-name> (or)
./openolt --intf <interface-name>
```

> **NOTE**:

* In the commands above, you should specify OLT management interface name or
  inband interface name if OLT is used in inband mode. Openolt gRPC server
  will start listening on given interface's IP address. If no interface
  specified gRPC server will listen on "0.0.0.0:9191" which will accpet
  requests from any of the interfaces present in OLT.
* The two executables will remain open in the terminals, unless they are put
  in background.

## Inband ONL Note

* Inband ONL image built by packaging Openolt debian package and some startup
  scripts. The startup scripts serve to enable inband channels, create inband
  tagged interfaces, install Openolt debian package and run dev_mgmt_daemon
  and Openolt services.
* Startup script named "start_inband_oltservices.sh" will be executed in
  background after ONL installation. Script execution could be watched in a
  log file located in /var/log/startup.log.
* Follow the procedure specified in Build OpenOLT section to build integrated
  Inband ONL image.

### Connect from VOLTHA

At the VOLTHA CLI, preprovision and enable the OLT:

```shell
(voltha) preprovision_olt -t openolt -H YOUR_OLT_MGMT_IP:9191
(voltha) enable
```

### Additional notes

* *9191* is the TCP port that the *OpenOLT* agent uses for its gRPC channel.
* See the [SECURITY.md](./SECURITY.md) for possible TLS configurations.
* In the commands above, you should use OLT inband interface IP address if OLT
  is used in inband mode, otherwise substitute all its occurrences with the
  management IP of your OLT.

## Log Collection for whitebox OLT Device

To collect logs from openolt, dev_mgmt_daemon and syslog processes install td-agent(fluentd variant) directly on OLT device which will capture and transmits the logs to elasticsearch pod running in voltha cluster.

Prerequisite:

OLT should have ntp installed to ensure it has correct time(which is used by td-agent for generated events)

```shell
apt-get install ntp
```

Installation of td-agent deb package:

* Download the deb package for td-agent

```shell
wget http://packages.treasuredata.com.s3.amazonaws.com/3/ubuntu/xenial/pool/contrib/t/td-agent/td-agent_3.8.0-0_amd64.deb
```

* Install td-agent on device

```shell
dpkg -i td-agent_3.8.0-0_amd64.deb
```

Post Installation:

We have created custom td-agent configuration file to handle format of involved log files using right input plugins and elasticsearch output plugin.

Copy the custom config file from [here](https://github.com/opencord/openolt/tree/master/logConf) in the `~` directory of the olt.
Then into the `/etc` folder in order for the agent to pick it up.

```shell
cp td-agent.conf /etc/td-agent.conf
```

* Set elasticsearch host and port in /etc/td-agent.conf
* Restart the td-agent service

```shell
service td-agent restart
```

Need to redirect syslog to default port of fluentd syslog plugin.

* Add  “*.* @127.0.0.1:42185” to  /etc/rsyslog.conf
* Restart syslog using

```shell
/etc/init.d/rsyslog restart
```

**Note**:

To enable TLS encryption features with td-agent [look at this google doc](https://docs.google.com/document/d/1KF1HhE-PN-VY4JN2bqKmQBrZghFC5HQM_s0mC0slapA/edit)

## Supporing Dynamic PON Transceiver (TRX) detection and configuration of MAC and PON mode

The `PonTrxBase` class defined in `agent/device/device.(h/cc)` provides base implementation with the following capabilities
- Read SFP/Trx presence information using the `onlpdump` utility on the OLT
- Read EEPROM data for the PON Trx
- Decode PON Trx EEPROM data for Vendor Name, P/N, Rev, OUI and Wavelength(s)
- Get SFP mode
- Get MAC mode
- Store SFP, EEPROM data

The base class implentation has to derived class `PonTrx` defined in vendor specific modules and if needed override the base class methods to provide vendor specific implementation. See `agent/device/sim/vendor.(h/cc)` for how this is done for the simulated vendor used in unit tests.

The usual steps involved in openolt-agent startup is below
1. Read and populate SFP presence data (check `read_sfp_presence_data`)
2. Read EEPROM data for all the detected SFPs (check `read_eeprom_data_for_sfp`)
3. Decode EEPROM data for the successfully read SFP EEPROM data (check `decode_eeprom_data`)
4. Then use `get_mac_system_mode` and `get_sfp_mode` to get the modes while configuring the MAC and the PON respectively.

If any vendor does not want to support dynamic MAC and PON mode configuration, then do not define `DYNAMIC_PON_TRX_SUPPORT` in `agent/device/<vendor-folder>/vendor.h` and then provide the definition of `DEFAULT_MAC_SYSTEM_MODE` and `DEFAULT_PON_MODE` that you would want to use for your device.

### What is supported today?
All that is described in the earlier section is supported today for supporting Dynamic PON Trx detection and configuration of MAC and PON mode.

### What is missing?
- Static configuration of PON Trx modes via a yaml configuration file that is discussed in this document https://docs.google.com/document/d/129YDzShMvYACsrM0dWV60tV79BRPI6SsBpf_nm0Byxc/edit#heading=h.f2l6z075wl6e
- Hot plug in/out of PON Trx and then reconfiguration of MAC and PON modes
- More error/exception handling in the `PonTrxBase` implementation. But this, as the name suggests, is a base class implementation - could always be overriden in the derived class if you need a sophesticated implementation.
- Test and enhance for Combo PON SFPs. Currently only tested for either GPON or XGSPON SFP.

## Build OpenOLT Agent

Refer [BUILDING guide](BUILDING.md) for details on how to build openolt agent.

## FAQ

The information here may be specific to specific OLT and ONU hardware such as
Edgecore ASFVOLT16 OLT, Radisys RLT-3200G-W OLT and Broadcom based ONUs.

### How to change speed of ASFVOLT16 NNI interface?

Auto-negotiation on the NNI (Network to Network Interface) uplink ports is not
tested.

By default, the OpenOLT agent sets the speed of the NNI interfaces to 100G. To
downgrade the network interface speed to 40G, add the following lines at the
end of the `/broadcom/qax.soc` configuration file.

```text
port ce128 sp=40000
```

A restart of the `dev_mgmt_daemon` and `openolt` services is required after
making this change.




### How to get access to BAL CLI on OLT box

To get access to the `BCM.0>` maple console, SSH into the OLT and then execute:

```shell
cd /broadcom
./example_user_appl
```

### How do I check packet counters on the OLT's PON interface?

Following is an example of retrieving the interface description for PON
`intf_id 0`

(TODO: document PON interface numbering for Edgecore OLT).

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

Following command retrieves NNI `intf_id 0`:

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

### How do I access device CLI when running `dev_mgmt_daemon` and `openolt` agent binary as services?

Navigate to `/opt/bcm68620` and execute the command `/broadcom/dev_mgmt_attach`
to access the QAX diagnostic shell. If you are not able to access the shell,
you need to recompile the BAL SDK with `SW_UTIL_SHELL=y` option set.

### DBA Scheduler fail to create on the OLT with errors related to Bandwidth configuration

Ensure that `additional_bw` indicator in the Technology Profile and the
`bandwidthprofile` configured in ONOS netcfg for the subscriber are following
the below guideline.

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

[MEF Wiki -
Bandwidth-Profiles-for-Ethernet-Services](https://wiki.mef.net/display/CESG/Bandwidth+Profile)
[Technology Profile Implementation
Note](https://www.opennetworking.org/wp-content/uploads/2019/09/2pm-Shaun-Missett-Technology-Profile-and-Speed-Profile-Implementation.pdf)


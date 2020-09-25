# ONL Image Upgrade Procedure
# Introduction:
After the NOS(Network Operating System) is booted into the active partition, the images on the standby partition can be
updated using the same NOS installer. In this case,
• The installer searches if ‘active’ and ‘standby’ partitions already exists, and if they exists
• Upgrades the NOS in the standby partition
• Copies the selected configuration files from the active NOS file system to the standby file
system
• Updates the GRUB to boot one time to the standby partition, through the ‘grub-reboot
<menu-entry> command’

Once the standby NOS boots, it can change (swap) the partition labels and setup the GRUB again to boot the new active partition by default. Before it does this, it must check if the system has booted correctly by verifying several resources and their states. Few of the checks it can perform are
• Check if all of the devices ( network, storage, peripherals etc ) are visible in the NOS
• Check if all of the deamons have started
• Check that the service ports if any are available for access
• Check if the forwarding plane is initialized and setup up accordingly

Following are the other modified files in OpenNetworkLinux source code and brief info:

1.  builds/any/rootfs/jessie/common/overlay/etc/mtab.yml and builds/any/rootfs/wheezy/common/overlay/etc/mtab.yml

These configuration files are used to create the partitions, permissions for the partitions(read or write), mount path, directory. In these files we create labels for portioning.

2.  packages/base/all/initrds/loader-initrd-files/src/bin/swiprep

The function of this file is to extract the root file system from the “SWITCH IMAGE(.swi)” file.

Note: The switch image file will be available in ‘/mnt/onl/images’ path if OLT is booted from Active Partition. The image will be available in ‘/mnt/onl/images/upgrade’ path if OLT is booted from Standby Partition.

Changes: Based on current boot partition, the extracted RFS is copied to either ‘ACTIVE DATA/STANDBY DATA’ partition.

3.  packages/base/any/kernels/3.7/configs/x86_64-all/x86_64-all.config: enabled CONFIG_VLAN_8021Q module

4.  ¬packages/base/all/initrds/loader-initrd-files/src/bin/sysinit: copies boot config file.

5.  packages/base/all/initrds/loader-initrd-files/src/bootmodes/installed: this file is used to get the root file system path

    a. /mnt/onl/active_data – if system is booted from active mode

    b. /mnt/onl/standby_data – if system is booted from standby mode

6.  packages/base/all/initrds/loader-initrd-files/src/etc/mtab.yml: it is a configuration file

7.  packages/base/all/vendor-config-onl/src/lib/platform-config-defaults-uboot.yml: this is an uboot configuration file and has following list of configurations

    a. size of the partitions in GB or MB

8.  packages/base/all/vendor-config-onl/src/lib/platform-config-defaults-x86-64.yml: this is x86 platform configuration file and it has following configurations

    a. size of the partitions in GB or MB

9.  packages/base/all/vendor-config-onl/src/python/onl/bootconfig/**init**.py: this is an ONL boot config file. the modification in this is : The label name ‘ONL-BOOT’ is changed as ‘ONL-ACTIVE-BOOT’.

10.  packages/base/all/vendor-config-onl/src/python/onl/grub/**init**.py: this is ONL grub config file changed from ‘ONL-BOOT’ to ‘ONL-ACTIVE-BOOT’.

11.  packages/base/all/vendor-config-onl/src/python/onl/install/BaseInstall.py: This is the main core file where important modifications are done. The modifications are

    a. partitioning – active and standby

    b. grub configuration changes – ‘ONL-BOOT’ to ‘ONL-ACTIVE-BOOT’

    c. mount paths from ONL-DATA to ‘ONL-ACTIVE-DATA’ and ‘ONL-STANDBY-DATA’

12.  packages/base/all/vendor-config-onl/src/python/onl/install/ShellApp.py: only mount paths got changed from ONL-BOOT to ONL-ACTIVE-BOOT.

13.  packages/base/all/vendor-config-onl/src/python/onl/upgrade/loader.py: only ONL-BOOT label is changed to ONL-ACTIVE-BOOT

14.  tools/switool.py: This script, iterates through “bal_packages” directory and adds the files to SWITCH image file.

# Upgrade procedure
Note:
1. OLT image upgrade patch set on OpenNetworkLinux source code applied when ONL built in inband mode)
2. To upgrade OLT image, OLT must have ONL installed with active and standby partitions on it.

## Prepare for the upgrade: 
Files included in image upgrade test are: 
**install_onl.sh**  - Used as part of upgrade process. ONL  image is passed as an argument to the file.

**change_label.sh** - This file change the block Id label once upgrade is successful and is integrated into ONL swi image.

Pre-requisites:
 Component |Specification  |
|----------|---------------|
|Ansible  |2.7.12 and above|

 1 . Have a OLT minicom session for the upgrade process since OLT reboots while upgrade.

 Check the OLT IP before upgrade  in below file

      ```shell
	vi $HOME/openolt/olt-sw-upgrade/test/ansible/inventory/hosts
	[device-olt]
	192.168.50.100 ansible_ssh_user=root ansible_ssh_pass=onl ansible_sudo_pass=onl ansible_ssh_common_args='-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null'

If required change  the OLT IP 192.168.50.100

2 . Upgrade procedure is executed using ansible playbook here.
a. Execute all commands in sudo mode
```
sudo su
cd $HOME/openolt/agent
``` 
b.  Build an ONL image used for upgrade as shown below
```shell
make OPENOLTDEVICE=asfvolt16 clean
make OPENOLTDEVICE=asfvolt16 INBAND=y VLAN_ID=<INBAND_VLAN>
e.g:
make OPENOLTDEVICE=asfvolt16 INBAND=y VLAN_ID=5
```
If no VLAN_ID is specified in above command it defaults to 4093.

c. The below command prepare the ansible environment to start upgrade process.
```
make ansible
```
## Start the Upgrade:
```shell
$cd $HOME/openolt/olt-sw-upgrade/test/ansible
$sudo ansible-playbook -i inventory/hosts upgrade_olt.yml
```

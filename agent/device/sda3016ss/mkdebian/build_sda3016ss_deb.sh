#!/bin/bash

#Copyright 2018-present Open Networking Foundation
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

export DEB_VERSION="3.10.0.0-ABXZ-a12"

echo -e "sda3016ss (${DEB_VERSION}) stable; urgency=high\n" > debian/changelog
echo -e "    * Based on code from SW-BCM686OLT_3_10_0_0.tgz.\n" >> debian/changelog
echo -e " -- Wilson Lee <Wilson.Lee@zyxel.com.tw>  Tue, 12 OCT 2021 01:01:03 +0800" >> debian/changelog

export ONL_ARCH="amd64"
dpkg-buildpackage -b -us -uc -a"$ONL_ARCH" --no-check-builddeps

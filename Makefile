# Copyright (C) 2018 Open Networking Foundation
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

########################################################################
##
##
##        Config
##
##
DEVICE = asfvolt16
#
# Three vendor proprietary source files are required to build BAL.
# SW-BCM68620_<VER>.zip - Broadcom BAL source and Maple SDK.
# sdk-all-<SDK_VER>.tar.gz - Broadcom Qumran SDK.
# ACCTON_BAL_<BAL_VER>-<ACCTON_VER>.patch - Accton/Edgecore's patch.
BAL_MAJOR_VER = 02.04
BAL_VER = 2.4.3.6
SDK_VER = 6.5.7
ACCTON_VER = 201710131639
#
# Version of Open Network Linux (ONL).
ONL_KERN_VER_MAJOR = 3.7
ONL_KERN_VER_MINOR = 10
#
# Build directory
BUILD_DIR = build
#
# GRPC installation
GRPC_ADDR = https://github.com/grpc/grpc
GRPC_DST = /tmp/grpc
GRPC_VER = v1.10.x
#
########################################################################
##
##
##        Install prerequisites
##
##
HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11 -fpermissive -Wno-literal-suffix
ifeq ($(SYSTEM),Darwin)
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
					 -lgrpc++_reflection\
					 -ldl
else
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
					 -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
					 -ldl
endif

prereq:
	sudo apt-get -q -y install git pkg-config build-essential autoconf libtool libgflags-dev libgtest-dev clang libc++-dev

	# Install GRPC plugins
	rm -rf $(GRPC_DST)
	git clone -b $(GRPC_VER) $(GRPC_ADDR) $(GRPC_DST)
	cd $(GRPC_DST) && git submodule update --init
	make -C $(GRPC_DST)
	sudo make -C $(GRPC_DST) install
	# Install libprotobuf and protoc
	cd $(GRPC_DST)/third_party/protobuf && ./autogen.sh
	cd $(GRPC_DST)/third_party/protobuf && ./configure
	make -C $(GRPC_DST)/third_party/protobuf
	sudo make -C $(GRPC_DST)/third_party/protobuf install
	sudo ldconfig

########################################################################
##
##
##        ONL
##
##
ONL_KERN_VER = $(ONL_KERN_VER_MAJOR).$(ONL_KERN_VER_MINOR)
ONL_REPO = $(DEVICE)-onl
ONL_DIR = $(BUILD_DIR)/$(ONL_REPO)
.PHONY: onl
onl:
	if [ ! -d "$(ONL_DIR)/OpenNetworkLinux" ]; then \
		mkdir -p $(ONL_DIR); \
		git clone https://gerrit.opencord.org/$(ONL_REPO) $(ONL_DIR); \
		make -C $(ONL_DIR) $(DEVICE)-$(ONL_KERN_VER_MAJOR); \
	fi;
onl-force:
	make -C $(ONL_DIR) $(DEVICE)-$(ONL_KERN_VER_MAJOR)
clean-onl:
distclean-onl:
	sudo rm -rf $(ONL_DIR)

########################################################################
##
##
##        BAL
##
##
BAL_ZIP = SW-BCM68620_$(subst .,_,$(BAL_VER)).zip
SDK_ZIP = sdk-all-$(SDK_VER).tar.gz
ACCTON_PATCH = ACCTON_BAL_$(BAL_VER)-V$(ACCTON_VER).patch
OPENOLT_BAL_PATCH = OPENOLT_BAL_$(BAL_VER).patch
BAL_DIR = $(BUILD_DIR)/$(DEVICE)-bal
ONL_KERNDIR = $(PWD)/$(ONL_DIR)/OpenNetworkLinux/packages/base/amd64/kernels/kernel-$(ONL_KERN_VER_MAJOR)-x86-64-all/builds
MAPLE_KERNDIR = $(BAL_DIR)/bcm68620_release/$(DEVICE)/kernels
BCM_SDK = $(BAL_DIR)/bal_release/3rdparty/bcm-sdk
BALLIBDIR = $(BAL_DIR)/bal_release/build/core/lib
BALLIBNAME = bal_api_dist
BAL_INC = -I$(BAL_DIR)/bal_release/src/common/os_abstraction \
	-I$(BAL_DIR)/bal_release/src/common/os_abstraction/posix \
	-I$(BAL_DIR)/bal_release/src/common/config \
	-I$(BAL_DIR)/bal_release/src/core/platform \
	-I$(BAL_DIR)/bal_release/src/common/include \
	-I$(BAL_DIR)/bal_release/src/lib/libbalapi \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/utils \
	-I$(BAL_DIR)/bal_release/src/balapiend \
	-I$(BAL_DIR)/bal_release/src/common/dev_log \
	-I$(BAL_DIR)/bal_release/src/common/bal_dist_utils  \
	-I$(BAL_DIR)/bal_release/src/lib/libtopology \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/model \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_driver/api \
	-I$(BAL_DIR)/bal_release/src/lib/libcmdline \
	-I$(BAL_DIR)/bal_release/src/lib/libutils \
	-I$(BAL_DIR)/bal_release/3rdparty/maple/sdk/host_reference/cli
CXXFLAGS += $(BAL_INC) -I $(BAL_DIR)/lib/cmdline
.PHONY: get-$(BAL_DIR)
get-$(BAL_DIR):
# if [ ! -e "download/$(BAL_ZIP)" ]; then \
#   rm -rf /tmp/broadcom-proprietary; \
#   git clone git@github.com:shadansari/broadcom-proprietary.git /tmp/broadcom-proprietary; \
#   mv -f /tmp/broadcom-proprietary/$(BAL_VER)/$(BAL_ZIP) ./download; \
#   mv -f /tmp/broadcom-proprietary/$(BAL_VER)/$(SDK_ZIP) ./download; \
#   mv -f /tmp/broadcom-proprietary/$(BAL_VER)/$(ACCTON_PATCH) ./download; \
#   mv -f /tmp/broadcom-proprietary/$(BAL_VER)/$(OPENOLT_BAL_PATCH) ./download; \
#   rm -rf /tmp/broadcom-proprietary; \
# fi;
.PHONY: bal
bal: onl get-$(BAL_DIR)
ifeq ("$(wildcard $(BAL_DIR))","")
	mkdir $(BAL_DIR)
	unzip download/$(BAL_ZIP) -d $(BAL_DIR)
	cp download/$(SDK_ZIP) $(BCM_SDK)
	chmod -R 744 $(BAL_DIR)
	cat download/$(ACCTON_PATCH) | patch -p1 -d $(BAL_DIR)
	mkdir -p $(MAPLE_KERNDIR)
	ln -s $(ONL_KERNDIR)/linux-$(ONL_KERN_VER) $(MAPLE_KERNDIR)/linux-$(ONL_KERN_VER)
	ln -s $(ONL_DIR)/OpenNetworkLinux/packages/base/any/kernels/archives/linux-$(ONL_KERN_VER).tar.xz $(MAPLE_KERNDIR)/linux-$(ONL_KERN_VER).tar.xz
	ln -s $(ONL_DIR)/OpenNetworkLinux/packages/base/any/kernels/$(ONL_KERN_VER_MAJOR)/configs/x86_64-all/x86_64-all.config $(MAPLE_KERNDIR)/x86_64-all.config
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) maple_sdk_dir
	cat download/$(OPENOLT_BAL_PATCH) | patch -p1 -d $(BAL_DIR)
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) maple_sdk
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) switch_sdk_dir
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) switch_sdk
	KERNDIR=$(ONL_KERNDIR)/linux-$(ONL_KERN_VER) BOARD=$(DEVICE) ARCH=x86_64 SDKBUILD=build_bcm_user make -C $(BCM_SDK)/build-$(DEVICE)/sdk-all-$(SDK_VER)/systems/linux/user/x86-generic_64-2_6
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) bal
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) release_board
	ln -s -f $(PWD)/$(BAL_DIR)/bcm68620_release/asfvolt16/release/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz . 
	ln -s -f $(PWD)/$(BAL_DIR)/bal_release/build/core/lib/libbal_api_dist.so .
	ln -s -f $(PWD)/$(BAL_DIR)/bal_release/build/core/src/apps/bal_core_dist/bal_core_dist
	mv -f release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz $(BUILD_DIR)
	mv -f libbal_api_dist.so $(BUILD_DIR)
	mv -f bal_core_dist $(BUILD_DIR)
endif
bal-rebuild:
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) maple_sdk
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) switch_sdk
	KERNDIR=$(ONL_KERNDIR)/linux-$(ONL_KERN_VER) BOARD=$(DEVICE) ARCH=x86_64 SDKBUILD=build_bcm_user make -C $(BCM_SDK)/build-$(DEVICE)/sdk-all-$(SDK_VER)/systems/linux/user/x86-generic_64-2_6
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) bal
	make -C $(BAL_DIR)/bal_release BOARD=$(DEVICE) release_board
	ln -s -f $(PWD)/$(BAL_DIR)/bcm68620_release/asfvolt16/release/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz .
	ln -s -f $(PWD)/$(BAL_DIR)/bal_release/build/core/lib/libbal_api_dist.so .
	ln -s -f $(PWD)/$(BAL_DIR)/bal_release/build/core/src/apps/bal_core_dist/bal_core_dist .
	mv -f release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz $(BUILD_DIR)
	mv -f libbal_api_dist.so $(BUILD_DIR)
	mv -f bal_core_dist $(BUILD_DIR)
clean-bal: clean-onl
distclean-bal: distclean-onl
	sudo rm -rf $(BAL_DIR)
	rm -f build/release_$(DEVICE)_V$(BAL_MAJOR_VER).$(ACCTON_VER).tar.gz

########################################################################
##
##
##        OpenOLT API
##
##
OPENOLT_API_DIR = $(BUILD_DIR)/openolt-api
OPENOLT_API_LIB = $(OPENOLT_API_DIR)/libopenoltapi.a
CXXFLAGS += -I./$(OPENOLT_API_DIR) -I $(OPENOLT_API_DIR)/googleapis/gens
.PHONY: openolt-api
openolt-api:
	if [ ! -e "$(OPENOLT_API_DIR)" ]; then \
		mkdir -p $(BUILD_DIR); \
		git clone https://gerrit.opencord.org/openolt-api $(OPENOLT_API_DIR); \
	fi;
	make -C $(OPENOLT_API_DIR) all
clean-openolt-api:
	-make -C $(OPENOLT_API_DIR) clean
distclean-openolt-api:

########################################################################
##
##
##        Main
##
##
SRCS = $(wildcard src/*.cc)
OBJS = $(SRCS:.cc=.o)
DEPS = $(SRCS:.cc=.d)
.DEFAULT_GOAL := all
all: $(BUILD_DIR)/openolt
$(BUILD_DIR)/openolt: openolt-api bal $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(OPENOLT_API_LIB) -o $@ -L$(BALLIBDIR) -l$(BALLIBNAME)
	ln -s $(shell ldconfig -p | grep libgrpc.so.6 | tr ' ' '\n' | grep /) $(BUILD_DIR)/libgrpc.so.6
	ln -s $(shell ldconfig -p | grep libgrpc++.so.1 | tr ' ' '\n' | grep /) $(BUILD_DIR)/libgrpc++.so.1
	ln -s $(shell ldconfig -p | grep libgrpc++_reflection.so.1 | tr ' ' '\n' | grep /) $(BUILD_DIR)/libgrpc++_reflection.so.1

src/%.o: %.cpp
	$(CXX) -MMD -c $< -o $@

clean: clean-bal
	rm -f $(OBJS) $(DEPS) $(BUILD_DIR)/openolt
	rm -rf $(OPENOLT_API_DIR)
	rm -f $(BUILD_DIR)/libgrpc.so.6 $(BUILD_DIR)/libgrpc++.so.1 $(BUILD_DIR)/libgrpc++_reflection.so.1
distclean: distclean-openolt-api distclean-bal
	rm -rf $(BUILD_DIR)
-include $(DEPS)

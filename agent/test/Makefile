# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

########################################################################
##
##
##        Config
##
##
TOP_DIR=`pwd`
OPENOLTDEVICE ?= asfvolt16

OPENOLT_PROTO_VER ?= v3.1.0

GTEST_VER ?= release-1.8.0
gtest-target = /usr/local/lib/libgtest.a

CMOCK_VER ?= 0207b30
cmock-target = /usr/local/include/cmock/cmock.h

GMOCK_GLOBAL_VER ?= 1.0.2
gmock-global-target = /usr/local/include/gmock-global/gmock-global.h


# GRPC installation
GRPC_ADDR = https://github.com/grpc/grpc
GRPC_DST = /tmp/grpc
GRPC_VER ?= v1.10.x
grpc-cpp-plugin-bin = /usr/local/bin/grpc_cpp_plugin

# Protobuf installation
PROTOBUF_VER ?= 3.5.0
PROTOBUF_ADDR = https://github.com/protocolbuffers/protobuf/releases/download/v$(PROTOBUF_VER)/protobuf-cpp-$(PROTOBUF_VER).tar.gz
PROTOBUF_DST = /tmp/protobuf-$(PROTOBUF_VER)
protoc-bin = /usr/local/bin/protoc

#
########################################################################
##
##
##        Install prerequisites
##
##

CXX = g++
CXXFLAGS += -g -O2
CXXFLAGS += `pkg-config --cflags-only-I grpc++`
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11 -fpermissive -Wno-literal-suffix
CXXFLAGS += -DTEST_MODE -DENABLE_LOG
LDFLAGS += -lpthread -lm `pkg-config --libs protobuf grpc++ grpc` -ldl -lgpr
CXXFLAGSDEVICE = -I../device -I../device/$(OPENOLTDEVICE) -I../device/generic

export CXX CXXFLAGS OPENOLT_PROTO_VER

BAL_API_VER ?= bal-api-3.2.3.2
BAL_API_DIR=$(BAL_API_VER)
BALLIBNAME=bcm_host_api_stubs
BALLIBDIR=$(BAL_API_DIR)/stubs
BAL_INC = -I$(BAL_API_DIR)/include \
          -I$(BAL_API_DIR)/include/object_model
CXXFLAGS += $(BAL_INC)


prereqs-system:
	sudo apt-get -q -y install git pkg-config build-essential autoconf libtool libgflags-dev libgtest-dev clang libc++-dev unzip docker.io
	sudo apt-get install -y build-essential autoconf libssl-dev gawk debhelper dh-systemd init-system-helpers curl cmake ccache g++-4.9

prereqs-local: $(protoc-bin) $(grpc-cpp-plugin-bin) prereqs-test-lib

$(protoc-bin):
	# Install protobuf
	rm -rf $(PROTOBUF_DST)
	cd /tmp && wget $(PROTOBUF_ADDR)
	cd /tmp && tar -zxvf protobuf-cpp-$(PROTOBUF_VER).tar.gz
	cd $(PROTOBUF_DST) && ./autogen.sh
	cd $(PROTOBUF_DST) && ./configure
	make -C $(PROTOBUF_DST)
	sudo make -C $(PROTOBUF_DST) install

$(grpc-cpp-plugin-bin):
	# Install GRPC, protoc
	rm -rf $(GRPC_DST)
	cd /tmp && git clone -b $(GRPC_VER) $(GRPC_ADDR) $(GRPC_DST)
	cd $(GRPC_DST) && git submodule update --init
	make -C $(GRPC_DST)
	sudo make -C $(GRPC_DST) install

prereqs-test-lib: $(gtest-target) $(cmock-target) $(gmock-global-target)

$(gtest-target):
	# Install gtest
	rm -rf /tmp/googletest && cd /tmp && git clone https://github.com/google/googletest.git
	cd /tmp/googletest && git checkout $(GTEST_VER)
	cd /tmp/googletest && cmake CMakeLists.txt
	make -C /tmp/googletest
	sudo make -C /tmp/googletest install

$(cmock-target):
	# Install c-mock
	rm -rf /tmp/C-Mock && cd /tmp && git clone https://github.com/hjagodzinski/C-Mock.git
	cd /tmp/C-Mock && git checkout $(CMOCK_VER)
	sudo make -C /tmp/C-Mock install

$(gmock-global-target):
	# Install gmock-global
	rm -rf /tmp/gmock-global && cd /tmp && git clone https://github.com/apriorit/gmock-global.git
	cd /tmp/gmock-global && git checkout $(GMOCK_GLOBAL_VER)
	sudo cp -rf /tmp/gmock-global/include/gmock-global /usr/local/include/

########################################################################
##
##
##        build
##
##

.DEFAULT_GOAL := all

.PHONY = bcm_host_api_stubs build clean prereqs-system prereqs-local

OPENOLT_PROTOS_DIR = ../../protos
OPENOLT_API_LIB = lib/libopenoltapi.a
OPENOLT_LIB = lib/libopenolt.a

all: bcm_host_api_stubs $(OPENOLT_API_LIB) $(OPENOLT_LIB) build

# test
TEST_OPENOLT_LIB_DIR=./lib
TEST_BIN=test_openolt
# The below way of getting source files is not working
#TEST_SRCS = $($(filter-out src/bal_stub.cc, $(wildcard src/*.cc)))
TEST_SRCS = src/main_test.cc src/bal_mocker.cc src/test_core.cc
TEST_OBJS = $(TEST_SRCS:.cc=.o)

build: $(TEST_OBJS)
	$(CXX) $(shell cmock-config --libs) -L$(TEST_OPENOLT_LIB_DIR) -L/usr/lib/ -I/usr/include -I./inc -I../src -o $(TEST_BIN) $(TEST_OBJS) -lgmock -lgtest -lopenolt -lopenoltapi $(LDFLAGS)
src/%.o: src/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I./inc -I../common -I../src -I/usr/include -c $< -o $@


# bcm_host_api_stubs
bcm_host_api_stubs:
	# TODO: This is temporary hack below to introduce the definition BAL IEs that are missing in https://github.com/balapi/bal-api-<BAL-VER>
	if [ -d $(BAL_API_DIR) ]; then \
		echo "$(BAL_API_DIR) exists"; \
	else \
		git clone https://github.com/balapi/$(BAL_API_VER).git ;\
		sed -i "\$$i #define BCMOLT_TM_QUEUE_KEY_TM_Q_SET_ID_DEFAULT 0" ./$(BAL_API_DIR)/include/bcmos_system.h; \
		sed -i "\$$i struct bcmos_mutex \{ pthread_mutex_t m; };" ./$(BAL_API_DIR)/include/bcmos_system.h; \
		sed -i "\$$i extern void* bcmos_calloc(uint32_t size);"  ./$(BAL_API_DIR)/include/bcmos_common.h; \
		sed -i "\$$i #define BCMOLT_INTERFACE_TYPE_EPON_1_G 3"  ./$(BAL_API_DIR)/include/bcmos_common.h; \
		sed -i "\$$i #define BCMOLT_INTERFACE_TYPE_EPON_10_G 4"  ./$(BAL_API_DIR)/include/bcmos_common.h; \
		sed -i "\$$i typedef struct \{ uint64_t rdi_errors;} bcmolt_onu_itu_pon_stats_data;" ./$(BAL_API_DIR)/include/object_model/bcmolt_api_model_api_structs.h; \
		sed -i "\$$i typedef struct \{ bcmolt_onu_key key; bcmolt_onu_itu_pon_stats_data data;} bcmolt_onu_itu_pon_stats;" ./$(BAL_API_DIR)/include/object_model/bcmolt_api_model_api_structs.h; \
		sed -i "\$$i #define bcmolt_onu_stat_subgroup_itu_pon_stats BCMOLT_ONU_STAT_SUBGROUP_ITU_PON_STATS" ./$(BAL_API_DIR)/include/object_model/bcmolt_api_model_api_structs.h; \
		sed -i "\$$i #define BCMOLT_ONU_STAT_SUBGROUP_ITU_PON_STATS 0" ./$(BAL_API_DIR)/include/object_model/bcmolt_api_model_api_structs.h; \
	fi

# openoltapi
CXXFLAGS += -I$(OPENOLT_PROTOS_DIR) -I$(OPENOLT_PROTOS_DIR)/googleapis/gens
$(OPENOLT_API_LIB): $(wildcard $(OPENOLT_PROTOS_DIR)/*.proto)
	make -C $(OPENOLT_PROTOS_DIR) all
	mkdir -p lib
	cp $(OPENOLT_PROTOS_DIR)/libopenoltapi.a lib/
libproto-clean:
	make -C $(OPENOLT_PROTOS_DIR) clean

# openolt
OPENOLT_SRCS = $(wildcard ../src/*.cc) ../common/server.cc $(wildcard ../device/$(OPENOLTDEVICE)/*.cc) src/bal_stub.cc
OPENOLT_OBJS = $(OPENOLT_SRCS:.cc=.o)
$(OPENOLT_LIB): $(OPENOLT_OBJS)
	mkdir -p lib
	ar cr $@ $^
	ranlib $@
../common/%.o: ../common/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@
../src/%.o: ../src/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@
src/%.o: src/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@
../device/$(OPENOLTDEVICE)/%.o: ../device/$(OPENOLTDEVICE)/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@

test: all
	./test_openolt --gtest_output="xml:./test_openolt_report_xunit.xml"

clean:
	rm -f src/*.o lib/*.a ../src/*.o ../common/*.o ./test_openolt  ./test_openolt_report_xunit.xml

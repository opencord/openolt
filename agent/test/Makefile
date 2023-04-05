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
OPENOLTDEVICE ?= sim

OPENOLT_PROTO_VER ?= v5.4.6

########################################################################
##
##
##        Install prerequisites
##
##

CXX != if [ "$(OPENOLTDEVICE)" = "rlt-3200g-w" ]; then echo "g++-6"; else echo "g++-4.9"; fi
CXXFLAGS += -g -O2
CXXFLAGS += `pkg-config --cflags-only-I grpc++` -I/usr/local/include/pcapplusplus/
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11 -fpermissive -Wno-literal-suffix
CXXFLAGS += -DTEST_MODE -DENABLE_LOG
LDFLAGS += -lpthread -lm `pkg-config --libs protobuf grpc++ grpc` -ldl -lgpr -lPcap++ -lPacket++ -lCommon++
CXXFLAGSDEVICE = -I../device -I../device/$(OPENOLTDEVICE)

export CXX CXXFLAGS OPENOLT_PROTO_VER

BAL_MAJOR_VER = 3
BAL_MINOR_VER = 10
BAL_REV_HOST_VER = 2
BAL_REV_EMBEDDED_VER = 2
BAL_API_SO_VER = $(BAL_MAJOR_VER).$(BAL_MINOR_VER).$(BAL_REV_HOST_VER)
BAL_API_VER = $(BAL_API_SO_VER).$(BAL_REV_EMBEDDED_VER)

BAL_API_DIR=bal-oss-api-$(BAL_API_VER)
BALLIBNAME=bcm_host_api_stubs
BALLIBDIR=$(BAL_API_DIR)/stubs
BAL_INC = -I$(BAL_API_DIR)/include \
          -I$(BAL_API_DIR)/include/object_model
CXXFLAGS += $(BAL_INC)


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

# Set the OPENOLT_ROOT_DIR variable to "/app" folder inside the docker container, if not already set.
ifeq ($(OPENOLT_ROOT_DIR),)
OPENOLT_ROOT_DIR := /app
endif

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
	echo $(OPENOLT_ROOT_DIR); \
	# TODO: This is temporary hack below to introduce the definition BAL IEs that are missing in https://github.com/balapi/bal-api-<BAL-VER>
	if [ -d $(BAL_API_DIR) ]; then \
		echo "$(BAL_API_DIR) exists"; \
	else \
		mkdir -p $(BAL_API_DIR); \
		mkdir -p lib; \
		cd $(BAL_API_DIR); \
		git clone https://github.com/balapi/bal-sdk.git -b $(BAL_API_VER); \
		mv bal-sdk bal-sdk-$(BAL_API_VER); \
		cd bal-sdk-$(BAL_API_VER); \
		make BOARD=$(OPENOLTDEVICE); \
		cp build/fs/libbal_host_api-oss.$(BAL_API_SO_VER).so ../../lib/libbal_host_api-oss.so; \
		cd .. && ln -s bal-sdk-$(BAL_API_VER)/build/fs/include include; \
		sed -i 's/BUG_ON_PRINT((condition),/printf(/g' include/bcmos_system.h; \
		cd ..;\
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
OPENOLT_SRCS = $(wildcard ../src/*.cc) ../common/server.cc ../device/device.cc $(wildcard ../device/$(OPENOLTDEVICE)/*.cc) src/bal_stub.cc
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
../device/%.o: ../device/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@
../device/$(OPENOLTDEVICE)/%.o: ../device/$(OPENOLTDEVICE)/%.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGSDEVICE) -I../common -I./inc -I../src -c $< -o $@

test: all
	./test_openolt --gtest_output="xml:./test_openolt_report_xunit.xml"

clean:
	rm -f src/*.o lib/*.a ../src/*.o ../common/*.o ../device/device.o ./test_openolt  ./test_openolt_report_xunit.xml

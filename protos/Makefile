# Copyright (c) 2018 Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)

# OpenOLT Proto version.
# This specifies the GIT tag in https://github.com/opencord/voltha-protos
# repo that we need to refer to, to pick the right version of
# openolt.proto and tech_profile.proto
OPENOLT_PROTO_VER ?= v3.3.4
GRPC_VER ?= v1.10.x

CXX ?= g++
CPPFLAGS += `pkg-config --cflags protobuf grpc` -I googleapis/gens -I./
CXXFLAGS += -std=c++11
#LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc` -ldl
PROTOC = $(shell which protoc)
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= $(shell which grpc_cpp_plugin)

OBJS = voltha_protos/common.pb.o voltha_protos/common.grpc.pb.o voltha_protos/tech_profile.pb.o voltha_protos/tech_profile.grpc.pb.o voltha_protos/openolt.pb.o voltha_protos/openolt.grpc.pb.o ./googleapis/gens/google/api/annotations.grpc.pb.o ./googleapis/gens/google/api/annotations.pb.o ./googleapis/gens/google/api/http.pb.o

.DEFAULT_GOAL := all

all: googleapis voltha_protos grpc-target protobuf-target libopenoltapi.a

libopenoltapi.a: $(OBJS)
	ar cr $@ $^
	ranlib $@

googleapis:
	if [ ! -e "googleapis" ]; then \
		git clone https://github.com/googleapis/googleapis.git; \
                cd googleapis; \
                git checkout 475d72b7405c92f06d7f2d4aba866278eb5ad8e9; \
                cd ..; \
		make -C googleapis LANGUAGE=cpp GRPCPLUGIN=$(GRPC_CPP_PLUGIN_PATH) all; \
	fi;

voltha_protos:
	mkdir voltha_protos
	if [ ! -e "voltha_protos/common.proto" ]; then \
		wget -O voltha_protos/common.proto https://raw.githubusercontent.com/opencord/voltha-protos/$(OPENOLT_PROTO_VER)/protos/voltha_protos/common.proto; \
	fi; \
	if [ ! -e "voltha_protos/openolt.proto" ]; then \
		wget -O voltha_protos/openolt.proto https://raw.githubusercontent.com/opencord/voltha-protos/$(OPENOLT_PROTO_VER)/protos/voltha_protos/openolt.proto; \
	fi; \
	if [ ! -e "voltha_protos/tech_profile.proto" ]; then \
		wget -O voltha_protos/tech_profile.proto https://raw.githubusercontent.com/opencord/voltha-protos/$(OPENOLT_PROTO_VER)/protos/voltha_protos/tech_profile.proto; \
	fi;

grpc-target:
	$(PROTOC) --proto_path=. -I./googleapis --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) voltha_protos/common.proto
	$(PROTOC) --proto_path=. -I./googleapis --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) voltha_protos/openolt.proto
	$(PROTOC) --proto_path=. -I./googleapis --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) voltha_protos/tech_profile.proto

protobuf-target: voltha_protos
	$(PROTOC) --proto_path=. -I./googleapis -I./ --cpp_out=. voltha_protos/common.proto
	$(PROTOC) --proto_path=. -I./googleapis -I./ --cpp_out=. voltha_protos/openolt.proto
	$(PROTOC) --proto_path=. -I./googleapis -I./ --cpp_out=. voltha_protos/tech_profile.proto

clean:
	rm -rf *.a voltha_protos

distclean: clean
	rm -rf googleapis

.PHONY = grpc-target protobuf-target

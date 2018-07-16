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
CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf grpc` -I googleapis/gens
CXXFLAGS += -std=c++11
#LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc` -ldl
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

OBJS = openolt.pb.o openolt.grpc.pb.o ./googleapis/gens/google/api/annotations.grpc.pb.o ./googleapis/gens/google/api/annotations.pb.o ./googleapis/gens/google/api/http.pb.o

all: googleapis libopenoltapi.a

libopenoltapi.a: $(OBJS)
	ar ru $@ $^
	ranlib $@

googleapis:
	if [ ! -e "googleapis" ]; then \
		git clone https://github.com/googleapis/googleapis.git; \
                cd googleapis; \
                git checkout 475d72b7405c92f06d7f2d4aba866278eb5ad8e9; \
                cd ..; \
		make -C googleapis LANGUAGE=cpp all; \
	fi;

.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) --proto_path=. -I./googleapis --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) --proto_path=. -I./googleapis --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h *.a

distclean: clean
	rm -rf googleapis

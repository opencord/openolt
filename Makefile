# -*- makefile -*-
# Copyright 2018-2023 Open Networking Foundation (ONF) and the ONF Contributors
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

########################################################################

# This Makefile provides hook for Jenkins to invoke the test target for
# CI integration.
# The main Makefile for product compilation is available at
# agent/Makefile. Please see ./README.md and ./agent/test/README.md for more
# details.

# set default shell options
SHELL = bash -e -o pipefail
## See also: https://github.com/opencord/onf-make/blob/master/makefiles/consts.mk

export OPENOLT_ROOT_DIR=$(shell pwd)

## Variables
OPENOLTDEVICE     ?= sim
OPENOLT_PROTO_VER ?= v5.6.5

DOCKER                     ?= docker
DOCKER_REGISTRY            ?=
DOCKER_REPOSITORY          ?= voltha/
DOCKER_EXTRA_ARGS          ?=
DOCKER_TAG                 ?= 2.1.1
IMAGENAME                  = ${DOCKER_REGISTRY}${DOCKER_REPOSITORY}openolt-test:${DOCKER_TAG}

DOCKER_BUILD_ARGS ?= \
	${DOCKER_EXTRA_ARGS} \
	--build-arg OPENOLTDEVICE=${OPENOLTDEVICE} \
	--build-arg OPENOLT_PROTO_VER=${OPENOLT_PROTO_VER}

## -----------------------------------------------------------------------
## See github::onf-make (docker --tty), docker -it inhibits jenkins logging output
## https://github.com/opencord/onf-make/blob/master/makefiles/docker/include.mk#L34
## -----------------------------------------------------------------------
## Overhead from $(shell pwd) can be avoided using a ./Makefile derived path
##   path := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
## -----------------------------------------------------------------------

test:
	${DOCKER} run --rm -v $(shell pwd):/app $(shell test -t 0 && echo "-it") ${IMAGENAME} make -C agent/test test

build:
	${DOCKER} run --rm -v $(shell pwd):/app $(shell test -t 0 && echo "-it") ${IMAGENAME} make -C agent/test all

clean:
	${DOCKER} run --rm -v $(shell pwd):/app $(shell test -t 0 && echo "-it") ${IMAGENAME} make -C agent/test clean

cleanall:
	${DOCKER} run --rm -v $(shell pwd):/app $(shell test -t 0 && echo "-it") ${IMAGENAME} make -C agent/test clean
	${DOCKER} run --rm -v $(shell pwd):/app $(shell test -t 0 && echo "-it") ${IMAGENAME} make -C protos distclean

# [EOF]

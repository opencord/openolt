// Copyright 2017-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import groovy.json.JsonSlurperClassic

node ('olt-agent-onf') {
  timeout (time: 240) {
    try {
      stage ('Clean up') {
        sh returnStdout: true, script: 'sudo rm -rf openolt'
      }
      stage ('Clone OpenOLT') {
        sh returnStdout: true, script: 'git clone https://gerrit.opencord.org/openolt'
      }
      dir ('openolt') {
        stage ('Copy over SDK, BAL, patch files and DEB generators') {
          sh returnStdout: true, script: 'cp ../../build-files/SW-BCM68620_2_4_3_6.zip download'
          sh returnStdout: true, script: 'cp ../../build-files/sdk-all-6.5.7.tar.gz download'
          sh returnStdout: true, script: 'cp ../../build-files/ACCTON_BAL_2.4.3.6-V201710131639.patch download'
          sh returnStdout: true, script: 'cp ../../build-files/OPENOLT_BAL_2.4.3.6.patch download'
          sh returnStdout: true, script: 'cp -r ../../build-files/mkdebian .'
        }
        stage ('Build packages and libraries') {
          sh returnStdout: true, script: '/bin/bash -c make'
        }
        stage ('Copy ASFvOLT16 release file to mkdebian folder') {
          sh returnStdout: true, script: 'cp build/*.tar.gz mkdebian/debian/release_asfvolt16.tar.gz'
        }
        stage ('Copy OpenOLT to mkdebian folder') {
          sh returnStdout: true, script: 'cp build/openolt mkdebian/debian'
        }
        stage ('Copy gRPC to mkdebian folder') {
          sh returnStdout: true, script: 'cp build/libgrpc++_reflection.so.1 mkdebian/debian'
          sh returnStdout: true, script: 'cp build/libgrpc++.so.1 mkdebian/debian'
          sh returnStdout: true, script: 'cp /usr/local/lib/libgrpc.so.6 mkdebian/debian'
        }
        stage ('Create Debian file') {
          sh returnStdout: true, script: '#!/bin/bash \n cd mkdebian; ./build_asfvolt16_deb.sh'
        }
        stage ('Publish Voltha BAL driver DEB package') {
          sh returnStdout: true, script: 'sudo cp *.deb /var/www/voltha-bal/voltha-bal.deb'
        }
      }
      currentBuild.result = 'SUCCESS'
    } catch (err) {
      currentBuild.result = 'FAILURE'
      step([$class: 'Mailer', notifyEveryUnstableBuild: true, recipients: "${notificationEmail}", sendToIndividuals: false])
    } finally {
      echo "RESULT: ${currentBuild.result}"
    }
  }
}


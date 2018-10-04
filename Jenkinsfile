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

node ('openolt_deb_onf_agent') {
  timeout (time: 240) {
    try {
      dir ('openolt') {
        stage ('Pull latest code') {
          sh returnStdout: true, script: 'git pull'
        }
        stage ('Copy over SDK, BAL, patch files and DEB generators') {
          sh returnStdout: true, script: 'cp ../../build-files/SW-BCM68620_2_4_3_6.zip download'
          sh returnStdout: true, script: 'cp ../../build-files/sdk-all-6.5.7.tar.gz download'
          sh returnStdout: true, script: 'cp ../../build-files/ACCTON_BAL_2.4.3.6-V201710131639.patch download'
          sh returnStdout: true, script: 'cp ../../build-files/OPENOLT_BAL_2.4.3.6.patch download'
        }
        stage ('Build packages and libraries') {
          sh returnStdout: true, script: '/bin/bash -c ./configure && make DEVICE=asfvolt16'
        }
        stage ('Create Debian file') {
          sh returnStdout: true, script: '/bin/bash -c "make DEVICE=asfvolt16 deb"'
        }
        stage ('Publish executables and DEB package to web server') {
          sh returnStdout: true, script: 'sudo mkdir -p /var/www/voltha-bal/executables'
          sh returnStdout: true, script: 'sudo cp build/*.tar.gz /var/www/voltha-bal/executables/'
          sh returnStdout: true, script: 'sudo cp build/openolt /var/www/voltha-bal/executables/'
          sh returnStdout: true, script: 'sudo cp build/libgrpc++_reflection.so.1 /var/www/voltha-bal/executables/'
          sh returnStdout: true, script: 'sudo cp build/libgrpc++.so.1 /var/www/voltha-bal/executables/'
          sh returnStdout: true, script: 'sudo cp /usr/local/lib/libgrpc.so.6 /var/www/voltha-bal/executables/'
          sh returnStdout: true, script: 'sudo cp build/openolt.deb /var/www/voltha-bal'
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

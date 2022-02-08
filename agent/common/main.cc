/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <unistd.h>

#include "server.h"
#include "core.h"
#include "src/core_data.h"

using namespace std;

/*
*   This function displays openolt version, BAL version, openolt build date
*   and other VCS params like VCS url, VCS ref, commit date and exits.
*
*   @param argc : number of arguments
*   @param argv : vector of arguments
*/
void display_version_info(int argc, char *argv[]) {

    string version = "";
    string bal_version = "";
    string label_vcs_url = "";
    string label_vcs_ref = "";
    string label_build_date = "";
    string label_commit_date = "";

    #ifdef VERSION
        version = VERSION;
    #endif

    #ifdef BAL_VER
        bal_version = BAL_VER;
    #endif

    #ifdef LABEL_VCS_URL
        label_vcs_url = LABEL_VCS_URL;
    #endif

    #ifdef LABEL_VCS_REF
        label_vcs_ref = LABEL_VCS_REF;
    #endif

    #ifdef LABEL_BUILD_DATE
       label_build_date = LABEL_BUILD_DATE;
    #endif

    #ifdef LABEL_COMMIT_DATE
       label_commit_date = LABEL_COMMIT_DATE;
    #endif

    for (int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "--version") == 0 || (strcmp(argv[i], "-v") == 0)) {
           std::cout << "OpenOLT agent: " << version << "\n";
           std::cout << "BAL version: " << bal_version << "\n";
           std::cout << "Label VCS Url: " << label_vcs_url << "\n";
           std::cout << "Label VCS Ref: " << label_vcs_ref << "\n";
           std::cout << "Label build date: " << label_build_date << "\n";
           std::cout << "Label commit date: " << label_commit_date << "\n";
           exit(0);
        }
    }
}

int main(int argc, char** argv) {

    display_version_info(argc, argv);

    Status status = Enable_(argc, argv);
    if (!status.ok()) {
        std::cout << "ERROR: Enable_ failed - "
                  << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return 1;
    }

    // Wait for successful activation before allowing VOLTHA to connect.
    // This is necessary to allow the device topology to be dynamically
    // queried from driver after initialization and activation is complete.
    int maxTrials = 300;
    while (!state.is_activated()) {
        sleep(1);
        if (--maxTrials == 0) {
            std::cout << "ERROR: OLT/PON Activation failed" << std::endl;
            return 1;
        }
    }

    status = ProbeDeviceCapabilities_();
    if (!status.ok()) {
        std::cout << "ERROR: Could not find the OLT Device capabilities" << std::endl;
        return 1;
    }

    sleep(2);
    // Enable all PON interfaces.
    for (int i = 0; i < NumPonIf_(); i++) {
        status = EnablePonIf_(i);
        if (!status.ok()) {
            // raise alarm to report error in enabling PON
            pushOltOperInd(i, "pon", "down", 0 /*Speed will be ignored in the adapter for PONs*/ );
        }
        else
            pushOltOperInd(i, "pon", "up", 0 /*Speed will be ignored in the adapter for PONs*/);
    }
    sleep(2);
    // Enable all NNI interfaces.
#if 0
    for (int i = 0; i < NumNniIf_(); i++) {
        status = EnableUplinkIf_(i);
        if (!status.ok()) {
            // raise alarm to report error in enabling PON
            pushOltOperInd(i, "nni", "down");
        }
        else
            pushOltOperInd(i, "nni", "up");
    }
#endif
    //only for nni-65536 mapping to intf_id 0
    uint32_t nni_speed = GetNniSpeed_(0);
    status = SetStateUplinkIf_(0, true);
    if (!status.ok()) {
        // raise alarm to report error in enabling NNI
        pushOltOperInd(0, "nni", "down", nni_speed);
    }
    else{
        pushOltOperInd(0, "nni", "up", nni_speed);
    }

    for (int i = 1; i < argc; ++i) {
       if(strcmp(argv[i-1], "--interface") == 0 || (strcmp(argv[i-1], "--intf") == 0)) {
          grpc_server_interface_name = argv[i];
          break;
       }
    }

    if (!RunServer(argc, argv)) {
        std::cerr << "FATAL: gRPC server creation failed\n";
        return 2;
    }

    return 0;
}

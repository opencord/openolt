/*
    Copyright (C) 2018 Open Networking Foundation 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <unistd.h>

#include "server.h"
#include "core.h"

int main(int argc, char** argv) {

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

    ProbeDeviceCapabilities_();
    sleep(2);
    // Enable all PON interfaces. 
    for (int i = 0; i < NumPonIf_(); i++) {
        status = EnablePonIf_(i);
        if (!status.ok()) {
            // raise alarm to report error in enabling PON
            pushOltOperInd(i, "pon", "down");
        }
        else
            pushOltOperInd(i, "pon", "up");
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
    status = SetStateUplinkIf_(0, true);
    if (!status.ok()) {
        // raise alarm to report error in enabling NNI
        pushOltOperInd(0, "nni", "down");
    }
    else
        pushOltOperInd(0, "nni", "up");
    RunServer();

    return 0;
}

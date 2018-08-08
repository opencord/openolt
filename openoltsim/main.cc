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
#include <pthread.h>

#include "server.h"
#include "core.h"
#include "state.h"

void* RunSim(void *) {
    while (!state::is_connected()) {
        sleep(5);
    }

    // Send Olt up indication
    {
        openolt::Indication ind;
        openolt::OltIndication* olt_ind = new openolt::OltIndication;
        olt_ind->set_oper_state("up");
        ind.set_allocated_olt_ind(olt_ind);
        std::cout << "olt indication, oper_state:" << ind.olt_ind().oper_state() << std::endl;
        oltIndQ.push(ind);
    }

    // TODO - Add interface and onu indication events
}

int main(int argc, char** argv) {
    pthread_t simThread;

    pthread_create(&simThread, NULL, RunSim, NULL);
    RunServer();

    return 0;
}

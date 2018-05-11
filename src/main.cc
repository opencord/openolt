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

#include "indications.h"
#include "server.h"
#include "core.h"

extern "C"
{
#include <bal_api.h>
#include <bal_api_end.h>
}

int main(int argc, char** argv) {

    if (argc < 5) {
        std::cout << "Missing arguments" << std::endl;
        exit(1);
    }

    bcmbal_init(argc, argv, NULL);

    Status status = SubscribeIndication();
    if (!status.ok()) {
        std::cout << "ERROR: SubscribeIndication failed - "
                  << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return 1;
    }

    status = Enable_();
    if (!status.ok()) {
        std::cout << "ERROR: Enable_ failed - "
                  << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return 1;
    }

    RunServer();

  return 0;
}

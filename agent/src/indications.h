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

#ifndef OPENOLT_INDICATIONS_H_
#define OPENOLT_INDICATIONS_H_

#include <grpc++/grpc++.h>
#include <openolt.grpc.pb.h>
#include "Queue.h"

extern "C" {
    #include <bcm_dev_log_task.h>
}

extern Queue<openolt::Indication> oltIndQ;
extern grpc::Status SubscribeIndication();
extern dev_log_id openolt_log_id;
extern dev_log_id omci_log_id;
extern uint32_t nni_intf_id;

#endif

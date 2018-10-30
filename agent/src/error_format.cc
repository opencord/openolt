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

#include "error_format.h"
using grpc::Status;
using grpc::StatusCode;


Status bcm_to_grpc_err(bcmos_errno bcm_err, std::string message) {
    StatusCode grpc_err = StatusCode::INTERNAL;

    switch (bcm_err) {
        case BCM_ERR_PARM:
            grpc_err = StatusCode::INVALID_ARGUMENT;
            break;
        case BCM_ERR_RANGE:
            grpc_err = StatusCode::OUT_OF_RANGE;
            break;
        case BCM_ERR_NOT_SUPPORTED:
            grpc_err = StatusCode::UNIMPLEMENTED;
            break;
        case BCM_ERR_NOENT:
        case BCM_ERR_NODEV:
            grpc_err = StatusCode::NOT_FOUND;
            break;
        case BCM_ERR_TIMEOUT:
        case BCM_ERR_TOO_LONG:
        case BCM_ERR_TOO_MANY_REQS:
            grpc_err = StatusCode::DEADLINE_EXCEEDED;
            break;
        case BCM_ERR_ALREADY:
            grpc_err = StatusCode::ALREADY_EXISTS;
            break;
        case BCM_ERR_NO_MORE:
        case BCM_ERR_INSUFFICIENT_LIST_MEM:
            grpc_err = StatusCode::RESOURCE_EXHAUSTED;
            break;
    }

    message.append(" BCM Error ");
    message.append(std::to_string(bcm_err));

    return Status(grpc_err, message);
}

std::string grpc_status_code_to_string(StatusCode status_code) {
    switch (status_code) {
        case StatusCode::OK:
            return "StatusCode::OK";
        case StatusCode::CANCELLED:
            return "StatusCode::CANCELED";
        case StatusCode::UNKNOWN:
            return "StatusCode::UNKNOWN";
        case StatusCode::INVALID_ARGUMENT:
            return "StatusCode::INVALID_ARGUMENT";
        case StatusCode::DEADLINE_EXCEEDED:
            return "StatusCode::DEADLINE_EXCEEDED";
        case StatusCode::NOT_FOUND:
            return "StatusCode::NOT_FOUND";
        case StatusCode::ALREADY_EXISTS:
            return "StatusCode::ALREADY_EXISTS";
        case StatusCode::PERMISSION_DENIED:
            return "StatusCode::PERMISSION_DENIED";
        case StatusCode::UNAUTHENTICATED:
            return "StatusCode::UNAUTHENTICATED";
        case StatusCode::RESOURCE_EXHAUSTED:
            return "StatusCode::RESOURCE_EXHAUSTED";
        case StatusCode::FAILED_PRECONDITION:
            return "StatusCode::FAILED_PRECONDITION";
        case StatusCode::ABORTED:
            return "StatusCode::ABORTED";
        case StatusCode::OUT_OF_RANGE:
            return "StatusCode::OUT_OF_RANGE";
        case StatusCode::INTERNAL:
            return "StatusCode::INTERNAL";
        case StatusCode::UNAVAILABLE:
            return "StatusCode::UNAVAILABLE";
        case StatusCode::DATA_LOSS:
            return "StatusCode::DATA_LOSS";
        case StatusCode::DO_NOT_USE:
            return "StatusCode::DO_NOT_USE";
    }
    return "Unknown GRPC status Code";

}

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

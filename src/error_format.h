#ifndef OPENOLT_ERROR_FORMAT_H_
#define OPENOLT_ERROR_FORMAT_H_

#include <string>
#include <grpc++/grpc++.h>

extern "C"
{
#include <bcmos_system.h>
#include <bal_api.h>
#include <bal_api_end.h>
}

grpc::Status bcm_to_grpc_err(bcmos_errno bcm_err, std::string message);
std::string grpc_status_code_to_string(grpc::StatusCode status_code);


#endif

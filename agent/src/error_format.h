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

#ifndef OPENOLT_ERROR_FORMAT_H_
#define OPENOLT_ERROR_FORMAT_H_

#include <string>
#include <grpc++/grpc++.h>

extern "C"
{
#include <bcmos_system.h>
#include <bcmolt_api.h>
}

grpc::Status bcm_to_grpc_err(bcmos_errno bcm_err, std::string message);
std::string grpc_status_code_to_string(grpc::StatusCode status_code);


#endif

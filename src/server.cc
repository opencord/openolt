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
#include <memory>
#include <string>
#include <time.h>
#include <pthread.h>

#include "Queue.h"
#include <iostream>
#include <sstream>

#include "server.h"
#include "core.h"
#include "indications.h"
#include "stats_collection.h"
#include "state.h"

#include <grpc++/grpc++.h>
#include <openolt.grpc.pb.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

const char *serverPort = "0.0.0.0:9191";
int signature;

class OpenoltService final : public openolt::Openolt::Service {

    Status ActivateOnu(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::Empty* response) override {
        return ActivateOnu_(
            request->intf_id(),
            request->onu_id(),
            ((request->serial_number()).vendor_id()).c_str(),
            ((request->serial_number()).vendor_specific()).c_str());
    }

    Status DeactivateOnu(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::Empty* response) override {
        return DeactivateOnu_(
            request->intf_id(),
            request->onu_id(),
            ((request->serial_number()).vendor_id()).c_str(),
            ((request->serial_number()).vendor_specific()).c_str());
    }

    Status DeleteOnu(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::Empty* response) override {
        return DeleteOnu_(
            request->intf_id(),
            request->onu_id(),
            ((request->serial_number()).vendor_id()).c_str(),
            ((request->serial_number()).vendor_specific()).c_str());
    }

    Status OmciMsgOut(
            ServerContext* context,
            const openolt::OmciMsg* request,
            openolt::Empty* response) override {
        return OmciMsgOut_(
            request->intf_id(),
            request->onu_id(),
            request->pkt());
    }

    Status OnuPacketOut(
            ServerContext* context,
            const openolt::OnuPacket* request,
            openolt::Empty* response) override {
        return OnuPacketOut_(
            request->intf_id(),
            request->onu_id(),
            request->pkt());
    }

    Status UplinkPacketOut(
            ServerContext* context,
            const openolt::UplinkPacket* request,
            openolt::Empty* response) override {
        return UplinkPacketOut_(
            request->intf_id(),
            request->pkt());
    }

    Status FlowAdd(
            ServerContext* context,
            const openolt::Flow* request,
            openolt::Empty* response) override {
        return FlowAdd_(
            request->onu_id(),
            request->flow_id(),
            request->flow_type(),
            request->access_intf_id(),
            request->network_intf_id(),
            request->gemport_id(),
            request->priority(),
            request->classifier(),
            request->action());
    }

    Status EnableIndication(
            ServerContext* context,
            const ::openolt::Empty* request,
            ServerWriter<openolt::Indication>* writer) override {
        std::cout << "Connection to Voltha established. Indications enabled"
        << std::endl;
        state::connect();

        while (state::is_connected) {
            auto oltInd = oltIndQ.pop();
            bool isConnected = writer->Write(oltInd);
            if (!isConnected) {
                //Lost connectivity to this Voltha instance
                //Put the indication back in the queue for next connecting instance
                oltIndQ.push(oltInd);
                state::disconnect();
            }
            //oltInd.release_olt_ind()
        }

        return Status::OK;
    }

    Status HeartbeatCheck(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::Heartbeat* response) override {
        response->set_heartbeat_signature(signature);

        return Status::OK;
    }

    Status EnablePonIf(
            ServerContext* context,
            const openolt::Interface* request,
            openolt::Empty* response) override {

        return EnablePonIf_(request->intf_id());
    }

    Status DisablePonIf(
            ServerContext* context,
            const openolt::Interface* request,
            openolt::Empty* response) override {

        return DisablePonIf_(request->intf_id());
    }

};

void RunServer() {
  OpenoltService service;
  std::string server_address(serverPort);
  ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());

  time_t now;
  time(&now);
  signature = (int)now;

  std::cout << "Server listening on " << server_address
  << ", connection signature : " << signature << std::endl;


  server->Wait();
}

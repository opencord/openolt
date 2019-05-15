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
#include "state.h"

#include <grpc++/grpc++.h>
#include <openolt.grpc.pb.h>
#include <tech_profile.grpc.pb.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

const char *serverPort = "0.0.0.0:9191";
int signature;

Queue<openolt::Indication> oltIndQ;

class OpenoltService final : public openolt::Openolt::Service {

    Status DisableOlt(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::Empty* response) override {
        return Disable_();
    }

    Status ReenableOlt(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::Empty* response) override {
        return Reenable_();
    }

    Status ActivateOnu(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::Empty* response) override {
        return ActivateOnu_(
            request->intf_id(),
            request->onu_id(),
            ((request->serial_number()).vendor_id()).c_str(),
            ((request->serial_number()).vendor_specific()).c_str(), request->pir());
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
            request->port_no(),
            request->gemport_id(),
            request->pkt());
    }

    Status UplinkPacketOut(
            ServerContext* context,
            const openolt::UplinkPacket* request,
            openolt::Empty* response) override {
        return UplinkPacketOut_(
            request->intf_id(),
            request->pkt(),
            request->flow_id());
    }

    Status FlowAdd(
            ServerContext* context,
            const openolt::Flow* request,
            openolt::Empty* response) override {
        return FlowAdd_(
            request->access_intf_id(),
            request->onu_id(),
            request->uni_id(),
            request->port_no(),
            request->flow_id(),
            request->flow_type(),
            request->alloc_id(),
            request->network_intf_id(),
            request->gemport_id(),
            request->classifier(),
            request->action(),
            request->priority(),
            request->cookie());
    }

    Status FlowRemove(
            ServerContext* context,
            const openolt::Flow* request,
            openolt::Empty* response) override {
        return FlowRemove_(
            request->flow_id(),
            request->flow_type());
    }

    Status EnableIndication(
            ServerContext* context,
            const ::openolt::Empty* request,
            ServerWriter<openolt::Indication>* writer) override {

        std::cout << "Connection to Voltha established. Indications enabled"
        << std::endl;

        if (state.previsouly_connected()) {
            // Reconciliation / recovery case
            std::cout << "Reconciliation / Recovery case" << std::endl;
            if (state.is_activated()){
                // Adding extra olt indication of current state
                openolt::Indication ind;
                openolt::OltIndication* oltInd = new openolt::OltIndication();
                if (state.is_activated()) {
                    oltInd->set_oper_state("up");
                    std::cout << "Extra OLT indication up" << std::endl;
                } else {
                    oltInd->set_oper_state("down");
                    std::cout << "Extra OLT indication down" << std::endl;
                }
                ind.set_allocated_olt_ind(oltInd);
                oltIndQ.push(ind);
            }
        }

        state.connect();

        while (state.is_connected()) {
            std::pair<openolt::Indication, bool> ind = oltIndQ.pop(COLLECTION_PERIOD);
            if (ind.second == false) {
                /* timeout - do lower priority periodic stuff like stats */
                stats_collection();
                continue;
            }
            openolt::Indication oltInd = ind.first;
            bool isConnected = writer->Write(oltInd);
            if (!isConnected) {
                //Lost connectivity to this Voltha instance
                //Put the indication back in the queue for next connecting instance
                oltIndQ.push(oltInd);
                state.disconnect();
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

    Status CollectStatistics(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::Empty* response) override {

        stats_collection();

        return Status::OK;
    }

    Status Reboot(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::Empty* response) override {

        uint8_t ret = system("shutdown -r now");

        return Status::OK;

    }

    Status GetDeviceInfo(
            ServerContext* context,
            const openolt::Empty* request,
            openolt::DeviceInfo* response) override {

        GetDeviceInfo_(response);

        return Status::OK;

    }

    Status CreateTrafficSchedulers(
            ServerContext* context,
            const tech_profile::TrafficSchedulers* request,
            openolt::Empty* response) override {
        CreateTrafficSchedulers_(request);
        return Status::OK;
    };

    Status RemoveTrafficSchedulers(
            ServerContext* context,
            const tech_profile::TrafficSchedulers* request,
            openolt::Empty* response) override {
        RemoveTrafficSchedulers_(request);
        return Status::OK;
    };

    Status CreateTrafficQueues(
            ServerContext* context,
            const tech_profile::TrafficQueues* request,
            openolt::Empty* response) override {
        CreateTrafficQueues_(request);
        return Status::OK;
    };

    Status RemoveTrafficQueues(
            ServerContext* context,
            const tech_profile::TrafficQueues* request,
            openolt::Empty* response) override {
        RemoveTrafficQueues_(request);
        return Status::OK;
    };

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

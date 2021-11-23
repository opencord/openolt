/*
 * Copyright 2018-present Open Networking Foundation

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include "../src/core_utils.h"

#include <grpc++/grpc++.h>
#include <voltha_protos/openolt.grpc.pb.h>
#include <voltha_protos/tech_profile.grpc.pb.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ResourceQuota;
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
            ((request->serial_number()).vendor_specific()).c_str(), request->pir(), request->omcc_encryption());
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
            request->pkt());
    }

    Status FlowAdd(
            ServerContext* context,
            const openolt::Flow* request,
            openolt::Empty* response) override {
        return FlowAddWrapper_(request);

    }

    Status FlowRemove(
            ServerContext* context,
            const openolt::Flow* request,
            openolt::Empty* response) override {
        return FlowRemoveWrapper_(request);
    }

    Status EnableIndication(
            ServerContext* context,
            const ::openolt::Empty* request,
            ServerWriter<openolt::Indication>* writer) override {

        std::cout << "Connection to Voltha established. Indications enabled"
        << std::endl;

        if (state.previously_connected()) {
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
            std::pair<openolt::Indication, bool> ind = oltIndQ.pop(COLLECTION_PERIOD*1000, 1000);
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

    /*Status GetPonIf(
            ServerContext* context,
            const openolt::Interface* request,
            openolt::IntfIndication* response) override {

        // TODO - Return the oper status of the pon interface
        return Status::OK;
    }*/

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
        return CreateTrafficSchedulers_(request);
    };

    Status RemoveTrafficSchedulers(
            ServerContext* context,
            const tech_profile::TrafficSchedulers* request,
            openolt::Empty* response) override {
        return RemoveTrafficSchedulers_(request);
    };

    Status CreateTrafficQueues(
            ServerContext* context,
            const tech_profile::TrafficQueues* request,
            openolt::Empty* response) override {
        return CreateTrafficQueues_(request);
    };

    Status RemoveTrafficQueues(
            ServerContext* context,
            const tech_profile::TrafficQueues* request,
            openolt::Empty* response) override {
        return RemoveTrafficQueues_(request);
    };

    Status PerformGroupOperation(
            ServerContext* context,
            const openolt::Group* request,
            openolt::Empty* response) override {
        return PerformGroupOperation_(request);
    };

    Status DeleteGroup(
            ServerContext* context,
            const openolt::Group* request,
            openolt::Empty* response) override {
        return DeleteGroup_(request->group_id());
    };

    Status OnuItuPonAlarmSet(
            ServerContext* context,
            const config::OnuItuPonAlarm* request,
            openolt::Empty* response) override {
        return OnuItuPonAlarmSet_(request);
    };

    Status GetLogicalOnuDistanceZero(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::OnuLogicalDistance* response) override {
        return GetLogicalOnuDistanceZero_(
            request->intf_id(),
            response);
    };

    Status GetLogicalOnuDistance(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::OnuLogicalDistance* response) override {
        return GetLogicalOnuDistance_(
            request->intf_id(),
            request->onu_id(),
            response);
    };

    Status GetOnuStatistics(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::OnuStatistics* response) override {
        return GetOnuStatistics_(
            request->intf_id(),
            request->onu_id(),
            response);
    }

    Status GetGemPortStatistics(
            ServerContext* context,
            const openolt::OnuPacket* request,
            openolt::GemPortStatistics* response) override {
        return GetGemPortStatistics_(
            request->intf_id(),
            request->gemport_id(),
            response);
    }

    Status GetPonRxPower(
            ServerContext* context,
            const openolt::Onu* request,
            openolt::PonRxPowerData* response) override {
        return GetPonRxPower_(
            request->intf_id(),
            request->onu_id(),
            response);
    }
};

bool RunServer(int argc, char** argv) {
    std::string ipAddress = "0.0.0.0";
    bool tls_enabled = false;
    std::pair<grpc_ssl_client_certificate_request_type, bool> grpc_security;
    std::shared_ptr<grpc::ServerCredentials> credentials;

    for (int i = 1; i < argc; ++i) {
        if(strcmp(argv[i-1], "--interface") == 0 || (strcmp(argv[i-1], "--intf") == 0)) {
            ipAddress = get_ip_address(argv[i]);
            break;
        }
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i-1], "--enable-tls") == 0) {
            grpc_security = get_grpc_tls_option(argv[i]);
            if (grpc_security.second) {
                tls_enabled = true;
            } else {
                std::cerr << "unknown security option: \"" << argv[i-1] << " " << argv[i] << "\"\n";
                return false;
            };
            break;
        }
    }

    if (tls_enabled) {
        std::string dir_cert{"./keystore"};
        auto read_root_crt = read_from_txt_file(dir_cert + "/root.crt");
        auto read_server_key = read_from_txt_file(dir_cert + "/server.key");
        auto read_server_crt = read_from_txt_file(dir_cert + "/server.crt");

        if (read_root_crt.second && read_server_key.second && read_server_crt.second) {
            std::cout << "certificate files read successfully\n";
        } else {
            std::cerr << std::boolalpha << "certificate files read failed - root.crt: " << read_root_crt.second
                                                                    << ", server.key: " << read_server_key.second
                                                                    << ", server.crt: " << read_server_crt.second << '\n';
            return false;
        }

        std::string root_crt = read_root_crt.first;
        std::string server_key = read_server_key.first;
        std::string server_crt = read_server_crt.first;

        grpc::SslServerCredentialsOptions ssl_opts{grpc_security.first};
        ssl_opts.pem_root_certs = root_crt;
        grpc::SslServerCredentialsOptions::PemKeyCertPair keycert = {server_key, server_crt};
        ssl_opts.pem_key_cert_pairs.push_back(keycert);
        credentials = grpc::SslServerCredentials(ssl_opts);
    } else {
        credentials = grpc::InsecureServerCredentials();
    }

    serverPort = ipAddress.append(":9191").c_str();
    OpenoltService service;
    std::string server_address(serverPort);
    ::ServerBuilder builder;
    ::ResourceQuota quota;
    quota.SetMaxThreads(GRPC_THREAD_POOL_SIZE);
    builder.SetResourceQuota(quota);
    builder.AddListeningPort(server_address, credentials);
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());

    time_t now;
    time(&now);
    signature = (int)now;

    std::cout << "Server listening on " << server_address
    << ", connection signature : " << signature << std::endl;

#ifdef TEST_MODE
    server->Shutdown();
#else
    server->Wait();
#endif

    return true;
}

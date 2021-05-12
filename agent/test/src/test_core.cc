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
#include "gtest/gtest.h"
#include "Queue.h"
#include "bal_mocker.h"
#include "core.h"
#include "core_data.h"
#include "server.h"
#include <future>
#include <fstream>
#include "trx_eeprom_reader.h"
using namespace testing;
using namespace std;

extern std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *> alloc_cfg_compltd_map;
extern dev_log_id openolt_log_id;
extern bcmos_fastlock alloc_cfg_wait_lock;

class TestOltEnable : public Test {
 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test
    // (right before the destructor).
  }
};

// This is used to set custom bcmolt_cfg value to bcmolt_cfg pointer coming in
// bcmolt_cfg_get__bal_state_stub.
ACTION_P(SetArg1ToBcmOltCfg, value) { *static_cast<bcmolt_olt_cfg*>(arg1) = value; };

// This is used to set custom bcmolt_onu_cfg value to bcmolt_onu_cfg pointer coming in
// bcmolt_cfg_get__onu_state_stub.
ACTION_P(SetArg1ToBcmOltOnuCfg, value) { *static_cast<bcmolt_onu_cfg*>(arg1) = value; };

// This is used to set custom bcmolt_tm_sched_cfg value to bcmolt_tm_sched_cfg pointer coming in
// bcmolt_cfg_get__tm_sched_stub.
ACTION_P(SetArg1ToBcmOltTmSchedCfg, value) { *static_cast<bcmolt_tm_sched_cfg*>(arg1) = value; };

// This is used to set custom bcmolt_pon_interface_cfg value to bcmolt_pon_interface_cfg pointer coming in
// bcmolt_cfg_get__pon_intf_stub.
ACTION_P(SetArg1ToBcmOltPonCfg, value) { *static_cast<bcmolt_pon_interface_cfg*>(arg1) = value; };

// This is used to set custom bcmolt_nni_interface_cfg value to bcmolt_nni_interface_cfg pointer coming in
// bcmolt_cfg_get__nni_intf_stub.
ACTION_P(SetArg1ToBcmOltNniCfg, value) { *static_cast<bcmolt_nni_interface_cfg*>(arg1) = value; };

// This is used to set custom bcmolt_flow_cfg value to bcmolt_flow_cfg pointer coming in
// bcmolt_cfg_get__flow_stub.
ACTION_P(SetArg1ToBcmOltFlowCfg, value) { *static_cast<bcmolt_flow_cfg*>(arg1) = value; };

// Create a mock function for bcmolt_cfg_get__bal_state_stub C++ function
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__bal_state_stub, bcmos_errno(bcmolt_oltid, void*));
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__onu_state_stub, bcmos_errno(bcmolt_oltid, void*));
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__tm_sched_stub, bcmos_errno(bcmolt_oltid, void*));
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__pon_intf_stub, bcmos_errno(bcmolt_oltid, void*));
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__nni_intf_stub, bcmos_errno(bcmolt_oltid, void*));
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__flow_stub, bcmos_errno(bcmolt_oltid, void*));

// Test Fixture for OltEnable

// Test 1: OltEnableSuccess case
TEST_F(TestOltEnable, OltEnableSuccess){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_res = BCM_ERR_NOT_CONNECTED;
    bcmos_errno olt_oper_res = BCM_ERR_OK;

    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    olt_cfg.data.bal_state = BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY;

    Status olt_enable_res;

    ON_CALL(balMock, bcmolt_host_init(_)).WillByDefault(Return(host_init_res));
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__bal_state_stub, bcmolt_cfg_get__bal_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltCfg(olt_cfg), Return(bal_cfg_get_stub_res)));
    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(BCM_MAX_DEVS_PER_LINE_CARD)
                         .WillRepeatedly(Return(bal_cfg_get_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));

    olt_enable_res = Enable_(1, NULL);
    ASSERT_TRUE( olt_enable_res.error_message() == Status::OK.error_message() );
}

// Test 2: OltEnableFail_host_init_fail
TEST_F(TestOltEnable, OltEnableFail_host_init_fail) {
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_INTERNAL;

    Status olt_enable_res;

    // Ensure that the state of the OLT is in deactivated to start with..
    state.deactivate();

    ON_CALL(balMock, bcmolt_host_init(_)).WillByDefault(Return(host_init_res));

    olt_enable_res = Enable_(1, NULL);
    ASSERT_TRUE( olt_enable_res.error_message() != Status::OK.error_message() );
}

// Test 3: OltEnableSuccess_PON_Device_Connected
TEST_F(TestOltEnable, OltEnableSuccess_PON_Device_Connected) {

    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_res = BCM_ERR_OK;
    bcmos_errno olt_oper_res = BCM_ERR_OK;

    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    olt_cfg.data.bal_state = BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY;

    Status olt_enable_res;

    // Ensure that the state of the OLT is in deactivated to start with..
    state.deactivate();

    ON_CALL(balMock, bcmolt_host_init(_)).WillByDefault(Return(host_init_res));
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__bal_state_stub, bcmolt_cfg_get__bal_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltCfg(olt_cfg), Return(bal_cfg_get_stub_res)));
    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(BCM_MAX_DEVS_PER_LINE_CARD)
                         .WillRepeatedly(Return(bal_cfg_get_res));

    olt_enable_res = Enable_(1, NULL);
    ASSERT_TRUE( olt_enable_res.error_message() == Status::OK.error_message() );

}

// Test 4: OltEnableFail_All_PON_Enable_Fail
TEST_F(TestOltEnable, OltEnableFail_All_PON_Enable_Fail) {

    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_res = BCM_ERR_NOT_CONNECTED;
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;

    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    olt_cfg.data.bal_state = BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY;

    Status olt_enable_res;

    // Ensure that the state of the OLT is in deactivated to start with..
    state.deactivate();

    ON_CALL(balMock, bcmolt_host_init(_)).WillByDefault(Return(host_init_res));
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__bal_state_stub, bcmolt_cfg_get__bal_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltCfg(olt_cfg), Return(bal_cfg_get_stub_res)));
    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(BCM_MAX_DEVS_PER_LINE_CARD)
                         .WillRepeatedly(Return(bal_cfg_get_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));

    olt_enable_res = Enable_(1, NULL);

    ASSERT_TRUE( olt_enable_res.error_message() != Status::OK.error_message() );
}

// Test 5 OltEnableSuccess_One_PON_Enable_Fail : One PON device enable fails, but all others succeed.
TEST_F(TestOltEnable, OltEnableSuccess_One_PON_Enable_Fail) {

    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_res = BCM_ERR_NOT_CONNECTED;
    bcmos_errno olt_oper_res_fail = BCM_ERR_INTERNAL;
    bcmos_errno olt_oper_res_success = BCM_ERR_OK;

    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };
    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);
    olt_cfg.data.bal_state = BCMOLT_BAL_STATE_BAL_AND_SWITCH_READY;

    Status olt_enable_res;

    // Ensure that the state of the OLT is in deactivated to start with..
    state.deactivate();

    ON_CALL(balMock, bcmolt_host_init(_)).WillByDefault(Return(host_init_res));
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__bal_state_stub, bcmolt_cfg_get__bal_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltCfg(olt_cfg), Return(bal_cfg_get_stub_res)));
    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(BCM_MAX_DEVS_PER_LINE_CARD)
                         .WillRepeatedly(Return(bal_cfg_get_res));
    // For the the first PON mac device, the activation result will fail, and will succeed for all other PON mac devices.
    EXPECT_CALL(balMock, bcmolt_oper_submit(_, _))
                         .WillOnce(Return(olt_oper_res_fail))
                         .WillRepeatedly(Return(olt_oper_res_success));
    olt_enable_res = Enable_(1, NULL);

    ASSERT_TRUE( olt_enable_res.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////
// For testing Enable/Disable functionality
////////////////////////////////////////////////////////////////////////

int num_of_pon_port = 16;

// Create a mock function for bcmolt_cfg_get__olt_topology_stub C++ function
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__olt_topology_stub, bcmos_errno(bcmolt_oltid, void*));

class TestOltDisableReenable : public Test {
 protected:
  virtual void SetUp() {
    NiceMock<BalMocker> balMock;
    bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;

    bcmolt_olt_cfg olt_cfg = { };
    bcmolt_olt_key olt_key = { };

    BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);

    olt_cfg.data.topology.topology_maps.len = num_of_pon_port;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__olt_topology_stub, bcmolt_cfg_get__olt_topology_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltCfg(olt_cfg), Return(bal_cfg_get_stub_res)));

    ProbeDeviceCapabilities_();

  }

  virtual void TearDown() {
    // Code here will be called immediately after each test
    // (right before the destructor).
  }
};


// Test Fixture for OltDisable

// Test 1: OltDisableSuccess  case
TEST_F(TestOltDisableReenable, OltDisableSuccess){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno olt_oper_res = BCM_ERR_OK;
    bcmos_errno olt_get_res = BCM_ERR_OK;

    Status olt_disable_res;
    state.deactivate();
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(olt_get_res));
    olt_disable_res = Disable_();
    ASSERT_TRUE( olt_disable_res.error_message() == Status::OK.error_message() );

}

// Test 2: OltDisableAllPonFailed  case
TEST_F(TestOltDisableReenable, OltDisableAllPonFailed){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;
    bcmos_errno pon_cfg_get_res = BCM_ERR_INTERNAL;

    Status olt_disable_res;
    state.deactivate();
    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(pon_cfg_get_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    olt_disable_res = Disable_();
    ASSERT_TRUE( olt_disable_res.error_code() == grpc::StatusCode::INTERNAL);
}


// Test Fixture for OltReenable

// Test 1: OltReenableSuccess  case
TEST_F(TestOltDisableReenable, OltReenableSuccess){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    uint32_t pon_id = 0;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    Status olt_reenable_res;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .Times(num_of_pon_port)
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .Times(num_of_pon_port)
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));

    olt_reenable_res = Reenable_();
    ASSERT_TRUE( olt_reenable_res.error_message() == Status::OK.error_message() );

}

// Test 2: OltReenableAllPonFailed case
TEST_F(TestOltDisableReenable, OltReenableAllPonFailed){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    uint32_t pon_id = 0;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;
    Status olt_reenable_res;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .Times(num_of_pon_port)
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));
    EXPECT_CALL(balMock,bcmolt_oper_submit(_, _))
                         .Times(num_of_pon_port)
                         .WillRepeatedly(Return(olt_oper_res));

    olt_reenable_res = Reenable_();
    ASSERT_TRUE( olt_reenable_res.error_code() == grpc::StatusCode::INTERNAL);
}

////////////////////////////////////////////////////////////////////////////
// For testing ProbeDeviceCapabilities functionality
////////////////////////////////////////////////////////////////////////////
class TestProbeDevCapabilities : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        bcmos_errno olt_res_success = BCM_ERR_OK;
        bcmos_errno olt_res_fail = BCM_ERR_COMM_FAIL;
        bcmos_errno dev_res_success = BCM_ERR_OK;
        bcmos_errno dev_res_fail = BCM_ERR_COMM_FAIL;

        virtual void SetUp() {
            bcmos_errno bal_cfg_get_stub_res = BCM_ERR_OK;

            bcmolt_olt_cfg olt_cfg = { };
            bcmolt_olt_key olt_key = { };

            BCMOLT_CFG_INIT(&olt_cfg, olt, olt_key);

            olt_cfg.data.topology.topology_maps.len = num_of_pon_port;
        }

        virtual void TearDown() {
        }
};

// Test 1 - If querying the OLT fails, the method must return error
TEST_F(TestProbeDevCapabilities, ProbeDev_OltQueryFailed) {

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__olt_topology_stub, bcmolt_cfg_get__olt_topology_stub(_,_))
                     .WillOnce(Return(olt_res_fail));

    Status query_status = ProbeDeviceCapabilities_();
    ASSERT_TRUE( query_status.error_message() != Status::OK.error_message() );
}

// Test 2 - If all devices are queried successfully, the method must return Status::OK
TEST_F(TestProbeDevCapabilities, ProbeDev_OltQuerySucceeded_DevQueriesSucceeded) {

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__olt_topology_stub, bcmolt_cfg_get__olt_topology_stub(_,_))
                     .WillOnce(Return(olt_res_success));

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
        .WillRepeatedly(Return(dev_res_success));

    Status query_status = ProbeDeviceCapabilities_();

    ASSERT_TRUE( query_status.error_message() == Status::OK.error_message() );
}

// Test 3 - After successfully probing the OLT, even if probing all devices failed, the method must return error
TEST_F(TestProbeDevCapabilities, ProbedDev_OltQuerySucceeded_AllDevQueriesFailed) {

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__olt_topology_stub, bcmolt_cfg_get__olt_topology_stub(_,_))
                     .WillOnce(Return(olt_res_success));

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
        .WillRepeatedly(Return(dev_res_fail));

    Status query_status = ProbeDeviceCapabilities_();

    ASSERT_TRUE( query_status.error_message() != Status::OK.error_message() );
}

// Test 4 - After successfully probing the OLT, if probing some devices fail, the method returns success
TEST_F(TestProbeDevCapabilities, ProbedDev_OltQuerySucceeded_SomeDevQueriesFailed) {

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__olt_topology_stub, bcmolt_cfg_get__olt_topology_stub(_,_))
                     .WillOnce(Return(olt_res_success));

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
        .WillOnce(Return(olt_res_success))
        .WillRepeatedly(Return(dev_res_fail));

    Status query_status = ProbeDeviceCapabilities_();

    ASSERT_TRUE( query_status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing EnablePonIf functionality
////////////////////////////////////////////////////////////////////////////

class TestEnablePonIf : public Test {
    protected:
        uint32_t pon_id = 0;
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - EnablePonIf, Downstream DefaultSched & DefaultQueues creation success case
TEST_F(TestEnablePonIf, EnablePonIfDefaultSchedQueuesSuccess) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - EnablePonIf success but Downstream DefaultSched Query failure case
TEST_F(TestEnablePonIf, EnablePonIfSuccessDefaultSchedQueryFailure) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_INTERNAL;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - EnablePonIf success but Downstream DefaultSched already in Configured state
TEST_F(TestEnablePonIf, EnablePonIfSuccessDefaultSchedAlreadyConfigured) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(olt_cfg_get_res));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - EnablePonIf success but Downstream DefaultSched & DefaultQueues creation failed
TEST_F(TestEnablePonIf, EnablePonIfSuccessDefaultSchedQueuesFailed) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_err = BCM_ERR_INTERNAL;
    bcmos_errno olt_cfg_get_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    EXPECT_CALL(balMock, bcmolt_cfg_set(_, _))
                         .WillOnce(Return(olt_cfg_set_res))
                         .WillRepeatedly(Return(olt_cfg_set_err));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(olt_cfg_get_res));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 5 - EnablePonIf already enabled success
TEST_F(TestEnablePonIf, EnablePonIfAlreadyEnabled) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 6 - EnablePonIf - enable onu discovery failure case
TEST_F(TestEnablePonIf, EnablePonIfEnableOnuDiscFailed) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 7 - EnablePonIf failure case
TEST_F(TestEnablePonIf, EnablePonIfFailed) {
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_INTERNAL;

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    Status status = EnablePonIf_(pon_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing SetStateUplinkIf functionality
////////////////////////////////////////////////////////////////////////////

class TestSetStateUplinkIf : public Test {
    protected:
        uint32_t intf_id = 0;
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - SetStateUplinkIf NNI intf already enabled, Upstream DefaultSched & DefaultQueues creation success case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfAlreadyEnabledDefaultSchedQueuesSuccess) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = SetStateUplinkIf_(intf_id, true);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - SetStateUplinkIf, NNI interface already disabled case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfAlreadyDisabled) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));

    Status status = SetStateUplinkIf_(intf_id, false);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - SetStateUplinkIf Enable success, Upstream DefaultSched & DefaultQueues creation success case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfDefaultSchedQueuesSuccess) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_get_tmstub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE__BEGIN;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltTmSchedCfg(tm_sched_cfg), Return(olt_cfg_get_tmstub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    Status status = SetStateUplinkIf_(intf_id, true);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - SetStateUplinkIf Enable failure case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfEnableFailure) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_INTERNAL;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE__BEGIN;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    Status status = SetStateUplinkIf_(intf_id, true);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - SetStateUplinkIf Disable success case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfDisableSuccess) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_OK;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    Status status = SetStateUplinkIf_(intf_id, false);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 6 - SetStateUplinkIf Disable failure case
TEST_F(TestSetStateUplinkIf, SetStateUplinkIfDisableFailure) {
    bcmos_errno olt_cfg_get_nni_stub_res = BCM_ERR_OK;
    bcmos_errno olt_oper_sub_res = BCM_ERR_INTERNAL;

    bcmolt_nni_interface_key nni_key;
    bcmolt_nni_interface_cfg nni_cfg;
    nni_key.id = intf_id;
    BCMOLT_CFG_INIT(&nni_cfg, nni_interface, nni_key);
    nni_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__nni_intf_stub, bcmolt_cfg_get__nni_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltNniCfg(nni_cfg), Return(olt_cfg_get_nni_stub_res)));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_sub_res));

    Status status = SetStateUplinkIf_(intf_id, false);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing DisablePonIf functionality
////////////////////////////////////////////////////////////////////////////

class TestDisablePonIf : public Test {
    protected:
        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - DisablePonIf success case - PON port in BCMOLT_INTERFACE_STATE_ACTIVE_WORKING state
TEST_F(TestDisablePonIf, DisablePonIfSuccess) {
    bcmos_errno olt_oper_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_set_res = BCM_ERR_OK;
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;

    bcmos_errno pon_intf_get_res = BCM_ERR_OK;
    bcmolt_pon_interface_cfg interface_obj;

    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)pon_id};
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([pon_intf_get_res, &interface_obj] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_pon_interface_cfg* pon_cfg = (bcmolt_pon_interface_cfg*)cfg;
                                                                   pon_cfg->data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
                                                                   memcpy(&interface_obj, pon_cfg, sizeof(bcmolt_pon_interface_cfg));
                                                                   return pon_intf_get_res;
                                                               }
    ));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DisablePonIf Failure case - bcmolt_oper_submit returns BCM_ERR_INTERNAL
TEST_F(TestDisablePonIf, DisablePonIf_OperSubmitErrInternal) {
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;

    bcmos_errno pon_intf_get_res = BCM_ERR_OK;
    bcmolt_pon_interface_cfg interface_obj;

    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)pon_id};
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([pon_intf_get_res, &interface_obj] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_pon_interface_cfg* pon_cfg = (bcmolt_pon_interface_cfg*)cfg;
                                                                   pon_cfg->data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
                                                                   memcpy(&interface_obj, pon_cfg, sizeof(bcmolt_pon_interface_cfg));
                                                                   return pon_intf_get_res;
                                                               }
    ));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - DisablePonIf Failure case - bcmolt_oper_submit returns BCM_ERR_INTERNAL
TEST_F(TestDisablePonIf, DisablePonIf_CfgSetErrInternal) {
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;
    bcmos_errno bal_cfg_set_res= BCM_ERR_INTERNAL;

    bcmos_errno pon_intf_get_res = BCM_ERR_OK;
    bcmolt_pon_interface_cfg interface_obj;

    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)pon_id};
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([pon_intf_get_res, &interface_obj] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_pon_interface_cfg* pon_cfg = (bcmolt_pon_interface_cfg*)cfg;
                                                                   pon_cfg->data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
                                                                   memcpy(&interface_obj, pon_cfg, sizeof(bcmolt_pon_interface_cfg));
                                                                   return pon_intf_get_res;
                                                               }
    ));


    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(bal_cfg_set_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - DisablePonIf success case - PON port in BCMOLT_INTERFACE_STATE_INACTIVE state
TEST_F(TestDisablePonIf, DisablePonIfSuccess_PonIntfInactive) {
    bcmos_errno olt_oper_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_set_res = BCM_ERR_OK;
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;

    bcmos_errno pon_intf_get_res = BCM_ERR_OK;
    bcmolt_pon_interface_cfg interface_obj;

    bcmolt_pon_interface_key intf_key = {.pon_ni = (bcmolt_interface)pon_id};
    BCMOLT_CFG_INIT(&interface_obj, pon_interface, intf_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([pon_intf_get_res, &interface_obj] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_pon_interface_cfg* pon_cfg = (bcmolt_pon_interface_cfg*)cfg;
                                                                   pon_cfg->data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
                                                                   memcpy(&interface_obj, pon_cfg, sizeof(bcmolt_pon_interface_cfg));
                                                                   return pon_intf_get_res;
                                                               }
    ));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing ActivateOnu functionality
////////////////////////////////////////////////////////////////////////////

class TestActivateOnu : public Test {
    protected:
        uint32_t pon_id = 0;
        uint32_t onu_id = 1;
        std::string vendor_id = "TWSH";
        std::string vendor_specific = "80808080";
        uint32_t pir = 1000000;
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - ActivateOnu success case - ONU in BCMOLT_ONU_STATE_NOT_CONFIGURED state
TEST_F(TestActivateOnu, ActivateOnuSuccessOnuNotConfigured) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_set_res = BCM_ERR_OK;
    bcmos_errno onu_oper_submit_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(onu_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_submit_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - ActivateOnu success case - ONU already in BCMOLT_ONU_STATE_ACTIVE state
TEST_F(TestActivateOnu, ActivateOnuSuccessOnuAlreadyActive) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - ActivateOnu success case - ONU in BCMOLT_ONU_STATE_INACTIVE state
TEST_F(TestActivateOnu, ActivateOnuSuccessOnuInactive) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_submit_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_submit_res));


    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - ActivateOnu failure case - ONU in invalid state (for this ex: BCMOLT_ONU_STATE_LOW_POWER_DOZE)
TEST_F(TestActivateOnu, ActivateOnuFailOnuInvalidState) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_LOW_POWER_DOZE; // some invalid state which we dont recognize or process
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 5 - ActivateOnu failure case - cfg_get failure
TEST_F(TestActivateOnu, ActivateOnuFailCfgGetFail) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_INTERNAL;// return cfg_get failure

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 6 - ActivateOnu failure case - oper_submit failure
TEST_F(TestActivateOnu, ActivateOnuFailOperSubmitFail) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_set_res = BCM_ERR_OK;
    bcmos_errno onu_oper_submit_res = BCM_ERR_INTERNAL; // return oper_submit failure

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_NOT_CONFIGURED;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(onu_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_submit_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir, true);
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing DeactivateOnu functionality
////////////////////////////////////////////////////////////////////////////

class TestDeactivateOnu : public Test {
    protected:
        uint32_t pon_id = 0;
        uint32_t onu_id = 1;
        std::string vendor_id = "TWSH";
        std::string vendor_specific = "80808080";
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - DeactivateOnu success case
TEST_F(TestDeactivateOnu, DeactivateOnuSuccess) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = DeactivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DeactivateOnu failure case
TEST_F(TestDeactivateOnu, DeactivateOnuFailure) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_INTERNAL;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = DeactivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing DeleteOnu functionality
////////////////////////////////////////////////////////////////////////////

class TestDeleteOnu : public Test {
    protected:
        uint32_t pon_id = 0;
        uint32_t onu_id = 1;
        std::string vendor_id = "TWSH";
        std::string vendor_specific = "80808080";
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
    public:
       static int PushOnuDeactCompltResult(bcmolt_result result, bcmolt_deactivation_fail_reason reason) {
                onu_deactivate_complete_result res;
                res.pon_intf_id = 0;
                res.onu_id = 1;
                res.result = result;
                res.reason = reason;
                // We need to wait for some time to allow the Onu Deactivation Reqeuest to be triggered
                // before we push the result.
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                bcmos_fastlock_lock(&onu_deactivate_wait_lock);
                onu_deact_compltd_key k(0, 1);
                std::map<onu_deact_compltd_key,  Queue<onu_deactivate_complete_result> *>::iterator it = onu_deact_compltd_map.find(k);
                if (it == onu_deact_compltd_map.end()) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "onu deact key not found for pon_intf=%d, onu_id=%d\n", 0, 1);
                } else {
                    it->second->push(res);
                    OPENOLT_LOG(INFO, openolt_log_id, "Pushed ONU deact completed result\n");
                }
                bcmos_fastlock_unlock(&onu_deactivate_wait_lock, 0);
                return 0;
       }
};

// Test 1 - DeleteOnu success case
TEST_F(TestDeleteOnu, DeleteOnuSuccess) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_clear_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(onu_cfg_clear_res));

    future<int> push_onu_deact_complt_res = \
             async(launch::async,TestDeleteOnu::PushOnuDeactCompltResult, BCMOLT_RESULT_SUCCESS, BCMOLT_DEACTIVATION_FAIL_REASON_NONE);
    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());
    Status status = future_res.get();
    int res = push_onu_deact_complt_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DeleteOnu failure case - BAL Clear ONU fails
TEST_F(TestDeleteOnu, DeleteOnuFailureClearOnuFail) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_clear_res = BCM_ERR_INTERNAL;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(onu_cfg_clear_res));

    future<int> push_onu_deact_complt_res = \
                async(launch::async,TestDeleteOnu::PushOnuDeactCompltResult, BCMOLT_RESULT_SUCCESS, BCMOLT_DEACTIVATION_FAIL_REASON_NONE);
    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());

    Status status = future_res.get();
    int res = push_onu_deact_complt_res.get();
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - DeleteOnu failure case - onu deactivation fails
TEST_F(TestDeleteOnu, DeleteOnuFailureDeactivationFail) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    future<int> push_onu_deact_complt_res = \
                async(launch::async,TestDeleteOnu::PushOnuDeactCompltResult, BCMOLT_RESULT_FAIL, BCMOLT_DEACTIVATION_FAIL_REASON_FAIL);
    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());

    Status status = future_res.get();
    int res = push_onu_deact_complt_res.get();
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - DeleteOnu failure case - onu deactivation timesout
TEST_F(TestDeleteOnu, DeleteOnuFailureDeactivationTimeout) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());

    Status status = future_res.get();
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 5 - DeleteOnu success case - Onu is Inactive so won't wait for onu deactivation response
TEST_F(TestDeleteOnu, DeleteOnuSuccessDontwaitforDeactivationResp) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_clear_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(onu_cfg_clear_res));

    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 6 - DeleteOnu failure case - Failed to fetch Onu status
TEST_F(TestDeleteOnu, DeleteOnuFailureFetchOnuStatusFailed) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_INTERNAL;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_INACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    future<Status> future_res = async(launch::async, DeleteOnu_, pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());

    Status status = future_res.get();
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing OmciMsgOut functionality
////////////////////////////////////////////////////////////////////////////

class TestOmciMsgOut : public Test {
    protected:
        uint32_t pon_id = 0;
        uint32_t onu_id = 1;
        std::string pkt = "omci-pkt";
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - OmciMsgOut success case
TEST_F(TestOmciMsgOut, OmciMsgOutSuccess) {
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = OmciMsgOut_(pon_id, onu_id, pkt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 1 - OmciMsgOut failure case
TEST_F(TestOmciMsgOut, OmciMsgOutFailure) {
    bcmos_errno onu_oper_sub_res = BCM_ERR_INTERNAL;

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = OmciMsgOut_(pon_id, onu_id, pkt);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing FlowAdd functionality
////////////////////////////////////////////////////////////////////////////

class TestFlowAdd : public Test {
    protected:
        int32_t access_intf_id = 0;
        int32_t onu_id = 1;
        int32_t uni_id = 0;
        uint32_t port_no = 16;
        uint32_t flow_id = 1;
        std::string flow_type = "upstream";
        int32_t alloc_id = 1024;
        int32_t network_intf_id = 0;
        int32_t gemport_id = 1024;
        int32_t priority_value = 0;
        uint64_t cookie = 0;
        int32_t group_id = -1;
        uint32_t tech_profile_id = 64;
        bool enable_encryption = true;

        NiceMock<BalMocker> balMock;
        openolt::Flow* flow;
        openolt::Classifier* classifier;
        openolt::Action* action;
        openolt::ActionCmd* cmd;

        bcmolt_flow_key flow_key;
        bcmolt_flow_cfg flow_cfg;

        tech_profile::TrafficQueues* traffic_queues;
        tech_profile::TrafficQueue* traffic_queue_1;
        tech_profile::TrafficQueue* traffic_queue_2;
        tech_profile::DiscardConfig* discard_config_1;
        tech_profile::DiscardConfig* discard_config_2;
        tech_profile::TailDropDiscardConfig* tail_drop_discard_config_1;
        tech_profile::TailDropDiscardConfig* tail_drop_discard_config_2;


        virtual void SetUp() {
            classifier = new openolt::Classifier;
            action = new openolt::Action;
            cmd = new openolt::ActionCmd;

            classifier->set_o_tpid(0);
            classifier->set_o_vid(7);
            classifier->set_i_tpid(0);
            classifier->set_i_vid(0);
            classifier->set_o_pbits(0);
            classifier->set_i_pbits(0);
            classifier->set_eth_type(0);
            classifier->set_ip_proto(0);
            classifier->set_src_port(0);
            classifier->set_dst_port(0);
            classifier->set_pkt_tag_type("single_tag");

            action->set_o_vid(12);
            action->set_o_pbits(0);
            action->set_o_tpid(0);
            action->set_i_vid(0);
            action->set_i_pbits(0);
            action->set_i_tpid(0);

            cmd->set_add_outer_tag(true);
            cmd->set_remove_outer_tag(false);
            cmd->set_trap_to_host(false);
            action->set_allocated_cmd(cmd);

            flow_key.flow_id = 1;
            flow_key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
            BCMOLT_CFG_INIT(&flow_cfg, flow, flow_key);
            flow_cfg.data.onu_id=1;
            flow_cfg.key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
            flow_cfg.data.svc_port_id=1024;
            flow_cfg.data.priority=0;
            flow_cfg.data.cookie=0;
            flow_cfg.data.ingress_intf.intf_type=BCMOLT_FLOW_INTERFACE_TYPE_PON;
            flow_cfg.data.egress_intf.intf_type=BCMOLT_FLOW_INTERFACE_TYPE_NNI;
            flow_cfg.data.ingress_intf.intf_id=0;
            flow_cfg.data.egress_intf.intf_id=0;
            flow_cfg.data.classifier.o_vid=7;
            flow_cfg.data.classifier.o_pbits=0;
            flow_cfg.data.classifier.i_vid=0;
            flow_cfg.data.classifier.i_pbits=0;
            flow_cfg.data.classifier.ether_type=0;
            flow_cfg.data.classifier.ip_proto=0;
            flow_cfg.data.classifier.src_port=0;
            flow_cfg.data.classifier.dst_port=0;
            flow_cfg.data.classifier.pkt_tag_type=BCMOLT_PKT_TAG_TYPE_SINGLE_TAG;
            flow_cfg.data.egress_qos.type=BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
            flow_cfg.data.egress_qos.u.fixed_queue.queue_id=0;
            flow_cfg.data.egress_qos.tm_sched.id=1020;
            flow_cfg.data.action.cmds_bitmask=BCMOLT_ACTION_CMD_ID_ADD_OUTER_TAG;
            flow_cfg.data.action.o_vid=12;
            flow_cfg.data.action.o_pbits=0;
            flow_cfg.data.action.i_vid=0;
            flow_cfg.data.action.i_pbits=0;
            flow_cfg.data.state=BCMOLT_FLOW_STATE_ENABLE;

            traffic_queues = new tech_profile::TrafficQueues;
            traffic_queues->set_intf_id(0);
            traffic_queues->set_onu_id(2);
            traffic_queue_1 = traffic_queues->add_traffic_queues();
            traffic_queue_1->set_gemport_id(1024);
            traffic_queue_1->set_pbit_map("0b00000101");
            traffic_queue_1->set_aes_encryption(true);
            traffic_queue_1->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
            traffic_queue_1->set_priority(0);
            traffic_queue_1->set_weight(0);
            traffic_queue_1->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            discard_config_1 = new tech_profile::DiscardConfig;
            discard_config_1->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            tail_drop_discard_config_1 = new tech_profile::TailDropDiscardConfig;
            tail_drop_discard_config_1->set_queue_size(8);
            discard_config_1->set_allocated_tail_drop_discard_config(tail_drop_discard_config_1);
            traffic_queue_1->set_allocated_discard_config(discard_config_1);

            traffic_queues->set_uni_id(0);
            traffic_queues->set_port_no(16);

            traffic_queue_2 = traffic_queues->add_traffic_queues();
            traffic_queue_2->set_gemport_id(1025);
            traffic_queue_2->set_pbit_map("0b00001010");
            traffic_queue_2->set_aes_encryption(true);
            traffic_queue_2->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
            traffic_queue_2->set_priority(1);
            traffic_queue_2->set_weight(0);
            traffic_queue_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            discard_config_2 = new tech_profile::DiscardConfig;
            discard_config_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            tail_drop_discard_config_2 = new tech_profile::TailDropDiscardConfig;
            tail_drop_discard_config_2->set_queue_size(8);
            discard_config_2->set_allocated_tail_drop_discard_config(tail_drop_discard_config_2);
            traffic_queue_2->set_allocated_discard_config(discard_config_2);
        }

        virtual void TearDown() {
        }
};

// Test 1 - FlowAdd - success case(HSIA-upstream FixedQueue)
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueUpstreamSuccess) {
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type,
        alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

#ifdef FLOW_CHECKER
// Test 2 - FlowAdd - Duplicate Flow case
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueUpstreamDuplicate) {
    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}
#endif

// Test 3 - FlowAdd - Failure case(bcmolt_cfg_set returns error)
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueUpstreamFailure) {
    gemport_id = 1025;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - FlowAdd - Failure case(Invalid flow direction)
TEST_F(TestFlowAdd, FlowAddFailureInvalidFlowDirection) {
    flow_type = "bidirectional";

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - FlowAdd - Failure case(Invalid network setting)
TEST_F(TestFlowAdd, FlowAddFailureInvalidNWCfg) {
    network_intf_id = -1;

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 6 - FlowAdd - Success case(Single tag & EAP Ether type)
TEST_F(TestFlowAdd, FlowAddEapEtherTypeSuccess) {
    flow_id = 2;

    classifier->set_eth_type(34958);
    action = new openolt::Action;
    cmd = new openolt::ActionCmd;
    cmd->set_trap_to_host(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id,
        network_intf_id, gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 7 - FlowAdd - Success case(Single tag & DHCP flow)
TEST_F(TestFlowAdd, FlowAddDhcpSuccess) {
    flow_id = 3;
    gemport_id = 1025;

    classifier->set_ip_proto(17);
    classifier->set_src_port(68);
    classifier->set_dst_port(67);
    action = new openolt::Action;
    cmd = new openolt::ActionCmd;
    cmd->set_trap_to_host(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 8 - FlowAdd - success case(HSIA-downstream FixedQueue)
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueDownstreamSuccess) {
    flow_id = 4;
    flow_type = "downstream";

    classifier->set_o_vid(12);
    classifier->set_i_vid(7);
    classifier->set_pkt_tag_type("double_tag");
    action = new openolt::Action;
    cmd = new openolt::ActionCmd;
    action->set_o_vid(0);
    cmd->set_remove_outer_tag(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 9 - FlowAdd - success case(HSIA-upstream PriorityQueue)
TEST_F(TestFlowAdd, FlowAddHsiaPriorityQueueUpstreamSuccess) {
    onu_id = 2;
    flow_id = 5;
    alloc_id = 1025;

    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    CreateTrafficQueues_(traffic_queues);

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 10 - FlowAdd - success case(HSIA-downstream PriorityQueue)
TEST_F(TestFlowAdd, FlowAddHsiaPriorityQueueDownstreamSuccess) {
    onu_id = 2;
    flow_id = 6;
    flow_type = "downstream";
    alloc_id = 1025;

    classifier->set_o_vid(12);
    classifier->set_i_vid(7);
    classifier->set_pkt_tag_type("double_tag");
    action = new openolt::Action;
    cmd = new openolt::ActionCmd;
    action->set_o_vid(0);
    cmd->set_remove_outer_tag(true);
    action->set_allocated_cmd(cmd);

    traffic_queue_1->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_queue_2->set_direction(tech_profile::Direction::DOWNSTREAM);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
    CreateTrafficQueues_(traffic_queues);

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 11 - FlowAdd - success case (Downstream-Encrypted GEM)
TEST_F(TestFlowAdd, FlowAddDownstreamEncryptedGemSuccess) {
    onu_id = 2;
    flow_id = 7;
    flow_type = "downstream";
    alloc_id = 1025;
    enable_encryption = true;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id, enable_encryption);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 12 - FlowAdd - success case (Downstream-Unencrypted GEM - prints warning that encryption will not applied)
TEST_F(TestFlowAdd, FlowAddDownstreamUnencryptedGemWarning) {
    onu_id = 2;
    flow_id = 8;
    flow_type = "downstream";
    alloc_id = 1025;
    enable_encryption = false;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id, enable_encryption);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 13 - FlowAdd - success case (Upstream-Encrypted GEM - prints warning that encryption will not applied)
TEST_F(TestFlowAdd, FlowAddUpstreamEncryptedGemWarning) {
    onu_id = 2;
    flow_id = 9;
    flow_type = "upstream";
    alloc_id = 1025;
    enable_encryption = true;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id, enable_encryption);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 14 - FlowAdd - success case (Multicast-Encrypted GEM - prints warning that encryption will not applied)
TEST_F(TestFlowAdd, FlowAddMulticastEncryptedGemWarning) {
    onu_id = 2;
    flow_id = 10;
    flow_type = "multicast";
    alloc_id = 1025;
    enable_encryption = true;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id,
        gemport_id, *classifier, *action, priority_value, cookie, group_id, tech_profile_id, enable_encryption);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}
////////////////////////////////////////////////////////////////////////////
// For testing OnuPacketOut functionality
////////////////////////////////////////////////////////////////////////////

class TestOnuPacketOut : public Test {
    protected:
        uint32_t pon_id = 0;
        uint32_t onu_id = 1;
        std::string pkt = "omci-pkt";
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - OnuPacketOut success case
TEST_F(TestOnuPacketOut, OnuPacketOutSuccess) {
    uint32_t port_no = 16;
    uint32_t gemport_id = 1024;

    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = OnuPacketOut_(pon_id, onu_id, port_no, gemport_id, pkt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - OnuPacketOut Port number as 0 case
TEST_F(TestOnuPacketOut, OnuPacketOutPortNo0) {
    uint32_t port_no = 0;
    uint32_t gemport_id = 1024;

    Status status = OnuPacketOut_(pon_id, onu_id, port_no, gemport_id, pkt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - OnuPacketOut success, Finding Flow ID from port no and Gem from Flow ID case
TEST_F(TestOnuPacketOut, OnuPacketOutFindGemFromFlowSuccess) {
    uint32_t port_no = 16;
    uint32_t gemport_id = 0;

    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = OnuPacketOut_(pon_id, onu_id, port_no, gemport_id, pkt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - OnuPacketOut success, Failure in finding Gem port case
TEST_F(TestOnuPacketOut, OnuPacketOutFindGemFromFlowFailure) {
    uint32_t port_no = 64;
    uint32_t gemport_id = 0;

    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));

    Status status = OnuPacketOut_(pon_id, onu_id, port_no, gemport_id, pkt);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing FlowRemove functionality
////////////////////////////////////////////////////////////////////////////

class TestFlowRemove : public Test {
    protected:
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }
};

// Test 1 - FlowRemove - Failure case
TEST_F(TestFlowRemove, FlowRemoveFailure) {
    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = FlowRemove_(1, "upstream");
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 2 - FlowRemove - success case
TEST_F(TestFlowRemove, FlowRemoveSuccess) {
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = FlowRemove_(1, "upstream");
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing UplinkPacketOut functionality
////////////////////////////////////////////////////////////////////////////

class TestUplinkPacketOut : public Test {
    protected:
        uint32_t pon_id = 0;
        std::string pkt = "omci-pkt";
        NiceMock<BalMocker> balMock;

        bcmolt_flow_key flow_key;
        bcmolt_flow_cfg flow_cfg;

        virtual void SetUp() {
            flow_key.flow_id = 1;
            flow_key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
            BCMOLT_CFG_INIT(&flow_cfg, flow, flow_key);
            flow_cfg.data.onu_id=1;
            flow_cfg.key.flow_type = BCMOLT_FLOW_TYPE_UPSTREAM;
            flow_cfg.data.svc_port_id=1024;
            flow_cfg.data.priority=0;
            flow_cfg.data.cookie=0;
            flow_cfg.data.ingress_intf.intf_type=BCMOLT_FLOW_INTERFACE_TYPE_PON;
            flow_cfg.data.egress_intf.intf_type=BCMOLT_FLOW_INTERFACE_TYPE_NNI;
            flow_cfg.data.ingress_intf.intf_id=0;
            flow_cfg.data.egress_intf.intf_id=0;
            flow_cfg.data.classifier.o_vid=7;
            flow_cfg.data.classifier.o_pbits=0;
            flow_cfg.data.classifier.i_vid=0;
            flow_cfg.data.classifier.i_pbits=0;
            flow_cfg.data.classifier.ether_type=0;
            flow_cfg.data.classifier.ip_proto=0;
            flow_cfg.data.classifier.src_port=0;
            flow_cfg.data.classifier.dst_port=0;
            flow_cfg.data.classifier.pkt_tag_type=BCMOLT_PKT_TAG_TYPE_SINGLE_TAG;
            flow_cfg.data.egress_qos.type=BCMOLT_EGRESS_QOS_TYPE_FIXED_QUEUE;
            flow_cfg.data.egress_qos.u.fixed_queue.queue_id=0;
            flow_cfg.data.egress_qos.tm_sched.id=1020;
            flow_cfg.data.action.cmds_bitmask=BCMOLT_ACTION_CMD_ID_ADD_OUTER_TAG;
            flow_cfg.data.action.o_vid=12;
            flow_cfg.data.action.o_pbits=0;
            flow_cfg.data.action.i_vid=0;
            flow_cfg.data.action.i_pbits=0;
            flow_cfg.data.state=BCMOLT_FLOW_STATE_ENABLE;
        }

        virtual void TearDown() {
        }
};

// Test 1 - UplinkPacketOut success case
TEST_F(TestUplinkPacketOut, UplinkPacketOutSuccess) {
    bcmos_errno send_eth_oper_sub_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(send_eth_oper_sub_res));
    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));

    Status status = UplinkPacketOut_(pon_id, pkt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - UplinkPacketOut Failure case
TEST_F(TestUplinkPacketOut, UplinkPacketOutFailure) {
    bcmos_errno send_eth_oper_sub_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(send_eth_oper_sub_res));
    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));

    Status status = UplinkPacketOut_(pon_id, pkt);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - UplinkPacketOut No matching flow id found for Uplink Packetout case
TEST_F(TestUplinkPacketOut, UplinkPacketOutFailureNoFlowIdFound) {
    flow_cfg.key.flow_type = BCMOLT_FLOW_TYPE_DOWNSTREAM;

    FlowRemove_(2, "upstream");
    FlowRemove_(3, "upstream");
    FlowRemove_(4, "downstream");
    FlowRemove_(5, "upstream");
    FlowRemove_(6, "downstream");
    FlowRemove_(7, "downstream");
    FlowRemove_(8, "downstream");
    FlowRemove_(9, "upstream");
    FlowRemove_(10, "multicast");

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));

    Status status = UplinkPacketOut_(pon_id, pkt);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing CreateTrafficSchedulers functionality
////////////////////////////////////////////////////////////////////////////

class TestCreateTrafficSchedulers : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        tech_profile::TrafficSchedulers* traffic_scheds;
        tech_profile::TrafficScheduler* traffic_sched;
        tech_profile::SchedulerConfig* scheduler;
        tech_profile::TrafficShapingInfo* traffic_shaping_info;

        virtual void SetUp() {
            traffic_scheds = new tech_profile::TrafficSchedulers;
            traffic_scheds->set_intf_id(0);
            traffic_scheds->set_onu_id(1);
            traffic_scheds->set_uni_id(0);
            traffic_scheds->set_port_no(16);
            traffic_sched = traffic_scheds->add_traffic_scheds();
            traffic_sched->set_alloc_id(1024);
            scheduler = new tech_profile::SchedulerConfig;
            scheduler->set_priority(0);
            scheduler->set_weight(0);
            scheduler->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
            traffic_shaping_info = new tech_profile::TrafficShapingInfo;
            traffic_shaping_info->set_cbs(60536);
            traffic_shaping_info->set_pbs(65536);
            traffic_shaping_info->set_gir(10000);
        }

        virtual void TearDown() {
        }

    public:
        static int PushAllocCfgResult(AllocObjectState state, AllocCfgStatus status) {
            alloc_cfg_compltd_key k(0, 1024);
            alloc_cfg_complete_result res;
            res.pon_intf_id = 0;
            res.alloc_id = 1024;
            res.state = state;
            res.status = status;

            // We need to wait for some time to allow the Alloc Cfg Request to be triggered
            // before we push the result.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            bcmos_fastlock_lock(&alloc_cfg_wait_lock);
            std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *>::iterator it = alloc_cfg_compltd_map.find(k);
            if (it == alloc_cfg_compltd_map.end()) {
                OPENOLT_LOG(ERROR, openolt_log_id, "alloc config key not found for alloc_id = %u, pon_intf = %u\n", 1024, 0);
            } else {
                it->second->push(res);
                OPENOLT_LOG(INFO, openolt_log_id, "Pushed mocked alloc cfg result\n");
            }
            bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
            return 0;
        }
};

// Test 1 - CreateTrafficSchedulers-Upstream success case
TEST_F(TestCreateTrafficSchedulers, CreateTrafficSchedulersUpstreamSuccess) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
                async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - CreateTrafficSchedulers-Upstream failure case(timeout waiting for alloc cfg indication)
TEST_F(TestCreateTrafficSchedulers, UpstreamAllocCfgTimeout) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - CreateTrafficSchedulers-Upstream failure case(error processing alloc cfg request)
TEST_F(TestCreateTrafficSchedulers, UpstreamErrorProcessingAllocCfg) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
                async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_FAIL);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();

    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - CreateTrafficSchedulers-Upstream failure case(alloc object not in active state)
TEST_F(TestCreateTrafficSchedulers, UpstreamAllocObjNotinActiveState) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
                async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_INACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - CreateTrafficSchedulers-Upstream Failure case
TEST_F(TestCreateTrafficSchedulers, CreateTrafficSchedulersUpstreamFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 6 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_BestEffort-Max BW set to 0) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_BestEffortMaxBWZeroFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(0);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 7 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_BestEffort-Max BW < Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_BestEffortMaxBWLtGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 8 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_BestEffort-Max BW = Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_BestEffortMaxBWEqGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(64000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 9 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_NA-Max BW set to 0) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NAMaxBWZeroFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_NA);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(0);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 10 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_NA-Guaranteed BW set to 0) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NAGuaranteedBwZeroFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_NA);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(0);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 11 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_NA-Max BW < Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NAMaxBWLtGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_NA);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 12 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_NA-Max BW = Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NAMaxBWEqGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_NA);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(64000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 13 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_None-Max BW set to 0) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWZeroFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(0);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 14 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_None-Guaranteed BW set to 0) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneGuaranteedBwZeroFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(0);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 15 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_None-Max BW > Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWGtGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 16 - CreateTrafficSchedulers-Upstream Failure (AdditionalBW_None-Max BW < Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWLtGuaranteedBwFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 17 - CreateTrafficSchedulers-Upstream Failure (Invalid AdditionalBW value) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_InvalidValueFailure) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(9);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(0);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 18 - CreateTrafficSchedulers-Upstream Success (AdditionalBW_None-Max BW = Fixed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWEqFixedBwSuccess) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_gir(64000);
    traffic_shaping_info->set_cir(0);
    traffic_shaping_info->set_pir(64000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 19 - CreateTrafficSchedulers-Downstream success case
TEST_F(TestCreateTrafficSchedulers, CreateTrafficSchedulersDownstreamSuccess) {
    scheduler->set_direction(tech_profile::Direction::DOWNSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 20 - CreateTrafficSchedulers-Downstream Failure case
TEST_F(TestCreateTrafficSchedulers, CreateTrafficSchedulersDownstreamFailure) {
    scheduler->set_direction(tech_profile::Direction::DOWNSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 21 - CreateTrafficSchedulers-Invalid direction Failure case
TEST_F(TestCreateTrafficSchedulers, CreateTrafficSchedulersInvalidDirectionFailure) {
    scheduler->set_direction(tech_profile::Direction::BIDIRECTIONAL);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
    traffic_sched->set_direction(tech_profile::Direction::BIDIRECTIONAL);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing RemoveTrafficSchedulers functionality
////////////////////////////////////////////////////////////////////////////

class TestRemoveTrafficSchedulers : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        tech_profile::TrafficSchedulers* traffic_scheds;
        tech_profile::TrafficScheduler* traffic_sched;
        tech_profile::SchedulerConfig* scheduler;
        tech_profile::TrafficShapingInfo* traffic_shaping_info;
        alloc_cfg_complete_result res;
        uint32_t pon_id = 0;

        virtual void SetUp() {
            traffic_scheds = new tech_profile::TrafficSchedulers;
            traffic_scheds->set_intf_id(0);
            traffic_scheds->set_onu_id(1);
            traffic_scheds->set_uni_id(0);
            traffic_scheds->set_port_no(16);
            traffic_sched = traffic_scheds->add_traffic_scheds();
            traffic_sched->set_alloc_id(1025);
            scheduler = new tech_profile::SchedulerConfig;
            scheduler->set_priority(0);
            scheduler->set_weight(0);
            scheduler->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
            scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_BestEffort);
            traffic_shaping_info = new tech_profile::TrafficShapingInfo;
            traffic_shaping_info->set_cir(64000);
            traffic_shaping_info->set_pir(128000);
            traffic_shaping_info->set_cbs(60536);
            traffic_shaping_info->set_pbs(65536);
            traffic_shaping_info->set_gir(10000);
            traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);
            bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
            ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));
        }

        virtual void TearDown() {
        }

    public:
        static int PushAllocCfgResult(AllocObjectState state, AllocCfgStatus status) {
            alloc_cfg_compltd_key k(0, 1025);
            alloc_cfg_complete_result res;
            res.pon_intf_id = 0;
            res.alloc_id = 1025;
            res.state = state;
            res.status = status;

            // We need to wait for some time to allow the Alloc Cfg Request to be triggered
            // before we push the result.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            bcmos_fastlock_lock(&alloc_cfg_wait_lock);
            std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *>::iterator it = alloc_cfg_compltd_map.find(k);
            if (it == alloc_cfg_compltd_map.end()) {
                OPENOLT_LOG(ERROR, openolt_log_id, "alloc config key not found for alloc_id = %u, pon_intf = %u\n", 1025, 0);
            } else {
                it->second->push(res);
                OPENOLT_LOG(INFO, openolt_log_id, "Pushed mocked alloc cfg result\n");
            }
            bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
            return 0;
        }
};

// Test 1 - RemoveTrafficSchedulers-Upstream success case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersUpstreamSuccess) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
    async(launch::async, TestRemoveTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_NOT_CONFIGURED, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - RemoveTrafficSchedulers-Upstream success case(alloc object is not reset)
TEST_F(TestRemoveTrafficSchedulers, UpstreamAllocObjNotReset) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_ACTIVE_WORKING;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);
    future<int> push_alloc_cfg_complt = \
    async(launch::async, TestRemoveTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_INACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    int res = push_alloc_cfg_complt.get();
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - RemoveTrafficSchedulers-Upstream success case(PON disable case - Don't wait for alloc object delete response)
TEST_F(TestRemoveTrafficSchedulers, UpstreamAllocObjPonDisable) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - RemoveTrafficSchedulers-Upstream success case(Get PON State failure case)
TEST_F(TestRemoveTrafficSchedulers, UpstreamAllocObjGetPonStateFailure) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    bcmolt_pon_interface_key pon_key;
    bcmolt_pon_interface_cfg pon_cfg;
    pon_key.pon_ni = pon_id;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, pon_key);
    pon_cfg.data.state = BCMOLT_INTERFACE_STATE_INACTIVE;
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_INTERNAL;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - RemoveTrafficSchedulers-Upstream Failure case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersUpstreamFailure) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 6 - RemoveTrafficSchedulers-Downstream Failure case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersDownstreamFailure) {
    //Create Scheduler
    scheduler->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    CreateTrafficSchedulers_(traffic_scheds);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 7 - RemoveTrafficSchedulers-Downstream success case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersDownstreamSuccess) {
    //Create Scheduler
    scheduler->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    CreateTrafficSchedulers_(traffic_scheds);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 8 - RemoveTrafficSchedulers-Downstream Scheduler not present case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersDownstreamSchedNotpresent) {
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);

    Status status = RemoveTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing CreateTrafficQueues functionality
////////////////////////////////////////////////////////////////////////////

class TestCreateTrafficQueues : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        tech_profile::TrafficQueues* traffic_queues;
        tech_profile::TrafficQueue* traffic_queue_1;
        tech_profile::TrafficQueue* traffic_queue_2;
        tech_profile::DiscardConfig* discard_config_1;
        tech_profile::DiscardConfig* discard_config_2;
        tech_profile::TailDropDiscardConfig* tail_drop_discard_config_1;
        tech_profile::TailDropDiscardConfig* tail_drop_discard_config_2;

        virtual void SetUp() {
            traffic_queues = new tech_profile::TrafficQueues;
            traffic_queues->set_intf_id(0);
            traffic_queues->set_onu_id(1);
            traffic_queue_1 = traffic_queues->add_traffic_queues();
            traffic_queue_1->set_gemport_id(1024);
            traffic_queue_1->set_pbit_map("0b00000101");
            traffic_queue_1->set_aes_encryption(true);
            traffic_queue_1->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
            traffic_queue_1->set_priority(0);
            traffic_queue_1->set_weight(0);
            traffic_queue_1->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            discard_config_1 = new tech_profile::DiscardConfig;
            discard_config_1->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
            tail_drop_discard_config_1 = new tech_profile::TailDropDiscardConfig;
            tail_drop_discard_config_1->set_queue_size(8);
            discard_config_1->set_allocated_tail_drop_discard_config(tail_drop_discard_config_1);
            traffic_queue_1->set_allocated_discard_config(discard_config_1);
        }

        virtual void TearDown() {
        }
};

// Test 1 - CreateTrafficQueues-Upstream/Downstream FIXED_QUEUE success case
TEST_F(TestCreateTrafficQueues, CreateUpstreamDownstreamFixedQueueSuccess) {
    Status status;
    traffic_queues->set_uni_id(0);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );

    traffic_queue_1->set_direction(tech_profile::Direction::DOWNSTREAM);
    status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - CreateTrafficQueues-Upstream PRIORITY_TO_QUEUE success case
TEST_F(TestCreateTrafficQueues, CreateUpstreamPriorityQueueSuccess) {
    traffic_queues->set_uni_id(1);
    traffic_queues->set_port_no(32);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_pbit_map("0b00001010");
    traffic_queue_2->set_aes_encryption(true);
    traffic_queue_2->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
    traffic_queue_2->set_priority(1);
    traffic_queue_2->set_weight(0);
    traffic_queue_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    discard_config_2 = new tech_profile::DiscardConfig;
    discard_config_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    tail_drop_discard_config_2 = new tech_profile::TailDropDiscardConfig;
    tail_drop_discard_config_2->set_queue_size(8);
    discard_config_2->set_allocated_tail_drop_discard_config(tail_drop_discard_config_2);
    traffic_queue_2->set_allocated_discard_config(discard_config_2);
    traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - CreateTrafficQueues-Upstream create tm queue mapping profile failure case
TEST_F(TestCreateTrafficQueues, CreateUpstreamPriorityQueueTMQMPCreationFailure) {
    traffic_queues->set_uni_id(2);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_pbit_map("0b10001010");
    traffic_queue_2->set_aes_encryption(true);
    traffic_queue_2->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
    traffic_queue_2->set_priority(1);
    traffic_queue_2->set_weight(0);
    traffic_queue_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    discard_config_2 = new tech_profile::DiscardConfig;
    discard_config_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    tail_drop_discard_config_2 = new tech_profile::TailDropDiscardConfig;
    tail_drop_discard_config_2->set_queue_size(8);
    discard_config_2->set_allocated_tail_drop_discard_config(tail_drop_discard_config_2);
    traffic_queue_2->set_allocated_discard_config(discard_config_2);
    traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - CreateTrafficQueues-Upstream PRIORITY_TO_QUEUE TM QMP already present case
TEST_F(TestCreateTrafficQueues, CreateUpstreamPriorityQueueTMQMPAlreadyPresent) {
    traffic_queues->set_uni_id(3);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_pbit_map("0b00001010");
    traffic_queue_2->set_aes_encryption(true);
    traffic_queue_2->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
    traffic_queue_2->set_priority(1);
    traffic_queue_2->set_weight(0);
    traffic_queue_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    discard_config_2 = new tech_profile::DiscardConfig;
    discard_config_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
    tail_drop_discard_config_2 = new tech_profile::TailDropDiscardConfig;
    tail_drop_discard_config_2->set_queue_size(8);
    discard_config_2->set_allocated_tail_drop_discard_config(tail_drop_discard_config_2);
    traffic_queue_2->set_allocated_discard_config(discard_config_2);
    traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 5 - CreateTrafficQueues-Upstream PRIORITY_TO_QUEUE TM QMP Max count reached case
TEST_F(TestCreateTrafficQueues, CreateUpstreamPriorityQueueReachedMaxTMQMPCount) {
    int uni_ids[17] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    int port_nos[17] = {16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256, 272};
    std::string pbit_maps[17] = {"0b00001010", "0b10001010", "0b00000001", "0b00000010", "0b00000100", "0b00001000", "0b00010000", "0b00100000", "0b01000000", "0b10000000", "0b10000001", "0b10000010", "0b10000100", "0b10001000", "0b10010000", "0b10100000", "0b11000000"};

    traffic_queue_2 = traffic_queues->add_traffic_queues();
    for(int i=0; i<sizeof(uni_ids)/sizeof(uni_ids[0]); i++) {
        traffic_queues->set_uni_id(uni_ids[i]);
        traffic_queues->set_port_no(port_nos[i]);
        traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

        traffic_queue_2->set_gemport_id(1025);
        traffic_queue_2->set_pbit_map(pbit_maps[i]);
        traffic_queue_2->set_aes_encryption(true);
        traffic_queue_2->set_sched_policy(tech_profile::SchedulingPolicy::StrictPriority);
        traffic_queue_2->set_priority(1);
        traffic_queue_2->set_weight(0);
        traffic_queue_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
        discard_config_2 = new tech_profile::DiscardConfig;
        discard_config_2->set_discard_policy(tech_profile::DiscardPolicy::TailDrop);
        tail_drop_discard_config_2 = new tech_profile::TailDropDiscardConfig;
        tail_drop_discard_config_2->set_queue_size(8);
        discard_config_2->set_allocated_tail_drop_discard_config(tail_drop_discard_config_2);
        traffic_queue_2->set_allocated_discard_config(discard_config_2);
        traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

        bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
        ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

        Status status = CreateTrafficQueues_(traffic_queues);
        if(i==16)
            ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
        else
            ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
    }
}

// Test 6 - CreateTrafficQueues-Upstream FIXED_QUEUE failure case
TEST_F(TestCreateTrafficQueues, CreateUpstreamFixedQueueFailure) {
    traffic_queues->set_uni_id(0);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = CreateTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing RemoveTrafficQueues functionality
////////////////////////////////////////////////////////////////////////////

class TestRemoveTrafficQueues : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        tech_profile::TrafficQueues* traffic_queues;
        tech_profile::TrafficQueue* traffic_queue_1;
        tech_profile::TrafficQueue* traffic_queue_2;

        virtual void SetUp() {
            traffic_queues = new tech_profile::TrafficQueues;
            traffic_queues->set_intf_id(0);
            traffic_queues->set_onu_id(1);
            traffic_queue_1 = traffic_queues->add_traffic_queues();
            traffic_queue_1->set_gemport_id(1024);
            traffic_queue_1->set_priority(0);
        }

        virtual void TearDown() {
        }
};

// Test 1 - RemoveTrafficQueues-Upstream/Downstream FIXED_QUEUE success case
TEST_F(TestRemoveTrafficQueues, RemoveUpstreamDownstreamFixedQueueSuccess) {
    Status status;
    traffic_queues->set_uni_id(0);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );

    traffic_queue_1->set_direction(tech_profile::Direction::DOWNSTREAM);
    status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - RemoveTrafficQueues-Downstream FIXED_QUEUE failure case
TEST_F(TestRemoveTrafficQueues, RemoveUpstreamDownstreamFixedQueueFailure) {
    Status status;
    traffic_queues->set_uni_id(0);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::DOWNSTREAM);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - RemoveTrafficQueues-Downstream_QUEUE not present case
TEST_F(TestRemoveTrafficQueues, RemoveDownstreamFixedQueueNotPresent) {
    //Remove scheduler so that is_tm_sched_id_present api call will return false
    tech_profile::TrafficSchedulers* traffic_scheds;
    tech_profile::TrafficScheduler* traffic_sched;
    traffic_scheds = new tech_profile::TrafficSchedulers;
    traffic_scheds->set_intf_id(0);
    traffic_scheds->set_onu_id(1);
    traffic_scheds->set_uni_id(0);
    traffic_scheds->set_port_no(16);
    traffic_sched = traffic_scheds->add_traffic_scheds();
    traffic_sched->set_alloc_id(1024);
    traffic_sched->set_direction(tech_profile::Direction::DOWNSTREAM);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));
    RemoveTrafficSchedulers_(traffic_scheds);

    traffic_queues->set_uni_id(0);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::DOWNSTREAM);

    Status status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

/* Test 4 - RemoveTrafficQueues-Upstream PRIORITY_TO_QUEUE, not removing TM QMP
as it is getting referred by some other queues case */
TEST_F(TestRemoveTrafficQueues, RemoveUpstreamPriorityQueueNotRemovingTMQMP) {
    traffic_queues->set_uni_id(3);
    traffic_queues->set_port_no(16);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_priority(1);
    traffic_queue_2->set_direction(tech_profile::Direction::UPSTREAM);

    Status status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

/* Test 5 - RemoveTrafficQueues-Upstream PRIORITY_TO_QUEUE, removing TM QMP as it
is not getting referred by any other queues case */
TEST_F(TestRemoveTrafficQueues, RemoveUpstreamPriorityQueueRemovingTMQMP) {
    traffic_queues->set_uni_id(1);
    traffic_queues->set_port_no(32);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_priority(1);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

/* Test 6 - RemoveTrafficQueues-Upstream PRIORITY_TO_QUEUE, error while removing TM QMP
having no reference to any other queues case */
TEST_F(TestRemoveTrafficQueues, RemoveUpstreamPriorityQueueErrorRemovingTMQMP) {
    traffic_queues->set_uni_id(4);
    traffic_queues->set_port_no(64);
    traffic_queue_1->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_queue_2 = traffic_queues->add_traffic_queues();
    traffic_queue_2->set_gemport_id(1025);
    traffic_queue_2->set_priority(1);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficQueues_(traffic_queues);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing OnuItuPonAlarmSet functionality
////////////////////////////////////////////////////////////////////////////

class TestOnuItuPonAlarmSet : public Test {
    protected:
        bcmolt_pon_ni pon_ni = 0;
        bcmolt_onu_id onu_id = 1;

        config::OnuItuPonAlarm *onu_itu_pon_alarm_rt;
        config::OnuItuPonAlarm::RateThresholdConfig *rate_threshold_config;
        config::OnuItuPonAlarm::SoakTime *soak_time_rt;

        config::OnuItuPonAlarm *onu_itu_pon_alarm_rr;
        config::OnuItuPonAlarm::RateRangeConfig *rate_range_config;
        config::OnuItuPonAlarm::SoakTime *soak_time_rr;

        config::OnuItuPonAlarm *onu_itu_pon_alarm_tc;
        config::OnuItuPonAlarm::ValueThresholdConfig *value_threshold_config;
        config::OnuItuPonAlarm::SoakTime *soak_time_tc;

        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
            onu_itu_pon_alarm_rt = new config::OnuItuPonAlarm;
            rate_threshold_config = new config::OnuItuPonAlarm::RateThresholdConfig;
            soak_time_rt = new config::OnuItuPonAlarm::SoakTime;
            onu_itu_pon_alarm_rt->set_pon_ni(0);
            onu_itu_pon_alarm_rt->set_onu_id(1);
            onu_itu_pon_alarm_rt->set_alarm_id(config::OnuItuPonAlarm_AlarmID::OnuItuPonAlarm_AlarmID_RDI_ERRORS);
            onu_itu_pon_alarm_rt->set_alarm_reporting_condition(config::OnuItuPonAlarm_AlarmReportingCondition::OnuItuPonAlarm_AlarmReportingCondition_RATE_THRESHOLD);
            rate_threshold_config->set_rate_threshold_rising(1);
            rate_threshold_config->set_rate_threshold_falling(4);
            soak_time_rt->set_active_soak_time(2);
            soak_time_rt->set_clear_soak_time(2);
            rate_threshold_config->set_allocated_soak_time(soak_time_rt);
            onu_itu_pon_alarm_rt->set_allocated_rate_threshold_config(rate_threshold_config);

            onu_itu_pon_alarm_rr = new config::OnuItuPonAlarm;
            rate_range_config = new config::OnuItuPonAlarm::RateRangeConfig;
            soak_time_rr = new config::OnuItuPonAlarm::SoakTime;
            onu_itu_pon_alarm_rr->set_pon_ni(0);
            onu_itu_pon_alarm_rr->set_onu_id(1);
            onu_itu_pon_alarm_rr->set_alarm_id(config::OnuItuPonAlarm_AlarmID::OnuItuPonAlarm_AlarmID_RDI_ERRORS);
            onu_itu_pon_alarm_rr->set_alarm_reporting_condition(config::OnuItuPonAlarm_AlarmReportingCondition::OnuItuPonAlarm_AlarmReportingCondition_RATE_RANGE);
            rate_range_config->set_rate_range_lower(1);
            rate_range_config->set_rate_range_upper(4);
            soak_time_rr->set_active_soak_time(2);
            soak_time_rr->set_clear_soak_time(2);
            rate_range_config->set_allocated_soak_time(soak_time_rr);
            onu_itu_pon_alarm_rr->set_allocated_rate_range_config(rate_range_config);

            onu_itu_pon_alarm_tc = new config::OnuItuPonAlarm;
            value_threshold_config = new config::OnuItuPonAlarm::ValueThresholdConfig;
            soak_time_tc = new config::OnuItuPonAlarm::SoakTime;
            onu_itu_pon_alarm_tc->set_pon_ni(0);
            onu_itu_pon_alarm_tc->set_onu_id(1);
            onu_itu_pon_alarm_tc->set_alarm_id(config::OnuItuPonAlarm_AlarmID::OnuItuPonAlarm_AlarmID_RDI_ERRORS);
            onu_itu_pon_alarm_tc->set_alarm_reporting_condition(config::OnuItuPonAlarm_AlarmReportingCondition::OnuItuPonAlarm_AlarmReportingCondition_VALUE_THRESHOLD);
            value_threshold_config->set_threshold_limit(6);
            soak_time_tc->set_active_soak_time(2);
            soak_time_tc->set_clear_soak_time(2);
            value_threshold_config->set_allocated_soak_time(soak_time_tc);
            onu_itu_pon_alarm_tc->set_allocated_value_threshold_config(value_threshold_config);
        }

        virtual void TearDown() {
        }
};

// Test 1 - OnuItuPonAlarmSet-Set RDI_errors to rate threshold type success case
// rate_threshold: The alarm is triggered if the stats delta value between samples crosses
//                 the configured threshold boundary.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsRateThresholdTypeSuccess) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_RATE_THRESHOLD;
    stat_cfg.data.rdi_errors.trigger.u.rate_threshold.rising = 50;
    stat_cfg.data.rdi_errors.trigger.u.rate_threshold.falling = 100;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_rt);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - OnuItuPonAlarmSet-Set RDI_errors to rate threshold type failure case
// rate_threshold: The alarm is triggered if the stats delta value between samples crosses
//                 the configured threshold boundary.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsRateThresholdTypeFailure) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_RATE_THRESHOLD;
    stat_cfg.data.rdi_errors.trigger.u.rate_threshold.rising = 50;
    stat_cfg.data.rdi_errors.trigger.u.rate_threshold.falling = 100;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_rt);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - OnuItuPonAlarmSet-Set RDI_errors to rate range type success case
// rate_range: The alarm is triggered if the stats delta value between samples deviates
//             from the configured range.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsRateRangeTypeSuccess) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_RATE_RANGE;
    stat_cfg.data.rdi_errors.trigger.u.rate_range.upper = 100;
    stat_cfg.data.rdi_errors.trigger.u.rate_range.lower = 50;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_rr);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 4 - OnuItuPonAlarmSet-Set RDI_errors to rate range type failure case
// rate_range: The alarm is triggered if the stats delta value between samples deviates
//             from the configured range.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsRateRangeTypeFailure) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_RATE_RANGE;
    stat_cfg.data.rdi_errors.trigger.u.rate_range.upper = 50;
    stat_cfg.data.rdi_errors.trigger.u.rate_range.lower = 100;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_rr);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - OnuItuPonAlarmSet-Set RDI_errors to value threshold type success case
// value_threshold: The alarm is raised if the stats sample value becomes greater than this
//                  level.  The alarm is cleared when the host read the stats.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsValueThresholdTypeSuccess) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_VALUE_THRESHOLD;
    stat_cfg.data.rdi_errors.trigger.u.value_threshold.limit = 100;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_tc);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 6 - OnuItuPonAlarmSet-Set RDI_errors to value threshold type failure case
// value_threshold: The alarm is raised if the stats sample value becomes greater than this
//                  level.  The alarm is cleared when the host read the stats.
TEST_F(TestOnuItuPonAlarmSet, OnuItuPonAlarmSetRdiErrorsValueThresholdTypeFailure) {
    bcmolt_onu_itu_pon_stats_cfg stat_cfg;
    bcmolt_onu_key key = {};
    BCMOLT_STAT_CFG_INIT(&stat_cfg, onu, itu_pon_stats, key);
    stat_cfg.data.rdi_errors.trigger.type = BCMOLT_STAT_CONDITION_TYPE_VALUE_THRESHOLD;
    stat_cfg.data.rdi_errors.trigger.u.value_threshold.limit = -1;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    ON_CALL(balMock, bcmolt_stat_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = OnuItuPonAlarmSet_(onu_itu_pon_alarm_tc);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing DeleteGroup functionality
////////////////////////////////////////////////////////////////////////////

class TestDeleteGroup : public Test {
    protected:
        uint32_t group_id = 1;
        NiceMock<BalMocker> balMock;

        virtual void SetUp() {
        }
};

// Test 1 - DeleteGroup success case
TEST_F(TestDeleteGroup, DeleteGroupSuccess) {
    bcmos_errno group_cfg_get_res = BCM_ERR_OK;
    bcmos_errno group_cfg_clear_res = BCM_ERR_OK;
    bcmolt_group_cfg grp_cfg_out;
    bcmolt_group_key grp_key = {};

    grp_key.id = group_id;
    BCMOLT_CFG_INIT(&grp_cfg_out, group, grp_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([group_cfg_get_res, &grp_cfg_out] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                     bcmolt_group_cfg* grp_cfg = (bcmolt_group_cfg*)cfg;
                     grp_cfg->data.state = BCMOLT_GROUP_STATE_CONFIGURED;
                     memcpy(&grp_cfg_out, grp_cfg, sizeof(bcmolt_group_cfg));
                     return group_cfg_get_res;
                 }
    ));

    EXPECT_CALL(balMock, bcmolt_cfg_clear(_, _)).WillOnce(Return(group_cfg_clear_res));

    Status status = DeleteGroup_(group_id);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DeleteGroup failure case: Group does not exist
TEST_F(TestDeleteGroup, DeleteGroupFailure_NotFound) {
    bcmos_errno group_cfg_get_res = BCM_ERR_OK;
    bcmolt_group_cfg grp_cfg_out;
    bcmolt_group_key grp_key = {};

    grp_key.id = group_id;
    BCMOLT_CFG_INIT(&grp_cfg_out, group, grp_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([group_cfg_get_res, &grp_cfg_out] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_group_cfg* grp_cfg = (bcmolt_group_cfg*)cfg;
                                                                   grp_cfg->data.state = BCMOLT_GROUP_STATE_NOT_CONFIGURED;
                                                                   memcpy(&grp_cfg_out, grp_cfg, sizeof(bcmolt_group_cfg));
                                                                   return group_cfg_get_res;
                                                               }
    ));

    Status status = DeleteGroup_(group_id);
    ASSERT_TRUE( status.error_code() == grpc::StatusCode::NOT_FOUND );
}

// Test 3 - DeleteGroup failure case: Group exists but cannot be deleted (due to flow association etc.)
TEST_F(TestDeleteGroup, DeleteGroupFailure_CannotDelete) {
    bcmos_errno group_cfg_get_res = BCM_ERR_OK;
    bcmos_errno group_cfg_clear_res = BCM_ERR_INTERNAL;
    bcmolt_group_cfg grp_cfg_out;
    bcmolt_group_key grp_key = {};

    grp_key.id = group_id;
    BCMOLT_CFG_INIT(&grp_cfg_out, group, grp_key);

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _)).WillOnce(Invoke([group_cfg_get_res, &grp_cfg_out] (bcmolt_oltid olt, bcmolt_cfg *cfg) {
                                                                   bcmolt_group_cfg* grp_cfg = (bcmolt_group_cfg*)cfg;
                                                                   grp_cfg->data.state = BCMOLT_GROUP_STATE_CONFIGURED;
                                                                   memcpy(&grp_cfg_out, grp_cfg, sizeof(bcmolt_group_cfg));
                                                                   return group_cfg_get_res;
                                                               }
    ));

    EXPECT_CALL(balMock, bcmolt_cfg_clear(_, _)).WillOnce(Return(group_cfg_clear_res));

    Status status = DeleteGroup_(group_id);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing OnuLogicalDistanceZero functionality
////////////////////////////////////////////////////////////////////////////
class TestOnuLogicalDistanceZero : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        bcmolt_pon_ni pon_ni = 0;
        openolt::OnuLogicalDistance *onu_logical_distance_zero;

        virtual void SetUp() {
            onu_logical_distance_zero = new openolt::OnuLogicalDistance;
            onu_logical_distance_zero->set_intf_id(pon_ni);
        }
        virtual void TearDown() {
        }
};

//
// Test 1 - GetLogicalOnuDistanceZero-Get 0KM logical ONU distance success case
//
TEST_F(TestOnuLogicalDistanceZero, OnuLogicalDistanceZeroSuccess) {
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg;
    bcmolt_pon_interface_key key = {};

    key.pon_ni = pon_ni;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);
    state.activate();
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    Status status = GetLogicalOnuDistanceZero_(pon_ni, onu_logical_distance_zero);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - GetLogicalOnuDistanceZero-Get 0KM logical ONU distance failure case
// The PON state is not ready for failure case
//
TEST_F(TestOnuLogicalDistanceZero, OnuLogicalDistanceZeroPonStateFailure) {
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg;
    bcmolt_pon_interface_key key = {};

    key.pon_ni = pon_ni;
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);
    state.deactivate();
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_INTERNAL;

    Status status = GetLogicalOnuDistanceZero_(pon_ni, onu_logical_distance_zero);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

class TestOnuLogicalDistance : public Test {
    protected:
        NiceMock<BalMocker> balMock;
        bcmolt_pon_ni pon_ni = 0;
        bcmolt_onu_id onu_id = 1;
        openolt::OnuLogicalDistance *onu_logical_distance;

        virtual void SetUp() {
            onu_logical_distance = new openolt::OnuLogicalDistance;
            onu_logical_distance->set_intf_id(pon_ni);
            onu_logical_distance->set_onu_id(onu_id);
        }
        virtual void TearDown() {
        }
};

//
// Test 1 - GetLogicalOnuDistance-Get logical ONU distance success case
//
TEST_F(TestOnuLogicalDistance, OnuLogicalDistanceSuccess) {
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg;
    bcmolt_pon_interface_key key = {};
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;

    key.pon_ni = pon_ni;
    state.activate();
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key = {};
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;

    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    Status status = GetLogicalOnuDistance_(pon_ni, onu_id, onu_logical_distance);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - GetLogicalOnuDistance-Get logical ONU distance failure case
// The failure case is for retrieving ONU ranging time
//
TEST_F(TestOnuLogicalDistance, OnuLogicalDistanceRetrieveOnuRangingTimeFailure) {
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg;
    bcmolt_pon_interface_key key = {};
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;

    key.pon_ni = pon_ni;
    state.activate();
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key = {};
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_INTERNAL;

    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    Status status = GetLogicalOnuDistance_(pon_ni, onu_id, onu_logical_distance);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - GetLogicalOnuDistance-Get logical ONU distance failure case
// The failure case is for ONU is not yet activated
//
TEST_F(TestOnuLogicalDistance, OnuLogicalDistanceOnuNotActivatedFailure) {
    bcmolt_pon_distance pon_distance = {};
    bcmolt_pon_interface_cfg pon_cfg;
    bcmolt_pon_interface_key key = {};
    bcmos_errno olt_cfg_get_pon_stub_res = BCM_ERR_OK;

    key.pon_ni = pon_ni;
    state.activate();
    BCMOLT_CFG_INIT(&pon_cfg, pon_interface, key);

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__pon_intf_stub, bcmolt_cfg_get__pon_intf_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key = {};
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;

    onu_key.pon_ni = pon_ni;
    onu_key.onu_id = onu_id;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_INACTIVE;
    Status status = GetLogicalOnuDistance_(pon_ni, onu_id, onu_logical_distance);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

////////////////////////////////////////////////////////////////////////////
// For testing Secure Server functionality
////////////////////////////////////////////////////////////////////////////

class TestSecureServer : public Test {
    protected:
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestSecureServer, StartInsecureServer) {
    // const to prevent the following warning:
    // warning: deprecated conversion from string constant to 'char*' [-Wwrite-strings]
    const char *args[] = {"./openolt"};
    int argc = sizeof(args) / sizeof(args[0]);
    char **argv = const_cast<char**>(args);

    bool ok = RunServer(argc, argv);

    ASSERT_TRUE(ok);

    OPENOLT_LOG(INFO, openolt_log_id, "insecure gRPC server has been started and shut down successfully\n");
}

TEST_F(TestSecureServer, StartWithInvalidTLSOption) {
    const char *args[] = {"./openolt", "--enable-tls", "DUMMY_GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE"};
    int argc = sizeof(args) / sizeof(args[0]);
    char **argv = const_cast<char**>(args);

    bool ok = RunServer(argc, argv);

    ASSERT_FALSE(ok);

    OPENOLT_LOG(INFO, openolt_log_id, "secure gRPC server could not be started due to invalid TLS option\n");
}

TEST_F(TestSecureServer, CertificatesAreMissing) {
    const char *args[] = {"./openolt", "--enable-tls", "GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE"};
    int argc = sizeof(args) / sizeof(args[0]);
    char **argv = const_cast<char**>(args);
    const std::string cmd = "exec [ -d './keystore' ] && rm -rf './keystore'";

    int res = std::system(cmd.c_str());
    if (res == 0) {
        std::cout << "directory ./keystore is deleted\n";
    } else {
        std::cout << "directory ./keystore is not existing\n";
    }

    bool ok = RunServer(argc, argv);

    ASSERT_FALSE(ok);

    OPENOLT_LOG(INFO, openolt_log_id, "secure gRPC server could not be started due to missing certificates\n");
}

TEST_F(TestSecureServer, StartWithValidTLSOption) {
    const char *args[] = {"./openolt", "--enable-tls", "GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE"};
    int argc = sizeof(args) / sizeof(args[0]);
    char **argv = const_cast<char**>(args);
    const std::string cmd_1 = "exec cp -r ./keystore-test ./keystore";
    const std::string cmd_2 = "exec rm -rf './keystore'";

    int res = std::system(cmd_1.c_str());
    if (res == 0) {
        std::cout << "directory ./keystore is copied from ./keystore-test\n";

        bool ok = RunServer(argc, argv);
        ASSERT_TRUE(ok);

        OPENOLT_LOG(INFO, openolt_log_id, "secure gRPC server has been started with the given certificates and TLS options, and shut down successfully\n");

        res = std::system(cmd_2.c_str());
        if (res == 0) {
            std::cout << "directory ./keystore is deleted\n";
        } else {
            std::cerr << "directory ./keystore could not be deleted\n";
        }
    } else {
        std::cerr << "directory ./keystore could not be prepared, err: " << res << '\n';
        FAIL();
    }
}

////////////////////////////////////////////////////////////////////////////
// For testing RxTx Power Read functionality
////////////////////////////////////////////////////////////////////////////

class TestPowerRead : public Test {
    protected:
        virtual void SetUp() {
            std::array<char, 600> content = {};
            content.fill(0x00); // not required for 0x00

            // for asfvolt16
            content[102] = 0x5D;
            content[103] = 0x38;
            content[104] = 0x1A;
            content[105] = 0xB5;

            // for asgvolt64
            content[358] = 0x5C;
            content[359] = 0x82;
            content[360] = 0x04;
            content[361] = 0xBE;

            std::ofstream test_file;
            test_file.open(file_name, std::ios::binary | std::ios::out);
            test_file.write(content.data(), content.size());
            test_file.close();

            const std::string cmd = "exec hexdump -C " + file_name + " > " + hex_dump;

            int res = std::system(cmd.c_str());
            if (res == 0) {
                std::ifstream dump_file(hex_dump) ;
                std::string hexdump = { std::istreambuf_iterator<char>(dump_file), std::istreambuf_iterator<char>() };
                std::cout << cmd << '\n';
                std::cout << hexdump << '\n';
            } else {
                std::cerr << "hexdump capture failed\n";
            }
        }
        virtual void TearDown() {
            std::remove(file_name.c_str());
            std::remove(hex_dump.c_str());
        }

        const std::string file_name = "eeprom.bin";
        const std::string hex_dump = file_name + ".hexdump";
};

TEST_F(TestPowerRead, TestAsfvolt16) {
    std::cout << "Test Power Reads on XGS-PON OLT:\n";

    int port = 20;
    auto trx_eeprom_reader1 = TrxEepromReader{TrxEepromReader::DEVICE_XGSPON, TrxEepromReader::RX_AND_TX_POWER, port};

    std::cout << "\tis port #" << port << " valid? " << std::boolalpha << trx_eeprom_reader1.is_valid_port() << '\n';

    ASSERT_FALSE(trx_eeprom_reader1.is_valid_port());

    port = 0;
    auto trx_eeprom_reader2 = TrxEepromReader{TrxEepromReader::DEVICE_XGSPON, TrxEepromReader::RX_AND_TX_POWER, port};

    std::cout << "\tis port #" << port << " valid? " << trx_eeprom_reader2.is_valid_port() << '\n';
    std::cout << "\tbuffer size: " << trx_eeprom_reader2.get_buf_size() << '\n';
    std::cout << "\tread offset: " << trx_eeprom_reader2.get_read_offset() << '\n';
    std::cout << "\tread num bytes: " << trx_eeprom_reader2.get_read_num_bytes() << '\n';
    std::cout << "\tmax ports: " << trx_eeprom_reader2.get_max_ports() << '\n';

    auto rxtx_power_raw = trx_eeprom_reader2.read_power_raw();

    ASSERT_TRUE(rxtx_power_raw.second);

    std::cout << "\tRx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.first
              << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.first
              << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << trx_eeprom_reader2.raw_rx_to_mw(rxtx_power_raw.first.first) << " mW, "
              << std::dec << std::setw(9) << trx_eeprom_reader2.mw_to_dbm(trx_eeprom_reader2.raw_rx_to_mw(rxtx_power_raw.first.first)) << " dBm\n";
    std::cout << "\tTx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.second
              << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.second
              << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << trx_eeprom_reader2.raw_tx_to_mw(rxtx_power_raw.first.second) << " mW, "
              << std::dec << std::setw(9) << trx_eeprom_reader2.mw_to_dbm(trx_eeprom_reader2.raw_tx_to_mw(rxtx_power_raw.first.second)) << " dBm\n";
    std::cout << "\tnode path: " << trx_eeprom_reader2.get_node_path() << '\n';

    ASSERT_TRUE(trx_eeprom_reader2.is_valid_port());
    ASSERT_EQ(trx_eeprom_reader2.get_buf_size(), 256);
    ASSERT_EQ(trx_eeprom_reader2.get_read_offset(), 104);
    ASSERT_EQ(trx_eeprom_reader2.get_read_num_bytes(), 2);
    ASSERT_EQ(trx_eeprom_reader2.get_max_ports(), 20);
    ASSERT_EQ(rxtx_power_raw.first.first, 0x1AB5);   // 6837
    ASSERT_EQ(rxtx_power_raw.first.second, 0x5D38);  // 23864
    ASSERT_STREQ(trx_eeprom_reader2.get_node_path(), "/sys/bus/i2c/devices/47-0050/sfp_eeprom");
}

TEST_F(TestPowerRead, TestAsgvolt64) {
    std::cout << "Test Power Reads on GPON OLT:\n";

    int port = 80;
    auto trx_eeprom_reader1 = TrxEepromReader{TrxEepromReader::DEVICE_GPON, TrxEepromReader::RX_AND_TX_POWER, port};

    std::cout << "\tis port #" << port << " valid? " << std::boolalpha << trx_eeprom_reader1.is_valid_port() << '\n';

    ASSERT_FALSE(trx_eeprom_reader1.is_valid_port());

    port = 0;
    auto trx_eeprom_reader2 = TrxEepromReader{TrxEepromReader::DEVICE_GPON, TrxEepromReader::RX_AND_TX_POWER, port};

    std::cout << "\tis port #" << port << " valid? " << trx_eeprom_reader2.is_valid_port() << '\n';
    std::cout << "\tbuffer size: " << trx_eeprom_reader2.get_buf_size() << '\n';
    std::cout << "\tread offset: " << trx_eeprom_reader2.get_read_offset() << '\n';
    std::cout << "\tread num bytes: " << trx_eeprom_reader2.get_read_num_bytes() << '\n';
    std::cout << "\tmax ports: " << trx_eeprom_reader2.get_max_ports() << '\n';

    auto rxtx_power_raw = trx_eeprom_reader2.read_power_raw();

    ASSERT_TRUE(rxtx_power_raw.second);

    std::cout << "\tRx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.first
              << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.first
              << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << trx_eeprom_reader2.raw_rx_to_mw(rxtx_power_raw.first.first) << " mW, "
              << std::dec << std::setw(9) << trx_eeprom_reader2.mw_to_dbm(trx_eeprom_reader2.raw_rx_to_mw(rxtx_power_raw.first.first)) << " dBm\n";
    std::cout << "\tTx power - raw: (hex) " << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << rxtx_power_raw.first.second
              << ", (dec) " << std::dec << std::setfill(' ') << std::setw(5) << rxtx_power_raw.first.second
              << "  | " << std::dec << std::fixed << std::setprecision(5) << std::setw(8) << trx_eeprom_reader2.raw_tx_to_mw(rxtx_power_raw.first.second) << " mW, "
              << std::dec << std::setw(9) << trx_eeprom_reader2.mw_to_dbm(trx_eeprom_reader2.raw_tx_to_mw(rxtx_power_raw.first.second)) << " dBm\n";
    std::cout << "\tnode path: " << trx_eeprom_reader2.get_node_path() << '\n';

    ASSERT_TRUE(trx_eeprom_reader2.is_valid_port());
    ASSERT_EQ(trx_eeprom_reader2.get_buf_size(), 600);
    ASSERT_EQ(trx_eeprom_reader2.get_read_offset(), 360);
    ASSERT_EQ(trx_eeprom_reader2.get_read_num_bytes(), 2);
    ASSERT_EQ(trx_eeprom_reader2.get_max_ports(), 74);
    ASSERT_EQ(rxtx_power_raw.first.first, 0x04BE);   // 1214
    ASSERT_EQ(rxtx_power_raw.first.second, 0x5C82);  // 23682
    ASSERT_STREQ(trx_eeprom_reader2.get_node_path(), "/sys/bus/i2c/devices/41-0050/eeprom");
}

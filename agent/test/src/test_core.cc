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
#include <future>
using namespace testing;
using namespace std;

extern std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *> alloc_cfg_compltd_map;
extern dev_log_id openolt_log_id;
extern bcmos_fastlock alloc_cfg_wait_lock;
extern bool ALLOC_CFG_FLAG;

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

int num_of_pon_ports = 16;

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

    olt_cfg.data.topology.topology_maps.len = num_of_pon_ports;
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

    Status olt_disable_res;
    state.deactivate();
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    olt_disable_res = Disable_();
    ASSERT_TRUE( olt_disable_res.error_message() == Status::OK.error_message() );

}

// Test 2: OltDisableAllPonFailed  case
TEST_F(TestOltDisableReenable, OltDisableAllPonFailed){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;

    Status olt_disable_res;
    state.deactivate();
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
                     .Times(num_of_pon_ports)
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));

    bcmolt_tm_sched_cfg tm_sched_cfg;
    bcmolt_tm_sched_key tm_sched_key = {.id = 1004};
    BCMOLT_CFG_INIT(&tm_sched_cfg, tm_sched, tm_sched_key);
    tm_sched_cfg.data.state = BCMOLT_CONFIG_STATE_NOT_CONFIGURED;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__tm_sched_stub, bcmolt_cfg_get__tm_sched_stub(_, _))
                     .Times(num_of_pon_ports)
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
                     .Times(num_of_pon_ports)
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltPonCfg(pon_cfg), Return(olt_cfg_get_pon_stub_res)));
    EXPECT_CALL(balMock,bcmolt_oper_submit(_, _))
                         .Times(num_of_pon_ports)
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

            olt_cfg.data.topology.topology_maps.len = num_of_pon_ports;
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

// Test 1 - DisablePonIf success case
TEST_F(TestDisablePonIf, DisablePonIfSuccess) {
    bcmos_errno olt_oper_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_set_res = BCM_ERR_OK;
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;

    //ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(bal_cfg_set_res));
    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DisablePonIf Failure case
TEST_F(TestDisablePonIf, DisablePonIfFailed) {
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(olt_oper_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - DisablePonIf ONU discovery failure case
TEST_F(TestDisablePonIf, DisablePonIfOnuDiscoveryFail) {
    NiceMock<BalMocker> balMock;
    uint32_t pon_id=1;
    bcmos_errno bal_cfg_set_res= BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(bal_cfg_set_res));
    state.deactivate();
    Status status = DisablePonIf_(pon_id);

    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
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

// Test 1 - ActivateOnu success case
TEST_F(TestActivateOnu, ActivateOnuSuccess) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_INTERNAL;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_INTERNAL;
    bcmos_errno onu_cfg_set_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(onu_cfg_set_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - ActivateOnu failure case
TEST_F(TestActivateOnu, ActivateOnuFailure) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_INTERNAL;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_INTERNAL;
    bcmos_errno onu_cfg_set_res = BCM_ERR_INTERNAL;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(onu_cfg_set_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir);
    ASSERT_FALSE( status.error_message() == Status::OK.error_message() );
}

// Test 3 - ActivateOnu - Onu already under processing case
TEST_F(TestActivateOnu, ActivateOnuProcessing) {
    bcmos_errno onu_cfg_get_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_get(_, _)).WillByDefault(Return(onu_cfg_get_res));

    Status status = ActivateOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str(), pir);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
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
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(onu_cfg_clear_res));

    Status status = DeleteOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - DeleteOnu failure case
TEST_F(TestDeleteOnu, DeleteOnuFailure) {
    bcmos_errno onu_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno onu_oper_sub_res = BCM_ERR_OK;
    bcmos_errno onu_cfg_clear_res = BCM_ERR_INTERNAL;

    bcmolt_onu_cfg onu_cfg;
    bcmolt_onu_key onu_key;
    BCMOLT_CFG_INIT(&onu_cfg, onu, onu_key);
    onu_cfg.data.onu_state = BCMOLT_ONU_STATE_ACTIVE;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__onu_state_stub, bcmolt_cfg_get__onu_state_stub(_, _))
                     .WillOnce(DoAll(SetArg1ToBcmOltOnuCfg(onu_cfg), Return(onu_cfg_get_stub_res)));

    ON_CALL(balMock, bcmolt_oper_submit(_, _)).WillByDefault(Return(onu_oper_sub_res));
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(onu_cfg_clear_res));

    Status status = DeleteOnu_(pon_id, onu_id, vendor_id.c_str(), vendor_specific.c_str());
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

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - FlowAdd - Duplicate Flow case
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueUpstreamDuplicate) {
    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - FlowAdd - Failure case(bcmolt_cfg_set returns error)
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueUpstreamFailure) {
    gemport_id = 1025;

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_INTERNAL;

    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - FlowAdd - Failure case(Invalid flow direction)
TEST_F(TestFlowAdd, FlowAddFailureInvalidFlowDirection) {
    flow_type = "bidirectional";

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 5 - FlowAdd - Failure case(Invalid network setting)
TEST_F(TestFlowAdd, FlowAddFailureInvalidNWCfg) {
    network_intf_id = -1;

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 6 - FlowAdd - Success case(Single tag & EAP Ether type)
TEST_F(TestFlowAdd, FlowAddEapEtherTypeSuccess) {
    flow_id = 2;

    classifier->set_eth_type(34958);
    cmd->set_add_outer_tag(false);
    cmd->set_trap_to_host(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 7 - FlowAdd - Success case(Single tag & DHCP flow)
TEST_F(TestFlowAdd, FlowAddDhcpSuccess) {
    flow_id = 3;
    gemport_id = 1025;

    classifier->set_ip_proto(17);
    classifier->set_src_port(68);
    classifier->set_dst_port(67);
    cmd->set_add_outer_tag(false);
    cmd->set_trap_to_host(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 8 - FlowAdd - success case(HSIA-downstream FixedQueue)
TEST_F(TestFlowAdd, FlowAddHsiaFixedQueueDownstreamSuccess) {
    flow_id = 4;
    flow_type = "downstream";

    classifier->set_o_vid(12);
    classifier->set_i_vid(7);
    classifier->set_pkt_tag_type("double_tag");
    action->set_o_vid(0);
    cmd->set_add_outer_tag(false);
    cmd->set_remove_outer_tag(true);
    action->set_allocated_cmd(cmd);

    bcmos_errno flow_cfg_get_stub_res = BCM_ERR_OK;
    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get__flow_stub, bcmolt_cfg_get__flow_stub(_, _))
                     .WillRepeatedly(DoAll(SetArg1ToBcmOltFlowCfg(flow_cfg), Return(flow_cfg_get_stub_res)));
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
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

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
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
    action->set_o_vid(0);
    cmd->set_add_outer_tag(false);
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

    Status status = FlowAdd_(access_intf_id, onu_id, uni_id, port_no, flow_id, flow_type, alloc_id, network_intf_id, gemport_id, *classifier, *action, priority_value, cookie);
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
        static void PushAllocCfgResult(AllocObjectState state, AllocCfgStatus status) {
            if(ALLOC_CFG_FLAG) {
                alloc_cfg_compltd_key k(0, 1024);
                alloc_cfg_complete_result res;
                res.pon_intf_id = 0;
                res.alloc_id = 1024;
                res.state = state;
                res.status = status;

                bcmos_fastlock_lock(&alloc_cfg_wait_lock);
                std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *>::iterator it = alloc_cfg_compltd_map.find(k);
                if (it == alloc_cfg_compltd_map.end()) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "alloc config key not found for alloc_id = %u, pon_intf = %u\n", 1024, 0);
                } else {
                    it->second->push(res);
                    OPENOLT_LOG(INFO, openolt_log_id, "Pushed mocked alloc cfg result\n");
                }
                bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
            } else {
                PushAllocCfgResult(state, status); 
            }
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
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
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
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_FAIL);

    Status status = future_res.get();
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
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_INACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
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
    traffic_shaping_info->set_cir(0);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    Status status = CreateTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 15 - CreateTrafficSchedulers-Upstream Success (AdditionalBW_None-Max BW > Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWGtGuaranteedBwSuccess) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(128000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 16 - CreateTrafficSchedulers-Upstream Success (AdditionalBW_None-Max BW < Guaranteed BW) case
TEST_F(TestCreateTrafficSchedulers, AdditionalBW_NoneMaxBWLtGuaranteedBwSuccess) {
    scheduler->set_direction(tech_profile::Direction::UPSTREAM);
    scheduler->set_additional_bw(tech_profile::AdditionalBW::AdditionalBW_None);
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    traffic_sched->set_allocated_scheduler(scheduler);
    traffic_shaping_info->set_cir(64000);
    traffic_shaping_info->set_pir(32000);
    traffic_sched->set_allocated_traffic_shaping_info(traffic_shaping_info);

    bcmos_errno olt_cfg_set_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_set(_, _)).WillByDefault(Return(olt_cfg_set_res));

    future<Status> future_res = async(launch::async, CreateTrafficSchedulers_, traffic_scheds);
    async(launch::async, TestCreateTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_ACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 17 - CreateTrafficSchedulers-Downstream success case
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

// Test 18 - CreateTrafficSchedulers-Downstream Failure case
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

// Test 19 - CreateTrafficSchedulers-Invalid direction Failure case
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
        static void PushAllocCfgResult(AllocObjectState state, AllocCfgStatus status) {
            if(ALLOC_CFG_FLAG) {
                alloc_cfg_compltd_key k(0, 1025);
                alloc_cfg_complete_result res;
                res.pon_intf_id = 0;
                res.alloc_id = 1025;
                res.state = state;
                res.status = status;

                bcmos_fastlock_lock(&alloc_cfg_wait_lock);
                std::map<alloc_cfg_compltd_key,  Queue<alloc_cfg_complete_result> *>::iterator it = alloc_cfg_compltd_map.find(k);
                if (it == alloc_cfg_compltd_map.end()) {
                    OPENOLT_LOG(ERROR, openolt_log_id, "alloc config key not found for alloc_id = %u, pon_intf = %u\n", 1025, 0);
                } else {
                    it->second->push(res);
                    OPENOLT_LOG(INFO, openolt_log_id, "Pushed mocked alloc cfg result\n");
                }
                bcmos_fastlock_unlock(&alloc_cfg_wait_lock, 0);
            } else {
                PushAllocCfgResult(state, status); 
            }
        }
};

// Test 1 - RemoveTrafficSchedulers-Upstream success case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersUpstreamSuccess) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);
    async(launch::async, TestRemoveTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_NOT_CONFIGURED, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() == Status::OK.error_message() );
}

// Test 2 - RemoveTrafficSchedulers-Upstream success case(alloc object is not reset)
TEST_F(TestRemoveTrafficSchedulers, UpstreamAllocObjNotReset) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);
    bcmos_errno olt_cfg_clear_res = BCM_ERR_OK;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    future<Status> future_res = async(launch::async, RemoveTrafficSchedulers_, traffic_scheds);
    async(launch::async, TestRemoveTrafficSchedulers::PushAllocCfgResult, ALLOC_OBJECT_STATE_INACTIVE, ALLOC_CFG_STATUS_SUCCESS);

    Status status = future_res.get();
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 3 - RemoveTrafficSchedulers-Upstream Failure case
TEST_F(TestRemoveTrafficSchedulers, RemoveTrafficSchedulersUpstreamFailure) {
    traffic_sched->set_direction(tech_profile::Direction::UPSTREAM);

    bcmos_errno olt_cfg_clear_res = BCM_ERR_INTERNAL;
    ON_CALL(balMock, bcmolt_cfg_clear(_, _)).WillByDefault(Return(olt_cfg_clear_res));

    Status status = RemoveTrafficSchedulers_(traffic_scheds);
    ASSERT_TRUE( status.error_message() != Status::OK.error_message() );
}

// Test 4 - RemoveTrafficSchedulers-Downstream Failure case
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

// Test 5 - RemoveTrafficSchedulers-Downstream success case
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

// Test 6 - RemoveTrafficSchedulers-Downstream Scheduler not present case
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

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
#include "bal_mocker.h"
#include "core.h"
using namespace testing;
using namespace std;

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


// Create a mock function for bcmolt_cfg_get__bal_state_stub C++ function
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get__bal_state_stub, bcmos_errno(bcmolt_oltid, void*));


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
    bcmos_errno bal_cfg_get_res = BCM_ERR_OK;

    Status olt_reenable_res;

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(num_of_pon_ports*2)
                         .WillRepeatedly(Return(bal_cfg_get_res));

    olt_reenable_res = Reenable_();
    ASSERT_TRUE( olt_reenable_res.error_message() == Status::OK.error_message() );

}

// Test 2: OltReenableAllPonFailed  case
TEST_F(TestOltDisableReenable, OltReenableAllPonFailed){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno bal_cfg_get_res = BCM_ERR_OK;
    bcmos_errno olt_oper_res = BCM_ERR_INTERNAL;

    Status olt_reenable_res;

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(num_of_pon_ports)
                         .WillRepeatedly(Return(bal_cfg_get_res));
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


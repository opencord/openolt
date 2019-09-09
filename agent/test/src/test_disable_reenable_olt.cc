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
int num_of_pon_ports = 16;

// This is used to set custom bcmolt_cfg value to bcmolt_cfg pointer coming in
// .bcmolt_cfg_get__olt_topology_stub
ACTION_P(SetArg1ToBcmOltCfg, value) {*static_cast<bcmolt_olt_cfg*>(arg1) = value;};


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
    bcmos_errno bal_cfg_get_res = BCM_ERR_OK;

    Status olt_disable_res;

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(num_of_pon_ports)
                         .WillRepeatedly(Return(bal_cfg_get_res));

    olt_disable_res = Disable_();
    ASSERT_TRUE( olt_disable_res.error_message() == Status::OK.error_message() );

}

// Test 2: OltDisableAllPonFailed  case
TEST_F(TestOltDisableReenable, OltDisableAllPonFailed){
    // NiceMock is used to suppress 'WillByDefault' return errors.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    NiceMock<BalMocker> balMock;
    bcmos_errno bal_cfg_get_res = BCM_ERR_INTERNAL;

    Status olt_disable_res;

    EXPECT_CALL(balMock, bcmolt_cfg_get(_, _))
                         .Times(num_of_pon_ports)
                         .WillRepeatedly(Return(bal_cfg_get_res));

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


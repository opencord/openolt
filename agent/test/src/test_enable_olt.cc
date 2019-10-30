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
#include "gtest/gtest.h"
#include "bal_mocker.h"
#include "core.h"

using namespace testing;

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
// bcmolt_cfg_get_stub.
ACTION_P(SetArg1ToBcmOltCfg, value) { *static_cast<bcmolt_olt_cfg*>(arg1) = value; };


// Create a mock function for bcmolt_cfg_get_stub C++ function
MOCK_GLOBAL_FUNC2(bcmolt_cfg_get_stub, bcmos_errno(bcmolt_oltid, void*));


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
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get_stub, bcmolt_cfg_get_stub(_, _))
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
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get_stub, bcmolt_cfg_get_stub(_, _))
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
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get_stub, bcmolt_cfg_get_stub(_, _))
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
    EXPECT_GLOBAL_CALL(bcmolt_cfg_get_stub, bcmolt_cfg_get_stub(_, _))
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

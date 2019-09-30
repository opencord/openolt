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

class TestOltEnable : public ::testing::Test {
 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test
    // (right before the destructor).
  }
};


// Test Fixture for OltEnable

// Test 1: OltEnableSuccess case
TEST_F(TestOltEnable, OltEnableSuccess){
    // NiceMock is used to suppress 'WillByDefault' return errors on using 'NiceMock'.
    // This is described in https://github.com/arangodb-helper/gtest/blob/master/googlemock/docs/CookBook.md
    ::testing::NiceMock<BalMocker> balMock;
    bcmos_errno host_init_res = BCM_ERR_OK;
    bcmos_errno bal_cfg_get_res = BCM_ERR_NOT_CONNECTED;
    bcmos_errno olt_oper_res = BCM_ERR_OK;

    Status olt_enable_res;

    // The 'EXPECT_CALL' will do strict validation of input parameters. This may not be relevant for
    // the current test case. Use 'ON_CALL' instead.
    // The ON_CALL results in WARNINGs when running tests. Use NickMock instead of directly using BalMocker.
    // https://github.com/google/googletest/blob/master/googlemock/docs/cook_book.md#the-nice-the-strict-and-the-naggy-nicestrictnaggy
    // In below tests '::testing::_' does no validation on argument.
    ON_CALL(balMock, bcmolt_host_init(::testing::_)).WillByDefault(::testing::Return(host_init_res));
    ON_CALL(balMock, bcmolt_cfg_get(::testing::_,::testing::_)).WillByDefault(::testing::Return(bal_cfg_get_res));
    ON_CALL(balMock, bcmolt_oper_submit(::testing::_, ::testing::_)).WillByDefault(::testing::Return(olt_oper_res));

    olt_enable_res = Enable_(1, NULL);
    ASSERT_TRUE( olt_enable_res.error_message() == Status::OK.error_message() );
}


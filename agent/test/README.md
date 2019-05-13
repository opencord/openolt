# OpenOLT Agent Unit Test

OpenOLT agent unit test framework is based on the following frameworks

- Google Test: https://github.com/google/googletest
- C-Mock: https://github.com/hjagodzinski/C-Mock
 
The goal of the unit tests for OpenOLT agent is to test the OpenOLT application by stubbing and mocking the external interfaces, especially the BAL API and GRPC.

## Building and Running Unit Test

Follow the below steps to build and run unit test
```shell
$ cd agent/test
# Run Autoconfig to generate the appropriate makefile scaffolding for the desired target
./configure
# This will build pre-requisite software to build openolt. Needed once only.
$ make prereq
# This will install gtest and c-mock libraries. Needed once only
$ make prereq-mock-lib
# This will build and run the unit test cases
$ make test
```

If you need to use a specific version of voltha-protos, then specify the git tag/branch corresponding to
tht specific version as below. For ex:

```shell
make OPENOLT_PROTO_VER=master test
```

By default, the OPENOLT_PROTO_VER defaults to git tag *v1.0.3* of https://github.com/opencord/voltha-protos repo.

Once you have successfully built and run the unit-test, the test report will be available in `test_openolt_report.xml` file in `agent/test`.
To clean all build artifacts and test reports, do `make clean` in `agent/test`.

## Adding new Unit Test Cases

Before you add new test cases please read [GOOGLE TEST COOKBOOK](https://github.com/google/googletest/blob/master/googlemock/docs/cook_book.md) and [USING CMOCK](https://github.com/hjagodzinski/C-Mock/blob/master/README.md) to get acquainted with frameworks used for the unit-test cases.

### Create mocks, if needed

Refer `agent/test/src/bal_mocker.cc` and `agent/test/src/bal_mocker.h` to see how mock functions are created (if needed by your test case). In the aforementioned examples, mocks are created for certain BAL APIs.

### Create Unit Test Case

Please refer example `agent/test/src/test_enable_olt.cc`.
Tests are grouped under Test Fixtures, i.e., `TEST_F` which takes argument of Test Fixture Name and the Test Case. There will typically be many test cases under a test fixture. Also it is preferrable to create a new file for different test fixtures.
If your test case needs mocking, you will need to instantiate the appropriate mocker class you have created and use the `ON_CALL`, `EXPECT_CALL` or other utilities from the gtest suite to produce the required behavior.
Use the `ASSERT_XXX` utility from the gtest suite to verify the required result.
Please refer the [GOOGLE TEST COOKBOOK](https://github.com/google/googletest/blob/master/googlemock/docs/cook_book.md) for detailed documentation.


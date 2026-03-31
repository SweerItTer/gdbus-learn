#pragma once
#include "DbusConstants.hpp"
#include <string>

namespace training::public_api {

class ITestService {
public:
    virtual ~ITestService() = default;

    //Set 接口
    virtual bool SetTestBool(bool param) = 0;
    virtual bool SetTestInt(int param) = 0;
    virtual bool SetTestDouble(double param) = 0;
    virtual bool SetTestString(std::string param) = 0;
    virtual bool SetTestInfo(TestInfo param) = 0;
    //Get 接口
    virtual bool GetTestBool() = 0;
    virtual int GetTestInt() = 0;
    virtual double GetTestDouble() = 0;
    virtual std::string GeTestString() = 0;
    virtual TestInfo GetTestInfo() = 0;
    // send 接口
    virtual bool SendFile(unsigned char* file_buf, size_t file_size) = 0;
};

} // namespace training::public_api

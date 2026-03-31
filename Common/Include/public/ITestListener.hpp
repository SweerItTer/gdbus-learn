#pragma once
#include "DbusConstants.hpp"
#include <string>

namespace training::public_api {

class ITestListener {
public:
    virtual ~ITestListener() = default;

    virtual void OnTestBoolChanged(bool param) = 0;
    virtual void OnTestIntChanged(int param) = 0;
    virtual void OnTestDoubleChanged(double param) = 0;
    virtual void OnTestStringChanged(std::string param) = 0;
    virtual void OnTestInfoChanged(TestInfo param) = 0;
};

} // namespace training::public_api

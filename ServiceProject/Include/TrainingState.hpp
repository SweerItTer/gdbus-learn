#pragma once

#include <public/DbusConstants.hpp>

#include <string>

namespace training::service {

class TrainingState {
public:
    bool SetTestBool(bool param);
    bool SetTestInt(int param);
    bool SetTestDouble(double param);
    bool SetTestString(std::string param);
    bool SetTestInfo(public_api::TestInfo param);

    bool GetTestBool() const;
    int GetTestInt() const;
    double GetTestDouble() const;
    std::string GetTestString() const;
    public_api::TestInfo GetTestInfo() const;

private:
    public_api::TestInfo test_info_{false, 0, 0.0, {}};
};

} // namespace training::service

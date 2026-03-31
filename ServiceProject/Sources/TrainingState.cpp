#include <TrainingState.hpp>

namespace training::service {

bool TrainingState::SetTestBool(bool param) {
    test_info_.bool_param = param;
    return true;
}

bool TrainingState::SetTestInt(int param) {
    test_info_.int_param = param;
    return true;
}

bool TrainingState::SetTestDouble(double param) {
    test_info_.double_param = param;
    return true;
}

bool TrainingState::SetTestString(std::string param) {
    test_info_.string_param = std::move(param);
    return true;
}

bool TrainingState::SetTestInfo(public_api::TestInfo param) {
    test_info_ = std::move(param);
    return true;
}

bool TrainingState::GetTestBool() const {
    return test_info_.bool_param;
}

int TrainingState::GetTestInt() const {
    return test_info_.int_param;
}

double TrainingState::GetTestDouble() const {
    return test_info_.double_param;
}

std::string TrainingState::GetTestString() const {
    return test_info_.string_param;
}

public_api::TestInfo TrainingState::GetTestInfo() const {
    return test_info_;
}

} // namespace training::service

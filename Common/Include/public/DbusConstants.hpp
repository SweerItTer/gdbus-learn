#pragma once

#include <string>

namespace training {

inline constexpr const char* kBusName = "com.example.Training";
inline constexpr const char* kObjectPath = "/com/example/Training";
inline constexpr const char* kInterfaceName = "com.example.Training";

namespace public_api{
    using TestInfo = struct _testinfo {
        bool bool_param;
        int int_param;
        double double_param;
        std::string string_param;
    };
} // namespace public_api

} // namespace training

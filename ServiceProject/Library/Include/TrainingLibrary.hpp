#pragma once

#include <public/DbusConstants.hpp>
#include <public/TrainingLibraryApi.hpp>
#include <utils/GLibWrappers.hpp>

#include "training-generated.h"

#include <mutex>
#include <string>

namespace training::library {

// 包含创建proxy、注册槽函数、调用method、槽函数转发和实现
class TrainingLibraryClient {
public:
    TrainingLibraryClient();

    void SetListener(const TrainingListenerCallbacks* callbacks);

    bool SetTestBool(bool param);
    bool SetTestInt(int param);
    bool SetTestDouble(double param);
    bool SetTestString(const char* param);
    bool SetTestInfo(const public_api::TestInfo* param);

    bool GetTestBool(bool* result);
    bool GetTestInt(int* result);
    bool GetTestDouble(double* result);
    bool GetTestString(const char** result);
    bool GetTestInfo(public_api::TestInfo* result);
    void PumpEvents();

private:
    static void OnRemoteTestBoolChanged(Training* proxy, gboolean param, gpointer user_data);
    static void OnRemoteTestIntChanged(Training* proxy, gint param, gpointer user_data);
    static void OnRemoteTestDoubleChanged(Training* proxy, gdouble param, gpointer user_data);
    static void OnRemoteTestStringChanged(Training* proxy, const gchar* param, gpointer user_data);
    static void OnRemoteTestInfoChanged(Training* proxy, GVariant* param, gpointer user_data);

    utils::UniqueGObject<Training> proxy_{nullptr};

    // info 本体
    public_api::TestInfo cached_info_{};

    std::mutex mutex_;
    TrainingListenerCallbacks listener_{};
    std::string last_string_result_{};
};

} // namespace training::library

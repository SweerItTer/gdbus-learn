#pragma once

#include <public/DbusConstants.hpp>
#include <public/TrainingLibraryApi.hpp>
#include <utils/GLibWrappers.hpp>

#include "training-generated.h"

#include <string>

namespace training::library {

class TrainingLibraryClient {
public:
    TrainingLibraryClient();

    void SetListener(const TrainingListenerCallbacks* callbacks);

    bool SetTestBool(bool param);
    bool SetTestInt(int param);
    bool SetTestDouble(double param);
    bool SetTestString(const char* param);
    bool SetTestInfo(const TrainingInfoView* param);

    bool GetTestBool(bool* result);
    bool GetTestInt(int* result);
    bool GetTestDouble(double* result);
    bool GetTestString(const char** result);
    bool GetTestInfo(TrainingInfoView* result);

private:
    static void OnRemoteTestBoolChanged(Training* proxy, gboolean param, gpointer user_data);
    static void OnRemoteTestIntChanged(Training* proxy, gint param, gpointer user_data);
    static void OnRemoteTestDoubleChanged(Training* proxy, gdouble param, gpointer user_data);
    static void OnRemoteTestStringChanged(Training* proxy, const gchar* param, gpointer user_data);
    static void OnRemoteTestInfoChanged(Training* proxy, GVariant* param, gpointer user_data);

    void NotifyTestBoolChanged(bool param);
    void NotifyTestIntChanged(int param);
    void NotifyTestDoubleChanged(double param);
    void NotifyTestStringChanged(const std::string& param);
    void NotifyTestInfoChanged(const public_api::TestInfo& param);

    void UpdateInfoView();

    utils::UniqueGObject<Training> proxy_{nullptr};
    TrainingListenerCallbacks listener_{};
    public_api::TestInfo cached_info_{};
    std::string last_string_result_{};
    TrainingInfoView cached_info_view_{};
};

} // namespace training::library

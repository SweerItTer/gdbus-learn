#pragma once

#include <public/ITestService.hpp>
#include <utils/GLibWrappers.hpp>
#include <utils/ScopedBusConnection.hpp>
#include <utils/ScopedBusNameOwner.hpp>

#include "training-generated.h"

namespace training::service {

class TrainingService : public public_api::ITestService {
public:
    TrainingService();
    ~TrainingService();

    TrainingService(const TrainingService&) = delete;
    TrainingService& operator=(const TrainingService&) = delete;

    void Run();

    // 方法实现
    bool SetTestBool(bool param) override;
    bool SetTestInt(int param) override;
    bool SetTestDouble(double param) override;
    bool SetTestString(std::string param) override;
    bool SetTestInfo(public_api::TestInfo param) override;
    bool GetTestBool() override;
    int GetTestInt() override;
    double GetTestDouble() override;
    std::string GeTestString() override;
    public_api::TestInfo GetTestInfo() override;
    bool SendFile(unsigned char* file_buf, size_t file_size) override;

private:
    // method调用
    static gboolean OnHandleSetTestBool(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gboolean param,
                                        gpointer user_data);
    static gboolean OnHandleSetTestInt(Training* object,
                                       GDBusMethodInvocation* invocation,
                                       gint param,
                                       gpointer user_data);
    static gboolean OnHandleSetTestDouble(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gdouble param,
                                          gpointer user_data);
    static gboolean OnHandleSetTestString(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          const gchar* param,
                                          gpointer user_data);
    static gboolean OnHandleSetTestInfo(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        GVariant* param,
                                        gpointer user_data);
    static gboolean OnHandleGetTestBool(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gpointer user_data);
    static gboolean OnHandleGetTestInt(Training* object,
                                       GDBusMethodInvocation* invocation,
                                       gpointer user_data);
    static gboolean OnHandleGetTestDouble(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gpointer user_data);
    static gboolean OnHandleGetTestString(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gpointer user_data);
    static gboolean OnHandleGetTestInfo(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gpointer user_data);

    utils::UniqueMainLoop loop_;
    utils::ScopedBusConnection connection_;
    utils::ScopedBusNameOwner bus_name_owner_;
    utils::UniqueGObject<Training> skeleton_;
    public_api::TestInfo state_{false, 0, 0.0, {}}; // service 端唯一状态
};

} // namespace training::service

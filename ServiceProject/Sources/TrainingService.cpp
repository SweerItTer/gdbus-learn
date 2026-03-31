#include <TrainingService.hpp>

#include <public/DbusConstants.hpp>
#include <utils/ContractSerializer.hpp>

#include "training-generated.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace training::service {

TrainingService::TrainingService()
    : loop_(g_main_loop_new(nullptr, FALSE)) // 循环对象
    , connection_(G_BUS_TYPE_SESSION)        // SESSION 连接
{   
    // 创建 skeleton
    skeleton_.reset(training_skeleton_new());
    // 根据现有 connection 获取 own_id 以便后续关闭
    bus_name_owner_.Acquire(connection_.Get(), training::kBusName);

    // 注册回调 (method) 到 skeleton 对象(通过‘detailed_signal’O(1)查表到具体回调函数)
    g_signal_connect(skeleton_.get(), "handle-set-test-bool", G_CALLBACK(&TrainingService::OnHandleSetTestBool), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-int", G_CALLBACK(&TrainingService::OnHandleSetTestInt), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-double", G_CALLBACK(&TrainingService::OnHandleSetTestDouble), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-string", G_CALLBACK(&TrainingService::OnHandleSetTestString), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-info", G_CALLBACK(&TrainingService::OnHandleSetTestInfo), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-bool", G_CALLBACK(&TrainingService::OnHandleGetTestBool), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-int", G_CALLBACK(&TrainingService::OnHandleGetTestInt), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-double", G_CALLBACK(&TrainingService::OnHandleGetTestDouble), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-string", G_CALLBACK(&TrainingService::OnHandleGetTestString), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-info", G_CALLBACK(&TrainingService::OnHandleGetTestInfo), this);

    // 将 skeleton 和 connection 上的 ObjectPath 关联
    GError* raw_error = nullptr;
    const gboolean exported = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(skeleton_.get()),
        connection_.Get(),      // 连接句柄
        training::kObjectPath,  // 接口路径
        &raw_error);
    utils::UniqueGError error(raw_error);

    if (!exported) {
        const std::string message = error != nullptr ? error->message : "unknown export failure";
        throw std::runtime_error("failed to export Training skeleton: " + message);
    }
}

TrainingService::~TrainingService() = default;

// 启动服务
void TrainingService::Run() {
    std::cout << "server: waiting for calls on " << training::kBusName << std::endl;
    g_main_loop_run(loop_.get());
}

/* ---------------------------------------
 *               info 操作   
 * --------------------------------------- */
bool TrainingService::SetTestBool(bool param) {
    state_.bool_param = param;
    return true;
}

bool TrainingService::SetTestInt(int param) {
    state_.int_param = param;
    return true;
}

bool TrainingService::SetTestDouble(double param) {
    state_.double_param = param;
    return true;
}

bool TrainingService::SetTestString(std::string param) {
    state_.string_param = std::move(param);
    return true;
}

bool TrainingService::SetTestInfo(public_api::TestInfo param) {
    state_ = std::move(param);
    return true;
}

bool TrainingService::GetTestBool() {
    return state_.bool_param;
}

int TrainingService::GetTestInt() {
    return state_.int_param;
}

double TrainingService::GetTestDouble() {
    return state_.double_param;
}

std::string TrainingService::GeTestString() {
    return state_.string_param;
}

public_api::TestInfo TrainingService::GetTestInfo() {
    return state_;
}

bool TrainingService::SendFile(unsigned char* file_buf, size_t file_size) {
    (void)file_buf;
    (void)file_size;
    return false;
}

/* ---------------------------------------
 *           method 调用具体方法   
 * --------------------------------------- */
gboolean TrainingService::OnHandleSetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gboolean param,
                                              gpointer user_data) {
    // 转换指针为具体类型
    auto* self = static_cast<TrainingService*>(user_data); 
    // 调用具体实现函数
    const bool ok = self->SetTestBool(static_cast<bool>(param));
    // 附带返回值完成method调用
    training_complete_set_test_bool(object, invocation, ok);
    // 广播信号
    training_emit_on_test_bool_changed(object, param);
    // 返回调用状态(必须)
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestInt(Training* object,
                                             GDBusMethodInvocation* invocation,
                                             gint param,
                                             gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestInt(param);
    training_complete_set_test_int(object, invocation, ok);
    training_emit_on_test_int_changed(object, param);
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestDouble(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gdouble param,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestDouble(param);
    training_complete_set_test_double(object, invocation, ok);
    training_emit_on_test_double_changed(object, param);
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestString(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                const gchar* param,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestString(param != nullptr ? param : "");
    training_complete_set_test_string(object, invocation, ok);
    training_emit_on_test_string_changed(object, param != nullptr ? param : "");
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestInfo(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              GVariant* param,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    // 反序列化(解析数据)
    const auto info = utils::VariantToTestInfo(param);
    const bool ok = self->SetTestInfo(info);
    training_complete_set_test_info(object, invocation, ok);
    training_emit_on_test_info_changed(object, utils::TestInfoToVariant(info));
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_bool(object, invocation, self->GetTestBool());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestInt(Training* object,
                                             GDBusMethodInvocation* invocation,
                                             gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_int(object, invocation, self->GetTestInt());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestDouble(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_double(object, invocation, self->GetTestDouble());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestString(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const auto result = self->GeTestString();
    training_complete_get_test_string(object, invocation, result.c_str());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestInfo(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_info(object, invocation, utils::TestInfoToVariant(self->GetTestInfo()));
    return TRUE;
}

} // namespace training::service

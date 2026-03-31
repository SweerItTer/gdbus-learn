#include <TrainingService.hpp>

#include <public/DbusConstants.hpp>

#include "training-generated.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace training::service {

namespace {

// 反序列化
public_api::TestInfo VariantToTestInfo(GVariant* value) {
    public_api::TestInfo info{};
    gboolean bool_param = FALSE;
    gint int_param = 0;
    gdouble double_param = 0.0;
    const gchar* string_param = "";

    g_variant_get(value, "(bids)", &bool_param, &int_param, &double_param, &string_param);

    info.bool_param = static_cast<bool>(bool_param);
    info.int_param = int_param;
    info.double_param = double_param;
    info.string_param = string_param != nullptr ? string_param : "";
    return info;
}

// 序列化
GVariant* TestInfoToVariant(const public_api::TestInfo& info) {
    return g_variant_new("(bids)",
                         static_cast<gboolean>(info.bool_param),
                         static_cast<gint>(info.int_param),
                         static_cast<gdouble>(info.double_param),
                         info.string_param.c_str());
}

}

TrainingService::TrainingService()
    : loop_(g_main_loop_new(nullptr, FALSE))
    , connection_(G_BUS_TYPE_SESSION)
    , skeleton_(training_skeleton_new()) {
    bus_name_owner_.Acquire(connection_.Get(), training::kBusName);

    // 注册回调
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

    // 在 connection 的 kObjectPath 上导出 interface()
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

    exported_interface_.Reset(G_DBUS_INTERFACE_SKELETON(skeleton_.get()));
}

TrainingService::~TrainingService() = default;

void TrainingService::Run() {
    std::cout << "server: waiting for calls on " << training::kBusName << std::endl;
    g_main_loop_run(loop_.get());
}

bool TrainingService::SetTestBool(bool param) {
    return state_.SetTestBool(param);
}

bool TrainingService::SetTestInt(int param) {
    return state_.SetTestInt(param);
}

bool TrainingService::SetTestDouble(double param) {
    return state_.SetTestDouble(param);
}

bool TrainingService::SetTestString(std::string param) {
    return state_.SetTestString(std::move(param));
}

bool TrainingService::SetTestInfo(public_api::TestInfo param) {
    return state_.SetTestInfo(std::move(param));
}

bool TrainingService::GetTestBool() {
    return state_.GetTestBool();
}

int TrainingService::GetTestInt() {
    return state_.GetTestInt();
}

double TrainingService::GetTestDouble() {
    return state_.GetTestDouble();
}

std::string TrainingService::GeTestString() {
    return state_.GetTestString();
}

public_api::TestInfo TrainingService::GetTestInfo() {
    return state_.GetTestInfo();
}

gboolean TrainingService::OnHandleSetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gboolean param,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestBool(static_cast<bool>(param));
    training_complete_set_test_bool(object, invocation, ok);
    training_emit_on_test_bool_changed(object, param);
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
    const auto info = VariantToTestInfo(param);
    const bool ok = self->SetTestInfo(info);
    training_complete_set_test_info(object, invocation, ok);
    training_emit_on_test_info_changed(object, TestInfoToVariant(info));
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
    training_complete_get_test_info(object, invocation, TestInfoToVariant(self->GetTestInfo()));
    return TRUE;
}

} // namespace training::service

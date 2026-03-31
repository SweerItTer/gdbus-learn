#include <training/TrainingLibrary.hpp>

#include <public/DbusConstants.hpp>

#include <stdexcept>
#include <string>

namespace training::library {

namespace detail {

thread_local std::string g_last_error;

void SetLastError(std::string message) {
    g_last_error = std::move(message);
}

void ClearLastError() {
    g_last_error.clear();
}

void DrainPendingSignals() {
    for (int i = 0; i < 8; ++i) {
        if (!g_main_context_iteration(nullptr, FALSE)) {
            break;
        }
    }
}

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

GVariant* TestInfoToVariant(const public_api::TestInfo& info) {
    return g_variant_new("(bids)",
                         static_cast<gboolean>(info.bool_param),
                         static_cast<gint>(info.int_param),
                         static_cast<gdouble>(info.double_param),
                         info.string_param.c_str());
}

public_api::TestInfo ViewToTestInfo(const TrainingInfoView* view) {
    public_api::TestInfo info{};
    if (view == nullptr) {
        return info;
    }

    info.bool_param = view->bool_param;
    info.int_param = view->int_param;
    info.double_param = view->double_param;
    info.string_param = view->string_param != nullptr ? view->string_param : "";
    return info;
}

template <typename Fn>
bool InvokeWithError(Fn&& fn) {
    try {
        ClearLastError();
        fn();
        return true;
    } catch (const std::exception& ex) {
        SetLastError(ex.what());
        return false;
    } catch (...) {
        SetLastError("unknown training library failure");
        return false;
    }
}

template <typename Fn>
auto CallWithError(Fn&& fn, const char* message) {
    GError* raw_error = nullptr;
    auto result = fn(&raw_error);
    utils::UniqueGError error(raw_error);

    if (error != nullptr) {
        throw std::runtime_error(std::string(message) + error->message);
    }

    return result;
}

} // namespace detail

TrainingLibraryClient::TrainingLibraryClient() {
    proxy_.reset(detail::CallWithError(
        [&](GError** error) {
            return training_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                   training::kBusName,
                                                   training::kObjectPath,
                                                   nullptr,
                                                   error);
        },
        "failed to create Training proxy: "));

    g_signal_connect(proxy_.get(),
                     "on-test-bool-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestBoolChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-int-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestIntChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-double-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestDoubleChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-string-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestStringChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-info-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestInfoChanged),
                     this);

    UpdateInfoView();
}

void TrainingLibraryClient::SetListener(const TrainingListenerCallbacks* callbacks) {
    listener_ = callbacks != nullptr ? *callbacks : TrainingListenerCallbacks{};
}

bool TrainingLibraryClient::SetTestBool(bool param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_bool_sync(proxy_.get(),
                                                    static_cast<gboolean>(param),
                                                    &result,
                                                    nullptr,
                                                    error);
        },
        "failed to call SetTestBool: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestInt(int param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_int_sync(proxy_.get(),
                                                   static_cast<gint>(param),
                                                   &result,
                                                   nullptr,
                                                   error);
        },
        "failed to call SetTestInt: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestDouble(double param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_double_sync(proxy_.get(),
                                                      static_cast<gdouble>(param),
                                                      &result,
                                                      nullptr,
                                                      error);
        },
        "failed to call SetTestDouble: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestString(const char* param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_string_sync(proxy_.get(),
                                                      param != nullptr ? param : "",
                                                      &result,
                                                      nullptr,
                                                      error);
        },
        "failed to call SetTestString: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestInfo(const TrainingInfoView* param) {
    gboolean result = FALSE;
    const auto info = detail::ViewToTestInfo(param);
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_info_sync(proxy_.get(),
                                                    detail::TestInfoToVariant(info),
                                                    &result,
                                                    nullptr,
                                                    error);
        },
        "failed to call SetTestInfo: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::GetTestBool(bool* result) {
    gboolean value = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_bool_sync(proxy_.get(),
                                                    &value,
                                                    nullptr,
                                                    error);
        },
        "failed to call GetTestBool: ");
    if (result != nullptr) {
        *result = static_cast<bool>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestInt(int* result) {
    gint value = 0;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_int_sync(proxy_.get(),
                                                   &value,
                                                   nullptr,
                                                   error);
        },
        "failed to call GetTestInt: ");
    if (result != nullptr) {
        *result = static_cast<int>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestDouble(double* result) {
    gdouble value = 0.0;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_double_sync(proxy_.get(),
                                                      &value,
                                                      nullptr,
                                                      error);
        },
        "failed to call GetTestDouble: ");
    if (result != nullptr) {
        *result = static_cast<double>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestString(const char** result) {
    gchar* raw_result = nullptr;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_string_sync(proxy_.get(),
                                                      &raw_result,
                                                      nullptr,
                                                      error);
        },
        "failed to call GetTestString: ");
    utils::UniqueGChar value(raw_result);
    last_string_result_ = value != nullptr ? value.get() : "";
    if (result != nullptr) {
        *result = last_string_result_.c_str();
    }
    return true;
}

bool TrainingLibraryClient::GetTestInfo(TrainingInfoView* result) {
    GVariant* raw_result = nullptr;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_info_sync(proxy_.get(),
                                                    &raw_result,
                                                    nullptr,
                                                    error);
        },
        "failed to call GetTestInfo: ");
    utils::UniqueGVariant value(raw_result);
    cached_info_ = detail::VariantToTestInfo(value.get());
    UpdateInfoView();
    if (result != nullptr) {
        *result = cached_info_view_;
    }
    return true;
}

void TrainingLibraryClient::NotifyTestBoolChanged(bool param) {
    cached_info_.bool_param = param;
    UpdateInfoView();
    if (listener_.on_test_bool_changed != nullptr) {
        listener_.on_test_bool_changed(listener_.user_data, param);
    }
}

void TrainingLibraryClient::NotifyTestIntChanged(int param) {
    cached_info_.int_param = param;
    UpdateInfoView();
    if (listener_.on_test_int_changed != nullptr) {
        listener_.on_test_int_changed(listener_.user_data, param);
    }
}

void TrainingLibraryClient::NotifyTestDoubleChanged(double param) {
    cached_info_.double_param = param;
    UpdateInfoView();
    if (listener_.on_test_double_changed != nullptr) {
        listener_.on_test_double_changed(listener_.user_data, param);
    }
}

void TrainingLibraryClient::NotifyTestStringChanged(const std::string& param) {
    cached_info_.string_param = param;
    UpdateInfoView();
    if (listener_.on_test_string_changed != nullptr) {
        listener_.on_test_string_changed(listener_.user_data, cached_info_.string_param.c_str());
    }
}

void TrainingLibraryClient::NotifyTestInfoChanged(const public_api::TestInfo& param) {
    cached_info_ = param;
    UpdateInfoView();
    if (listener_.on_test_info_changed != nullptr) {
        listener_.on_test_info_changed(listener_.user_data, &cached_info_view_);
    }
}

void TrainingLibraryClient::UpdateInfoView() {
    cached_info_view_.bool_param = cached_info_.bool_param;
    cached_info_view_.int_param = cached_info_.int_param;
    cached_info_view_.double_param = cached_info_.double_param;
    cached_info_view_.string_param = cached_info_.string_param.c_str();
}

void TrainingLibraryClient::OnRemoteTestBoolChanged(Training*, gboolean param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    self->NotifyTestBoolChanged(static_cast<bool>(param));
}

void TrainingLibraryClient::OnRemoteTestIntChanged(Training*, gint param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    self->NotifyTestIntChanged(static_cast<int>(param));
}

void TrainingLibraryClient::OnRemoteTestDoubleChanged(Training*, gdouble param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    self->NotifyTestDoubleChanged(static_cast<double>(param));
}

void TrainingLibraryClient::OnRemoteTestStringChanged(Training*, const gchar* param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    self->NotifyTestStringChanged(param != nullptr ? param : "");
}

void TrainingLibraryClient::OnRemoteTestInfoChanged(Training*, GVariant* param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    self->NotifyTestInfoChanged(detail::VariantToTestInfo(param));
}

} // namespace training::library

struct TrainingLibraryHandle {
    training::library::TrainingLibraryClient client;
};

extern "C" TrainingLibraryHandle* Training_Create() {
    TrainingLibraryHandle* handle = nullptr;
    training::library::detail::InvokeWithError([&]() {
        handle = new TrainingLibraryHandle{};
    });
    return handle;
}

extern "C" void Training_Destroy(TrainingLibraryHandle* handle) {
    delete handle;
}

extern "C" void Training_SetListener(TrainingLibraryHandle* handle, const TrainingListenerCallbacks* callbacks) {
    if (handle == nullptr) {
        return;
    }

    handle->client.SetListener(callbacks);
}

extern "C" bool Training_SetTestBool(TrainingLibraryHandle* handle, bool param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestBool(param)) {
            throw std::runtime_error("SetTestBool returned false");
        }
    });
}

extern "C" bool Training_SetTestInt(TrainingLibraryHandle* handle, int param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestInt(param)) {
            throw std::runtime_error("SetTestInt returned false");
        }
    });
}

extern "C" bool Training_SetTestDouble(TrainingLibraryHandle* handle, double param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestDouble(param)) {
            throw std::runtime_error("SetTestDouble returned false");
        }
    });
}

extern "C" bool Training_SetTestString(TrainingLibraryHandle* handle, const char* param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestString(param)) {
            throw std::runtime_error("SetTestString returned false");
        }
    });
}

extern "C" bool Training_SetTestInfo(TrainingLibraryHandle* handle, const TrainingInfoView* param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestInfo(param)) {
            throw std::runtime_error("SetTestInfo returned false");
        }
    });
}

extern "C" bool Training_GetTestBool(TrainingLibraryHandle* handle, bool* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestBool(result);
    });
}

extern "C" bool Training_GetTestInt(TrainingLibraryHandle* handle, int* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestInt(result);
    });
}

extern "C" bool Training_GetTestDouble(TrainingLibraryHandle* handle, double* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestDouble(result);
    });
}

extern "C" bool Training_GetTestString(TrainingLibraryHandle* handle, const char** result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestString(result);
    });
}

extern "C" bool Training_GetTestInfo(TrainingLibraryHandle* handle, TrainingInfoView* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestInfo(result);
    });
}

extern "C" const char* Training_GetLastError() {
    return training::library::detail::g_last_error.c_str();
}

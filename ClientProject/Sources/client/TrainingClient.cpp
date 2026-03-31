#include <client/TrainingClient.hpp>

#include <dlfcn.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace training::client {

namespace {

void* OpenTrainingLibrary() {
    void* handle = dlopen(TRAINING_LIBRARY_PATH, RTLD_NOW);
    if (handle == nullptr) {
        throw std::runtime_error(std::string("failed to load training library: ") + dlerror());
    }

    return handle;
}

template <typename Fn>
Fn ResolveSymbol(void* library_handle, const char* symbol_name) {
    dlerror();
    void* raw_symbol = dlsym(library_handle, symbol_name);
    const char* error = dlerror();
    if (error != nullptr) {
        throw std::runtime_error(std::string("failed to load symbol ") + symbol_name + ": " + error);
    }

    return reinterpret_cast<Fn>(raw_symbol);
}

} // namespace

TrainingClient::TrainingClient() {
    library_handle_ = OpenTrainingLibrary();
    api_ = LoadApi(library_handle_);
    handle_ = api_.create();
    if (handle_ == nullptr) {
        ThrowLastError("failed to create training handle: ");
    }

    RegisterListener();
}

TrainingClient::~TrainingClient() {
    if (handle_ != nullptr && api_.destroy != nullptr) {
        api_.destroy(handle_);
        handle_ = nullptr;
    }

    if (library_handle_ != nullptr) {
        dlclose(library_handle_);
        library_handle_ = nullptr;
    }
}

bool TrainingClient::SetTestBool(bool param) {
    if (!api_.set_test_bool(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestBool: ");
    }
    return true;
}

bool TrainingClient::SetTestInt(int param) {
    if (!api_.set_test_int(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestInt: ");
    }
    return true;
}

bool TrainingClient::SetTestDouble(double param) {
    if (!api_.set_test_double(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestDouble: ");
    }
    return true;
}

bool TrainingClient::SetTestString(std::string param) {
    if (!api_.set_test_string(handle_, param.c_str())) {
        ThrowLastError("failed to call Training_SetTestString: ");
    }
    return true;
}

bool TrainingClient::SetTestInfo(public_api::TestInfo param) {
    const auto view = ToInfoView(param);
    if (!api_.set_test_info(handle_, &view)) {
        ThrowLastError("failed to call Training_SetTestInfo: ");
    }
    return true;
}

bool TrainingClient::GetTestBool() {
    bool result = false;
    if (!api_.get_test_bool(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestBool: ");
    }
    return result;
}

int TrainingClient::GetTestInt() {
    int result = 0;
    if (!api_.get_test_int(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestInt: ");
    }
    return result;
}

double TrainingClient::GetTestDouble() {
    double result = 0.0;
    if (!api_.get_test_double(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestDouble: ");
    }
    return result;
}

std::string TrainingClient::GeTestString() {
    const char* result = nullptr;
    if (!api_.get_test_string(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestString: ");
    }
    return result != nullptr ? result : "";
}

public_api::TestInfo TrainingClient::GetTestInfo() {
    TrainingInfoView result{};
    if (!api_.get_test_info(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestInfo: ");
    }
    return ToTestInfo(result);
}

void TrainingClient::OnTestBoolChanged(bool param) {
    cached_info_.bool_param = param;
    std::cout << "[Listener] OnTestBoolChanged: " << std::boolalpha << param << std::noboolalpha << std::endl;
}

void TrainingClient::OnTestIntChanged(int param) {
    cached_info_.int_param = param;
    std::cout << "[Listener] OnTestIntChanged: " << param << std::endl;
}

void TrainingClient::OnTestDoubleChanged(double param) {
    cached_info_.double_param = param;
    std::cout << "[Listener] OnTestDoubleChanged: " << param << std::endl;
}

void TrainingClient::OnTestStringChanged(std::string param) {
    cached_info_.string_param = std::move(param);
    std::cout << "[Listener] OnTestStringChanged: " << cached_info_.string_param << std::endl;
}

void TrainingClient::OnTestInfoChanged(public_api::TestInfo param) {
    cached_info_ = std::move(param);
    std::cout << "[Listener] OnTestInfoChanged: { bool=" << std::boolalpha << cached_info_.bool_param
              << ", int=" << cached_info_.int_param
              << ", double=" << cached_info_.double_param
              << ", string=\"" << cached_info_.string_param << "\" }"
              << std::noboolalpha << std::endl;
}

void TrainingClient::OnRemoteTestBoolChanged(void* user_data, bool param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestBoolChanged(param);
}

void TrainingClient::OnRemoteTestIntChanged(void* user_data, int param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestIntChanged(param);
}

void TrainingClient::OnRemoteTestDoubleChanged(void* user_data, double param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestDoubleChanged(param);
}

void TrainingClient::OnRemoteTestStringChanged(void* user_data, const char* param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestStringChanged(param != nullptr ? param : "");
}

void TrainingClient::OnRemoteTestInfoChanged(void* user_data, const TrainingInfoView* param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestInfoChanged(param != nullptr ? ToTestInfo(*param) : public_api::TestInfo{});
}

TrainingClient::Api TrainingClient::LoadApi(void* library_handle) {
    Api api{};
    api.create = ResolveSymbol<TrainingCreateFn>(library_handle, "Training_Create");
    api.destroy = ResolveSymbol<TrainingDestroyFn>(library_handle, "Training_Destroy");
    api.set_listener = ResolveSymbol<TrainingSetListenerFn>(library_handle, "Training_SetListener");
    api.set_test_bool = ResolveSymbol<TrainingSetTestBoolFn>(library_handle, "Training_SetTestBool");
    api.set_test_int = ResolveSymbol<TrainingSetTestIntFn>(library_handle, "Training_SetTestInt");
    api.set_test_double = ResolveSymbol<TrainingSetTestDoubleFn>(library_handle, "Training_SetTestDouble");
    api.set_test_string = ResolveSymbol<TrainingSetTestStringFn>(library_handle, "Training_SetTestString");
    api.set_test_info = ResolveSymbol<TrainingSetTestInfoFn>(library_handle, "Training_SetTestInfo");
    api.get_test_bool = ResolveSymbol<TrainingGetTestBoolFn>(library_handle, "Training_GetTestBool");
    api.get_test_int = ResolveSymbol<TrainingGetTestIntFn>(library_handle, "Training_GetTestInt");
    api.get_test_double = ResolveSymbol<TrainingGetTestDoubleFn>(library_handle, "Training_GetTestDouble");
    api.get_test_string = ResolveSymbol<TrainingGetTestStringFn>(library_handle, "Training_GetTestString");
    api.get_test_info = ResolveSymbol<TrainingGetTestInfoFn>(library_handle, "Training_GetTestInfo");
    api.get_last_error = ResolveSymbol<TrainingGetLastErrorFn>(library_handle, "Training_GetLastError");
    return api;
}

[[noreturn]] void TrainingClient::ThrowLastError(const char* operation) const {
    const char* message = api_.get_last_error != nullptr ? api_.get_last_error() : nullptr;
    throw std::runtime_error(std::string(operation) + (message != nullptr ? message : "unknown training library error"));
}

void TrainingClient::RegisterListener() {
    TrainingListenerCallbacks callbacks{};
    callbacks.user_data = this;
    callbacks.on_test_bool_changed = &TrainingClient::OnRemoteTestBoolChanged;
    callbacks.on_test_int_changed = &TrainingClient::OnRemoteTestIntChanged;
    callbacks.on_test_double_changed = &TrainingClient::OnRemoteTestDoubleChanged;
    callbacks.on_test_string_changed = &TrainingClient::OnRemoteTestStringChanged;
    callbacks.on_test_info_changed = &TrainingClient::OnRemoteTestInfoChanged;
    api_.set_listener(handle_, &callbacks);
}

public_api::TestInfo TrainingClient::ToTestInfo(const TrainingInfoView& info) {
    return public_api::TestInfo{
        info.bool_param,
        info.int_param,
        info.double_param,
        info.string_param != nullptr ? info.string_param : ""};
}

TrainingInfoView TrainingClient::ToInfoView(const public_api::TestInfo& info) {
    return TrainingInfoView{
        info.bool_param,
        info.int_param,
        info.double_param,
        info.string_param.c_str()};
}

} // namespace training::client

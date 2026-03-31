#include <client/TrainingClient.hpp>

#include <dlfcn.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace training::client {

namespace {

// 依据cmake生成位置宏：TRAINING_LIBRARY_PATH
void* OpenTrainingLibrary() {
    void* handle = dlopen(TRAINING_LIBRARY_PATH, RTLD_NOW);
    if (handle == nullptr) {
        throw std::runtime_error(std::string("failed to load training library: ") + dlerror());
    }

    return handle;
}

// 获取到具体符号，并以具体函数类型指针返回
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
    // 获取动态库句柄
    library_handle_ = OpenTrainingLibrary();
    // 获取所有方法
    api_ = LoadApi(library_handle_);
    // 创建
    handle_ = api_.create();
    if (handle_ == nullptr) {
        ThrowLastError("failed to create training handle: ");
    }
    // 注册信号触发回调
    RegisterListener();
    // 后台持续处理广播，避免菜单阻塞时收不到信号
    StartEventPump();
}

TrainingClient::~TrainingClient() {
    StopEventPump();
    // 释放动态库下的资源
    if (handle_ != nullptr && api_.destroy != nullptr) {
        api_.destroy(handle_);
        handle_ = nullptr;
    }
    // 析构动态库句柄
    if (library_handle_ != nullptr) {
        dlclose(library_handle_);
        library_handle_ = nullptr;
    }
}

/* ---------------------------------------
 *              调用动态库方法  
 * --------------------------------------- */
bool TrainingClient::SetTestBool(bool param) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.set_test_bool(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestBool: ");
    }
    return true;
}

bool TrainingClient::SetTestInt(int param) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.set_test_int(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestInt: ");
    }
    return true;
}

bool TrainingClient::SetTestDouble(double param) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.set_test_double(handle_, param)) {
        ThrowLastError("failed to call Training_SetTestDouble: ");
    }
    return true;
}

bool TrainingClient::SetTestString(std::string param) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.set_test_string(handle_, param.c_str())) {
        ThrowLastError("failed to call Training_SetTestString: ");
    }
    return true;
}

bool TrainingClient::SetTestInfo(public_api::TestInfo param) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.set_test_info(handle_, &param)) {
        ThrowLastError("failed to call Training_SetTestInfo: ");
    }
    return true;
}

bool TrainingClient::GetTestBool() {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    bool result = false;
    if (!api_.get_test_bool(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestBool: ");
    }
    return result;
}

int TrainingClient::GetTestInt() {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    int result = 0;
    if (!api_.get_test_int(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestInt: ");
    }
    return result;
}

double TrainingClient::GetTestDouble() {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    double result = 0.0;
    if (!api_.get_test_double(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestDouble: ");
    }
    return result;
}

std::string TrainingClient::GeTestString() {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    const char* result = nullptr;
    if (!api_.get_test_string(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestString: ");
    }
    return result != nullptr ? result : "";
}

public_api::TestInfo TrainingClient::GetTestInfo() {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    public_api::TestInfo result{};
    if (!api_.get_test_info(handle_, &result)) {
        ThrowLastError("failed to call Training_GetTestInfo: ");
    }
    return result;
}

bool TrainingClient::SendFile(unsigned char* file_buf, size_t file_size) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.send_file_buffer(handle_,
                               file_buf,
                               static_cast<unsigned long long>(file_size),
                               "upload_buffer.bin")) {
        ThrowLastError("failed to call Training_SendFileBuffer: ");
    }
    return true;
}

bool TrainingClient::SendFileByPath(const std::string& file_path) {
    std::lock_guard<std::recursive_mutex> lock(api_mutex_);
    if (!api_.send_file_path(handle_, file_path.c_str())) {
        ThrowLastError("failed to call Training_SendFilePath: ");
    }
    return true;
}

/* ---------------------------------------
 *       信号触发后回调获取最新数据  
 * --------------------------------------- */
void TrainingClient::OnTestBoolChanged(bool param) {
    (void)param;
    // 信号只表示“数据发生变化”，缓存内容以最新 Get 结果为准
    cached_info_.bool_param = GetTestBool();
    std::cout << "[Listener] OnTestBoolChanged: " << std::boolalpha << cached_info_.bool_param << std::noboolalpha
              << std::endl;
}

void TrainingClient::OnTestIntChanged(int param) {
    (void)param;
    // 信号只表示“数据发生变化”，缓存内容以最新 Get 结果为准
    cached_info_.int_param = GetTestInt();
    std::cout << "[Listener] OnTestIntChanged: " << cached_info_.int_param << std::endl;
}

void TrainingClient::OnTestDoubleChanged(double param) {
    (void)param;
    // 信号只表示“数据发生变化”，缓存内容以最新 Get 结果为准
    cached_info_.double_param = GetTestDouble();
    std::cout << "[Listener] OnTestDoubleChanged: " << cached_info_.double_param << std::endl;
}

void TrainingClient::OnTestStringChanged(std::string param) {
    (void)param;
    // 信号只表示“数据发生变化”，缓存内容以最新 Get 结果为准
    cached_info_.string_param = GeTestString();
    std::cout << "[Listener] OnTestStringChanged: " << cached_info_.string_param << std::endl;
}

void TrainingClient::OnTestInfoChanged(public_api::TestInfo param) {
    (void)param;
    // 信号只表示“数据发生变化”，缓存内容以最新 Get 结果为准
    cached_info_ = GetTestInfo();
    std::cout << "[Listener] OnTestInfoChanged: { bool=" << std::boolalpha << cached_info_.bool_param
              << ", int=" << cached_info_.int_param
              << ", double=" << cached_info_.double_param
              << ", string=\"" << cached_info_.string_param << "\" }"
              << std::noboolalpha << std::endl;
}

/* ---------------------------------------
 *       信号转发(实际调用self下方法)  
 * --------------------------------------- */
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

void TrainingClient::OnRemoteTestInfoChanged(void* user_data, const public_api::TestInfo* param) {
    auto* self = static_cast<TrainingClient*>(user_data);
    self->OnTestInfoChanged(param != nullptr ? *param : public_api::TestInfo{});
}

TrainingClient::Api TrainingClient::LoadApi(void* library_handle) {
    // 缓存所有方法(函数指针)到结构体
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
    api.send_file_buffer = ResolveSymbol<TrainingSendFileBufferFn>(library_handle, "Training_SendFileBuffer");
    api.send_file_path = ResolveSymbol<TrainingSendFilePathFn>(library_handle, "Training_SendFilePath");
    api.get_last_error = ResolveSymbol<TrainingGetLastErrorFn>(library_handle, "Training_GetLastError");
    api.pump_events = ResolveSymbol<TrainingPumpEventsFn>(library_handle, "Training_PumpEvents");
    return api;
}

[[noreturn]] void TrainingClient::ThrowLastError(const char* operation) const {
    const char* message = api_.get_last_error != nullptr ? api_.get_last_error() : nullptr;
    throw std::runtime_error(std::string(operation) + (message != nullptr ? message : "unknown training library error"));
}

// 在此注册信号触发的回调(更新数据)
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

void TrainingClient::StartEventPump() {
    stop_event_pump_.store(false);
    event_pump_thread_ = std::thread([this]() {
        while (!stop_event_pump_.load()) {
            {
                std::lock_guard<std::recursive_mutex> lock(api_mutex_);
                if (handle_ != nullptr && api_.pump_events != nullptr) {
                    api_.pump_events(handle_);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
}

void TrainingClient::StopEventPump() {
    stop_event_pump_.store(true);
    if (event_pump_thread_.joinable()) {
        event_pump_thread_.join();
    }
}
} // namespace training::client

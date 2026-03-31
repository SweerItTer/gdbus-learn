#pragma once

#include <public/ITestListener.hpp>
#include <public/ITestService.hpp>
#include <public/TrainingLibraryApi.hpp>

#include <atomic>
#include <mutex>
#include <thread>

namespace training::client {

// 重写方法(调用动态库方法，绕过proxy创建)
class TrainingClient : public public_api::ITestService, public public_api::ITestListener {
public:
    struct Api {
        TrainingCreateFn create{nullptr};
        TrainingDestroyFn destroy{nullptr};
        TrainingSetListenerFn set_listener{nullptr};
        TrainingSetTestBoolFn set_test_bool{nullptr};
        TrainingSetTestIntFn set_test_int{nullptr};
        TrainingSetTestDoubleFn set_test_double{nullptr};
        TrainingSetTestStringFn set_test_string{nullptr};
        TrainingSetTestInfoFn set_test_info{nullptr};
        TrainingGetTestBoolFn get_test_bool{nullptr};
        TrainingGetTestIntFn get_test_int{nullptr};
        TrainingGetTestDoubleFn get_test_double{nullptr};
        TrainingGetTestStringFn get_test_string{nullptr};
        TrainingGetTestInfoFn get_test_info{nullptr};
        TrainingSendFileBufferFn send_file_buffer{nullptr};
        TrainingSendFilePathFn send_file_path{nullptr};
        TrainingGetLastErrorFn get_last_error{nullptr};
        TrainingPumpEventsFn pump_events{nullptr};
    };

    TrainingClient();
    ~TrainingClient() override;

    TrainingClient(const TrainingClient&) = delete;
    TrainingClient& operator=(const TrainingClient&) = delete;

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
    bool SendFileByPath(const std::string& file_path);

    void OnTestBoolChanged(bool param) override;
    void OnTestIntChanged(int param) override;
    void OnTestDoubleChanged(double param) override;
    void OnTestStringChanged(std::string param) override;
    void OnTestInfoChanged(public_api::TestInfo param) override;

protected:
    // 远端广播到达后的可重写入口，便于后续 UI 子类接管
    virtual void OnRemoteTestBoolChanged(bool param);
    virtual void OnRemoteTestIntChanged(int param);
    virtual void OnRemoteTestDoubleChanged(double param);
    virtual void OnRemoteTestStringChanged(const std::string& param);
    virtual void OnRemoteTestInfoChanged(const public_api::TestInfo& param);

private:
    // 静态函数只负责把 C 风格回调转发到具体对象
    static void DispatchRemoteTestBoolChanged(void* user_data, bool param);
    static void DispatchRemoteTestIntChanged(void* user_data, int param);
    static void DispatchRemoteTestDoubleChanged(void* user_data, double param);
    static void DispatchRemoteTestStringChanged(void* user_data, const char* param);
    static void DispatchRemoteTestInfoChanged(void* user_data, const public_api::TestInfo* param);

    static Api LoadApi(void* library_handle);
    void RegisterListener();
    void StartEventPump();
    void StopEventPump();
    [[noreturn]] void ThrowLastError(const char* operation) const;

    void* library_handle_{nullptr};
    Api api_{};
    TrainingLibraryHandle* handle_{nullptr};
    public_api::TestInfo cached_info_{};
    std::recursive_mutex api_mutex_;
    std::atomic<bool> stop_event_pump_{false};
    std::thread event_pump_thread_;
};

} // namespace training::client

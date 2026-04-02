#include <client/TrainingClient.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// 继承覆盖验证
// ---------------------------------------------------------------------------
class DerivedTrainingClient : public training::client::TrainingClient {
public:
    int remote_int_count = 0;
    int ui_int_count = 0;

protected:
    // 远端变化先走一层可继承回调。
    void OnRemoteTestIntChanged(int param) override {
        ++remote_int_count;
        TrainingClient::OnRemoteTestIntChanged(param);
    }

public:
    // UI 层回调也要支持重写。
    void OnTestIntChanged(int param) override {
        ++ui_int_count;
        TrainingClient::OnTestIntChanged(param);
    }
};

// ---------------------------------------------------------------------------
// 冒烟入口
// ---------------------------------------------------------------------------
int main() {
    try {
        DerivedTrainingClient client;
        // 触发一次广播，等待监听线程回调。
        client.SetTestInt(77);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        if (client.remote_int_count <= 0 || client.ui_int_count <= 0) {
            std::cerr << "derived client override was not triggered" << std::endl;
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "inheritable_client_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

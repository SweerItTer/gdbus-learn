#include <client/TrainingClient.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

class DerivedTrainingClient : public training::client::TrainingClient {
public:
    int remote_int_count = 0;
    int ui_int_count = 0;

protected:
    void OnRemoteTestIntChanged(int param) override {
        ++remote_int_count;
        TrainingClient::OnRemoteTestIntChanged(param);
    }

public:
    void OnTestIntChanged(int param) override {
        ++ui_int_count;
        TrainingClient::OnTestIntChanged(param);
    }
};

int main() {
    try {
        DerivedTrainingClient client;
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

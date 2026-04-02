#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>

// ---------------------------------------------------------------------------
// 基础读写冒烟
// ---------------------------------------------------------------------------
int main() {
    try {
        training::client::TrainingClient client;
        // 先写后读，验证最基本的代理链路。
        client.SetTestInt(42);

        const int result = client.GetTestInt();
        if (result != 42) {
            std::cerr << "unexpected GetTestInt result: " << result << std::endl;
            return 1;
        }

        std::cout << "GetTestInt: " << result << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "client_wrapper_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

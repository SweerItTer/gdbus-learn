#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        training::client::TrainingClient client;
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

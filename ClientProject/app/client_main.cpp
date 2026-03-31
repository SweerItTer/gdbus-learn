#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>

int main() {
    try {
        training::client::TrainingClient client;
        client.SetTestInt(42);

        const int result = client.GetTestInt();
        std::cout << "GetTestInt: " << result << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "training_client error: " << ex.what() << std::endl;
        return 1;
    }
}

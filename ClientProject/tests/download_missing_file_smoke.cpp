#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>
#include <string>

int main() {
    try {
        training::client::TrainingClient client;
        client.DownloadFile("missing/not_found.bin", "download_missing_file_smoke.bin");
        std::cerr << "DownloadFile unexpectedly succeeded" << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        const std::string message = ex.what();
        if (message.find("not exist") == std::string::npos &&
            message.find("not found") == std::string::npos) {
            std::cerr << "unexpected error message: " << message << std::endl;
            return 1;
        }
        return 0;
    }
}

#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 2 && argc != 3) {
            std::cerr << "usage: send_file_once <file_path> [remote_relative_path]" << std::endl;
            return 1;
        }

        training::client::TrainingClient client;
        if (!client.SendFileByPath(argv[1], argc == 3 ? argv[2] : "")) {
            std::cerr << "SendFileByPath returned false" << std::endl;
            return 1;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "send_file_once error: " << ex.what() << std::endl;
        return 1;
    }
}

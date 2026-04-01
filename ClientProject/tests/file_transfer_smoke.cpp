#include <client/TrainingClient.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    try {
        training::client::TrainingClient client;

        const fs::path source = fs::temp_directory_path() / "training_file_transfer_source.bin";
        const fs::path expected_target = fs::path(TRAINING_SERVER_DIR) / "file" / "test" / "nested" / source.filename();
        const std::string payload(2500, 'x');

        {
            std::ofstream output(source, std::ios::binary);
            output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }

        fs::remove(expected_target);

        if (!client.SendFileByPath(source.string(), "./test/nested/" + source.filename().string())) {
            std::cerr << "SendFileByPath returned false" << std::endl;
            return 1;
        }

        if (!fs::exists(expected_target)) {
            std::cerr << "target file does not exist: " << expected_target << std::endl;
            return 1;
        }

        const std::string actual = ReadFile(expected_target);
        if (actual != payload) {
            std::cerr << "target file payload mismatch" << std::endl;
            return 1;
        }

        fs::remove(source);
        fs::remove(expected_target);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "file_transfer_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

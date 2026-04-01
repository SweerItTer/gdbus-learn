#include <client/TrainingClient.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open file for overwrite smoke");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
        throw std::runtime_error("failed to write file for overwrite smoke");
    }
}

} // namespace

int main() {
    try {
        training::client::TrainingClient client;

        const fs::path server_root = fs::path(TRAINING_SERVER_DIR) / "file";
        const fs::path remote_path = server_root / "downloads" / "overwrite.bin";
        const fs::path local_target = fs::temp_directory_path() / "download_overwrite_smoke.bin";
        const std::string remote_payload(2048, 'n');
        const std::string local_payload = "old-local-content";

        fs::create_directories(remote_path.parent_path());
        WriteFile(remote_path, remote_payload);
        // 先放一个旧文件，验证下载成功后最终内容来自服务端而不是残留旧内容。
        WriteFile(local_target, local_payload);

        if (!client.DownloadFile("downloads/overwrite.bin", local_target.string())) {
            std::cerr << "DownloadFile returned false" << std::endl;
            return 1;
        }

        if (ReadFile(local_target) != remote_payload) {
            std::cerr << "download overwrite target payload mismatch" << std::endl;
            return 1;
        }

        fs::remove(local_target);
        fs::remove(remote_path);
        fs::remove_all(server_root / "downloads");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "download_overwrite_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

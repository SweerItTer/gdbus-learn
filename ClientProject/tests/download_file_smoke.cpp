#include <client/TrainingClient.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// 文件辅助
// ---------------------------------------------------------------------------
namespace {

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

} // namespace

// ---------------------------------------------------------------------------
// 下载冒烟
// ---------------------------------------------------------------------------
int main() {
    try {
        training::client::TrainingClient client;

        const fs::path server_root = fs::path(TRAINING_SERVER_DIR) / "file";
        const fs::path remote_path = server_root / "downloads" / "sample.bin";
        const fs::path local_target = fs::temp_directory_path() / "download_file_smoke.bin";
        const std::string payload(3072, 'd');

        // 先在服务端目录预置待下载文件。
        fs::create_directories(remote_path.parent_path());
        {
            std::ofstream output(remote_path, std::ios::binary | std::ios::trunc);
            output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }

        fs::remove(local_target);

        // 客户端按相对路径发起下载。
        if (!client.DownloadFile("./downloads/sample.bin", local_target.string())) {
            std::cerr << "DownloadFile returned false" << std::endl;
            return 1;
        }

        // 校验本地目标文件。
        if (!fs::exists(local_target)) {
            std::cerr << "download target file does not exist: " << local_target << std::endl;
            return 1;
        }

        if (ReadFile(local_target) != payload) {
            std::cerr << "download target file payload mismatch" << std::endl;
            return 1;
        }

        // 清理测试痕迹。
        fs::remove(local_target);
        fs::remove(remote_path);
        fs::remove_all(server_root / "downloads");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "download_file_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

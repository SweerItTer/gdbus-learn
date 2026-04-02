#include <client/TrainingClient.hpp>

#include <exception>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// 缺失文件下载
// ---------------------------------------------------------------------------
int main() {
    try {
        training::client::TrainingClient client;
        // 缺失路径必须抛错，不能静默成功。
        client.DownloadFile("missing/not_found.bin", "download_missing_file_smoke.bin");
        std::cerr << "DownloadFile unexpectedly succeeded" << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        const std::string message = ex.what();
        // 错误消息要能体现“文件不存在”。
        if (message.find("not exist") == std::string::npos &&
            message.find("not found") == std::string::npos) {
            std::cerr << "unexpected error message: " << message << std::endl;
            return 1;
        }
        return 0;
    }
}

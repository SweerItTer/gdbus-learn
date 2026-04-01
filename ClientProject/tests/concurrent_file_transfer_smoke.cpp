#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

int RunSendFile(const fs::path& file_path) {
    const std::string command =
        std::string("\"") + SEND_FILE_ONCE_PATH + "\" \"" + file_path.string() + "\"";
    return std::system(command.c_str());
}

int RunSendFile(const fs::path& file_path, const std::string& remote_path) {
    const std::string command =
        std::string("\"") + SEND_FILE_ONCE_PATH + "\" \"" + file_path.string() + "\" \"" + remote_path + "\"";
    return std::system(command.c_str());
}

} // namespace

int main() {
    try {
        const fs::path server_dir = fs::path(TRAINING_SERVER_DIR) / "file";

        // 不同目标路径并发发送：都应该成功
        const fs::path source_a = fs::temp_directory_path() / "concurrent_a.bin";
        const fs::path source_b = fs::temp_directory_path() / "concurrent_b.bin";
        const std::string payload_a(4096, 'A');
        const std::string payload_b(4096, 'B');

        {
            std::ofstream out_a(source_a, std::ios::binary);
            out_a.write(payload_a.data(), static_cast<std::streamsize>(payload_a.size()));
            std::ofstream out_b(source_b, std::ios::binary);
            out_b.write(payload_b.data(), static_cast<std::streamsize>(payload_b.size()));
        }

        fs::remove(server_dir / "parallel" / "a" / source_a.filename());
        fs::remove(server_dir / "parallel" / "b" / source_b.filename());

        auto future_a = std::async(std::launch::async, [&]() {
            return RunSendFile(source_a, "parallel/a/concurrent_a.bin");
        });
        auto future_b = std::async(std::launch::async, [&]() {
            return RunSendFile(source_b, "/parallel/b/concurrent_b.bin");
        });
        const int status_a = future_a.get();
        const int status_b = future_b.get();

        if (status_a != 0 || status_b != 0) {
            std::cerr << "concurrent different-name transfer failed" << std::endl;
            return 1;
        }

        if (ReadFile(server_dir / "parallel" / "a" / source_a.filename()) != payload_a ||
            ReadFile(server_dir / "parallel" / "b" / source_b.filename()) != payload_b) {
            std::cerr << "concurrent different-name target mismatch" << std::endl;
            return 1;
        }

        // 不同目录下同名文件：都应该成功
        const fs::path same_ok_dir_a = fs::temp_directory_path() / "same_ok_a";
        const fs::path same_ok_dir_b = fs::temp_directory_path() / "same_ok_b";
        fs::create_directories(same_ok_dir_a);
        fs::create_directories(same_ok_dir_b);
        const fs::path same_ok_a = same_ok_dir_a / "same_ok.bin";
        const fs::path same_ok_b = same_ok_dir_b / "same_ok.bin";
        const std::string same_ok_payload(4096, 'S');

        {
            std::ofstream out_a(same_ok_a, std::ios::binary);
            out_a.write(same_ok_payload.data(), static_cast<std::streamsize>(same_ok_payload.size()));
            std::ofstream out_b(same_ok_b, std::ios::binary);
            out_b.write(same_ok_payload.data(), static_cast<std::streamsize>(same_ok_payload.size()));
        }

        fs::remove(server_dir / "alpha" / "same_ok.bin");
        fs::remove(server_dir / "beta" / "same_ok.bin");

        auto future_same_ok_a = std::async(std::launch::async, [&]() {
            return RunSendFile(same_ok_a, "alpha/same_ok.bin");
        });
        auto future_same_ok_b = std::async(std::launch::async, [&]() {
            return RunSendFile(same_ok_b, "./beta/same_ok.bin");
        });
        const int same_ok_status_a = future_same_ok_a.get();
        const int same_ok_status_b = future_same_ok_b.get();

        if (same_ok_status_a != 0 || same_ok_status_b != 0) {
            std::cerr << "same-name different-directory transfers should both succeed" << std::endl;
            return 1;
        }

        if (!fs::exists(server_dir / "alpha" / "same_ok.bin") ||
            !fs::exists(server_dir / "beta" / "same_ok.bin") ||
            ReadFile(server_dir / "alpha" / "same_ok.bin") != same_ok_payload ||
            ReadFile(server_dir / "beta" / "same_ok.bin") != same_ok_payload) {
            std::cerr << "same-name different-directory final file mismatch" << std::endl;
            return 1;
        }

        // 同一路径并发发送：后来者直接拒绝，最终只保留一个结果文件
        const fs::path dir_a = fs::temp_directory_path() / "same_name_a";
        const fs::path dir_b = fs::temp_directory_path() / "same_name_b";
        fs::create_directories(dir_a);
        fs::create_directories(dir_b);
        const fs::path same_a = dir_a / "same_name.bin";
        const fs::path same_b = dir_b / "same_name.bin";
        const std::string same_payload_a(4096, 'X');
        const std::string same_payload_b(4096, 'Y');

        {
            std::ofstream out_a(same_a, std::ios::binary);
            out_a.write(same_payload_a.data(), static_cast<std::streamsize>(same_payload_a.size()));
            std::ofstream out_b(same_b, std::ios::binary);
            out_b.write(same_payload_b.data(), static_cast<std::streamsize>(same_payload_b.size()));
        }

        fs::remove(server_dir / "same" / "same_name.bin");

        auto future_same_a = std::async(std::launch::async, [&]() {
            return RunSendFile(same_a, "same/same_name.bin");
        });
        auto future_same_b = std::async(std::launch::async, [&]() {
            return RunSendFile(same_b, "/same/same_name.bin");
        });
        const int same_status_a = future_same_a.get();
        const int same_status_b = future_same_b.get();

        if (!((same_status_a == 0 && same_status_b != 0) ||
              (same_status_a != 0 && same_status_b == 0))) {
            std::cerr << "same-name different-md5 should keep exactly one successful transfer" << std::endl;
            return 1;
        }

        if (!fs::exists(server_dir / "same" / "same_name.bin")) {
            std::cerr << "same-name different-md5 should leave one final file" << std::endl;
            return 1;
        }

        fs::remove(source_a);
        fs::remove(source_b);
        fs::remove(server_dir / "parallel" / "a" / source_a.filename());
        fs::remove(server_dir / "parallel" / "b" / source_b.filename());
        fs::remove(server_dir / "alpha" / "same_ok.bin");
        fs::remove(server_dir / "beta" / "same_ok.bin");
        fs::remove(server_dir / "same" / "same_name.bin");
        fs::remove_all(server_dir / "parallel");
        fs::remove_all(server_dir / "alpha");
        fs::remove_all(server_dir / "beta");
        fs::remove_all(server_dir / "same");
        fs::remove_all(dir_a);
        fs::remove_all(dir_b);
        fs::remove_all(same_ok_dir_a);
        fs::remove_all(same_ok_dir_b);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "concurrent_file_transfer_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

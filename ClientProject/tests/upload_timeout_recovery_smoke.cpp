#include <public/TrainingLibraryApi.hpp>
#include <public/DbusConstants.hpp>
#include <utils/DbusRuntime.hpp>
#include <utils/FileTransferUtils.hpp>
#include <utils/GLibWrappers.hpp>

#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
namespace utils = training::utils;

namespace {

template <typename Fn>
Fn LoadSymbol(void* handle, const char* name) {
    dlerror();
    void* symbol = dlsym(handle, name);
    const char* error = dlerror();
    if (error != nullptr) {
        throw std::runtime_error(std::string("failed to load symbol ") + name + ": " + error);
    }
    return reinterpret_cast<Fn>(symbol);
}

std::string CreateId(const char* prefix) {
    return std::string(prefix) + "_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open local source file");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
        throw std::runtime_error("failed to write local source file");
    }
}

} // namespace

int main() {
    try {
        const std::string payload(utils::kFileChunkSize * 2, 'p');
        const std::string transfer_id = CreateId("partial_upload");
        const std::string shm_name = "/" + CreateId("partial_upload_shm");
        const fs::path local_source = fs::temp_directory_path() / "upload_timeout_recovery_source.bin";
        const fs::path server_target = fs::path(TRAINING_SERVER_DIR) / "file" / "timeouts" / "recover.bin";

        WriteFile(local_source, payload);
        fs::remove(server_target);
        fs::remove_all(server_target.parent_path());

        GError* raw_error = nullptr;
        GDBusConnection* raw_connection = g_bus_get_sync(utils::kBusType, nullptr, &raw_error);
        utils::UniqueGObject<GDBusConnection> connection(raw_connection);
        utils::UniqueGError proxy_error(raw_error);
        if (proxy_error != nullptr) {
            throw std::runtime_error(std::string("failed to create D-Bus connection: ") + proxy_error->message);
        }

        auto shm_fd = utils::OpenSharedMemory(shm_name, O_CREAT | O_RDWR | O_TRUNC);
        utils::ResizeSharedMemory(shm_fd.Get(), utils::kFileChunkSize);
        auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ | PROT_WRITE);
        std::memset(mapped.Get(), 0, utils::kFileChunkSize);
        std::memcpy(mapped.Get(), payload.data(), utils::kFileChunkSize);

        // 先只发第一片，故意制造一个“客户端异常退出，服务端保留活跃上传状态”的现场。
        const std::string md5 = utils::ComputeMd5(local_source);
        raw_error = nullptr;
        GVariant* raw_reply = g_dbus_connection_call_sync(connection.get(),
                                                          training::kBusName,
                                                          training::kObjectPath,
                                                          training::kInterfaceName,
                                                          "SendFileChunk",
                                                          g_variant_new("(ssstuuuss)",
                                                                        transfer_id.c_str(),
                                                                        "recover.bin",
                                                                        "timeouts/recover.bin",
                                                                        static_cast<guint64>(payload.size()),
                                                                        0U,
                                                                        2U,
                                                                        static_cast<guint>(utils::kFileChunkSize),
                                                                        md5.c_str(),
                                                                        shm_name.c_str()),
                                                          G_VARIANT_TYPE("(b)"),
                                                          G_DBUS_CALL_FLAGS_NONE,
                                                          -1,
                                                          nullptr,
                                                          &raw_error);
        utils::UniqueGVariant reply(raw_reply);
        utils::UniqueGError send_call_error(raw_error);
        utils::UnlinkSharedMemory(shm_name);
        gboolean send_result = FALSE;
        if (reply != nullptr) {
            g_variant_get(reply.get(), "(b)", &send_result);
        }
        if (send_call_error != nullptr || !send_result) {
            throw std::runtime_error("failed to send the first partial upload chunk");
        }

        // 服务端测试进程把超时配置成 200ms，这里等 500ms 足够覆盖自动回收窗口。
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        void* library_handle = dlopen(TRAINING_LIBRARY_PATH, RTLD_NOW);
        if (library_handle == nullptr) {
            throw std::runtime_error(std::string("failed to load training library: ") + dlerror());
        }

        const auto create_fn = LoadSymbol<TrainingCreateFn>(library_handle, "Training_Create");
        const auto destroy_fn = LoadSymbol<TrainingDestroyFn>(library_handle, "Training_Destroy");
        const auto send_file_path_fn = LoadSymbol<TrainingSendFilePathFn>(library_handle, "Training_SendFilePath");
        const auto get_last_error_fn = LoadSymbol<TrainingGetLastErrorFn>(library_handle, "Training_GetLastError");

        TrainingLibraryHandle* handle = create_fn();
        if (handle == nullptr) {
            dlclose(library_handle);
            throw std::runtime_error("failed to create training handle");
        }

        const bool upload_ok = send_file_path_fn(handle, local_source.string().c_str(), "timeouts/recover.bin");
        if (!upload_ok) {
            const char* error = get_last_error_fn();
            destroy_fn(handle);
            dlclose(library_handle);
            throw std::runtime_error(std::string("retry upload failed after timeout: ") +
                                     (error != nullptr ? error : "unknown"));
        }
        destroy_fn(handle);
        dlclose(library_handle);

        if (!fs::exists(server_target)) {
            std::cerr << "server target was not created after timeout recovery" << std::endl;
            return 1;
        }

        fs::remove(local_source);
        fs::remove(server_target);
        fs::remove_all(server_target.parent_path());
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "upload_timeout_recovery_smoke error: " << ex.what() << std::endl;
        return 1;
    }
}

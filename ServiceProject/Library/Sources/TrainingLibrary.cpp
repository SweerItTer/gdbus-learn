#include <TrainingLibrary.hpp>

#include <public/DbusConstants.hpp>
#include <utils/ContractSerializer.hpp>
#include <utils/DbusRuntime.hpp>
#include <utils/FileTransferUtils.hpp>

#include <chrono>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace training::library {

namespace detail {

// 动态库层通过 thread_local 保存最近一次错误，供外层 wrapper 用统一方式读取。
thread_local std::string g_last_error;

void SetLastError(std::string message) {
    g_last_error = std::move(message);
}

void ClearLastError() {
    g_last_error.clear();
}

void DrainPendingSignals() {
    for (int i = 0; i < 8; ++i) {
        if (!g_main_context_iteration(nullptr, FALSE)) {
            break;
        }
    }
}

// 所有导出的 C API 最终都通过这里把异常转换成 bool + last_error。
template <typename Fn>
bool InvokeWithError(Fn&& fn) {
    try {
        ClearLastError();
        fn();
        return true;
    } catch (const std::exception& ex) {
        SetLastError(ex.what());
        return false;
    } catch (...) {
        SetLastError("unknown training library failure");
        return false;
    }
}

// 把 GLib 的 GError 提升成 C++ 异常，避免业务层同时处理两套错误模型。
template <typename Fn>
auto CallWithError(Fn&& fn, const char* message) {
    GError* raw_error = nullptr;
    auto result = fn(&raw_error);
    utils::UniqueGError error(raw_error);

    if (error != nullptr) {
        throw std::runtime_error(std::string(message) + error->message);
    }

    return result;
}

// 生成一次完整传输会话的唯一 ID，prefix 用于区分上传/下载两类传输。
std::string CreateTransferId(const char* prefix) {
    return std::string(prefix) + "_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

// 上传和下载都共享一套共享内存命名规则，避免多个进程同时传输时撞名。
std::string CreateSharedMemoryName() {
    return "/training_file_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

// 当调用方没有显式指定远端路径时，默认退化成“basename 上传到服务端 file/ 根目录”。
std::filesystem::path ResolveRemotePath(const char* file_name, const char* remote_relative_path) {
    if (remote_relative_path != nullptr && std::string(remote_relative_path).size() > 0) {
        return utils::NormalizeRelativeFilePath(remote_relative_path);
    }
    if (file_name == nullptr || std::string(file_name).empty()) {
        throw std::runtime_error("remote relative path is empty");
    }
    return utils::NormalizeRelativeFilePath(file_name);
}

} // namespace detail

TrainingLibraryClient::TrainingLibraryClient() {
    // 动态库内部直接持有一个 proxy，后续所有 D-Bus 调用都经由这条连接发出。
    proxy_.reset(detail::CallWithError(
        [&](GError** error) {
            return training_proxy_new_for_bus_sync(utils::kBusType,
                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                   training::kBusName,
                                                   training::kObjectPath,
                                                   nullptr,
                                                   error);
        },
        "failed to create Training proxy: "));

    g_signal_connect(proxy_.get(),
                     "on-test-bool-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestBoolChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-int-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestIntChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-double-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestDoubleChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-string-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestStringChanged),
                     this);
    g_signal_connect(proxy_.get(),
                     "on-test-info-changed",
                     G_CALLBACK(&TrainingLibraryClient::OnRemoteTestInfoChanged),
                     this);
}

void TrainingLibraryClient::SetListener(const TrainingListenerCallbacks* callbacks) {
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = (callbacks != nullptr) ? *callbacks : TrainingListenerCallbacks{};
}

bool TrainingLibraryClient::SetTestBool(bool param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_bool_sync(proxy_.get(),
                                                    static_cast<gboolean>(param),
                                                    &result,
                                                    nullptr,
                                                    error);
        },
        "failed to call SetTestBool: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestInt(int param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_int_sync(proxy_.get(),
                                                   static_cast<gint>(param),
                                                   &result,
                                                   nullptr,
                                                   error);
        },
        "failed to call SetTestInt: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestDouble(double param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_double_sync(proxy_.get(),
                                                      static_cast<gdouble>(param),
                                                      &result,
                                                      nullptr,
                                                      error);
        },
        "failed to call SetTestDouble: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestString(const char* param) {
    gboolean result = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_string_sync(proxy_.get(),
                                                      param != nullptr ? param : "",
                                                      &result,
                                                      nullptr,
                                                      error);
        },
        "failed to call SetTestString: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::SetTestInfo(const public_api::TestInfo* param) {
    gboolean result = FALSE;
    const auto info = param != nullptr ? *param : public_api::TestInfo{};
    detail::CallWithError(
        [&](GError** error) {
            return training_call_set_test_info_sync(proxy_.get(),
                                                    utils::TestInfoToVariant(info),
                                                    &result,
                                                    nullptr,
                                                    error);
        },
        "failed to call SetTestInfo: ");
    detail::DrainPendingSignals();
    return static_cast<bool>(result);
}

bool TrainingLibraryClient::GetTestBool(bool* result) {
    gboolean value = FALSE;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_bool_sync(proxy_.get(), &value, nullptr, error);
        },
        "failed to call GetTestBool: ");
    if (result != nullptr) {
        *result = static_cast<bool>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestInt(int* result) {
    gint value = 0;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_int_sync(proxy_.get(), &value, nullptr, error);
        },
        "failed to call GetTestInt: ");
    if (result != nullptr) {
        *result = static_cast<int>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestDouble(double* result) {
    gdouble value = 0.0;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_double_sync(proxy_.get(), &value, nullptr, error);
        },
        "failed to call GetTestDouble: ");
    if (result != nullptr) {
        *result = static_cast<double>(value);
    }
    return true;
}

bool TrainingLibraryClient::GetTestString(const char** result) {
    gchar* raw_result = nullptr;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_string_sync(proxy_.get(), &raw_result, nullptr, error);
        },
        "failed to call GetTestString: ");
    utils::UniqueGChar value(raw_result);
    last_string_result_ = value != nullptr ? value.get() : "";
    if (result != nullptr) {
        *result = last_string_result_.c_str();
    }
    return true;
}

bool TrainingLibraryClient::GetTestInfo(public_api::TestInfo* result) {
    GVariant* raw_result = nullptr;
    detail::CallWithError(
        [&](GError** error) {
            return training_call_get_test_info_sync(proxy_.get(), &raw_result, nullptr, error);
        },
        "failed to call GetTestInfo: ");
    utils::UniqueGVariant value(raw_result);
    cached_info_ = utils::VariantToTestInfo(value.get());
    if (result != nullptr) {
        *result = cached_info_;
    }
    return true;
}

bool TrainingLibraryClient::SendChunks(const std::string& file_name,
                                       const std::string& remote_relative_path,
                                       std::uint64_t total_size,
                                       const std::string& md5_hex,
                                       const std::function<std::size_t(unsigned char*, std::size_t)>& reader) {
    // 即便是空文件，也仍然发送一片，保证服务端能收到完整的“开始/结束”语义。
    const std::uint32_t chunk_count = total_size == 0
                                          ? 1U
                                          : static_cast<std::uint32_t>((total_size + utils::kFileChunkSize - 1) /
                                                                       utils::kFileChunkSize);
    const std::string transfer_id = detail::CreateTransferId("transfer");
    const std::string shm_name = detail::CreateSharedMemoryName();

    // 发送端先创建并映射一块固定大小的共享内存，之后每片都复用这同一块 1KB 空间。
    auto shm_fd = utils::OpenSharedMemory(shm_name, O_CREAT | O_RDWR | O_TRUNC);
    utils::ResizeSharedMemory(shm_fd.Get(), utils::kFileChunkSize);
    auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ | PROT_WRITE);
    auto* mapped_bytes = static_cast<unsigned char*>(mapped.Get());

    try {
        for (std::uint32_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
            // 先把旧内容清掉，避免最后一片不足 1KB 时残留上一片脏数据。
            std::memset(mapped_bytes, 0, utils::kFileChunkSize);
            const std::size_t current_size = reader(mapped_bytes, utils::kFileChunkSize);
            gboolean result = FALSE;

            detail::CallWithError(
                [&](GError** error) {
                    return training_call_send_file_chunk_sync(proxy_.get(),
                                                              transfer_id.c_str(),
                                                              file_name.c_str(),
                                                              remote_relative_path.c_str(),
                                                              total_size,
                                                              chunk_index,
                                                              chunk_count,
                                                              static_cast<guint>(current_size),
                                                              md5_hex.c_str(),
                                                              shm_name.c_str(),
                                                              &result,
                                                              nullptr,
                                                              error);
                },
                "failed to call SendFileChunk: ");

            if (!result) {
                throw std::runtime_error("SendFileChunk returned false");
            }
        }
    } catch (...) {
        utils::UnlinkSharedMemory(shm_name);
        throw;
    }

    utils::UnlinkSharedMemory(shm_name);
    return true;
}

bool TrainingLibraryClient::SendFileBuffer(const unsigned char* file_buf,
                                           std::size_t file_size,
                                           const char* file_name,
                                           const char* remote_relative_path) {
    if (file_buf == nullptr && file_size != 0) {
        throw std::runtime_error("file buffer is null");
    }

    const auto resolved_remote_path = detail::ResolveRemotePath(file_name, remote_relative_path);
    // buffer 上传统一先落一个临时文件算 MD5，这样上传 buffer / 上传文件路径共用同一套校验逻辑。
    const std::filesystem::path temp_source =
        std::filesystem::temp_directory_path() / (detail::CreateTransferId("training_buffer") + ".bin");
    utils::ScopedPathCleanup temp_cleanup(temp_source);

    {
        std::ofstream output(temp_source, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error("failed to create temp source file for md5");
        }
        output.write(reinterpret_cast<const char*>(file_buf), static_cast<std::streamsize>(file_size));
        if (!output.good()) {
            throw std::runtime_error("failed to write temp source file for md5");
        }
    }

    const std::string md5_hex = utils::ComputeMd5(temp_source);
    std::size_t offset = 0;
    return SendChunks(resolved_remote_path.filename().string(),
                      resolved_remote_path.generic_string(),
                      file_size,
                      md5_hex,
                      [&](unsigned char* destination, std::size_t capacity) {
                          const std::size_t remain = file_size - offset;
                          const std::size_t chunk_size = remain < capacity ? remain : capacity;
                          if (chunk_size > 0) {
                            std::memcpy(destination, file_buf + offset, chunk_size);
                            offset += chunk_size;
                          }
                          return chunk_size;
                      });
}

bool TrainingLibraryClient::SendFilePath(const char* file_path, const char* remote_relative_path) {
    if (file_path == nullptr || std::string(file_path).empty()) {
        throw std::runtime_error("file path is empty");
    }

    const std::filesystem::path path(file_path);
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("file path is invalid");
    }

    const auto resolved_remote_path =
        detail::ResolveRemotePath(path.filename().string().c_str(), remote_relative_path);
    const std::uint64_t total_size = std::filesystem::file_size(path);
    const std::string md5_hex = utils::ComputeMd5(path);
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open source file");
    }

    return SendChunks(resolved_remote_path.filename().string(),
                      resolved_remote_path.generic_string(),
                      total_size,
                      md5_hex,
                      [&](unsigned char* destination, std::size_t capacity) {
                          input.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(capacity));
                          return static_cast<std::size_t>(input.gcount());
                      });
}

bool TrainingLibraryClient::DownloadFile(const char* remote_relative_path, const char* local_file_path) {
    if (remote_relative_path == nullptr || std::string(remote_relative_path).empty()) {
        throw std::runtime_error("remote relative path is empty");
    }
    if (local_file_path == nullptr || std::string(local_file_path).empty()) {
        throw std::runtime_error("local file path is empty");
    }

    const std::filesystem::path local_path(local_file_path);
    utils::EnsureParentDirectory(local_path);

    std::error_code error;
    if (std::filesystem::exists(local_path, error) && !error &&
        std::filesystem::is_directory(local_path, error) && !error) {
        throw std::runtime_error("local file path points to a directory");
    }

    gboolean begin_result = FALSE;
    gchar* raw_transfer_id = nullptr;
    gchar* raw_file_name = nullptr;
    guint64 total_size = 0;
    guint chunk_count = 0;
    gchar* raw_md5 = nullptr;
    // 第一步只拿元数据快照，真正的数据搬运在后续逐片读取时发生。
    detail::CallWithError(
        [&](GError** call_error) {
            return training_call_begin_file_download_sync(proxy_.get(),
                                                          remote_relative_path,
                                                          &begin_result,
                                                          &raw_transfer_id,
                                                          &raw_file_name,
                                                          &total_size,
                                                          &chunk_count,
                                                          &raw_md5,
                                                          nullptr,
                                                          call_error);
        },
        "failed to call BeginFileDownload: ");

    utils::UniqueGChar transfer_id_holder(raw_transfer_id);
    utils::UniqueGChar file_name_holder(raw_file_name);
    utils::UniqueGChar md5_holder(raw_md5);
    if (!begin_result) {
        throw std::runtime_error("BeginFileDownload returned false");
    }

    // 下载同样采用 .part 临时文件，只有校验通过后才替换最终文件。
    const std::filesystem::path temp_path = local_path.string() + ".part";
    utils::ScopedPathCleanup temp_cleanup(temp_path);
    // 下载时共享内存的角色与上传相反：服务端写，共享内存；客户端读，共享内存。
    const std::string shm_name = detail::CreateSharedMemoryName();
    auto shm_fd = utils::OpenSharedMemory(shm_name, O_CREAT | O_RDWR | O_TRUNC);
    utils::ResizeSharedMemory(shm_fd.Get(), utils::kFileChunkSize);
    auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ | PROT_WRITE);

    try {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error("failed to open local temp file for download");
        }

        for (guint chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
            gboolean read_result = FALSE;
            guint chunk_size = 0;
            detail::CallWithError(
                [&](GError** call_error) {
                    return training_call_read_file_chunk_sync(proxy_.get(),
                                                              transfer_id_holder.get(),
                                                              chunk_index,
                                                              shm_name.c_str(),
                                                              &read_result,
                                                              &chunk_size,
                                                              nullptr,
                                                              call_error);
                },
                "failed to call ReadFileChunk: ");

            if (!read_result) {
                throw std::runtime_error("ReadFileChunk returned false");
            }

            output.write(static_cast<const char*>(mapped.Get()), static_cast<std::streamsize>(chunk_size));
            if (!output.good()) {
                throw std::runtime_error("failed to write local download chunk");
            }
        }

        output.close();
        const std::uint64_t local_size = std::filesystem::file_size(temp_path);
        if (local_size != total_size) {
            throw std::runtime_error("download size verification failed");
        }
        if (utils::ComputeMd5(temp_path) != (md5_holder != nullptr ? md5_holder.get() : "")) {
            throw std::runtime_error("download md5 verification failed");
        }

        if (std::filesystem::exists(local_path, error) && !error &&
            std::filesystem::is_directory(local_path, error) && !error) {
            throw std::runtime_error("local file path points to a directory");
        }

        utils::ReplaceFileAtomically(temp_path, local_path);
    } catch (...) {
        utils::UnlinkSharedMemory(shm_name);
        throw;
    }

    temp_cleanup.Cancel();
    utils::UnlinkSharedMemory(shm_name);
    return static_cast<std::uint64_t>(chunk_count) == 0 ? total_size == 0 : true;
}

void TrainingLibraryClient::PumpEvents() {
    detail::DrainPendingSignals();
}

void TrainingLibraryClient::OnRemoteTestBoolChanged(Training*, gboolean param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    TrainingListenerCallbacks callbacks{};
    const bool value = static_cast<bool>(param);
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->cached_info_.bool_param = value;
        callbacks = self->listener_;
    }
    if (callbacks.on_test_bool_changed != nullptr) {
        callbacks.on_test_bool_changed(callbacks.user_data, value);
    }
}

void TrainingLibraryClient::OnRemoteTestIntChanged(Training*, gint param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    TrainingListenerCallbacks callbacks{};
    const int value = static_cast<int>(param);
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->cached_info_.int_param = value;
        callbacks = self->listener_;
    }
    if (callbacks.on_test_int_changed != nullptr) {
        callbacks.on_test_int_changed(callbacks.user_data, value);
    }
}

void TrainingLibraryClient::OnRemoteTestDoubleChanged(Training*, gdouble param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    TrainingListenerCallbacks callbacks{};
    const double value = static_cast<double>(param);
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->cached_info_.double_param = value;
        callbacks = self->listener_;
    }
    if (callbacks.on_test_double_changed != nullptr) {
        callbacks.on_test_double_changed(callbacks.user_data, value);
    }
}

void TrainingLibraryClient::OnRemoteTestStringChanged(Training*, const gchar* param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    TrainingListenerCallbacks callbacks{};
    const std::string value = param != nullptr ? param : "";
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->cached_info_.string_param = value;
        callbacks = self->listener_;
    }
    if (callbacks.on_test_string_changed != nullptr) {
        callbacks.on_test_string_changed(callbacks.user_data, value.c_str());
    }
}

void TrainingLibraryClient::OnRemoteTestInfoChanged(Training*, GVariant* param, gpointer user_data) {
    auto* self = static_cast<TrainingLibraryClient*>(user_data);
    TrainingListenerCallbacks callbacks{};
    const public_api::TestInfo value = utils::VariantToTestInfo(param);
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->cached_info_ = value;
        callbacks = self->listener_;
    }
    if (callbacks.on_test_info_changed != nullptr) {
        callbacks.on_test_info_changed(callbacks.user_data, &value);
    }
}

} // namespace training::library

struct TrainingLibraryHandle {
    training::library::TrainingLibraryClient client;
};

extern "C" TrainingLibraryHandle* Training_Create() {
    TrainingLibraryHandle* handle = nullptr;
    training::library::detail::InvokeWithError([&]() {
        handle = new TrainingLibraryHandle{};
    });
    return handle;
}

extern "C" void Training_Destroy(TrainingLibraryHandle* handle) {
    delete handle;
}

extern "C" void Training_SetListener(TrainingLibraryHandle* handle, const TrainingListenerCallbacks* callbacks) {
    if (handle == nullptr) {
        return;
    }
    handle->client.SetListener(callbacks);
}

extern "C" bool Training_SetTestBool(TrainingLibraryHandle* handle, bool param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestBool(param)) {
            throw std::runtime_error("SetTestBool returned false");
        }
    });
}

extern "C" bool Training_SetTestInt(TrainingLibraryHandle* handle, int param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestInt(param)) {
            throw std::runtime_error("SetTestInt returned false");
        }
    });
}

extern "C" bool Training_SetTestDouble(TrainingLibraryHandle* handle, double param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestDouble(param)) {
            throw std::runtime_error("SetTestDouble returned false");
        }
    });
}

extern "C" bool Training_SetTestString(TrainingLibraryHandle* handle, const char* param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestString(param)) {
            throw std::runtime_error("SetTestString returned false");
        }
    });
}

extern "C" bool Training_SetTestInfo(TrainingLibraryHandle* handle, const training::public_api::TestInfo* param) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SetTestInfo(param)) {
            throw std::runtime_error("SetTestInfo returned false");
        }
    });
}

extern "C" bool Training_GetTestBool(TrainingLibraryHandle* handle, bool* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestBool(result);
    });
}

extern "C" bool Training_GetTestInt(TrainingLibraryHandle* handle, int* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestInt(result);
    });
}

extern "C" bool Training_GetTestDouble(TrainingLibraryHandle* handle, double* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestDouble(result);
    });
}

extern "C" bool Training_GetTestString(TrainingLibraryHandle* handle, const char** result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestString(result);
    });
}

extern "C" bool Training_GetTestInfo(TrainingLibraryHandle* handle, training::public_api::TestInfo* result) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        handle->client.GetTestInfo(result);
    });
}

extern "C" bool Training_SendFileBuffer(TrainingLibraryHandle* handle,
                                        const unsigned char* file_buf,
                                        unsigned long long file_size,
                                        const char* file_name,
                                        const char* remote_relative_path) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SendFileBuffer(file_buf,
                                           static_cast<std::size_t>(file_size),
                                           file_name,
                                           remote_relative_path)) {
            throw std::runtime_error("SendFileBuffer returned false");
        }
    });
}

extern "C" bool Training_SendFilePath(TrainingLibraryHandle* handle,
                                      const char* file_path,
                                      const char* remote_relative_path) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.SendFilePath(file_path, remote_relative_path)) {
            throw std::runtime_error("SendFilePath returned false");
        }
    });
}

extern "C" bool Training_DownloadFile(TrainingLibraryHandle* handle,
                                      const char* remote_relative_path,
                                      const char* local_file_path) {
    if (handle == nullptr) {
        return false;
    }
    return training::library::detail::InvokeWithError([&]() {
        if (!handle->client.DownloadFile(remote_relative_path, local_file_path)) {
            throw std::runtime_error("DownloadFile returned false");
        }
    });
}

extern "C" const char* Training_GetLastError() {
    return training::library::detail::g_last_error.c_str();
}

extern "C" void Training_PumpEvents(TrainingLibraryHandle* handle) {
    if (handle == nullptr) {
        return;
    }
    training::library::detail::InvokeWithError([&]() {
        handle->client.PumpEvents();
    });
}

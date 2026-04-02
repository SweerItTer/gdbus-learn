#include "../Include/TrainingService.hpp"

#include <public/DbusConstants.hpp>
#include <utils/ContractSerializer.hpp>
#include <utils/DbusRuntime.hpp>
#include <utils/FileTransferUtils.hpp>

#include "training-generated.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace training::service {

// ---------------------------------------------------------------------------
// 文件内辅助函数
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kDbusErrorName = "com.example.Training.Error";
constexpr std::chrono::milliseconds kDefaultTransferTimeout{30000}; // 30s

// 服务端对下载传输也使用 transfer_id，把一次“下载初始化 + 多次分片读取”绑定起来。
std::string CreateTransferIdValue() {
    return "transfer_" + std::to_string(::getpid()) + "_" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

// 把总文件大小转换成协议层的 chunk_count。
std::uint32_t CalculateChunkCount(std::uint64_t total_size) {
    if (total_size == 0) {
        return 1;
    }
    return static_cast<std::uint32_t>((total_size + utils::kFileChunkSize - 1) / utils::kFileChunkSize);
}

// sender 是 D-Bus 连接唯一标识，和 transfer_id 组合后可以唯一标识一条活跃传输。
std::string BuildTransferKeyValue(const std::string& sender, const std::string& transfer_id) {
    return sender + "|" + transfer_id;
}

// 所有服务端业务错误都走统一的 D-Bus error name，客户端就能收到带 message 的 GError。
void ReturnInvocationErrorValue(GDBusMethodInvocation* invocation, const std::string& message) {
    g_dbus_method_invocation_return_dbus_error(invocation, kDbusErrorName, message.c_str());
}

// 获取超时时间
std::chrono::milliseconds GetTransferTimeoutValue() {
    // 测试时使用环境变量 TRAINING_TRANSFER_TIMEOUT_MS 
    const char* raw_value = std::getenv("TRAINING_TRANSFER_TIMEOUT_MS");
    if (raw_value == nullptr || std::string(raw_value).empty()) {
        return kDefaultTransferTimeout;
    }

    try {
        // 转为整数
        const auto parsed = std::stoll(raw_value); // 异常源
        if (parsed <= 0) {
            return kDefaultTransferTimeout;
        }
        return std::chrono::milliseconds(parsed);
    } catch (...) {
        return kDefaultTransferTimeout;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------
TrainingService::TrainingService()
    : loop_(g_main_loop_new(nullptr, FALSE))
    , connection_(utils::kBusType) {
    // 请求占用bus_name
    bus_name_owner_.Acquire(connection_.Get(), training::kBusName);

    // 构造时一次性把所有 method handler 绑定到 skeleton 上
    skeleton_.reset(training_skeleton_new());
    g_signal_connect(skeleton_.get(), "handle-set-test-bool", G_CALLBACK(&TrainingService::OnHandleSetTestBool), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-int", G_CALLBACK(&TrainingService::OnHandleSetTestInt), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-double", G_CALLBACK(&TrainingService::OnHandleSetTestDouble), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-string", G_CALLBACK(&TrainingService::OnHandleSetTestString), this);
    g_signal_connect(skeleton_.get(), "handle-set-test-info", G_CALLBACK(&TrainingService::OnHandleSetTestInfo), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-bool", G_CALLBACK(&TrainingService::OnHandleGetTestBool), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-int", G_CALLBACK(&TrainingService::OnHandleGetTestInt), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-double", G_CALLBACK(&TrainingService::OnHandleGetTestDouble), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-string", G_CALLBACK(&TrainingService::OnHandleGetTestString), this);
    g_signal_connect(skeleton_.get(), "handle-get-test-info", G_CALLBACK(&TrainingService::OnHandleGetTestInfo), this);
    g_signal_connect(skeleton_.get(), "handle-send-file-chunk", G_CALLBACK(&TrainingService::OnHandleSendFileChunk), this);
    g_signal_connect(skeleton_.get(),
                     "handle-begin-file-download",
                     G_CALLBACK(&TrainingService::OnHandleBeginFileDownload),
                     this);
    g_signal_connect(skeleton_.get(),
                     "handle-read-file-chunk",
                     G_CALLBACK(&TrainingService::OnHandleReadFileChunk),
                     this);

    // 建立关联 { ObjectPath : skeleton }
    GError* raw_error = nullptr;
    const gboolean exported = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(skeleton_.get()),
        connection_.Get(),
        training::kObjectPath,
        &raw_error);
    utils::UniqueGError error(raw_error);
    if (!exported) {
        const std::string message = error != nullptr ? error->message : "unknown export failure";
        throw std::runtime_error("failed to export Training skeleton: " + message);
    }
}

TrainingService::~TrainingService() = default;

// 运行事件循环
void TrainingService::Run() {
    std::cout << "server: waiting for calls on " << training::kBusName << std::endl;
    g_main_loop_run(loop_.get());
}

// ---------------------------------------------------------------------------
// 基础数据读写
// ---------------------------------------------------------------------------
bool TrainingService::SetTestBool(bool param) {
    state_.bool_param = param;
    return true;
}

bool TrainingService::SetTestInt(int param) {
    state_.int_param = param;
    return true;
}

bool TrainingService::SetTestDouble(double param) {
    state_.double_param = param;
    return true;
}

bool TrainingService::SetTestString(std::string param) {
    state_.string_param = std::move(param);
    return true;
}

bool TrainingService::SetTestInfo(public_api::TestInfo param) {
    state_ = std::move(param);
    return true;
}

bool TrainingService::GetTestBool() {
    return state_.bool_param;
}

int TrainingService::GetTestInt() {
    return state_.int_param;
}

double TrainingService::GetTestDouble() {
    return state_.double_param;
}

std::string TrainingService::GeTestString() {
    return state_.string_param;
}

public_api::TestInfo TrainingService::GetTestInfo() {
    return state_;
}

// ---------------------------------------------------------------------------
// D-Bus 方法处理
// ---------------------------------------------------------------------------
gboolean TrainingService::OnHandleSetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gboolean param,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestBool(static_cast<bool>(param));
    training_complete_set_test_bool(object, invocation, ok);
    training_emit_on_test_bool_changed(object, param);
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestInt(Training* object,
                                             GDBusMethodInvocation* invocation,
                                             gint param,
                                             gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestInt(param);
    training_complete_set_test_int(object, invocation, ok);
    training_emit_on_test_int_changed(object, param);
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestDouble(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gdouble param,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestDouble(param);
    training_complete_set_test_double(object, invocation, ok);
    training_emit_on_test_double_changed(object, param);
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestString(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                const gchar* param,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const bool ok = self->SetTestString(param != nullptr ? param : "");
    training_complete_set_test_string(object, invocation, ok);
    training_emit_on_test_string_changed(object, param != nullptr ? param : "");
    return TRUE;
}

gboolean TrainingService::OnHandleSetTestInfo(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              GVariant* param,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const auto info = utils::VariantToTestInfo(param);
    const bool ok = self->SetTestInfo(info);
    training_complete_set_test_info(object, invocation, ok);
    training_emit_on_test_info_changed(object, utils::TestInfoToVariant(info));
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_bool(object, invocation, self->GetTestBool());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestInt(Training* object,
                                             GDBusMethodInvocation* invocation,
                                             gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_int(object, invocation, self->GetTestInt());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestDouble(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_double(object, invocation, self->GetTestDouble());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestString(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const auto result = self->GeTestString();
    training_complete_get_test_string(object, invocation, result.c_str());
    return TRUE;
}

gboolean TrainingService::OnHandleGetTestInfo(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    training_complete_get_test_info(object, invocation, utils::TestInfoToVariant(self->GetTestInfo()));
    return TRUE;
}

// ---------------------------------------------------------------------------
// 上传请求入口
// ---------------------------------------------------------------------------
// 客户端 -> 服务端
gboolean TrainingService::OnHandleSendFileChunk(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                const gchar* transfer_id,
                                                const gchar* file_name,
                                                const gchar* target_relative_path,
                                                guint64 total_size,
                                                guint chunk_index,
                                                guint chunk_count,
                                                guint chunk_size,
                                                const gchar* md5_hex,
                                                const gchar* shm_name,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    // 获取唯一连接标识
    const char* sender = g_dbus_method_invocation_get_sender(invocation);

    try {
        // 接收文件
        self->HandleIncomingFileChunk(sender != nullptr ? sender : "",
                                      transfer_id != nullptr ? transfer_id : "",
                                      file_name != nullptr ? file_name : "",
                                      target_relative_path != nullptr ? target_relative_path : "",
                                      total_size,
                                      chunk_index,
                                      chunk_count,
                                      chunk_size,
                                      md5_hex != nullptr ? md5_hex : "",
                                      shm_name != nullptr ? shm_name : "");
        // 完成函数调用
        training_complete_send_file_chunk(object, invocation, TRUE);
    } catch (const std::exception& ex) { // 文件写入失败
        // 清理连接和临时文件
        self->ResetFileTransfer(BuildTransferKeyValue(sender != nullptr ? sender : "",
                                                      transfer_id != nullptr ? transfer_id : ""),
                                true);
        ReturnInvocationErrorValue(invocation, ex.what());
    }

    return TRUE;
}

// ---------------------------------------------------------------------------
// 下载请求入口
// ---------------------------------------------------------------------------
// 服务端 -> 客户端(准备阶段)
gboolean TrainingService::OnHandleBeginFileDownload(Training* object,
                                                    GDBusMethodInvocation* invocation,
                                                    const gchar* remote_relative_path,
                                                    gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const char* sender = g_dbus_method_invocation_get_sender(invocation);

    try {
        // 信息传输预处理
        const auto state = self->PrepareDownloadTransfer(sender != nullptr ? sender : "",
                                                         remote_relative_path != nullptr ? remote_relative_path : "");
        // 返回文件信息
        training_complete_begin_file_download(object,
                                              invocation,
                                              TRUE,
                                              state.transfer_id.c_str(),
                                              state.file_name.c_str(),
                                              state.total_size,
                                              state.chunk_count,
                                              state.expected_md5.c_str());
    } catch (const std::exception& ex) {
        ReturnInvocationErrorValue(invocation, ex.what());
    }

    return TRUE;
}

gboolean TrainingService::OnHandleReadFileChunk(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                const gchar* transfer_id,
                                                guint chunk_index,
                                                const gchar* shm_name,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    const char* sender = g_dbus_method_invocation_get_sender(invocation);

    try {
        // 读取一段切片并写入共享内存
        const auto chunk_size = self->ReadFileChunk(sender != nullptr ? sender : "",
                                                    transfer_id != nullptr ? transfer_id : "",
                                                    chunk_index,
                                                    shm_name != nullptr ? shm_name : "");
        // 同步信息
        training_complete_read_file_chunk(object, invocation, TRUE, chunk_size);
    } catch (const std::exception& ex) {
        self->ResetDownloadTransfer(BuildTransferKeyValue(sender != nullptr ? sender : "",
                                                          transfer_id != nullptr ? transfer_id : ""));
        ReturnInvocationErrorValue(invocation, ex.what());
    }

    return TRUE;
}

// ---------------------------------------------------------------------------
// 上传处理
// ---------------------------------------------------------------------------
// 切片文件保存
bool TrainingService::HandleIncomingFileChunk(const std::string& sender,
                                              const std::string& transfer_id,
                                              const std::string& file_name,
                                              const std::string& target_relative_path,
                                              std::uint64_t total_size,
                                              std::uint32_t chunk_index,
                                              std::uint32_t chunk_count,
                                              std::uint32_t chunk_size,
                                              const std::string& md5_hex,
                                              const std::string& shm_name) {
    if (sender.empty() || transfer_id.empty() || file_name.empty() || md5_hex.empty() || shm_name.empty()) {
        throw std::runtime_error("upload request metadata is incomplete");
    }
    if (chunk_count == 0 || chunk_size > utils::kFileChunkSize) {
        throw std::runtime_error("upload chunk metadata is invalid");
    }

    // target_relative_path 是完整“远端相对文件路径”，因此它的 filename 必须和 file_name 对得上。
    const auto relative_path = utils::NormalizeRelativeFilePath(target_relative_path);
    if (relative_path.filename() != file_name) {
        throw std::runtime_error("remote path file name does not match upload file name");
    }

    // 构造唯一连接标识
    const std::string transfer_key = BuildTransferKeyValue(sender, transfer_id);
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        CleanupExpiredFileTransfersLocked(now);
        // 第一次连接需要构造文件信息
        if (file_transfers_.find(transfer_key) == file_transfers_.end()) {
            if (chunk_index != 0) {
                throw std::runtime_error("upload must start from chunk 0");
            }
            if (!PrepareTransferState(transfer_key,
                                      sender,
                                      transfer_id,
                                      file_name,
                                      relative_path,
                                      total_size,
                                      chunk_count,
                                      md5_hex)) {
                throw std::runtime_error("upload target path is already active");
            }
        }
    }

    // 查询缓存
    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto it = file_transfers_.find(transfer_key);
        if (it == file_transfers_.end()) {
            throw std::runtime_error("upload state was lost");
        }
        // 更新活跃时间
        it->second.last_activity = now;
        transfer_state = it->second;
    }

    // 比对检验文件信息
    if (transfer_state.owner_sender != sender ||
        transfer_state.transfer_id != transfer_id ||
        transfer_state.file_name != file_name ||
        transfer_state.relative_path != relative_path ||
        transfer_state.total_size != total_size ||
        transfer_state.chunk_count != chunk_count ||
        transfer_state.expected_md5 != md5_hex) {
        throw std::runtime_error("upload metadata changed during transfer");
    }
    // 预期切片错误
    if (chunk_index != transfer_state.next_expected_chunk) {
        throw std::runtime_error("upload chunk order is invalid");
    }
    // 预期文件大小错误
    if (transfer_state.received_size + chunk_size > transfer_state.total_size) {
        throw std::runtime_error("upload chunk exceeds total file size");
    }

    // 分片先写入临时文件，全部完成后再校验和 rename 到最终目标。
    {
        // 获取共享内存fd
        auto shm_fd = utils::OpenSharedMemory(shm_name, O_RDONLY);
        // 映射共享内存
        auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ);
    {
        std::ofstream output(transfer_state.temp_file_path, std::ios::binary | std::ios::app);
        if (!output.is_open()) {
            throw std::runtime_error("failed to open upload temp file");
        }
        // 读取chunk_size长度的内存数据到临时文件
        output.write(static_cast<const char*>(mapped.Get()), static_cast<std::streamsize>(chunk_size));
        if (!output.good()) {
            throw std::runtime_error("failed to write upload temp file");
        }
    }
    }

    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto it = file_transfers_.find(transfer_key);
        if (it == file_transfers_.end()) {
            throw std::runtime_error("upload state was lost after writing chunk");
        }
        // 更新缓存
        it->second.received_size += chunk_size;
        it->second.next_expected_chunk += 1;
        it->second.last_activity = std::chrono::steady_clock::now();
    }

    // 最后一包
    if (chunk_index + 1 == chunk_count) {
        const bool finalized = FinalizeFileTransfer(transfer_key);
        // 根据落盘情况清理缓存
        ResetFileTransfer(transfer_key, !finalized);
        if (!finalized) {
            throw std::runtime_error("failed to finalize uploaded file");
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// 上传状态收尾
// ---------------------------------------------------------------------------
// 文件落盘 
bool TrainingService::FinalizeFileTransfer(const std::string& transfer_key) {
    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto transfer_it = file_transfers_.find(transfer_key);
        if (transfer_it == file_transfers_.end()) {
            throw std::runtime_error("upload state was not found during finalize");
        }
        transfer_state = transfer_it->second;
    }

    if (transfer_state.received_size != transfer_state.total_size) {
        throw std::runtime_error("uploaded file size mismatch during finalize");
    }
    // md5 检验
    const std::string actual_md5 = utils::ComputeMd5(transfer_state.temp_file_path);
    if (actual_md5 != transfer_state.expected_md5) {
        throw std::runtime_error("uploaded file md5 mismatch during finalize: expected=" +
                                 transfer_state.expected_md5 + ", actual=" + actual_md5);
    }

    // 目标路径固定落在 server/file/ 根目录下，不允许越界到根目录外。
    const auto target_path = utils::GetServiceFilePath(transfer_state.relative_path.string());
    utils::EnsureParentDirectory(target_path);
    utils::ReplaceFileAtomically(transfer_state.temp_file_path, target_path);
    return true;
}

// 清理缓存
void TrainingService::ResetFileTransfer(const std::string& transfer_key, bool remove_temp_file) {
    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto transfer_it = file_transfers_.find(transfer_key);
        if (transfer_it == file_transfers_.end()) {
            return;
        }
        transfer_state = transfer_it->second;
        file_transfers_.erase(transfer_it);

        // 关闭连接(删除连接表示)
        auto claim_it = file_name_claims_.find(transfer_state.relative_path.string());
        if (claim_it != file_name_claims_.end() && claim_it->second == transfer_key) {
            file_name_claims_.erase(claim_it);
        }
    }

    // 删除缓存文件
    if (remove_temp_file && !transfer_state.temp_file_path.empty()) {
        try {
            utils::RemoveIfExists(transfer_state.temp_file_path);
        } catch (...) {
        }
    }
}

// 构造文件信息
bool TrainingService::PrepareTransferState(const std::string& transfer_key,
                                           const std::string& sender,
                                           const std::string& transfer_id,
                                           const std::string& file_name,
                                           const std::filesystem::path& relative_path,
                                           std::uint64_t total_size,
                                           std::uint32_t chunk_count,
                                           const std::string& md5_hex) {
    // 并发冲突粒度是“完整相对文件路径”，不再是单纯文件名。
    const std::string claim_key = relative_path.string();
    CleanupExpiredFileTransfersLocked(std::chrono::steady_clock::now());
    if (file_name_claims_.find(claim_key) != file_name_claims_.end()) {
        return false;
    }

    utils::EnsureParentDirectory(utils::GetServiceFileRoot() / ".keep");

    FileTransferState state{};
    state.owner_sender = sender;
    state.transfer_id = transfer_id;
    state.transfer_key = transfer_key;
    state.file_name = file_name;
    state.relative_path = relative_path;
    state.total_size = total_size;
    state.chunk_count = chunk_count;
    state.next_expected_chunk = 0;
    state.received_size = 0;
    state.expected_md5 = md5_hex;
    state.temp_file_path = utils::GetServiceFileRoot() / ("." + transfer_id + ".part");
    state.last_activity = std::chrono::steady_clock::now();

    utils::RemoveIfExists(state.temp_file_path);
    file_name_claims_[claim_key] = transfer_key;
    file_transfers_[transfer_key] = std::move(state);
    return true;
}

// ---------------------------------------------------------------------------
// 下载初始化
// ---------------------------------------------------------------------------
// 准备文件信息和连接标识
TrainingService::DownloadTransferState TrainingService::PrepareDownloadTransfer(
    const std::string& sender,
    const std::string& remote_relative_path) {
    if (sender.empty()) {
        throw std::runtime_error("download sender is empty");
    }

    // 下载初始化阶段就把路径、类型、大小和 MD5 都固定下来；
    // 后续分片读取只允许消费这份快照，避免下载过程中源文件被悄悄替换。
    const auto normalized_path = utils::NormalizeRelativeFilePath(remote_relative_path); // 一般化路径
    const auto source_file_path = utils::GetServiceFilePath(normalized_path.string());   // 实际文件路径
    if (!std::filesystem::exists(source_file_path)) {
        throw std::runtime_error("remote file does not exist");
    }
    // 检查是否为文件，避免处理文件夹
    if (!std::filesystem::is_regular_file(source_file_path)) {
        throw std::runtime_error("remote path must point to a file");
    }

    // 构建文件信息
    DownloadTransferState state{};
    state.owner_sender = sender;
    state.transfer_id = CreateTransferIdValue();
    state.transfer_key = BuildTransferKeyValue(sender, state.transfer_id);
    state.file_name = normalized_path.filename().string();
    state.relative_path = normalized_path;
    state.source_file_path = source_file_path;
    state.total_size = std::filesystem::file_size(source_file_path);
    state.chunk_count = CalculateChunkCount(state.total_size);
    state.next_expected_chunk = 0;
    state.expected_md5 = utils::ComputeMd5(source_file_path);
    state.source_last_write_time = std::filesystem::last_write_time(source_file_path);
    state.last_activity = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(download_transfer_mutex_);
    // 超时清理
    CleanupExpiredDownloadTransfersLocked(state.last_activity);
    // 缓存连接标识
    download_transfers_[state.transfer_key] = state;
    return state;
}
// ---------------------------------------------------------------------------
// 下载分片读取
// ---------------------------------------------------------------------------
std::uint32_t TrainingService::ReadFileChunk(const std::string& sender,
                                             const std::string& transfer_id,
                                             std::uint32_t chunk_index,
                                             const std::string& shm_name) {
    if (sender.empty() || transfer_id.empty() || shm_name.empty()) {
        throw std::runtime_error("download chunk metadata is incomplete");
    }

    // sender + transfer_id 共同确定一条唯一下载会话；
    // 后续所有分片读取都依赖这把 key 找回初始化阶段保存的快照状态。
    const std::string transfer_key = BuildTransferKeyValue(sender, transfer_id);
    DownloadTransferState state{};
    {
        std::lock_guard<std::mutex> lock(download_transfer_mutex_);
        // 读分片前先做一次过期清理，避免长时间无响应的旧下载状态一直滞留。
        CleanupExpiredDownloadTransfersLocked(std::chrono::steady_clock::now());
        auto it = download_transfers_.find(transfer_key);
        if (it == download_transfers_.end()) {
            throw std::runtime_error("download transfer was not found");
        }
        // last_activity 用来支持下载超时回收；
        // 只要客户端仍在按顺序拉片，这个时间就会持续刷新。
        it->second.last_activity = std::chrono::steady_clock::now();
        state = it->second;
    }

    // 下载协议要求严格顺序读取，避免跳片、重片或越界访问源文件。
    if (chunk_index != state.next_expected_chunk) {
        throw std::runtime_error("download chunk order is invalid");
    }
    if (chunk_index >= state.chunk_count) {
        throw std::runtime_error("download chunk index is out of range");
    }
    if (!std::filesystem::exists(state.source_file_path) ||
        std::filesystem::file_size(state.source_file_path) != state.total_size ||
        std::filesystem::last_write_time(state.source_file_path) != state.source_last_write_time) {
        throw std::runtime_error("remote file changed during download");
    }

    // 按 chunk_index 计算当前片在源文件里的偏移量，再读取这一片真正需要返回的字节数。
    const std::uint64_t offset = static_cast<std::uint64_t>(chunk_index) * utils::kFileChunkSize;
    const auto bytes = utils::ReadFileChunk(state.source_file_path,
                                            offset,
                                            static_cast<std::size_t>(std::min<std::uint64_t>(
                                                utils::kFileChunkSize,
                                                state.total_size > offset ? state.total_size - offset : 0)));

    // 下载时共享内存由客户端预先创建，服务端只打开并写入，避免跨用户 system bus 场景下
    // 因 O_CREAT/O_TRUNC 重新创建对象而触发权限冲突。
    auto shm_fd = utils::OpenSharedMemory(shm_name, O_RDWR);
    auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ | PROT_WRITE);
    std::memset(mapped.Get(), 0, utils::kFileChunkSize);
    if (!bytes.empty()) {
        std::memcpy(mapped.Get(), bytes.data(), bytes.size());
    }

    {
        std::lock_guard<std::mutex> lock(download_transfer_mutex_);
        auto it = download_transfers_.find(transfer_key);
        if (it == download_transfers_.end()) {
            throw std::runtime_error("download transfer was lost");
        }
        // 当前分片成功写入共享内存后，推进服务端记录的“下一片期望值”。
        it->second.next_expected_chunk += 1;
        it->second.last_activity = std::chrono::steady_clock::now();
        // 最后一片读完后立刻释放服务端侧下载状态。
        if (it->second.next_expected_chunk >= it->second.chunk_count) {
            download_transfers_.erase(it);
        }
    }

    return static_cast<std::uint32_t>(bytes.size());
}

// 下载过程中只要某一片失败，就直接丢弃整条下载状态；
// 这样客户端下次必须重新走 BeginFileDownload，避免继续消费一份不完整会话。
void TrainingService::ResetDownloadTransfer(const std::string& transfer_key) {
    std::lock_guard<std::mutex> lock(download_transfer_mutex_);
    download_transfers_.erase(transfer_key);
}

// ---------------------------------------------------------------------------
// 超时清理
// ---------------------------------------------------------------------------
void TrainingService::CleanupExpiredFileTransfersLocked(const std::chrono::steady_clock::time_point& now) {
    const auto timeout = GetTransferTimeoutValue();
    for (auto it = file_transfers_.begin(); it != file_transfers_.end();) {
        if (now - it->second.last_activity < timeout) {
            ++it;
            continue;
        }

        // 超时上传不仅要删状态，还要同步释放 claim 和未完成的 .part，避免后续同路径一直被卡死。
        const auto relative_path = it->second.relative_path.string();
        const auto temp_file_path = it->second.temp_file_path;
        auto claim_it = file_name_claims_.find(relative_path);
        if (claim_it != file_name_claims_.end() && claim_it->second == it->first) {
            file_name_claims_.erase(claim_it);
        }
        it = file_transfers_.erase(it);
        try {
            utils::RemoveIfExists(temp_file_path);
        } catch (...) {
        }
    }
}

// 下载超时只需要释放内存里的下载状态；
// 因为下载方向没有服务端临时文件，也没有额外 claim 需要回收。
void TrainingService::CleanupExpiredDownloadTransfersLocked(const std::chrono::steady_clock::time_point& now) {
    const auto timeout = GetTransferTimeoutValue();
    for (auto it = download_transfers_.begin(); it != download_transfers_.end();) {
        if (now - it->second.last_activity < timeout) {
            ++it;
            continue;
        }
        // 下载没有服务端临时文件要清，因此这里只需要回收内存里的会话状态。
        it = download_transfers_.erase(it);
    }
}

} // namespace training::service

#include <TrainingService.hpp>

#include <public/DbusConstants.hpp>
#include <utils/ContractSerializer.hpp>
#include <utils/FileTransferUtils.hpp>

#include "training-generated.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace training::service {

TrainingService::TrainingService()
    : loop_(g_main_loop_new(nullptr, FALSE)) // 循环对象
    , connection_(G_BUS_TYPE_SESSION)        // SESSION 连接
{   
    // 创建 skeleton
    skeleton_.reset(training_skeleton_new());
    // 根据现有 connection 获取 own_id 以便后续关闭
    bus_name_owner_.Acquire(connection_.Get(), training::kBusName);

    // 注册回调 (method) 到 skeleton 对象(通过‘detailed_signal’O(1)查表到具体回调函数)
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

    // 将 skeleton 和 connection 上的 ObjectPath 关联
    GError* raw_error = nullptr;
    const gboolean exported = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(skeleton_.get()),
        connection_.Get(),      // 连接句柄
        training::kObjectPath,  // 接口路径
        &raw_error);
    utils::UniqueGError error(raw_error);

    if (!exported) {
        const std::string message = error != nullptr ? error->message : "unknown export failure";
        throw std::runtime_error("failed to export Training skeleton: " + message);
    }
}

TrainingService::~TrainingService() = default;

// 启动服务
void TrainingService::Run() {
    std::cout << "server: waiting for calls on " << training::kBusName << std::endl;
    g_main_loop_run(loop_.get());
}

/* ---------------------------------------
 *               info 操作   
 * --------------------------------------- */
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

bool TrainingService::SendFile(unsigned char* file_buf, size_t file_size) {
    if (file_buf == nullptr && file_size != 0) {
        return false;
    }

    const auto target_path = utils::GetExecutableDir() / "upload_buffer.bin";
    std::ofstream output(target_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(file_buf), static_cast<std::streamsize>(file_size));
    return output.good();
}

/* ---------------------------------------
 *           method 调用具体方法   
 * --------------------------------------- */
gboolean TrainingService::OnHandleSetTestBool(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              gboolean param,
                                              gpointer user_data) {
    // 转换指针为具体类型
    auto* self = static_cast<TrainingService*>(user_data); 
    // 调用具体实现函数
    const bool ok = self->SetTestBool(static_cast<bool>(param));
    // 附带返回值完成method调用
    training_complete_set_test_bool(object, invocation, ok);
    // 广播信号
    training_emit_on_test_bool_changed(object, param);
    // 返回调用状态(必须)
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
    // 反序列化(解析数据)
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

gboolean TrainingService::OnHandleSendFileChunk(Training* object,
                                                GDBusMethodInvocation* invocation,
                                                const gchar* transfer_id,
                                                const gchar* file_name,
                                                guint64 total_size,
                                                guint chunk_index,
                                                guint chunk_count,
                                                guint chunk_size,
                                                const gchar* md5_hex,
                                                const gchar* shm_name,
                                                gpointer user_data) {
    auto* self = static_cast<TrainingService*>(user_data);
    // 获取唯一的连接ID
    const char* sender = g_dbus_method_invocation_get_sender(invocation);
    bool ok = false;
    try {
        // 传递参数
        ok = self->HandleIncomingFileChunk(sender != nullptr ? sender : "",
                                           transfer_id != nullptr ? transfer_id : "",
                                           file_name != nullptr ? file_name : "",
                                           total_size,
                                           chunk_index,
                                           chunk_count,
                                           chunk_size,
                                           md5_hex != nullptr ? md5_hex : "",
                                           shm_name != nullptr ? shm_name : "");
    } catch (...) {
        // 失败删除旧文件
        self->ResetFileTransfer((sender != nullptr ? sender : "") + std::string("|") +
                                    (transfer_id != nullptr ? transfer_id : ""),
                                true);
        ok = false;
    }
    training_complete_send_file_chunk(object, invocation, ok);
    return TRUE;
}

bool TrainingService::HandleIncomingFileChunk(const std::string& sender,
                                              const std::string& transfer_id,
                                              const std::string& file_name,
                                              std::uint64_t total_size,
                                              std::uint32_t chunk_index,
                                              std::uint32_t chunk_count,
                                              std::uint32_t chunk_size,
                                              const std::string& md5_hex,
                                              const std::string& shm_name) {
    if (sender.empty() || transfer_id.empty() || file_name.empty() || md5_hex.empty() || shm_name.empty()) {
        return false;
    }

    // 限制1K
    if (chunk_count == 0 || chunk_size > utils::kFileChunkSize) {
        return false;
    }
    // sender|id 组合确定传输唯一性
    const std::string transfer_key = sender + "|" + transfer_id;

    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        // 检查是否为第一次连接
        if (file_transfers_.find(transfer_key) == file_transfers_.end()) {
            // 检查发送的切片是否为的一个
            if (chunk_index != 0) {
                return false;
            }
            // 准备元数据到transfer_key下
            if (!PrepareTransferState(transfer_key, sender, transfer_id, file_name, total_size, chunk_count, md5_hex)) {
                return false;
            }
        }
    }

    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        // 检查是否有准备的元数据
        auto it = file_transfers_.find(transfer_key);
        if (it == file_transfers_.end()) {
            return false;
        }
        transfer_state = it->second;
    }

    // 发送中断(sender一致但是数据对不上)
    if (transfer_state.owner_sender != sender ||
        transfer_state.transfer_id != transfer_id ||
        transfer_state.file_name != file_name ||
        transfer_state.total_size != total_size ||
        transfer_state.chunk_count != chunk_count ||
        transfer_state.expected_md5 != md5_hex) {
        ResetFileTransfer(transfer_key, true);
        return false;
    }

    // 校验下一段数据标号
    if (chunk_index != transfer_state.next_expected_chunk) {
        ResetFileTransfer(transfer_key, true);
        return false;
    }

    // 校验总大小
    if (transfer_state.received_size + chunk_size > transfer_state.total_size) {
        ResetFileTransfer(transfer_key, true);
        return false;
    }

    try {
        // 只读共享内存，并且获取共享内存对象的fd
        auto shm_fd = utils::OpenSharedMemory(shm_name, O_RDONLY);
        // 只读映射共享内存数据
        auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ);

        // 打开.part文件
        std::ofstream output(transfer_state.temp_file_path, std::ios::binary | std::ios::app);
        if (!output.is_open()) {
            ResetFileTransfer(transfer_key, true);
            return false;
        }
        // 读取chunk_size长度的内存数据追加到.part文件
        output.write(static_cast<const char*>(mapped.Get()), static_cast<std::streamsize>(chunk_size));
        if (!output.good()) {
            ResetFileTransfer(transfer_key, true);
            return false;
        }
    } catch (...) {
        ResetFileTransfer(transfer_key, true);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto it = file_transfers_.find(transfer_key);
        if (it == file_transfers_.end()) {
            return false;
        }
        // 更新接受的数据
        it->second.received_size += chunk_size;
        // 更新下一判断标号
        it->second.next_expected_chunk += 1;
        transfer_state = it->second;
    }

    // 最后一包到达
    if (chunk_index + 1 == chunk_count) {
        try {
            // 校验和重命名 (test.txt.part->text.txt)
            const bool finalized = FinalizeFileTransfer(transfer_key);
            ResetFileTransfer(transfer_key, !finalized);
            return finalized;
        } catch (...) {
            ResetFileTransfer(transfer_key, true);
            return false;
        }
    }

    return true;
}

bool TrainingService::FinalizeFileTransfer(const std::string& transfer_key) {
    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto transfer_it = file_transfers_.find(transfer_key);
        if (transfer_it == file_transfers_.end()) {
            return false;
        }
        // 获取传输元数据
        transfer_state = transfer_it->second;
    }

    // 校验接收数据总量大小
    if (transfer_state.received_size != transfer_state.total_size) {
        return false;
    }

    // 使用工具完成MD5校验
    const std::string actual_md5 = utils::ComputeMd5(transfer_state.temp_file_path);
    if (actual_md5 != transfer_state.expected_md5) {
        return false;
    }

    // 获取可执行文件根目录路径
    const auto target_path = utils::GetExecutableDir() / transfer_state.file_name;
    std::filesystem::remove(target_path);
    std::filesystem::rename(transfer_state.temp_file_path, target_path);
    return true;
}

// 删除旧文件并重置状态
void TrainingService::ResetFileTransfer(const std::string& transfer_key, bool remove_temp_file) {
    FileTransferState transfer_state{};
    {
        std::lock_guard<std::mutex> lock(file_transfer_mutex_);
        auto transfer_it = file_transfers_.find(transfer_key);
        if (transfer_it == file_transfers_.end()) {
            return;
        }
        transfer_state = transfer_it->second;
        // 移除连接
        file_transfers_.erase(transfer_it);

        auto claim_it = file_name_claims_.find(transfer_state.file_name);
        if (claim_it != file_name_claims_.end() && claim_it->second == transfer_key) {
            // 移除元数据
            file_name_claims_.erase(claim_it);
        }
    }

    // 根据remove_temp_file判断是否删除旧的临时(.part)文件
    if (remove_temp_file && !transfer_state.temp_file_path.empty()) {
        std::filesystem::remove(transfer_state.temp_file_path);
    }
}

bool TrainingService::PrepareTransferState(const std::string& transfer_key,
                                           const std::string& sender,
                                           const std::string& transfer_id,
                                           const std::string& file_name,
                                           std::uint64_t total_size,
                                           std::uint32_t chunk_count,
                                           const std::string& md5_hex) {
    // 同名文件已存在活跃发送：直接拒绝后者
    if (file_name_claims_.find(file_name) != file_name_claims_.end()) {
        return false;
    }
    // 为当前文件元数据和transfer_key关联
    file_name_claims_[file_name] = transfer_key;

    FileTransferState state{};
    state.owner_sender = sender;        // 连接唯一ID
    state.transfer_id = transfer_id;    // 发送批次
    state.transfer_key = transfer_key;  // 组合的key(sender|transfer_id)
    state.file_name = file_name;        // 发送的文件名
    state.total_size = total_size;      // 文件总大小
    state.chunk_count = chunk_count;    // 切片总数
    state.next_expected_chunk = 0;      // 以一个预期切片id
    state.received_size = 0;            // 已接收字大小
    state.expected_md5 = md5_hex;       // 预期MD5
    // 临时文件名 ‘.id.part’
    state.temp_file_path = utils::GetExecutableDir() / ("." + transfer_id + ".part");
    // 删除同名文件(覆写)
    std::filesystem::remove(state.temp_file_path);
    // 同步文件元信息到transfer_key下
    file_transfers_[transfer_key] = std::move(state);
    return true;
}

} // namespace training::service

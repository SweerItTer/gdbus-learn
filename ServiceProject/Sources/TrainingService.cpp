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
        self->ResetFileTransfer(true);
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
    
    if (!file_transfer_.active) {
        if (chunk_index != 0) {
            return false;
        }

        file_transfer_.active = true;
        file_transfer_.owner_sender = sender;
        file_transfer_.transfer_id = transfer_id;
        file_transfer_.file_name = file_name;
        file_transfer_.total_size = total_size;
        file_transfer_.chunk_count = chunk_count; // 切片总数
        file_transfer_.next_expected_chunk = 0;
        file_transfer_.received_size = 0;
        file_transfer_.expected_md5 = md5_hex;
        file_transfer_.temp_file_path = utils::GetExecutableDir() / ("." + transfer_id + ".part");
        std::filesystem::remove(file_transfer_.temp_file_path); // 直接删除已经存在的文件(覆盖)
    } else if (file_transfer_.owner_sender != sender) {
        // 只允许一个活跃传输，后来者直接拒绝
        return false;
    }

    // 发送中断(sender一致但是数据对不上)
    if (file_transfer_.transfer_id != transfer_id ||
        file_transfer_.file_name != file_name ||
        file_transfer_.total_size != total_size ||
        file_transfer_.chunk_count != chunk_count ||
        file_transfer_.expected_md5 != md5_hex) {
        // 删除旧文件
        ResetFileTransfer(true);
        return false;
    }

    // 校验下一段数据标号
    if (chunk_index != file_transfer_.next_expected_chunk) {
        ResetFileTransfer(true);
        return false;
    }

    // 校验总大小
    if (file_transfer_.received_size + chunk_size > file_transfer_.total_size) {
        ResetFileTransfer(true);
        return false;
    }

    try {
        // 只读共享内存，并且获取共享内存对象的fd
        auto shm_fd = utils::OpenSharedMemory(shm_name, O_RDONLY);
        // 只读映射共享内存数据
        auto mapped = utils::MapSharedMemory(shm_fd.Get(), utils::kFileChunkSize, PROT_READ);

        // 打开.part文件
        std::ofstream output(file_transfer_.temp_file_path, std::ios::binary | std::ios::app);
        if (!output.is_open()) {
            ResetFileTransfer(true);
            return false;
        }
        // 读取chunk_size长度的内存数据追加到.part文件
        output.write(static_cast<const char*>(mapped.Get()), static_cast<std::streamsize>(chunk_size));
        if (!output.good()) {
            ResetFileTransfer(true);
            return false;
        }
    } catch (...) {
        ResetFileTransfer(true);
        return false;
    }

    // 更新接受的数据
    file_transfer_.received_size += chunk_size;
    // 更新下一判断标号
    file_transfer_.next_expected_chunk += 1;

    // 最后一包到达
    if (chunk_index + 1 == chunk_count) {
        try {
            // 校验和重命名 (test.txt.part->text.txt)
            const bool finalized = FinalizeFileTransfer();
            ResetFileTransfer(!finalized);
            return finalized;
        } catch (...) {
            ResetFileTransfer(true);
            return false;
        }
    }

    return true;
}

bool TrainingService::FinalizeFileTransfer() {
    // 校验接收数据总量大小
    if (file_transfer_.received_size != file_transfer_.total_size) {
        return false;
    }

    // 使用工具完成MD5校验
    const std::string actual_md5 = utils::ComputeMd5(file_transfer_.temp_file_path);
    if (actual_md5 != file_transfer_.expected_md5) {
        return false;
    }

    // 获取可执行文件根目录路径
    const auto target_path = utils::GetExecutableDir() / file_transfer_.file_name;
    std::filesystem::remove(target_path);
    std::filesystem::rename(file_transfer_.temp_file_path, target_path);
    return true;
}

// 删除旧文件并重置状态
void TrainingService::ResetFileTransfer(bool remove_temp_file) {
    if (remove_temp_file && !file_transfer_.temp_file_path.empty()) {
        std::filesystem::remove(file_transfer_.temp_file_path);
    }
    file_transfer_ = FileTransferState{};
}

} // namespace training::service

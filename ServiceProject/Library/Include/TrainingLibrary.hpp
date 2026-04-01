#pragma once

#include <public/DbusConstants.hpp>
#include <public/TrainingLibraryApi.hpp>
#include <utils/FileTransferUtils.hpp>
#include <utils/GLibWrappers.hpp>

#include "training-generated.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

namespace training::library {

// 包含创建proxy、注册槽函数、调用method、槽函数转发和实现
class TrainingLibraryClient {
public:
    TrainingLibraryClient();

    // 注册上层监听器。这里仅缓存回调，不直接和 D-Bus 信号做强绑定。
    void SetListener(const TrainingListenerCallbacks* callbacks);

    // 普通 Set/Get 能力仍然保持原有接口。
    bool SetTestBool(bool param);
    bool SetTestInt(int param);
    bool SetTestDouble(double param);
    bool SetTestString(const char* param);
    bool SetTestInfo(const public_api::TestInfo* param);

    bool GetTestBool(bool* result);
    bool GetTestInt(int* result);
    bool GetTestDouble(double* result);
    bool GetTestString(const char** result);
    bool GetTestInfo(public_api::TestInfo* result);
    // 发送内存 buffer 时，调用方需要提供逻辑文件名，并可额外指定服务端相对路径。
    bool SendFileBuffer(const unsigned char* file_buf,
                        std::size_t file_size,
                        const char* file_name,
                        const char* remote_relative_path);
    // 发送本地文件路径，remote_relative_path 为空时退化为 basename 上传。
    bool SendFilePath(const char* file_path, const char* remote_relative_path);
    // 从服务端相对路径下载文件到本地指定路径。
    bool DownloadFile(const char* remote_relative_path, const char* local_file_path);
    void PumpEvents();

private:
    static void OnRemoteTestBoolChanged(Training* proxy, gboolean param, gpointer user_data);
    static void OnRemoteTestIntChanged(Training* proxy, gint param, gpointer user_data);
    static void OnRemoteTestDoubleChanged(Training* proxy, gdouble param, gpointer user_data);
    static void OnRemoteTestStringChanged(Training* proxy, const gchar* param, gpointer user_data);
    static void OnRemoteTestInfoChanged(Training* proxy, GVariant* param, gpointer user_data);
    // Upload 内部统一走这一条分片发送通道：
    // 先把每片写进共享内存，再同步发一个 D-Bus method 告诉服务端去读这一片。
    bool SendChunks(const std::string& file_name,
                    const std::string& remote_relative_path,
                    std::uint64_t total_size,
                    const std::string& md5_hex,
                    const std::function<std::size_t(unsigned char*, std::size_t)>& reader);

    utils::UniqueGObject<Training> proxy_{nullptr};

    // info 本体
    public_api::TestInfo cached_info_{};

    std::mutex mutex_;
    TrainingListenerCallbacks listener_{};
    std::string last_string_result_{};
};

} // namespace training::library

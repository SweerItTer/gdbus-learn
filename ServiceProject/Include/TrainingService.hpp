#pragma once

#include <public/ITestService.hpp>
#include <utils/FileTransferUtils.hpp>
#include <utils/GLibWrappers.hpp>
#include <utils/ScopedBusConnection.hpp>
#include <utils/ScopedBusNameOwner.hpp>

#include "training-generated.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace training::service {

class TrainingService : public public_api::ITestService {
public:
    TrainingService();
    ~TrainingService();

    TrainingService(const TrainingService&) = delete;
    TrainingService& operator=(const TrainingService&) = delete;

    void Run();

    bool SetTestBool(bool param) override;
    bool SetTestInt(int param) override;
    bool SetTestDouble(double param) override;
    bool SetTestString(std::string param) override;
    bool SetTestInfo(public_api::TestInfo param) override;
    bool GetTestBool() override;
    int GetTestInt() override;
    double GetTestDouble() override;
    std::string GeTestString() override;
    public_api::TestInfo GetTestInfo() override;
    bool SendFile(unsigned char* file_buf, size_t file_size) override;

private:
    static gboolean OnHandleSetTestBool(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gboolean param,
                                        gpointer user_data);
    static gboolean OnHandleSetTestInt(Training* object,
                                       GDBusMethodInvocation* invocation,
                                       gint param,
                                       gpointer user_data);
    static gboolean OnHandleSetTestDouble(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gdouble param,
                                          gpointer user_data);
    static gboolean OnHandleSetTestString(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          const gchar* param,
                                          gpointer user_data);
    static gboolean OnHandleSetTestInfo(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        GVariant* param,
                                        gpointer user_data);
    static gboolean OnHandleGetTestBool(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gpointer user_data);
    static gboolean OnHandleGetTestInt(Training* object,
                                       GDBusMethodInvocation* invocation,
                                       gpointer user_data);
    static gboolean OnHandleGetTestDouble(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gpointer user_data);
    static gboolean OnHandleGetTestString(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          gpointer user_data);
    static gboolean OnHandleGetTestInfo(Training* object,
                                        GDBusMethodInvocation* invocation,
                                        gpointer user_data);
    static gboolean OnHandleSendFileChunk(Training* object,
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
                                          gpointer user_data);
    static gboolean OnHandleBeginFileDownload(Training* object,
                                              GDBusMethodInvocation* invocation,
                                              const gchar* remote_relative_path,
                                              gpointer user_data);
    static gboolean OnHandleReadFileChunk(Training* object,
                                          GDBusMethodInvocation* invocation,
                                          const gchar* transfer_id,
                                          guint chunk_index,
                                          const gchar* shm_name,
                                          gpointer user_data);

    struct FileTransferState {
        std::string owner_sender{};
        std::string transfer_id{};
        std::string transfer_key{};
        std::string file_name{};
        std::filesystem::path relative_path{};
        std::uint64_t total_size = 0;
        std::uint32_t chunk_count = 0;
        std::uint32_t next_expected_chunk = 0;
        std::uint64_t received_size = 0;
        std::string expected_md5{};
        std::filesystem::path temp_file_path{};
    };

    struct DownloadTransferState {
        std::string owner_sender{};
        std::string transfer_id{};
        std::string transfer_key{};
        std::string file_name{};
        std::filesystem::path relative_path{};
        std::filesystem::path source_file_path{};
        std::uint64_t total_size = 0;
        std::uint32_t chunk_count = 0;
        std::uint32_t next_expected_chunk = 0;
        std::string expected_md5{};
    };

    bool HandleIncomingFileChunk(const std::string& sender,
                                 const std::string& transfer_id,
                                 const std::string& file_name,
                                 const std::string& target_relative_path,
                                 std::uint64_t total_size,
                                 std::uint32_t chunk_index,
                                 std::uint32_t chunk_count,
                                 std::uint32_t chunk_size,
                                 const std::string& md5_hex,
                                 const std::string& shm_name);
    void ResetFileTransfer(const std::string& transfer_key, bool remove_temp_file);
    bool FinalizeFileTransfer(const std::string& transfer_key);
    bool PrepareTransferState(const std::string& transfer_key,
                              const std::string& sender,
                              const std::string& transfer_id,
                              const std::string& file_name,
                              const std::filesystem::path& relative_path,
                              std::uint64_t total_size,
                              std::uint32_t chunk_count,
                              const std::string& md5_hex);
    DownloadTransferState BeginFileDownload(const std::string& sender,
                                            const std::string& remote_relative_path);
    std::uint32_t ReadFileChunk(const std::string& sender,
                                const std::string& transfer_id,
                                std::uint32_t chunk_index,
                                const std::string& shm_name);
    void ResetDownloadTransfer(const std::string& transfer_key);

    utils::UniqueMainLoop loop_;
    utils::ScopedBusConnection connection_;
    utils::ScopedBusNameOwner bus_name_owner_;
    utils::UniqueGObject<Training> skeleton_;
    public_api::TestInfo state_{false, 0, 0.0, {}};
    std::mutex file_transfer_mutex_;
    std::unordered_map<std::string, FileTransferState> file_transfers_{};
    std::unordered_map<std::string, std::string> file_name_claims_{};
    std::mutex download_transfer_mutex_;
    std::unordered_map<std::string, DownloadTransferState> download_transfers_{};
};

} // namespace training::service

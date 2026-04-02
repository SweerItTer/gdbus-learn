#pragma once

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace training::utils {

// 文件分片大小固定为 1KB。
// 上传和下载都围绕这块固定大小的共享内存做“控制面走 D-Bus、数据面走共享内存”的协议。
inline constexpr std::size_t kFileChunkSize = 1024;

// 对 shell 命令参数做最小转义，当前只服务于 md5sum 调用。
// 这里不尝试做通用 shell escaping，只覆盖当前工程会遇到的单引号场景。
inline std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

// 执行一个外部命令并把标准输出完整读回。
// md5sum 在当前工程里已经足够稳定，因此这里直接把它封装成同步调用。
inline std::string ReadCommandOutput(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("failed to run command: " + command);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    const int status = pclose(pipe.release());
    if (status != 0) {
        throw std::runtime_error("command failed: " + command);
    }

    return output;
}

// 统一的 MD5 计算入口。
// 上传前客户端先算一次，服务端组包完成后再算一次，两边结果必须完全一致。
inline std::string ComputeMd5(const std::filesystem::path& file_path) {
    // 使用 md5sum 工具
    const std::string output = ReadCommandOutput("md5sum " + ShellQuote(file_path.string()));
    const auto space = output.find(' ');
    if (space == std::string::npos) {
        throw std::runtime_error("unexpected md5sum output");
    }
    return output.substr(0, space);
}

// 获取当前进程可执行文件所在目录。
// 这里优先读取 /proc/self/exe 的符号链接，避免 canonical 在某些临时测试环境下过于严格。
inline std::filesystem::path GetExecutableDir() {
    std::error_code error;
    const auto symlink_path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !symlink_path.empty()) {
        return symlink_path.parent_path();
    }

    const auto canonical_path = std::filesystem::canonical("/proc/self/exe", error);
    if (!error && !canonical_path.empty()) {
        return canonical_path.parent_path();
    }

    throw std::runtime_error("failed to resolve executable directory from /proc/self/exe");
}

// 约定服务端所有文件都位于 server 可执行文件同级目录下的 file/ 子目录。
// 上传最终落盘和下载源文件查找都必须从这里开始拼接。
inline std::filesystem::path GetServiceFileRoot() {
    return GetExecutableDir() / "file";
}

// 把外部传入的“服务端相对文件路径”规范化成一个安全的相对路径。
// 允许 ./a/b.txt、/a/b.txt、a/b.txt 这类写法，但统一剥掉根语义，只保留 file/ 下的相对路径。
// 任何 .. 越界、空路径、目录路径都会在这里被拒绝。
inline std::filesystem::path NormalizeRelativeFilePath(const std::string& raw_path) {
    std::filesystem::path normalized;
    bool saw_component = false;

    for (const auto& part : std::filesystem::path(raw_path)) {
        const std::string value = part.string();
        if (value.empty() || value == "." || value == "/") {
            continue;
        }
        if (value == "..") {
            throw std::runtime_error("path must stay under service file root");
        }
        normalized /= part;
        saw_component = true;
    }

    normalized = normalized.lexically_normal();
    if (!saw_component || normalized.empty() || normalized.filename().empty()) {
        throw std::runtime_error("path must point to a file under service file root");
    }

    return normalized;
}

// 把规范化后的相对路径拼到固定服务端文件根目录下，得到真实文件路径。
inline std::filesystem::path GetServiceFilePath(const std::string& raw_relative_path) {
    return GetServiceFileRoot() / NormalizeRelativeFilePath(raw_relative_path);
}

// 确保目标文件所在的父目录存在。
// 上传最终 rename 前、客户端下载落盘前都要先调用它。
inline void EnsureParentDirectory(const std::filesystem::path& file_path) {
    const auto parent = file_path.parent_path();
    if (parent.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        throw std::runtime_error("failed to create directory " + parent.string() + ": " + error.message());
    }
}

// 以“若存在则删除”的语义删除文件，避免上层反复写重复的 std::error_code 逻辑。
// 对“不存在”视为成功，对其他错误转成异常交给调用方决定是否中断流程。
inline void RemoveIfExists(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("failed to remove path " + path.string() + ": " + error.message());
    }
}

// 通过 rename 把临时文件原子替换到最终位置。
// 这里依赖 Linux/POSIX 语义：同一文件系统内 rename 会直接替换已有普通文件，不需要先删旧文件。
inline void ReplaceFileAtomically(const std::filesystem::path& source_path,
                                  const std::filesystem::path& target_path) {
    std::error_code error;
    if (std::filesystem::exists(target_path, error) && !error &&
        std::filesystem::is_directory(target_path, error) && !error) {
        throw std::runtime_error("target path points to a directory: " + target_path.string());
    }

    error.clear();
    std::filesystem::rename(source_path, target_path, error);
    if (error) {
        throw std::runtime_error("failed to replace path " + target_path.string() + ": " + error.message());
    }
}

// 简单的路径清理守卫。
// 主要用于临时文件：默认析构时删除，只有在流程完全成功后才显式 Cancel。
class ScopedPathCleanup {
public:
    ScopedPathCleanup() = default;
    explicit ScopedPathCleanup(std::filesystem::path path) : path_(std::move(path)) {}

    ~ScopedPathCleanup() {
        if (!active_ || path_.empty()) {
            return;
        }

        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    ScopedPathCleanup(const ScopedPathCleanup&) = delete;
    ScopedPathCleanup& operator=(const ScopedPathCleanup&) = delete;

    ScopedPathCleanup(ScopedPathCleanup&& other) noexcept
        : path_(std::move(other.path_)), active_(other.active_) {
        other.active_ = false;
    }

    ScopedPathCleanup& operator=(ScopedPathCleanup&& other) noexcept {
        if (this != &other) {
            path_ = std::move(other.path_);
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    void Cancel() { active_ = false; }

private:
    std::filesystem::path path_{};
    bool active_{true};
};

// 从文件中读取一个指定偏移和长度的分片。
// 下载时服务端按 chunk_index 读取这一段，再写入共享内存给客户端消费。
inline std::vector<unsigned char> ReadFileChunk(const std::filesystem::path& path,
                                                std::uint64_t offset,
                                                std::size_t size) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open file " + path.string());
    }

    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input.good()) {
        throw std::runtime_error("failed to seek file " + path.string());
    }

    std::vector<unsigned char> bytes(size, 0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    const auto actual_size = static_cast<std::size_t>(input.gcount());
    bytes.resize(actual_size);
    return bytes;
}

// 文件描述符 RAII 封装，避免异常路径遗忘 close。
class ScopedFd {
public:
    ScopedFd() = default;
    explicit ScopedFd(int fd) : fd_(fd) {}
    ~ScopedFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    ScopedFd& operator=(ScopedFd&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int Get() const { return fd_; }

private:
    int fd_ = -1;
};

// mmap 结果的 RAII 封装，保证异常和早返回时都会自动 munmap。
class ScopedMappedMemory {
public:
    ScopedMappedMemory() = default;
    ScopedMappedMemory(void* data, std::size_t size) : data_(data), size_(size) {}
    ~ScopedMappedMemory() {
        if (data_ != nullptr && data_ != MAP_FAILED) {
            munmap(data_, size_);
        }
    }

    ScopedMappedMemory(const ScopedMappedMemory&) = delete;
    ScopedMappedMemory& operator=(const ScopedMappedMemory&) = delete;

    ScopedMappedMemory(ScopedMappedMemory&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    ScopedMappedMemory& operator=(ScopedMappedMemory&& other) noexcept {
        if (this != &other) {
            if (data_ != nullptr && data_ != MAP_FAILED) {
                munmap(data_, size_);
            }
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    [[nodiscard]] void* Get() const { return data_; }

private:
    void* data_ = nullptr;
    std::size_t size_ = 0;
};

// 打开一块 POSIX 共享内存。
// 上传时客户端以创建者身份读写，服务端只读；下载时反过来由服务端写、客户端读。
inline ScopedFd OpenSharedMemory(const std::string& shm_name, int oflag, mode_t mode = 0600) {
    const int fd = shm_open(shm_name.c_str(), oflag, mode);
    if (fd < 0) {
        throw std::runtime_error("failed to open shared memory " + shm_name + ": " + std::strerror(errno));
    }
    return ScopedFd(fd);
}

// 把共享内存映射到当前进程地址空间。
inline ScopedMappedMemory MapSharedMemory(int fd, std::size_t size, int prot) {
    void* address = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (address == MAP_FAILED) {
        throw std::runtime_error(std::string("failed to mmap shared memory: ") + std::strerror(errno));
    }
    return ScopedMappedMemory(address, size);
}

// 把共享内存扩成约定好的固定块大小。
inline void ResizeSharedMemory(int fd, std::size_t size) {
    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        throw std::runtime_error(std::string("failed to resize shared memory: ") + std::strerror(errno));
    }
}

// 删除共享内存名字。
// 真正的底层对象在最后一个 fd / 映射释放后才会消失，这里只负责撤销名字。
inline void UnlinkSharedMemory(const std::string& shm_name) {
    shm_unlink(shm_name.c_str());
}

} // namespace training::utils

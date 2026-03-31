#pragma once

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace training::utils {

// 共享内存单片大小：
// client 每次只往这 1KB 区域里写一个分片，
// service 每次也只按当前 chunk_size 读取这一片。
inline constexpr std::size_t kFileChunkSize = 1024;

// 为 shell 命令做最小转义。
// 当前主要给 md5sum 这种工具调用服务，避免路径里有空格或单引号时命令失效。
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

// 执行外部命令并抓取标准输出。
// 当前调用栈：
// client/service -> ComputeMd5 -> ReadCommandOutput -> md5sum
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

// 计算完整文件的 md5。
// client 侧在“发送前”调用一次，用于生成 expected_md5；
// service 侧在“组包完成后”调用一次，用于校验最终文件。
inline std::string ComputeMd5(const std::filesystem::path& file_path) {
    const std::string output = ReadCommandOutput("md5sum " + ShellQuote(file_path.string()));
    const auto space = output.find(' ');
    if (space == std::string::npos) {
        throw std::runtime_error("unexpected md5sum output");
    }
    return output.substr(0, space);
}

// 获取当前进程可执行文件所在目录。
// service 侧用它决定最终落盘目录和 .part 临时文件目录。
inline std::filesystem::path GetExecutableDir() {
    return std::filesystem::canonical("/proc/self/exe").parent_path();
}

// fd 的最小 RAII 封装。
// 主要给 shm_open 返回值使用，避免异常路径忘记 close。
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

// mmap 返回区域的最小 RAII 封装。
// client 用它持有 1KB 共享内存映射并写入分片；
// service 用它持有同一块共享内存映射并读取分片。
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
// client 一般用 O_CREAT | O_RDWR | O_TRUNC 创建；
// service 一般用 O_RDONLY 打开 client 已创建好的同名共享内存。
inline ScopedFd OpenSharedMemory(const std::string& shm_name, int oflag, mode_t mode = 0600) {
    const int fd = shm_open(shm_name.c_str(), oflag, mode);
    if (fd < 0) {
        throw std::runtime_error("failed to open shared memory " + shm_name + ": " + std::strerror(errno));
    }
    return ScopedFd(fd);
}

// 将共享内存映射到当前进程地址空间。
// 返回的地址仅在当前进程内有效，不会跨进程传递；
// 真正跨进程共享的是 shm_name 对应的那块共享内存对象。
inline ScopedMappedMemory MapSharedMemory(int fd, std::size_t size, int prot) {
    void* address = mmap(nullptr, size, prot, MAP_SHARED, fd, 0);
    if (address == MAP_FAILED) {
        throw std::runtime_error(std::string("failed to mmap shared memory: ") + std::strerror(errno));
    }
    return ScopedMappedMemory(address, size);
}

// 调整共享内存大小。
// 当前固定扩成 1KB，保证每次可以容纳一个分片。
inline void ResizeSharedMemory(int fd, std::size_t size) {
    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        throw std::runtime_error(std::string("failed to resize shared memory: ") + std::strerror(errno));
    }
}

// 删除共享内存名字。
// 当前由 client 在发送完成或失败时做最终清理；
// service 只负责读取，不负责销毁 client 创建的共享内存对象。
inline void UnlinkSharedMemory(const std::string& shm_name) {
    shm_unlink(shm_name.c_str());
}

} // namespace training::utils

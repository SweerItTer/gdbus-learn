#include <utils/ScopedBusNameOwner.hpp>

#include <utils/GLibWrappers.hpp>

#include <stdexcept>
#include <string>

namespace training::utils {

namespace {

// DBus 服务
constexpr const char* kDbusService = "org.freedesktop.DBus";
constexpr const char* kDbusPath = "/org/freedesktop/DBus";
constexpr const char* kDbusInterface = "org.freedesktop.DBus";
constexpr const char* kRequestNameMethod = "RequestName";

// 失败直接返回
constexpr guint32 kRequestNameDoNotQueue = 4;
// 申请成功
constexpr guint32 kRequestNameReplyPrimaryOwner = 1;
// 已经占用
constexpr guint32 kRequestNameReplyAlreadyOwner = 4;

// - DO_NOT_QUEUE = 4
// - PRIMARY_OWNER = 1
// - ALREADY_OWNER = 4

} // namespace

ScopedBusNameOwner::ScopedBusNameOwner(ScopedBusNameOwner &&other) noexcept
    : owner_id_(other.owner_id_)
{
    other.owner_id_ = 0;
}

ScopedBusNameOwner::~ScopedBusNameOwner() = default; // 不再手动关闭连接

ScopedBusNameOwner& ScopedBusNameOwner::operator=(ScopedBusNameOwner&& other) noexcept {
    if (this != &other) {
        owner_id_ = other.owner_id_;
        other.owner_id_ = 0;
    }

    return *this;
}

void ScopedBusNameOwner::Acquire(GDBusConnection* connection, const char* bus_name) {
    if (owner_id_ != 0) {
        throw std::logic_error("ScopedBusNameOwner has already acquired a bus name");
    }

    GError* raw_error = nullptr;
    UniqueGVariant reply{g_dbus_connection_call_sync(connection,
                                                      // 调用 DBus 方法
                                                      kDbusService,
                                                      kDbusPath,
                                                      kDbusInterface,
                                                      kRequestNameMethod,
                                                      // 请求占用 bus_name
                                                      g_variant_new("(su)", bus_name, kRequestNameDoNotQueue),
                                                      G_VARIANT_TYPE("(u)"),
                                                      G_DBUS_CALL_FLAGS_NONE,
                                                      -1,
                                                      nullptr,
                                                      &raw_error)};
    UniqueGError error(raw_error);

    if (reply == nullptr) {
        const std::string message = error != nullptr ? error->message : "unknown bus name ownership error";
        throw std::runtime_error("failed to request D-Bus name \"" + std::string(bus_name) + "\": " + message);
    }

    guint32 request_result = 0;
    g_variant_get(reply.get(), "(u)", &request_result); // 获取结果
    if (request_result != kRequestNameReplyPrimaryOwner && request_result != kRequestNameReplyAlreadyOwner) {
        // 申请失败/冲突
        throw std::runtime_error("failed to own D-Bus name \"" + std::string(bus_name) +
                                 "\": name already owned or denied by bus policy"); 
                                 // 确保 polity文件正确安装到/usr/share/dbus-1/system.d
    }

    // dbus 连接生命周期由system管理
    owner_id_ = 1;
}

guint ScopedBusNameOwner::Get() const {
    return owner_id_;
}

} // namespace training::utils

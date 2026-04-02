#include <utils/ScopedBusNameOwner.hpp>

#include <utils/GLibWrappers.hpp>

#include <stdexcept>
#include <string>

namespace training::utils {

namespace {

// RequestName 调用参数。
constexpr const char* kDbusService = "org.freedesktop.DBus";
constexpr const char* kDbusPath = "/org/freedesktop/DBus";
constexpr const char* kDbusInterface = "org.freedesktop.DBus";
constexpr const char* kRequestNameMethod = "RequestName";

// 不排队，抢不到就直接失败。
constexpr guint32 kRequestNameDoNotQueue = 4;
// 已成为主拥有者。
constexpr guint32 kRequestNameReplyPrimaryOwner = 1;
// 已经持有该名字。
constexpr guint32 kRequestNameReplyAlreadyOwner = 4;

} // namespace

ScopedBusNameOwner::ScopedBusNameOwner(ScopedBusNameOwner &&other) noexcept
    : owner_id_(other.owner_id_)
{
    // 移动时转移占用标记。
    other.owner_id_ = 0;
}

ScopedBusNameOwner::~ScopedBusNameOwner() = default;

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
                                                      kDbusService,
                                                      kDbusPath,
                                                      kDbusInterface,
                                                      kRequestNameMethod,
                                                      // 同步申请 bus name。
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
    g_variant_get(reply.get(), "(u)", &request_result);
    // 只接受主拥有者或已拥有这两种结果。
    if (request_result != kRequestNameReplyPrimaryOwner && request_result != kRequestNameReplyAlreadyOwner) {
        throw std::runtime_error("failed to own D-Bus name \"" + std::string(bus_name) +
                                 "\": name already owned or denied by bus policy");
    }

    // 标记已成功占用。
    owner_id_ = 1;
}

guint ScopedBusNameOwner::Get() const {
    return owner_id_;
}

} // namespace training::utils

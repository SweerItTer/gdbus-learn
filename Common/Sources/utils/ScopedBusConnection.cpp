#include <utils/ScopedBusConnection.hpp>

#include <stdexcept>
#include <string>

namespace training::utils {

ScopedBusConnection::ScopedBusConnection(GBusType bus_type) {
    GError* raw_error = nullptr;
    // 根据连接类型 bus_type 创建 GDBusConnection 
    connection_.reset(g_bus_get_sync(bus_type, nullptr, &raw_error));
    UniqueGError error(raw_error);

    if (connection_ == nullptr) {
        const std::string message = error != nullptr ? error->message : "unknown bus connection error";
        throw std::runtime_error("failed to connect to D-Bus: " + message);
    }
}

// 获取原始 GDBusConnection
GDBusConnection* ScopedBusConnection::Get() const {
    return connection_.get();
}

} // namespace training::utils

#pragma once

#include <utils/GLibWrappers.hpp>

#include <gio/gio.h>

namespace training::utils {

class ScopedBusConnection {
public:
    explicit ScopedBusConnection(GBusType bus_type);

    ScopedBusConnection(const ScopedBusConnection&) = delete;
    ScopedBusConnection& operator=(const ScopedBusConnection&) = delete;

    ScopedBusConnection(ScopedBusConnection&&) noexcept = default;
    ScopedBusConnection& operator=(ScopedBusConnection&&) noexcept = default;

    [[nodiscard]] GDBusConnection* Get() const;

private:
    // RAII 封装
    UniqueGObject<GDBusConnection> connection_;
};

} // namespace training::utils

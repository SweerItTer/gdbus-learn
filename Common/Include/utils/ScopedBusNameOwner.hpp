#pragma once

#include <gio/gio.h>

namespace training::utils {

class ScopedBusNameOwner {
public:
    ScopedBusNameOwner() = default;
    ~ScopedBusNameOwner();

    ScopedBusNameOwner(const ScopedBusNameOwner&) = delete;
    ScopedBusNameOwner& operator=(const ScopedBusNameOwner&) = delete;

    ScopedBusNameOwner(ScopedBusNameOwner&& other) noexcept;
    ScopedBusNameOwner& operator=(ScopedBusNameOwner&& other) noexcept;

    void Acquire(GDBusConnection* connection, const char* bus_name);
    [[nodiscard]] guint Get() const;

private:
    guint owner_id_ = 0;
};

} // namespace training::utils

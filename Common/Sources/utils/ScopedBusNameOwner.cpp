#include <utils/ScopedBusNameOwner.hpp>

#include <utils/GLibWrappers.hpp>

#include <stdexcept>
#include <string>

namespace training::utils {

ScopedBusNameOwner::~ScopedBusNameOwner() {
    if (owner_id_ != 0) {
        g_bus_unown_name(owner_id_);
    }
}

ScopedBusNameOwner::ScopedBusNameOwner(ScopedBusNameOwner&& other) noexcept
    : owner_id_(other.owner_id_) {
    other.owner_id_ = 0;
}

ScopedBusNameOwner& ScopedBusNameOwner::operator=(ScopedBusNameOwner&& other) noexcept {
    if (this != &other) {
        if (owner_id_ != 0) {
            g_bus_unown_name(owner_id_);
        }
        owner_id_ = other.owner_id_;
        other.owner_id_ = 0;
    }

    return *this;
}

void ScopedBusNameOwner::Acquire(GDBusConnection* connection, const char* bus_name) {
    if (owner_id_ != 0) {
        throw std::logic_error("ScopedBusNameOwner has already acquired a bus name");
    }

    owner_id_ = g_bus_own_name_on_connection(connection,
                                             bus_name,
                                             G_BUS_NAME_OWNER_FLAGS_NONE,
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             nullptr);

    if (owner_id_ == 0) {
        throw std::runtime_error("failed to own D-Bus name on existing connection");
    }
}

guint ScopedBusNameOwner::Get() const {
    return owner_id_;
}

} // namespace training::utils

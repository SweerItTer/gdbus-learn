#include <utils/ScopedExportedInterface.hpp>

namespace training::utils {

ScopedExportedInterface::~ScopedExportedInterface() {
    Reset();
}

ScopedExportedInterface::ScopedExportedInterface(ScopedExportedInterface&& other) noexcept
    : skeleton_(other.skeleton_) {
    other.skeleton_ = nullptr;
}

ScopedExportedInterface& ScopedExportedInterface::operator=(ScopedExportedInterface&& other) noexcept {
    if (this != &other) {
        Reset();
        skeleton_ = other.skeleton_;
        other.skeleton_ = nullptr;
    }

    return *this;
}

void ScopedExportedInterface::Reset(GDBusInterfaceSkeleton* skeleton) {
    if (skeleton_ != nullptr) {
        g_dbus_interface_skeleton_unexport(skeleton_);
    }

    skeleton_ = skeleton;
}

} // namespace training::utils

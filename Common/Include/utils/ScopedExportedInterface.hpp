#pragma once

#include <gio/gio.h>

namespace training::utils {

class ScopedExportedInterface {
public:
    ScopedExportedInterface() = default;
    ~ScopedExportedInterface();

    ScopedExportedInterface(const ScopedExportedInterface&) = delete;
    ScopedExportedInterface& operator=(const ScopedExportedInterface&) = delete;

    ScopedExportedInterface(ScopedExportedInterface&& other) noexcept;
    ScopedExportedInterface& operator=(ScopedExportedInterface&& other) noexcept;

    void Reset(GDBusInterfaceSkeleton* skeleton = nullptr);

private:
    GDBusInterfaceSkeleton* skeleton_ = nullptr;
};

} // namespace training::utils

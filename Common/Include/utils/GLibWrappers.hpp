#pragma once

#include <gio/gio.h>

#include <memory>

namespace training::utils {

struct GErrorDeleter {
    void operator()(GError* error) const {
        if (error != nullptr) {
            g_error_free(error);
        }
    }
};

struct GVariantDeleter {
    void operator()(GVariant* variant) const {
        if (variant != nullptr) {
            g_variant_unref(variant);
        }
    }
};

struct GCharDeleter {
    void operator()(gchar* value) const {
        if (value != nullptr) {
            g_free(value);
        }
    }
};

struct GMainLoopDeleter {
    void operator()(GMainLoop* loop) const {
        if (loop != nullptr) {
            g_main_loop_unref(loop);
        }
    }
};

template <typename T>
struct GObjectDeleter {
    void operator()(T* object) const {
        if (object != nullptr) {
            g_object_unref(object);
        }
    }
};

using UniqueGError = std::unique_ptr<GError, GErrorDeleter>;
using UniqueGVariant = std::unique_ptr<GVariant, GVariantDeleter>;
using UniqueGChar = std::unique_ptr<gchar, GCharDeleter>;
using UniqueMainLoop = std::unique_ptr<GMainLoop, GMainLoopDeleter>;

template <typename T>
using UniqueGObject = std::unique_ptr<T, GObjectDeleter<T>>;

} // namespace training::utils

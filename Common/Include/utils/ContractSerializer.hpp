#pragma once

#include <gio/gio.h>
#include <public/DbusConstants.hpp>

namespace training::utils {

// 反序列化
inline public_api::TestInfo VariantToTestInfo(GVariant* value) {
    public_api::TestInfo info{};
    gboolean bool_param = FALSE;
    gint int_param = 0;
    gdouble double_param = 0.0;
    const gchar* string_param = "";

    g_variant_get(value, "(bids)", &bool_param, &int_param, &double_param, &string_param);

    info.bool_param = static_cast<bool>(bool_param);
    info.int_param = int_param;
    info.double_param = double_param;
    info.string_param = string_param != nullptr ? string_param : "";
    return info;
}

// 序列化
inline GVariant* TestInfoToVariant(const public_api::TestInfo& info) {
    return g_variant_new("(bids)",
                         static_cast<gboolean>(info.bool_param),
                         static_cast<gint>(info.int_param),
                         static_cast<gdouble>(info.double_param),
                         info.string_param.c_str());
}

} // namespace training::utils

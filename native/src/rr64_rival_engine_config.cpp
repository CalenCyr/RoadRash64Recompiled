#include "rr64_rival_engine_config.hpp"
#include "rr64_rival_engine.hpp"
#include "librecomp/config.hpp"
#include <algorithm>
#include <cmath>

namespace rr64::rival_engine {
namespace {
double normalized_percent(double value) {
    return std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : default_volume_percent;
}
}

void configure_volume(recomp::config::Config &config) {
    config.add_percent_number_option(
        volume_option, "Rival Engine Volume",
        "Volume of nearby opponent bikes, including online players. 0 turns these engines off.",
        default_volume_percent);
    // Numeric UI bounds do not clamp manually edited JSON. Keep the stored
    // slider value and the engine gain consistent when loading older settings.
    config.on_json_parse_option(
        volume_option, [](const nlohmann::json &value) -> recomp::config::ConfigValueVariant {
            return value.is_number() ? normalized_percent(value.get<double>())
                                     : default_volume_percent;
        });
    config.on_json_serialize_option(
        volume_option, [](const recomp::config::ConfigValueVariant &value) -> nlohmann::json {
            return normalized_percent(std::get<double>(value));
        });
    config.add_option_change_callback(volume_option, [](auto value, auto, auto) {
        set_volume_percent(normalized_percent(std::get<double>(value)));
    });
}

void apply_volume_config(const recomp::config::Config &config) {
    // A first-run config uses schema defaults without firing load callbacks.
    set_volume_percent(
        normalized_percent(std::get<double>(config.get_option_value(volume_option))));
}
}

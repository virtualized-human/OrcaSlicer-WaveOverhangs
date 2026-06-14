#include "ObjectProcessPreset.hpp"

#include <algorithm>

namespace Slic3r {

static const char* s_process_to_object_map[][2] = {
    { "initial_layer_print_height", "object_initial_layer_height" },
    { "initial_layer_speed",        "object_initial_layer_speed" },
    { "initial_layer_line_width",   "object_initial_layer_line_width" },
};

const std::vector<std::string>& object_supported_keys()
{
    static const std::vector<std::string> keys = PrintObjectConfig().keys();
    return keys;
}

const std::vector<std::string>& region_supported_keys()
{
    static const std::vector<std::string> keys = PrintRegionConfig().keys();
    return keys;
}

const std::map<std::string, std::string>& process_to_object_key_map()
{
    static const std::map<std::string, std::string> mapping = [] {
        std::map<std::string, std::string> out;
        for (const auto &pair : s_process_to_object_map)
            out[pair[0]] = pair[1];
        return out;
    }();
    return mapping;
}

const std::map<std::string, std::string>& object_to_process_key_map()
{
    static const std::map<std::string, std::string> mapping = [] {
        std::map<std::string, std::string> out;
        for (const auto &pair : s_process_to_object_map)
            out[pair[1]] = pair[0];
        return out;
    }();
    return mapping;
}

static bool contains_key(const std::vector<std::string> &keys, const std::string &key)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

static bool is_region_override_key(const std::string &key)
{
    return contains_key(region_supported_keys(), key);
}

static bool is_object_override_key(const std::string &key)
{
    if (contains_key(object_supported_keys(), key) || is_region_override_key(key))
        return true;

    const auto &mapping = process_to_object_key_map();
    return mapping.find(key) != mapping.end();
}

static bool differs_from_default(const DynamicPrintConfig &config, const std::string &key)
{
    const ConfigOption *opt = config.option(key);
    const ConfigOptionDef *def = config.def()->get(key);
    return opt != nullptr && def != nullptr && def->default_value && *opt != *def->default_value;
}

static bool differs_from_baseline(
    const DynamicPrintConfig &local_config,
    const DynamicPrintConfig &global_baseline,
    const std::string        &local_key,
    const std::string        &baseline_key)
{
    const ConfigOption *local_opt = local_config.option(local_key);
    if (local_opt == nullptr)
        return false;

    if (global_baseline.has(baseline_key)) {
        const ConfigOption *base_opt = global_baseline.option(baseline_key);
        return base_opt == nullptr || *local_opt != *base_opt;
    }

    return differs_from_default(local_config, local_key);
}

static ObjectProcessPresetResult apply_process_preset(
    const DynamicPrintConfig &process_config,
    const DynamicPrintConfig &global_baseline,
    bool                      object_target)
{
    ObjectProcessPresetResult result;

    for (const std::string &key : process_config.keys()) {
        const auto &mapping = process_to_object_key_map();
        auto map_it = mapping.find(key);

        if (object_target && map_it != mapping.end()) {
            const std::string &obj_key = map_it->second;
            if (global_baseline.has(key)) {
                const ConfigOption *base_opt = global_baseline.option(key);
                const ConfigOption *src_opt  = process_config.option(key);
                if (base_opt && src_opt && *base_opt == *src_opt)
                    continue;
            }
            result.object_overrides.set_key_value(obj_key, process_config.option(key)->clone());
        } else if (object_target ? is_object_override_key(key) : is_region_override_key(key)) {
            if (global_baseline.has(key)) {
                const ConfigOption *base_opt = global_baseline.option(key);
                const ConfigOption *src_opt  = process_config.option(key);
                if (base_opt && src_opt && *base_opt == *src_opt)
                    continue;
            } else if (!differs_from_default(process_config, key)) {
                continue;
            }
            result.object_overrides.set_key_value(key, process_config.option(key)->clone());
        } else {
            result.skipped_count++;
        }
    }

    return result;
}

ObjectProcessPresetResult apply_process_preset_to_object(
    const DynamicPrintConfig &process_config,
    const DynamicPrintConfig &global_baseline)
{
    return apply_process_preset(process_config, global_baseline, true);
}

ObjectProcessPresetResult apply_process_preset_to_region(
    const DynamicPrintConfig &process_config,
    const DynamicPrintConfig &global_baseline)
{
    return apply_process_preset(process_config, global_baseline, false);
}

static DynamicPrintConfig build_process_preset(
    const DynamicPrintConfig &global_baseline,
    const DynamicPrintConfig &local_config,
    bool                      object_target)
{
    DynamicPrintConfig result;
    result.apply(global_baseline, true);

    for (const std::string &key : local_config.keys()) {
        const auto &reverse_mapping = object_to_process_key_map();
        auto map_it = reverse_mapping.find(key);

        if (object_target && map_it != reverse_mapping.end()) {
            result.set_key_value(map_it->second, local_config.option(key)->clone());
        } else if (object_target ? is_object_override_key(key) : is_region_override_key(key)) {
            result.set_key_value(key, local_config.option(key)->clone());
        }
    }

    return result;
}

DynamicPrintConfig build_process_preset_from_object(
    const DynamicPrintConfig &global_baseline,
    const DynamicPrintConfig &object_config)
{
    return build_process_preset(global_baseline, object_config, true);
}

DynamicPrintConfig build_process_preset_from_region(
    const DynamicPrintConfig &global_baseline,
    const DynamicPrintConfig &region_config)
{
    return build_process_preset(global_baseline, region_config, false);
}

DynamicPrintConfig extract_object_process_overrides(
    const DynamicPrintConfig &local_config,
    const DynamicPrintConfig &global_baseline,
    bool                     object_target)
{
    DynamicPrintConfig result;

    if (object_target) {
        for (const std::string &key : object_supported_keys()) {
            if (object_to_process_key_map().find(key) != object_to_process_key_map().end())
                continue;
            if (local_config.has(key) && differs_from_baseline(local_config, global_baseline, key, key))
                result.set_key_value(key, local_config.option(key)->clone());
        }

        for (const auto &pair : process_to_object_key_map()) {
            const std::string &process_key = pair.first;
            const std::string &object_key  = pair.second;

            if (local_config.has(object_key) && differs_from_baseline(local_config, global_baseline, object_key, process_key))
                result.set_key_value(object_key, local_config.option(object_key)->clone());
            else if (local_config.has(process_key) && differs_from_baseline(local_config, global_baseline, process_key, process_key))
                result.set_key_value(object_key, local_config.option(process_key)->clone());
        }
    }

    for (const std::string &key : region_supported_keys()) {
        if (local_config.has(key) && differs_from_baseline(local_config, global_baseline, key, key))
            result.set_key_value(key, local_config.option(key)->clone());
    }

    return result;
}

void erase_process_overrides(ModelConfig &config, bool object_target)
{
    if (object_target) {
        for (const std::string &key : object_supported_keys())
            config.erase(key);
        for (const auto &pair : process_to_object_key_map()) {
            config.erase(pair.first);
            config.erase(pair.second);
        }
    }

    for (const std::string &key : region_supported_keys())
        config.erase(key);
}

KeyMigrationResult migrate_legacy_object_keys(DynamicPrintConfig &process_config)
{
    KeyMigrationResult result;
    result.clean_config = process_config;

    const auto &reverse_mapping = object_to_process_key_map();

    for (const auto &pair : reverse_mapping) {
        const std::string &obj_key  = pair.first;
        const std::string &proc_key = pair.second;

        if (result.clean_config.has(obj_key)) {
            if (!result.clean_config.has(proc_key) && differs_from_default(result.clean_config, obj_key)) {
                result.clean_config.set_key_value(proc_key, result.clean_config.option(obj_key)->clone());
                result.migrated_count++;
            }
            result.clean_config.erase(obj_key);
        }
    }

    process_config = result.clean_config;
    return result;
}

} // namespace Slic3r

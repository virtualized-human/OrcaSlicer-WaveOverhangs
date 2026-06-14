#ifndef slic3r_ObjectProcessPreset_hpp_
#define slic3r_ObjectProcessPreset_hpp_

#include "PrintConfig.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace Slic3r {

static constexpr const char* OBJECT_PROCESS_PRESET_KEY = "object_process_preset";

struct ObjectProcessPresetResult {
    DynamicPrintConfig object_overrides;
    size_t             skipped_count = 0;
};

const std::vector<std::string>& object_supported_keys();
const std::vector<std::string>& region_supported_keys();

const std::map<std::string, std::string>& process_to_object_key_map();
const std::map<std::string, std::string>& object_to_process_key_map();

ObjectProcessPresetResult apply_process_preset_to_object(
    const DynamicPrintConfig &process_config,
    const DynamicPrintConfig &global_baseline);

ObjectProcessPresetResult apply_process_preset_to_region(
    const DynamicPrintConfig &process_config,
    const DynamicPrintConfig &global_baseline);

DynamicPrintConfig build_process_preset_from_object(
    const DynamicPrintConfig &global_baseline,
    const DynamicPrintConfig &object_config);

DynamicPrintConfig build_process_preset_from_region(
    const DynamicPrintConfig &global_baseline,
    const DynamicPrintConfig &region_config);

DynamicPrintConfig extract_object_process_overrides(
    const DynamicPrintConfig &local_config,
    const DynamicPrintConfig &global_baseline,
    bool                     object_target);

void erase_process_overrides(ModelConfig &config, bool object_target);

struct KeyMigrationResult {
    DynamicPrintConfig clean_config;
    size_t             migrated_count = 0;
};

KeyMigrationResult migrate_legacy_object_keys(DynamicPrintConfig &process_config);

} // namespace Slic3r

#endif

#pragma once

#include "bao.h"

// Parse bao.yaml into heap-allocated BaoConfig. Returns NULL on error.
BaoConfig *load_config(const char *path);

// Same as load_config but overrides profile before resolving prompts_dir/prompt file.
// Useful for `bao config --list --profile <name>` without editing bao.yaml.
BaoConfig *load_config_with_profile(const char *path, const char *profile_override);

void free_config(BaoConfig *cfg);

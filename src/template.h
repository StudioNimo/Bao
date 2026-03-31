#pragma once

#include "cJSON.h"

// Replace {{key}} in tmpl using string values from variables object. Returns malloc'ed string or NULL.
char *render_template(const char *tmpl, cJSON *variables);

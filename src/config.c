#include "config.h"

#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cfg_set(BaoConfig *cfg, const char *key, const char *val) {
  if (!key || !val) return;
  if (strcmp(key, "provider") == 0) {
    free(cfg->provider);
    cfg->provider = strdup(val);
    if (!cfg->provider) bao_die("out of memory");
  } else if (strcmp(key, "profile") == 0) {
    free(cfg->profile);
    cfg->profile = strdup(val);
    if (!cfg->profile) bao_die("out of memory");
  } else if (strcmp(key, "model") == 0) {
    free(cfg->model);
    cfg->model = strdup(val);
    if (!cfg->model) bao_die("out of memory");
  } else if (strcmp(key, "temperature") == 0) {
    cfg->temperature = strtod(val, NULL);
  } else if (strcmp(key, "max_tokens") == 0) {
    cfg->max_tokens = (int)strtol(val, NULL, 10);
  } else if (strcmp(key, "prompt_file") == 0 || strcmp(key, "prompt_filepath") == 0) {
    free(cfg->prompt_filepath);
    cfg->prompt_filepath = strdup(val);
    if (!cfg->prompt_filepath) bao_die("out of memory");
  } else if (strcmp(key, "prompts_dir") == 0) {
    free(cfg->prompts_dir);
    cfg->prompts_dir = strdup(val);
    if (!cfg->prompts_dir) bao_die("out of memory");
  } else if (strcmp(key, "dataset") == 0) {
    free(cfg->dataset_path);
    cfg->dataset_path = strdup(val);
    if (!cfg->dataset_path) bao_die("out of memory");
  }
}

static char *trim_inplace(char *s) {
  if (!s) return s;
  while (*s && isspace((unsigned char)*s)) s++;
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
  s[n] = 0;
  return s;
}

// Flat `key: value` lines only (same subset as typical bao.yaml). No libyaml required.
static int load_flat_yaml(FILE *f, BaoConfig *cfg) {
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    char *p = strchr(line, '#');
    if (p) *p = 0;
    trim_inplace(line);
    if (!line[0]) continue;

    char *colon = strchr(line, ':');
    if (!colon) continue;
    *colon = 0;
    char *key = trim_inplace(line);
    char *val = trim_inplace(colon + 1);
    if (!key[0]) continue;
    cfg_set(cfg, key, val);
  }
  return 0;
}

static int read_prompt_file(BaoConfig *cfg) {
  if (cfg->prompt_text) {
    free(cfg->prompt_text);
    cfg->prompt_text = NULL;
  }

  if (cfg->prompt_filepath && bao_file_exists(cfg->prompt_filepath)) {
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(cfg->prompt_filepath, &buf, &len) != 0) return -1;
    cfg->prompt_text = (char *)buf;
    return 0;
  }

  if (cfg->prompts_dir && bao_file_exists(cfg->prompts_dir)) {
    DIR *d = opendir(cfg->prompts_dir);
    if (!d) return -1;
    char *best = NULL;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (de->d_name[0] == '.') continue;
      if (!best || strcmp(de->d_name, best) < 0) {
        free(best);
        best = strdup(de->d_name);
        if (!best) {
          closedir(d);
          bao_die("out of memory");
        }
      }
    }
    closedir(d);
    if (!best) return -1;
    char *path = bao_strdup_printf("%s/%s", cfg->prompts_dir, best);
    free(best);
    if (!path) return -1;
    free(cfg->prompt_filepath);
    cfg->prompt_filepath = path;
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(cfg->prompt_filepath, &buf, &len) != 0) {
      free(path);
      return -1;
    }
    cfg->prompt_text = (char *)buf;
    return 0;
  }

  return -1;
}

BaoConfig *load_config(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;

  BaoConfig *cfg = (BaoConfig *)calloc(1, sizeof(BaoConfig));
  if (!cfg) {
    fclose(f);
    bao_die("out of memory");
  }
  cfg->temperature = 0.0;
  cfg->max_tokens = 4096;

  if (load_flat_yaml(f, cfg) != 0) {
    fclose(f);
    free_config(cfg);
    return NULL;
  }
  fclose(f);

  if (!cfg->model || !*cfg->model) {
    free_config(cfg);
    return NULL;
  }

  /* prompts_dir + profile -> prompts/<profile>/ (multi-provider layout) */
  if (cfg->prompts_dir && cfg->profile && strcmp(cfg->prompts_dir, "prompts") == 0) {
    char *nd = bao_strdup_printf("prompts/%s", cfg->profile);
    if (!nd) {
      free_config(cfg);
      return NULL;
    }
    free(cfg->prompts_dir);
    cfg->prompts_dir = nd;
  }

  if (read_prompt_file(cfg) != 0) {
    free_config(cfg);
    return NULL;
  }

  if (!cfg->prompt_text) {
    free_config(cfg);
    return NULL;
  }

  return cfg;
}

BaoConfig *load_config_with_profile(const char *path, const char *profile_override) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;

  BaoConfig *cfg = (BaoConfig *)calloc(1, sizeof(BaoConfig));
  if (!cfg) {
    fclose(f);
    bao_die("out of memory");
  }
  cfg->temperature = 0.0;
  cfg->max_tokens = 4096;

  if (load_flat_yaml(f, cfg) != 0) {
    fclose(f);
    free_config(cfg);
    return NULL;
  }
  fclose(f);

  if (profile_override && profile_override[0]) {
    free(cfg->profile);
    cfg->profile = strdup(profile_override);
    if (!cfg->profile) bao_die("out of memory");
  }

  if (!cfg->model || !*cfg->model) {
    free_config(cfg);
    return NULL;
  }

  /* prompts_dir + profile -> prompts/<profile>/ (multi-provider layout) */
  if (cfg->prompts_dir && cfg->profile && strcmp(cfg->prompts_dir, "prompts") == 0) {
    char *nd = bao_strdup_printf("prompts/%s", cfg->profile);
    if (!nd) {
      free_config(cfg);
      return NULL;
    }
    free(cfg->prompts_dir);
    cfg->prompts_dir = nd;
  }

  if (read_prompt_file(cfg) != 0) {
    free_config(cfg);
    return NULL;
  }
  if (!cfg->prompt_text) {
    free_config(cfg);
    return NULL;
  }
  return cfg;
}

void free_config(BaoConfig *cfg) {
  if (!cfg) return;
  free(cfg->provider);
  free(cfg->profile);
  free(cfg->model);
  free(cfg->prompt_filepath);
  free(cfg->prompts_dir);
  free(cfg->dataset_path);
  free(cfg->prompt_text);
  free(cfg);
}

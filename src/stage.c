#include "stage.h"

#include "bao.h"
#include "commits.h"
#include "config.h"
#include "util.h"

#include "cJSON.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int path_has_dotdot(const char *p) {
  return p && strstr(p, "..") != NULL;
}

static int path_is_unsafe(const char *p) {
  return !bao_is_safe_relpath(p);
}

static char *strip_dot_slash(const char *p) {
  while (p[0] == '.' && p[1] == '/') p += 2;
  return strdup(p ? p : "");
}

static int cmp_strptr(const void *a, const void *b) {
  const char *const *sa = (const char *const *)a;
  const char *const *sb = (const char *const *)b;
  return strcmp(*sa, *sb);
}

static int unique_sort(char **paths, size_t n) {
  if (n == 0) return 0;
  qsort(paths, n, sizeof(char *), cmp_strptr);
  size_t w = 0;
  for (size_t i = 0; i < n; i++) {
    if (w == 0 || strcmp(paths[i], paths[w - 1]) != 0) {
      paths[w++] = paths[i];
    } else {
      free(paths[i]);
    }
  }
  for (size_t i = w; i < n; i++) paths[i] = NULL;
  return (int)w;
}

int bao_staged_contains(const char *const *paths, size_t n, const char *path) {
  if (!path) return 0;
  char *norm = strip_dot_slash(path);
  if (!norm) return 0;
  for (size_t i = 0; i < n; i++) {
    if (paths[i] && strcmp(paths[i], norm) == 0) {
      free(norm);
      return 1;
    }
  }
  free(norm);
  return 0;
}

int bao_staged_load(char ***paths_out, size_t *count_out) {
  *paths_out = NULL;
  *count_out = 0;
  if (!bao_file_exists(BAO_STAGED_FILE)) return 0;

  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(BAO_STAGED_FILE, &buf, &len) != 0) return -1;

  size_t cap = 16, n = 0;
  char **paths = (char **)malloc(cap * sizeof(char *));
  if (!paths) {
    free(buf);
    return -1;
  }

  char *s = (char *)buf;
  while (*s) {
    char *line = s;
    while (*s && *s != '\n' && *s != '\r') s++;
    char saved = *s;
    *s = 0;
    char *t = line;
    while (*t == ' ' || *t == '\t') t++;
    char *end = t + strlen(t);
    while (end > t && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = 0;
    if (*t && !path_has_dotdot(t)) {
      char *copy = strip_dot_slash(t);
      if (!copy) {
        for (size_t i = 0; i < n; i++) free(paths[i]);
        free(paths);
        free(buf);
        return -1;
      }
      if (n == cap) {
        cap *= 2;
        char **np = (char **)realloc(paths, cap * sizeof(char *));
        if (!np) {
          free(copy);
          for (size_t i = 0; i < n; i++) free(paths[i]);
          free(paths);
          free(buf);
          return -1;
        }
        paths = np;
      }
      paths[n++] = copy;
    }
    *s = saved;
    if (*s == '\r' || *s == '\n') {
      s++;
      if (saved == '\r' && *s == '\n') s++;
    } else {
      break;
    }
  }
  free(buf);

  int u = unique_sort(paths, n);
  if (u < 0) {
    for (size_t i = 0; i < n; i++) free(paths[i]);
    free(paths);
    return -1;
  }
  *paths_out = paths;
  *count_out = (size_t)u;
  return 0;
}

void bao_staged_free(char **paths, size_t count) {
  if (!paths) return;
  for (size_t i = 0; i < count; i++) free(paths[i]);
  free(paths);
}

static int write_paths(char **paths, size_t n) {
  size_t total = 0;
  for (size_t i = 0; i < n; i++) total += strlen(paths[i]) + 1;
  char *buf = (char *)malloc(total + 1);
  if (!buf) return -1;
  size_t o = 0;
  for (size_t i = 0; i < n; i++) {
    memcpy(buf + o, paths[i], strlen(paths[i]));
    o += strlen(paths[i]);
    buf[o++] = '\n';
  }
  int rc = bao_write_file_atomic(BAO_STAGED_FILE, (const unsigned char *)buf, o);
  free(buf);
  return rc;
}

int bao_staged_merge_in(char **new_paths, size_t new_n) {
  char **old = NULL;
  size_t old_n = 0;
  if (bao_staged_load(&old, &old_n) != 0) return -1;

  size_t cap = old_n + new_n + 8;
  char **merged = (char **)malloc(cap * sizeof(char *));
  if (!merged) {
    bao_staged_free(old, old_n);
    return -1;
  }
  size_t m = 0;
  for (size_t i = 0; i < old_n; i++) merged[m++] = strdup(old[i]);
  for (size_t i = 0; i < new_n; i++) {
    if (!new_paths[i] || path_is_unsafe(new_paths[i])) continue;
    char *c = strip_dot_slash(new_paths[i]);
    if (!c) {
      for (size_t k = 0; k < m; k++) free(merged[k]);
      free(merged);
      bao_staged_free(old, old_n);
      return -1;
    }
    merged[m++] = c;
  }
  bao_staged_free(old, old_n);

  int u = unique_sort(merged, m);
  if (u < 0) {
    for (size_t i = 0; i < m; i++) free(merged[i]);
    free(merged);
    return -1;
  }
  m = (size_t)u;
  int w = write_paths(merged, m);
  for (size_t i = 0; i < m; i++) free(merged[i]);
  free(merged);
  return w;
}

static int append_single_file(const char *rel, char ***acc, size_t *n, size_t *cap) {
  struct stat st;
  if (stat(rel, &st) != 0) return -1;
  if (!S_ISREG(st.st_mode)) return -1;
  char *c = strip_dot_slash(rel);
  if (!c) return -1;
  if (*n == *cap) {
    size_t ncap = *cap ? *cap * 2 : 16;
    char **na = (char **)realloc(*acc, ncap * sizeof(char *));
    if (!na) {
      free(c);
      return -1;
    }
    *acc = na;
    *cap = ncap;
  }
  (*acc)[(*n)++] = c;
  return 0;
}

static int walk_dir(const char *rel_dir, char ***acc, size_t *n, size_t *cap) {
  DIR *d = opendir(rel_dir);
  if (!d) return -1;
  struct dirent *de;
  while ((de = readdir(d)) != NULL) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
    if (strcmp(de->d_name, ".bao") == 0 || strcmp(de->d_name, ".git") == 0) continue;
    char *sub = bao_strdup_printf("%s/%s", rel_dir, de->d_name);
    if (!sub) {
      closedir(d);
      return -1;
    }
    struct stat st;
    if (stat(sub, &st) != 0) {
      free(sub);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if (walk_dir(sub, acc, n, cap) != 0) {
        free(sub);
        closedir(d);
        return -1;
      }
    } else if (S_ISREG(st.st_mode)) {
      if (append_single_file(sub, acc, n, cap) != 0) {
        free(sub);
        closedir(d);
        return -1;
      }
    }
    free(sub);
  }
  closedir(d);
  return 0;
}

int bao_staged_add_path(const char *path) {
  if (!path || path_is_unsafe(path) || path_has_dotdot(path)) return -1;

  char **batch = NULL;
  size_t n = 0, cap = 0;

  struct stat st;
  if (stat(path, &st) != 0) {
    bao_warn("cannot add %s: %s", path, strerror(errno));
    return -1;
  }

  if (S_ISREG(st.st_mode)) {
    if (append_single_file(path, &batch, &n, &cap) != 0) {
      for (size_t i = 0; i < n; i++) free(batch[i]);
      free(batch);
      return -1;
    }
  } else if (S_ISDIR(st.st_mode)) {
    if (walk_dir(path, &batch, &n, &cap) != 0) {
      for (size_t i = 0; i < n; i++) free(batch[i]);
      free(batch);
      return -1;
    }
  } else {
    bao_warn("not a file or directory: %s", path);
    return -1;
  }

  if (n == 0) {
    free(batch);
    return 0;
  }

  int rc = bao_staged_merge_in(batch, n);
  if (rc != 0) {
    for (size_t i = 0; i < n; i++) free(batch[i]);
    free(batch);
    return -1;
  }
  int added = (int)n;
  for (size_t i = 0; i < n; i++) free(batch[i]);
  free(batch);
  return added;
}

static int push_dup(char ***acc, size_t *n, size_t *cap, const char *s) {
  char *d = strdup(s);
  if (!d) return -1;
  if (*n == *cap) {
    size_t ncap = *cap ? *cap * 2 : 8;
    char **np = (char **)realloc(*acc, ncap * sizeof(char *));
    if (!np) {
      free(d);
      return -1;
    }
    *acc = np;
    *cap = ncap;
  }
  (*acc)[(*n)++] = d;
  return 0;
}

int bao_staged_add_all(void) {
  char **paths = NULL;
  size_t n = 0, cap = 0;

  if (bao_file_exists("bao.yaml")) {
    if (push_dup(&paths, &n, &cap, "bao.yaml") != 0) goto fail;
  }

  BaoConfig *cfg = load_config("bao.yaml");
  if (cfg) {
    if (cfg->prompt_filepath && bao_file_exists(cfg->prompt_filepath)) {
      if (push_dup(&paths, &n, &cap, cfg->prompt_filepath) != 0) {
        free_config(cfg);
        goto fail;
      }
    }
    if (cfg->dataset_path && bao_file_exists(cfg->dataset_path)) {
      if (push_dup(&paths, &n, &cap, cfg->dataset_path) != 0) {
        free_config(cfg);
        goto fail;
      }
    }
    free_config(cfg);
  }

  if (bao_file_exists("prompts") && bao_is_dir("prompts")) {
    if (walk_dir("prompts", &paths, &n, &cap) != 0) goto fail;
  }
  if (bao_file_exists("providers") && bao_is_dir("providers")) {
    if (walk_dir("providers", &paths, &n, &cap) != 0) goto fail;
  }

  if (n == 0) {
    free(paths);
    return 0;
  }

  int rc = bao_staged_merge_in(paths, n);
  for (size_t i = 0; i < n; i++) free(paths[i]);
  free(paths);
  return rc;

fail:
  for (size_t i = 0; i < n; i++) free(paths[i]);
  free(paths);
  return -1;
}

void bao_staged_clear(void) {
  (void)bao_write_file_atomic(BAO_STAGED_FILE, (const unsigned char *)"", 0);
}

int bao_staged_collect_from_path(const char *path, char ***out, size_t *n) {
  *out = NULL;
  *n = 0;
  if (!path || path_is_unsafe(path) || path_has_dotdot(path)) return -1;
  size_t cap = 0;
  struct stat st;
  if (stat(path, &st) != 0) return -1;
  if (S_ISREG(st.st_mode)) {
    if (append_single_file(path, out, n, &cap) != 0) return -1;
    return 0;
  }
  if (S_ISDIR(st.st_mode)) {
    if (walk_dir(path, out, n, &cap) != 0) return -1;
    return 0;
  }
  return -1;
}

int bao_staged_collect_add_all_paths(char ***out, size_t *n) {
  *out = NULL;
  *n = 0;
  size_t cap = 0;

  if (bao_file_exists("bao.yaml")) {
    if (push_dup(out, n, &cap, "bao.yaml") != 0) goto fail;
  }

  BaoConfig *cfg = load_config("bao.yaml");
  if (cfg) {
    if (cfg->prompt_filepath && bao_file_exists(cfg->prompt_filepath)) {
      if (bao_is_safe_relpath(cfg->prompt_filepath) && push_dup(out, n, &cap, cfg->prompt_filepath) != 0) {
        free_config(cfg);
        goto fail;
      }
    }
    if (cfg->dataset_path && bao_file_exists(cfg->dataset_path)) {
      if (bao_is_safe_relpath(cfg->dataset_path) && push_dup(out, n, &cap, cfg->dataset_path) != 0) {
        free_config(cfg);
        goto fail;
      }
    }
    free_config(cfg);
  }

  if (bao_file_exists("prompts") && bao_is_dir("prompts")) {
    if (walk_dir("prompts", out, n, &cap) != 0) goto fail;
  }
  if (bao_file_exists("providers") && bao_is_dir("providers")) {
    if (walk_dir("providers", out, n, &cap) != 0) goto fail;
  }

  if (*n > 0) {
    int u = unique_sort(*out, *n);
    *n = (size_t)u;
  }

  return 0;

fail:
  for (size_t i = 0; i < *n; i++) free((*out)[i]);
  free(*out);
  *out = NULL;
  *n = 0;
  return -1;
}

int bao_staged_remove_paths(const char **paths, size_t npaths) {
  if (!paths || npaths == 0) return 0;
  char **cur = NULL;
  size_t cn = 0;
  if (bao_staged_load(&cur, &cn) != 0) return -1;

  char **norm_rm = (char **)malloc(npaths * sizeof(char *));
  if (!norm_rm) {
    bao_staged_free(cur, cn);
    return -1;
  }
  for (size_t j = 0; j < npaths; j++) {
    norm_rm[j] = paths[j] ? strip_dot_slash(paths[j]) : NULL;
  }

  size_t w = 0;
  for (size_t i = 0; i < cn; i++) {
    int drop = 0;
    for (size_t j = 0; j < npaths; j++) {
      if (norm_rm[j] && cur[i] && strcmp(cur[i], norm_rm[j]) == 0) {
        drop = 1;
        break;
      }
    }
    if (drop) {
      free(cur[i]);
    } else {
      cur[w++] = cur[i];
    }
  }

  for (size_t j = 0; j < npaths; j++) free(norm_rm[j]);
  free(norm_rm);

  int rc = write_paths(cur, w);
  for (size_t i = 0; i < w; i++) free(cur[i]);
  free(cur);
  return rc;
}

int bao_staged_update_index(void) {
  char **old = NULL;
  size_t old_n = 0;
  if (bao_staged_load(&old, &old_n) != 0) return -1;
  char **keep = NULL;
  size_t k = 0, cap = 0;
  for (size_t i = 0; i < old_n; i++) {
    if (bao_file_exists(old[i])) {
      if (k == cap) {
        cap = cap ? cap * 2 : 8;
        char **nk = (char **)realloc(keep, cap * sizeof(char *));
        if (!nk) {
          for (size_t j = 0; j < k; j++) free(keep[j]);
          free(keep);
          bao_staged_free(old, old_n);
          return -1;
        }
        keep = nk;
      }
      keep[k] = strdup(old[i]);
      if (!keep[k]) {
        for (size_t j = 0; j < k; j++) free(keep[j]);
        free(keep);
        bao_staged_free(old, old_n);
        return -1;
      }
      k++;
    }
  }
  bao_staged_free(old, old_n);
  int rc = write_paths(keep, k);
  for (size_t i = 0; i < k; i++) free(keep[i]);
  free(keep);
  return rc;
}

int bao_staged_restore_from_commit_short(const char *commit_id) {
  if (!commit_id || !*commit_id) return -1;
  unsigned char *buf = NULL;
  size_t len = 0;
  char *full = NULL;
  if (bao_read_commit_json_any(commit_id, &full, &buf, &len) != 0) return -1;
  free(full);
  cJSON *root = cJSON_Parse((char *)buf);
  free(buf);
  if (!root) return -1;
  cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "staged_files");
  if (!cJSON_IsArray(arr)) {
    cJSON_Delete(root);
    return -1;
  }
  int n = cJSON_GetArraySize(arr);
  char **lines = (char **)calloc((size_t)n + 1, sizeof(char *));
  if (!lines) {
    cJSON_Delete(root);
    return -1;
  }
  int w = 0;
  for (int i = 0; i < n; i++) {
    cJSON *it = cJSON_GetArrayItem(arr, i);
    if (!cJSON_IsString(it) || !it->valuestring) continue;
    lines[w] = strdup(it->valuestring);
    if (!lines[w]) {
      for (int j = 0; j < w; j++) free(lines[j]);
      free(lines);
      cJSON_Delete(root);
      return -1;
    }
    w++;
  }
  cJSON_Delete(root);
  bao_staged_clear();
  bao_staged_merge_in(lines, (size_t)w);
  for (int j = 0; j < w; j++) free(lines[j]);
  free(lines);
  return 0;
}

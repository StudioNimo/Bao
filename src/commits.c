#include "commits.h"

#include "bao.h"
#include "util.h"

#include "cJSON.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bao_is_hex64(const char *s) {
  if (!s || strlen(s) != 64) return 0;
  for (int i = 0; i < 64; i++) {
    if (!isxdigit((unsigned char)s[i])) return 0;
  }
  return 1;
}

static int is_hex_n(const char *s, size_t n) {
  if (!s) return 0;
  for (size_t i = 0; i < n; i++) {
    if (!s[i] || !isxdigit((unsigned char)s[i])) return 0;
  }
  return s[n] == 0;
}

static char *lower_hex_dup(const char *s, size_t n) {
  char *o = (char *)malloc(n + 1);
  if (!o) return NULL;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
    o[i] = c;
  }
  o[n] = 0;
  return o;
}

char *bao_display_short_from_full(const char *full64) {
  if (!full64 || strlen(full64) < 7) return NULL;
  return lower_hex_dup(full64, 7);
}

int bao_commit_json_path_full(const char *full64, char *out, size_t outsz) {
  if (!bao_is_hex64(full64) || !out || outsz < strlen(BAO_COMMITS_DIR) + 1 + 64 + 6) return -1;
  snprintf(out, outsz, "%s/%s.json", BAO_COMMITS_DIR, full64);
  return 0;
}

static char *read_full_from_legacy_short_file(const char *short7) {
  if (!short7 || strlen(short7) != 7 || !is_hex_n(short7, 7)) return NULL;
  char *path = bao_strdup_printf("%s/%s.json", BAO_COMMITS_DIR, short7);
  if (!path) return NULL;
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(path, &buf, &len) != 0) {
    free(path);
    return NULL;
  }
  free(path);
  cJSON *j = cJSON_Parse((char *)buf);
  free(buf);
  if (!j) return NULL;
  cJSON *fh = cJSON_GetObjectItemCaseSensitive(j, "full_hash");
  char *out = NULL;
  if (fh && cJSON_IsString(fh) && fh->valuestring && bao_is_hex64(fh->valuestring)) {
    out = strdup(fh->valuestring);
  }
  cJSON_Delete(j);
  return out;
}

char *bao_ref_line_to_commit_full(const char *line) {
  if (!line) return NULL;
  const char *p = line;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  const char *e = p;
  while (*e && *e != '\n' && *e != '\r' && *e != ' ' && *e != '\t') e++;
  size_t n = (size_t)(e - p);
  if (n == 0) return NULL;
  char *tmp = (char *)malloc(n + 1);
  if (!tmp) return NULL;
  memcpy(tmp, p, n);
  tmp[n] = 0;
  for (size_t i = 0; i < n; i++) {
    if (tmp[i] >= 'A' && tmp[i] <= 'F') tmp[i] = (char)(tmp[i] - 'A' + 'a');
  }
  if (n == 64 && bao_is_hex64(tmp)) return tmp;
  if (n == 7 && is_hex_n(tmp, 7)) {
    char *f = read_full_from_legacy_short_file(tmp);
    free(tmp);
    return f;
  }
  free(tmp);
  return NULL;
}

char *bao_resolve_head_full(void) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(BAO_HEAD_FILE, &buf, &len) != 0) return NULL;
  char *s = (char *)buf;
  if (strncmp(s, "ref:", 4) == 0) {
    s += 4;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s;
    while (*e && *e != '\n' && *e != '\r') e++;
    *e = 0;
    char *refpath = bao_strdup_printf("%s/%s", BAO_DIR, s);
    free(buf);
    buf = NULL;
    if (!refpath) return NULL;
    if (bao_read_file(refpath, &buf, &len) != 0) {
      free(refpath);
      return NULL;
    }
    free(refpath);
    s = (char *)buf;
  }
  char *full = bao_ref_line_to_commit_full(s);
  free(buf);
  return full;
}

static int push_unique(char ***m, size_t *nm, size_t *cap, const char *full64) {
  for (size_t i = 0; i < *nm; i++) {
    if (!strcmp((*m)[i], full64)) return 0;
  }
  if (*nm == *cap) {
    size_t ncap = *cap ? *cap * 2 : 8;
    char **nm2 = (char **)realloc(*m, ncap * sizeof(char *));
    if (!nm2) return -1;
    *m = nm2;
    *cap = ncap;
  }
  (*m)[*nm] = strdup(full64);
  if (!(*m)[*nm]) return -1;
  (*nm)++;
  return 0;
}

static char *scan_prefix_unique(const char *prefix) {
  if (!prefix || !*prefix) return NULL;
  size_t pl = strlen(prefix);
  if (pl < 4 || pl > 64) return NULL;
  for (size_t i = 0; i < pl; i++) {
    if (!isxdigit((unsigned char)prefix[i])) return NULL;
  }

  DIR *d = opendir(BAO_COMMITS_DIR);
  if (!d) return NULL;
  char **matches = NULL;
  size_t nm = 0, cap = 0;
  struct dirent *de;
  while ((de = readdir(d)) != NULL) {
    if (de->d_name[0] == '.') continue;
    size_t L = strlen(de->d_name);
    if (L < 8 || strcmp(de->d_name + L - 5, ".json") != 0) continue;
    char id[128];
    if (L - 5 >= sizeof(id)) continue;
    memcpy(id, de->d_name, L - 5);
    id[L - 5] = 0;

    if (strlen(id) == 64 && bao_is_hex64(id)) {
      if (strncmp(id, prefix, pl) != 0) continue;
      if (push_unique(&matches, &nm, &cap, id) != 0) goto fail;
      continue;
    }
    if (strlen(id) == 7 && is_hex_n(id, 7)) {
      char *f = read_full_from_legacy_short_file(id);
      if (!f || !bao_is_hex64(f)) {
        free(f);
        continue;
      }
      if (strncmp(f, prefix, pl) != 0) {
        free(f);
        continue;
      }
      if (push_unique(&matches, &nm, &cap, f) != 0) {
        free(f);
        goto fail;
      }
      free(f);
    }
  }
  closedir(d);
  if (nm != 1) {
  fail:
    for (size_t i = 0; i < nm; i++) free(matches[i]);
    free(matches);
    return NULL;
  }
  char *out = matches[0];
  free(matches);
  return out;
}

char *bao_resolve_commit_full(const char *spec) {
  if (!spec || !*spec) return NULL;
  if (!strcmp(spec, "HEAD")) return bao_resolve_head_full();

  if (strncmp(spec, "HEAD~", 5) == 0) {
    const char *num = spec + 5;
    char *end = NULL;
    unsigned long n = strtoul(num, &end, 10);
    if (end == num || *end) return NULL;
    if (n == 0) return bao_resolve_head_full();

    char *cur = bao_resolve_head_full();
    if (!cur || !bao_is_hex64(cur)) {
      free(cur);
      return NULL;
    }
    for (unsigned long i = 0; i < n; i++) {
      char path[512];
      if (bao_commit_json_path_full(cur, path, sizeof(path)) != 0) {
        free(cur);
        return NULL;
      }
      unsigned char *buf = NULL;
      size_t len = 0;
      if (bao_read_file(path, &buf, &len) != 0) {
        free(cur);
        return NULL;
      }
      cJSON *j = cJSON_Parse((char *)buf);
      free(buf);
      if (!j) {
        free(cur);
        return NULL;
      }
      cJSON *p = cJSON_GetObjectItemCaseSensitive(j, "parent");
      char *next = NULL;
      if (p && cJSON_IsString(p) && p->valuestring) {
        if (bao_is_hex64(p->valuestring)) next = strdup(p->valuestring);
        else if (strlen(p->valuestring) == 7) next = read_full_from_legacy_short_file(p->valuestring);
      }
      cJSON_Delete(j);
      free(cur);
      cur = next;
      if (!cur || !bao_is_hex64(cur)) {
        free(cur);
        return NULL;
      }
    }
    return cur;
  }

  if (strncmp(spec, "refs/", 5) == 0) {
    char *path = bao_strdup_printf("%s/%s", BAO_DIR, spec);
    if (!path) return NULL;
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(path, &buf, &len) != 0) {
      free(path);
      return NULL;
    }
    free(path);
    char *full = bao_ref_line_to_commit_full((char *)buf);
    free(buf);
    return full;
  }

  char *path = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, spec);
  if (!path) return NULL;
  if (bao_file_exists(path)) {
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(path, &buf, &len) == 0) {
      free(path);
      char *full = bao_ref_line_to_commit_full((char *)buf);
      free(buf);
      return full;
    }
  }
  free(path);

  path = bao_strdup_printf("%s/%s", BAO_TAGS_DIR, spec);
  if (!path) return NULL;
  if (bao_file_exists(path)) {
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(path, &buf, &len) == 0) {
      free(path);
      char *full = bao_ref_line_to_commit_full((char *)buf);
      free(buf);
      return full;
    }
  }
  free(path);

  size_t sl = strlen(spec);
  if (sl == 64 && bao_is_hex64(spec)) return strdup(spec);
  if (sl >= 4 && sl <= 64) return scan_prefix_unique(spec);
  return NULL;
}

int bao_read_commit_json_any(const char *id, char **full_out, unsigned char **buf, size_t *len) {
  *full_out = NULL;
  *buf = NULL;
  *len = 0;
  if (!id) return -1;

  char *full = NULL;
  if (strlen(id) == 64 && bao_is_hex64(id)) {
    full = strdup(id);
  } else {
    full = bao_resolve_commit_full(id);
  }

  char path[512];
  if (full && bao_commit_json_path_full(full, path, sizeof(path)) == 0 && bao_read_file(path, buf, len) == 0) {
    *full_out = full;
    return 0;
  }
  free(full);

  if (strlen(id) == 7 && is_hex_n(id, 7)) {
    char *p2 = bao_strdup_printf("%s/%s.json", BAO_COMMITS_DIR, id);
    if (!p2) return -1;
    if (bao_read_file(p2, buf, len) != 0) {
      free(p2);
      return -1;
    }
    free(p2);
    cJSON *j = cJSON_Parse((char *)*buf);
    if (!j) {
      free(*buf);
      *buf = NULL;
      return -1;
    }
    cJSON *fh = cJSON_GetObjectItemCaseSensitive(j, "full_hash");
    if (!fh || !cJSON_IsString(fh) || !fh->valuestring || !bao_is_hex64(fh->valuestring)) {
      cJSON_Delete(j);
      free(*buf);
      *buf = NULL;
      return -1;
    }
    *full_out = strdup(fh->valuestring);
    cJSON_Delete(j);
    return 0;
  }

  if (*buf) {
    free(*buf);
    *buf = NULL;
  }
  return -1;
}

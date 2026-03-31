#include "refs.h"

#include "bao.h"
#include "commits.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *bao_resolve_head_short(void) {
  char *f = bao_resolve_head_full();
  if (!f) return NULL;
  char *s = bao_display_short_from_full(f);
  free(f);
  return s;
}

char *bao_current_branch(void) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(BAO_HEAD_FILE, &buf, &len) != 0) return NULL;
  const char *s = (const char *)buf;
  const char *p = strstr(s, "ref:");
  if (!p) {
    free(buf);
    return NULL;
  }
  p += 4;
  while (*p == ' ' || *p == '\t') p++;
  const char *e = p;
  while (*e && *e != '\n' && *e != '\r') e++;
  size_t n = (size_t)(e - p);
  char *tmp = (char *)malloc(n + 1);
  if (!tmp) {
    free(buf);
    return NULL;
  }
  memcpy(tmp, p, n);
  tmp[n] = 0;
  free(buf);

  const char *prefix = "refs/heads/";
  size_t pl = strlen(prefix);
  if (strncmp(tmp, prefix, pl) == 0) {
    char *name = strdup(tmp + pl);
    free(tmp);
    return name;
  }
  free(tmp);
  return NULL;
}

char *bao_rev_parse(const char *name) {
  return bao_resolve_commit_full(name);
}

int bao_branch_exists(const char *name) {
  if (!name || !*name) return 0;
  char *p = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, name);
  if (!p) return 0;
  int ex = bao_file_exists(p);
  free(p);
  return ex;
}

int bao_write_head_symbolic(const char *branch_name) {
  if (!branch_name || !*branch_name) return -1;
  char *bp = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, branch_name);
  if (!bp) return -1;
  if (!bao_file_exists(bp)) {
    free(bp);
    return -1;
  }
  free(bp);
  char *content = bao_strdup_printf("ref: refs/heads/%s\n", branch_name);
  if (!content) return -1;
  int rc = bao_write_file_atomic(BAO_HEAD_FILE, (const unsigned char *)content, strlen(content));
  free(content);
  return rc;
}

int bao_write_head_detached(const char *full_hash_64) {
  if (!bao_is_hex64(full_hash_64)) return -1;
  char *content = bao_strdup_printf("%s\n", full_hash_64);
  if (!content) return -1;
  int rc = bao_write_file_atomic(BAO_HEAD_FILE, (const unsigned char *)content, strlen(content));
  free(content);
  return rc;
}

int bao_update_head_to_commit(const char *full_hash_64) {
  if (!bao_is_hex64(full_hash_64)) return -1;
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(BAO_HEAD_FILE, &buf, &len) != 0) return -1;
  const char *s = (const char *)buf;
  const char *p = strstr(s, "ref:");
  if (p) {
    p += 4;
    while (*p == ' ' || *p == '\t') p++;
    const char *e = p;
    while (*e && *e != '\n' && *e != '\r') e++;
    size_t n = (size_t)(e - p);
    char *ref = (char *)malloc(n + 1);
    if (!ref) {
      free(buf);
      return -1;
    }
    memcpy(ref, p, n);
    ref[n] = 0;
    free(buf);
    char *full = bao_strdup_printf("%s/%s", BAO_DIR, ref);
    free(ref);
    if (!full) return -1;
    char *content = bao_strdup_printf("%s\n", full_hash_64);
    if (!content) {
      free(full);
      return -1;
    }
    int rc = bao_write_file_atomic(full, (const unsigned char *)content, strlen(content));
    free(content);
    free(full);
    return rc;
  }
  free(buf);
  char *content = bao_strdup_printf("%s\n", full_hash_64);
  if (!content) return -1;
  int rc = bao_write_file_atomic(BAO_HEAD_FILE, (const unsigned char *)content, strlen(content));
  free(content);
  return rc;
}

char *bao_resolve_revision(const char *spec) {
  return bao_resolve_commit_full(spec);
}

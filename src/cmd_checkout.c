#include "bao.h"

#include "commits.h"
#include "refs.h"
#include "stage.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verify_commit_object(const char *full64) {
  if (!full64 || !bao_is_hex64(full64)) return -1;
  char path[512];
  if (bao_commit_json_path_full(full64, path, sizeof(path)) != 0) return -1;
  return bao_file_exists(path) ? 0 : -1;
}

static char *json_get_string(const char *json, const char *key) {
  char pat[128];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return NULL;
  p = strchr(p, ':');
  if (!p) return NULL;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return NULL;
  p++;
  const char *e = strchr(p, '"');
  if (!e) return NULL;
  size_t n = (size_t)(e - p);
  char *out = (char *)malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, p, n);
  out[n] = 0;
  return out;
}

static int create_branch_at(const char *name, const char *full_hash_64, int force) {
  if (!name) return -1;
  char *path = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, name);
  if (!path) return -1;
  if (bao_file_exists(path) && !force) {
    free(path);
    return -2;
  }
  if (bao_mkdir_p(BAO_HEADS_DIR) != 0) {
    free(path);
    return -1;
  }
  char *content;
  if (full_hash_64 && bao_is_hex64(full_hash_64)) {
    content = bao_strdup_printf("%s\n", full_hash_64);
  } else {
    content = strdup("\n");
  }
  if (!content) {
    free(path);
    return -1;
  }
  int rc = bao_write_file_atomic(path, (const unsigned char *)content, strlen(content));
  free(path);
  free(content);
  return rc;
}

int cmd_checkout(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int create_b = 0, create_B = 0, detach = 0;
  char *new_branch = NULL;
  /* main passes argv+1 so argv[0] is the subcommand "checkout" */
  int i = 0;
  if (argc > 0 && !strcmp(argv[0], "checkout")) i = 1;
  while (i < argc) {
    if (!strcmp(argv[i], "--")) {
      i++;
      break;
    }
    if (argv[i][0] != '-') break;
    if (!strcmp(argv[i], "-b")) {
      create_b = 1;
      if (i + 1 >= argc) bao_die("usage: bao checkout -b <branch> [<start>]");
      new_branch = argv[++i];
      i++;
      continue;
    }
    if (!strcmp(argv[i], "-B")) {
      create_B = 1;
      if (i + 1 >= argc) bao_die("usage: bao checkout -B <branch> [<start>]");
      new_branch = argv[++i];
      i++;
      continue;
    }
    if (!strcmp(argv[i], "--detach")) {
      detach = 1;
      i++;
      continue;
    }
    if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force")) {
      i++;
      continue;
    }
    bao_die("unknown option: %s", argv[i]);
  }

  int npos = argc - i;
  char **pos = &argv[i];

  if (create_b || create_B) {
    if (!new_branch) bao_die("internal: -b without name");
    const char *start = NULL;
    if (npos >= 1) start = pos[0];
    char *base = NULL;
    if (start) {
      base = bao_resolve_revision(start);
      if (!base) base = bao_rev_parse(start);
    } else {
      base = bao_resolve_head_full();
    }
    if (!base) bao_die("could not resolve start for new branch");
    if (!bao_is_hex64(base) || verify_commit_object(base) != 0) {
      free(base);
      bao_die("commit object not found for start revision");
    }
    int rc = create_branch_at(new_branch, base, create_B);
    free(base);
    if (rc == -2) bao_die("branch already exists: %s (use -B to overwrite)", new_branch);
    if (rc != 0) bao_die("failed to create branch");
    if (bao_write_head_symbolic(new_branch) != 0) bao_die("failed to switch to new branch");
    printf("Switched to branch '%s'\n", new_branch);
    return 0;
  }

  if (detach) {
    if (npos < 1) {
      char *hf = bao_resolve_head_full();
      if (!hf || !bao_is_hex64(hf)) {
        free(hf);
        bao_die("cannot resolve HEAD");
      }
      if (bao_write_head_detached(hf) != 0) {
        free(hf);
        bao_die("detach failed");
      }
      char *ds = bao_display_short_from_full(hf);
      free(hf);
      printf("HEAD is now at %s (detached)\n", ds ? ds : "?");
      free(ds);
      return 0;
    }
    char *sh = bao_resolve_revision(pos[0]);
    if (!sh) sh = bao_rev_parse(pos[0]);
    if (!sh) bao_die("failed to resolve '%s'", pos[0]);
    if (!bao_is_hex64(sh) || verify_commit_object(sh) != 0) {
      free(sh);
      bao_die("commit object not found for %s", pos[0]);
    }
    if (bao_write_head_detached(sh) != 0) {
      free(sh);
      bao_die("failed to update HEAD");
    }
    char *ds = bao_display_short_from_full(sh);
    printf("HEAD is now at %s (detached)\n", ds ? ds : sh);
    free(ds);
    free(sh);
    return 0;
  }

  if (npos < 1) bao_die("usage: bao checkout [-b|-B <branch>] [--detach] <branch|hash>");

  const char *target = pos[0];

  if (bao_branch_exists(target)) {
    char *path = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, target);
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(path, &buf, &len) != 0) {
      free(path);
      bao_die("failed to read branch ref");
    }
    free(path);
    char *s = (char *)buf;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    if (!*s) {
      free(buf);
      bao_warn("branch is unborn; staying symbolic");
    }
    free(buf);
    if (bao_write_head_symbolic(target) != 0) bao_die("failed to switch branch");
    printf("Switched to branch '%s'\n", target);
    return 0;
  }

  char *full_hash = bao_resolve_revision(target);
  if (!full_hash) full_hash = bao_rev_parse(target);
  if (!full_hash) bao_die("failed to resolve '%s' to a commit hash", target);

  if (!bao_is_hex64(full_hash) || verify_commit_object(full_hash) != 0) {
    free(full_hash);
    bao_die("commit object not found for prefix %s", target);
  }

  char path[512];
  if (bao_commit_json_path_full(full_hash, path, sizeof(path)) != 0) {
    free(full_hash);
    bao_die("commit path error");
  }
  unsigned char *fbuf = NULL;
  size_t flen = 0;
  if (bao_read_file(path, &fbuf, &flen) != 0) {
    free(full_hash);
    bao_die("commit object not found for prefix %s", target);
  }

  char *msg = json_get_string((const char *)fbuf, "message");
  char *created_at = json_get_string((const char *)fbuf, "created_at");
  free(fbuf);

  if (bao_write_head_detached(full_hash) != 0) {
    free(full_hash);
    free(msg);
    free(created_at);
    bao_die("failed to update HEAD");
  }

  char *ds = bao_display_short_from_full(full_hash);
  printf("HEAD is now at %s (detached)\n", ds ? ds : full_hash);
  if (created_at || msg) printf("    %s %s\n", created_at ? created_at : "", msg ? msg : "");

  free(full_hash);
  free(ds);
  free(msg);
  free(created_at);
  return 0;
}

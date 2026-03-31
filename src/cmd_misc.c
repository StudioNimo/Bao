#include "bao.h"

#include "commits.h"
#include "config.h"
#include "refs.h"
#include "stage.h"
#include "util.h"

#include "cJSON.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static int not_impl(bao_ctx_t *ctx, int argc, char **argv, const char *name) {
  (void)ctx;
  (void)argc;
  (void)argv;
  fprintf(stderr, "bao: '%s' is not implemented in this version (local-first prototype).\n", name);
  return 1;
}

static int is_safe_ref_component(const char *name) {
  if (!name || !*name) return 0;
  if (name[0] == '-') return 0;
  if (strstr(name, "..")) return 0;
  if (strchr(name, '/') || strchr(name, '\\')) return 0;
  for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
    if (*p <= 0x20 || *p == 0x7f) return 0; // control/space
  }
  if (!strcmp(name, ".") || !strcmp(name, "..")) return 0;
  if (!strcmp(name, BAO_DIR)) return 0;
  return 1;
}

int cmd_clone(bao_ctx_t *ctx, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--abort") || !strcmp(argv[i], "--continue")) {
      fprintf(stderr, "bao: clone: %s has no effect in bao.\n", argv[i]);
      return 0;
    }
  }
  return not_impl(ctx, argc, argv, "clone");
}
int cmd_merge(bao_ctx_t *ctx, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--abort") || !strcmp(argv[i], "--continue")) {
      fprintf(stderr, "bao: merge: %s has no effect in bao.\n", argv[i]);
      return 0;
    }
  }
  return not_impl(ctx, argc, argv, "merge");
}
int cmd_fetch(bao_ctx_t *ctx, int argc, char **argv) {
  (void)argc;
  (void)argv;
  return not_impl(ctx, argc, argv, "fetch");
}
int cmd_rebase(bao_ctx_t *ctx, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--abort") || !strcmp(argv[i], "--continue") || !strcmp(argv[i], "--skip")) {
      fprintf(stderr, "bao: rebase: %s has no effect in bao.\n", argv[i]);
      return 0;
    }
  }
  return not_impl(ctx, argc, argv, "rebase");
}
int cmd_filter_branch(bao_ctx_t *ctx, int argc, char **argv) { return not_impl(ctx, argc, argv, "filter-branch"); }
int cmd_cherry_pick(bao_ctx_t *ctx, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--abort") || !strcmp(argv[i], "--continue")) {
      fprintf(stderr, "bao: cherry-pick: %s has no effect in bao.\n", argv[i]);
      return 0;
    }
  }
  return not_impl(ctx, argc, argv, "cherry-pick");
}
int cmd_revert(bao_ctx_t *ctx, int argc, char **argv) { return not_impl(ctx, argc, argv, "revert"); }
int cmd_blame(bao_ctx_t *ctx, int argc, char **argv) { return not_impl(ctx, argc, argv, "blame"); }

static int create_branch_at_switch(const char *name, const char *full_hash_64, int force) {
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

static int verify_commit_object_misc(const char *id) {
  char *f = bao_resolve_commit_full(id);
  if (!f || !bao_is_hex64(f)) {
    free(f);
    return -1;
  }
  char path[512];
  if (bao_commit_json_path_full(f, path, sizeof(path)) != 0) {
    free(f);
    return -1;
  }
  free(f);
  return bao_file_exists(path) ? 0 : -1;
}

int cmd_switch(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int create_c = 0, create_C = 0, detach = 0;
  char *new_branch = NULL;
  int i = 0;
  if (argc > 0 && !strcmp(argv[0], "switch")) i = 1;
  while (i < argc) {
    if (!strcmp(argv[i], "--")) {
      i++;
      break;
    }
    if (argv[i][0] != '-') break;
    if (!strcmp(argv[i], "-c")) {
      create_c = 1;
      if (i + 1 >= argc) bao_die("usage: bao switch -c <branch> [<start>]");
      new_branch = argv[++i];
      i++;
      continue;
    }
    if (!strcmp(argv[i], "-C")) {
      create_C = 1;
      if (i + 1 >= argc) bao_die("usage: bao switch -C <branch> [<start>]");
      new_branch = argv[++i];
      i++;
      continue;
    }
    if (!strcmp(argv[i], "--detach")) {
      detach = 1;
      i++;
      continue;
    }
    if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force") || !strcmp(argv[i], "--force-detach")) {
      i++;
      continue;
    }
    bao_die("unknown option: %s", argv[i]);
  }

  int npos = argc - i;
  char **pos = &argv[i];

  if (create_c || create_C) {
    if (!new_branch) bao_die("internal: -c without name");
    const char *start = npos >= 1 ? pos[0] : NULL;
    char *base = NULL;
    if (start) {
      base = bao_resolve_revision(start);
      if (!base) base = bao_rev_parse(start);
    } else {
      base = bao_resolve_head_full();
    }
    if (!base) bao_die("could not resolve start for new branch");
    if (!bao_is_hex64(base) || verify_commit_object_misc(base) != 0) {
      free(base);
      bao_die("commit object not found for start revision");
    }
    int rc = create_branch_at_switch(new_branch, base, create_C);
    free(base);
    if (rc == -2) bao_die("branch already exists: %s (use -C to overwrite)", new_branch);
    if (rc != 0) bao_die("failed to create branch");
    if (bao_write_head_symbolic(new_branch) != 0) bao_die("failed to switch");
    printf("Switched to branch '%s'\n", new_branch);
    return 0;
  }

  if (detach) {
    if (npos < 1) {
      char *hf = bao_resolve_head_full();
      if (!hf || !bao_is_hex64(hf)) {
        free(hf);
        bao_die("cannot detach: no commit at HEAD");
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
    if (!bao_is_hex64(sh) || verify_commit_object_misc(sh) != 0) {
      free(sh);
      bao_die("commit object not found");
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

  if (npos < 1) bao_die("usage: bao switch [-c|-C <branch>] [--detach] <branch|hash>");

  const char *target = pos[0];
  if (bao_branch_exists(target)) {
    if (bao_write_head_symbolic(target) != 0) bao_die("failed to switch branch");
    printf("Switched to branch '%s'\n", target);
    return 0;
  }

  return cmd_checkout(ctx, argc, argv);
}

int cmd_status(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  int short_fmt = 0;
  int show_branch = 0;
  int porcelain = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--short")) short_fmt = 1;
    else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--branch"))
      show_branch = 1;
    else if (!strcmp(argv[i], "--porcelain") || !strcmp(argv[i], "--porcelain=v1"))
      porcelain = 1;
    else if (!strcmp(argv[i], "--ignored"))
      bao_warn("--ignored is not implemented");
    else if (argv[i][0] == '-')
      bao_die("unknown option: %s", argv[i]);
  }

  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  char *br = bao_current_branch();
  char *h = bao_resolve_head_short();

  if (porcelain) {
    printf("# branch.oid %s\n", h && *h ? h : "");
    printf("# branch.head %s\n", br ? br : "");
    char **st = NULL;
    size_t n = 0;
    if (bao_staged_load(&st, &n) != 0) bao_die("failed to read staging area");
    for (size_t i = 0; i < n; i++) printf("M  %s\n", st[i]);
    bao_staged_free(st, n);
    if (br) free(br);
    if (h) free(h);
    return 0;
  }

  if (short_fmt) {
    printf("## %s\n", br ? br : "HEAD (detached)");
    char **st = NULL;
    size_t n = 0;
    if (bao_staged_load(&st, &n) != 0) bao_die("failed to read staging area");
    for (size_t i = 0; i < n; i++) printf("M  %s\n", st[i]);
    bao_staged_free(st, n);
    if (br) free(br);
    if (h) free(h);
    return 0;
  }

  if (show_branch) {
    printf("On branch %s\n", br ? br : "(detached HEAD)");
    if (br) free(br);
    if (h) free(h);
    return 0;
  }

  printf("On branch %s\n", br ? br : "(detached HEAD)");
  if (br) free(br);
  printf("HEAD: %s\n", h && *h ? h : "(no commits yet)");
  if (h) free(h);
  char **st = NULL;
  size_t n = 0;
  if (bao_staged_load(&st, &n) != 0) bao_die("failed to read staging area");
  printf("Changes to be committed:\n");
  if (n == 0) {
    printf("  (none - use `bao add`)\n");
  } else {
    for (size_t i = 0; i < n; i++) printf("  staged: %s\n", st[i]);
  }
  bao_staged_free(st, n);
  return 0;
}

int cmd_reset(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int mode_soft = 0, mode_mixed = 0, mode_hard = 0;
  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "--soft")) {
      mode_soft = 1;
      continue;
    }
    if (!strcmp(argv[i], "--mixed") || !strcmp(argv[i], "--merge")) {
      mode_mixed = 1;
      continue;
    }
    if (!strcmp(argv[i], "--hard")) {
      mode_hard = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    break;
  }

  int npos = argc - i;
  char **pos = &argv[i];

  if (mode_soft + mode_mixed + mode_hard > 1) bao_die("multiple reset modes specified");

  if (npos == 0) {
    if (mode_soft) bao_die("usage: bao reset --soft <commit>");
    if (mode_hard) {
      bao_staged_clear();
      printf("Staging area cleared (--hard; working tree not modified by bao).\n");
      return 0;
    }
    bao_staged_clear();
    printf("Staging area cleared.\n");
    return 0;
  }

  char *rev = bao_resolve_revision(pos[0]);
  if (!rev) rev = bao_rev_parse(pos[0]);
  if (!rev) bao_die("could not resolve '%s'", pos[0]);
  if (!bao_is_hex64(rev) || verify_commit_object_misc(rev) != 0) {
    free(rev);
    bao_die("unknown commit: %s", pos[0]);
  }

  if (mode_soft) {
    if (bao_update_head_to_commit(rev) != 0) {
      free(rev);
      bao_die("failed to move HEAD");
    }
    char *ds = bao_display_short_from_full(rev);
    printf("HEAD is now at %s (staged changes kept).\n", ds ? ds : rev);
    free(ds);
    free(rev);
    return 0;
  }

  if (mode_hard) {
    if (bao_update_head_to_commit(rev) != 0) {
      free(rev);
      bao_die("failed to move HEAD");
    }
    bao_staged_clear();
    char *ds = bao_display_short_from_full(rev);
    printf("HEAD is now at %s; index cleared (--hard; working tree not reset).\n", ds ? ds : rev);
    free(ds);
    free(rev);
    return 0;
  }

  /* mixed (default) */
  if (bao_update_head_to_commit(rev) != 0) {
    free(rev);
    bao_die("failed to move HEAD");
  }
  bao_staged_clear();
  char *ds2 = bao_display_short_from_full(rev);
  printf("HEAD is now at %s; staging area cleared.\n", ds2 ? ds2 : rev);
  free(ds2);
  free(rev);
  return 0;
}

static char *commit_subject_for_hash(const char *ref_line_or_id) {
  if (!ref_line_or_id || !*ref_line_or_id) return strdup("");
  unsigned char *buf = NULL;
  size_t len = 0;
  char *full = NULL;
  if (bao_read_commit_json_any(ref_line_or_id, &full, &buf, &len) != 0) return strdup("");
  free(full);
  cJSON *j = cJSON_Parse((char *)buf);
  free(buf);
  if (!j) return strdup("");
  cJSON *m = cJSON_GetObjectItemCaseSensitive(j, "message");
  char *out = NULL;
  if (m && cJSON_IsString(m) && m->valuestring) out = strdup(m->valuestring);
  else
    out = strdup("");
  cJSON_Delete(j);
  return out;
}

int cmd_branch(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int del = 0, del_force = 0, verbose = 0, create = 0;
  int rename_m = 0, rename_M = 0;
  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "-d")) {
      del = 1;
      continue;
    }
    if (!strcmp(argv[i], "-D")) {
      del = 1;
      del_force = 1;
      continue;
    }
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "-vv") || !strcmp(argv[i], "--verbose")) {
      verbose = 1;
      continue;
    }
    if (!strcmp(argv[i], "-c")) {
      create = 1;
      continue;
    }
    if (!strcmp(argv[i], "-m")) {
      rename_m = 1;
      continue;
    }
    if (!strcmp(argv[i], "-M")) {
      rename_M = 1;
      continue;
    }
    if (!strcmp(argv[i], "--list") || !strcmp(argv[i], "-l")) continue;
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    break;
  }

  int npos = argc - i;
  char **pos = &argv[i];

  if (rename_m || rename_M) {
    int force_m = rename_M != 0;
    const char *oldn = NULL;
    const char *newn = NULL;
    char *cur_owned = NULL;
    if (npos == 1) {
      cur_owned = bao_current_branch();
      if (!cur_owned) bao_die("cannot rename: detached HEAD");
      oldn = cur_owned;
      newn = pos[0];
    } else if (npos == 2) {
      oldn = pos[0];
      newn = pos[1];
    } else {
      bao_die("usage: bao branch -m|-M [<old>] <new>");
    }
    char *op = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, oldn);
    char *np = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, newn);
    if (!op || !np) bao_die("oom");
    if (!bao_file_exists(op)) {
      free(op);
      free(np);
      free(cur_owned);
      bao_die("branch not found: %s", oldn);
    }
    if (bao_file_exists(np) && !force_m) {
      free(op);
      free(np);
      free(cur_owned);
      bao_die("branch exists: %s", newn);
    }
    if (rename(op, np) != 0) {
      free(op);
      free(np);
      free(cur_owned);
      bao_die("rename failed");
    }
    free(op);
    free(np);
    char *cur2 = bao_current_branch();
    if (cur2 && oldn && !strcmp(cur2, oldn)) {
      free(cur2);
      if (bao_write_head_symbolic(newn) != 0) {
        free(cur_owned);
        bao_die("failed to update HEAD");
      }
    } else {
      free(cur2);
    }
    free(cur_owned);
    printf("Renamed branch to %s\n", newn);
    return 0;
  }

  if (del && npos >= 1) {
    const char *name = pos[0];
    char *cur = bao_current_branch();
    if (cur && !strcmp(cur, name)) {
      free(cur);
      bao_die("cannot delete checked-out branch");
    }
    free(cur);
    char *path = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, name);
    if (!bao_file_exists(path)) {
      free(path);
      bao_die("branch not found: %s", name);
    }
    if (!del_force) {
      unsigned char *buf = NULL;
      size_t len = 0;
      if (bao_read_file(path, &buf, &len) == 0 && buf) {
        char *s = (char *)buf;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s && strlen(s) >= 7) {
          free(buf);
          free(path);
          bao_warn("branch %s is not fully merged (use -D)", name);
          return 1;
        }
        free(buf);
      } else {
        free(buf);
      }
    }
    if (remove(path) != 0) {
      free(path);
      bao_die("delete failed");
    }
    free(path);
    printf("Deleted branch %s\n", name);
    return 0;
  }

  if (create && npos >= 1) {
    const char *name = pos[0];
    const char *start = npos >= 2 ? pos[1] : NULL;
    if (!is_safe_ref_component(name)) bao_die("invalid branch name");
    char *base = NULL;
    if (start) {
      base = bao_resolve_revision(start);
      if (!base) base = bao_rev_parse(start);
    } else {
      base = bao_resolve_head_full();
    }
    if (!base) bao_die("could not resolve start");
    int rc = create_branch_at_switch(name, base, 0);
    free(base);
    if (rc == -2) bao_die("branch already exists: %s", name);
    if (rc != 0) bao_die("failed to create branch");
    printf("Created branch %s\n", name);
    return 0;
  }

  if (npos == 2 && !del && !create && !rename_m && !rename_M) {
    const char *name = pos[0];
    const char *start = pos[1];
    if (!is_safe_ref_component(name)) bao_die("invalid branch name");
    char *base = bao_resolve_revision(start);
    if (!base) base = bao_rev_parse(start);
    if (!base) bao_die("could not resolve start");
    if (!bao_is_hex64(base) || verify_commit_object_misc(base) != 0) {
      free(base);
      bao_die("unknown commit: %s", start);
    }
    int rc = create_branch_at_switch(name, base, 0);
    free(base);
    if (rc == -2) bao_die("branch already exists: %s", name);
    if (rc != 0) bao_die("failed to create branch");
    printf("Created branch %s at %s\n", name, start);
    return 0;
  }

  if (npos == 0) {
    char *current = bao_current_branch();
    DIR *d = opendir(BAO_HEADS_DIR);
    if (!d) bao_die("cannot open %s", BAO_HEADS_DIR);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (de->d_name[0] == '.') continue;
      printf("%s%s\n", current && !strcmp(current, de->d_name) ? "* " : "  ", de->d_name);
      if (verbose) {
        char *bp = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, de->d_name);
        unsigned char *buf = NULL;
        size_t len = 0;
        char *subj = strdup("");
        if (bp && bao_read_file(bp, &buf, &len) == 0) {
          char *s = (char *)buf;
          while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
          char *e = s;
          while (*e && *e != '\n' && *e != '\r' && *e != ' ' && *e != '\t') e++;
          *e = 0;
          if (*s) {
            char *tmp = commit_subject_for_hash(s);
            free(subj);
            subj = tmp;
          }
        }
        free(buf);
        free(bp);
        printf("    %s\n", subj);
        free(subj);
      }
    }
    closedir(d);
    free(current);
    return 0;
  }

  const char *name = pos[0];
  if (!is_safe_ref_component(name)) bao_die("invalid branch name");
  char *path = bao_strdup_printf("%s/%s", BAO_HEADS_DIR, name);
  if (!path) bao_die("out of memory");
  if (bao_file_exists(path)) {
    free(path);
    bao_die("branch already exists: %s", name);
  }
  char *h = bao_resolve_head_full();
  if (!h || !bao_is_hex64(h)) {
    free(h);
    free(path);
    bao_die("cannot resolve HEAD");
  }
  char *content = bao_strdup_printf("%s\n", h);
  free(h);
  if (!content) {
    free(path);
    bao_die("out of memory");
  }
  if (bao_mkdir_p(BAO_HEADS_DIR) != 0 || bao_write_file_atomic(path, (const unsigned char *)content, strlen(content)) != 0) {
    free(path);
    free(content);
    bao_die("failed to create branch");
  }
  free(path);
  free(content);
  printf("Created branch %s at current HEAD\n", name);
  return 0;
}

int cmd_tag(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int list_only = 0, delete_tag = 0;
  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list")) {
      list_only = 1;
      continue;
    }
    if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--delete")) {
      delete_tag = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    break;
  }
  int npos = argc - i;
  char **pos = &argv[i];

  if (list_only || npos == 0) {
    const char *pat = npos >= 1 ? pos[0] : NULL;
    if (bao_mkdir_p(BAO_TAGS_DIR) != 0) bao_die("mkdir tags");
    DIR *d = opendir(BAO_TAGS_DIR);
    if (!d) {
      printf("(no tags)\n");
      return 0;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (de->d_name[0] == '.') continue;
      if (pat && strstr(de->d_name, pat) == NULL) continue;
      printf("  %s\n", de->d_name);
    }
    closedir(d);
    return 0;
  }

  if (delete_tag) {
    if (npos < 1) bao_die("usage: bao tag -d <name>");
    const char *name = pos[0];
    if (!is_safe_ref_component(name)) bao_die("invalid tag name");
    char *path = bao_strdup_printf("%s/%s", BAO_TAGS_DIR, name);
    if (!path) bao_die("oom");
    if (!bao_file_exists(path)) {
      free(path);
      bao_die("tag not found: %s", name);
    }
    if (remove(path) != 0) {
      free(path);
      bao_die("delete failed");
    }
    free(path);
    printf("Deleted tag %s\n", name);
    return 0;
  }

  const char *name = pos[0];
  if (!is_safe_ref_component(name)) bao_die("invalid tag name");
  char *path = bao_strdup_printf("%s/%s", BAO_TAGS_DIR, name);
  if (!path) bao_die("out of memory");
  if (bao_file_exists(path)) {
    free(path);
    bao_die("tag already exists: %s", name);
  }
  char *h = bao_resolve_head_full();
  if (!h || !bao_is_hex64(h)) {
    free(h);
    free(path);
    bao_die("cannot resolve HEAD");
  }
  char *content = bao_strdup_printf("%s\n", h);
  free(h);
  if (bao_mkdir_p(BAO_TAGS_DIR) != 0 || bao_write_file_atomic(path, (const unsigned char *)content, strlen(content)) != 0) {
    free(path);
    free(content);
    bao_die("failed to create tag");
  }
  free(path);
  free(content);
  printf("Tagged HEAD as %s\n", name);
  return 0;
}

int cmd_show(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int stat_only = 0, name_only = 0;
  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "--stat")) {
      stat_only = 1;
      continue;
    }
    if (!strcmp(argv[i], "--name-only") || !strcmp(argv[i], "--name-status")) {
      name_only = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    break;
  }
  if (argc - i < 1) bao_die("usage: bao show [--stat|--name-only] <hash-or-ref>");
  char *sh = bao_resolve_revision(argv[i]);
  if (!sh) sh = bao_rev_parse(argv[i]);
  if (!sh) bao_die("unknown ref or hash: %s", argv[i]);
  if (!bao_is_hex64(sh)) {
    free(sh);
    bao_die("unknown ref or hash: %s", argv[i]);
  }
  char path[512];
  if (bao_commit_json_path_full(sh, path, sizeof(path)) != 0) {
    free(sh);
    bao_die("commit path error");
  }
  free(sh);
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(path, &buf, &len) != 0) {
    bao_die("commit not found");
  }
  cJSON *j = cJSON_Parse((char *)buf);
  free(buf);
  if (!j) bao_die("invalid commit JSON");
  if (name_only) {
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(j, "staged_files");
    if (cJSON_IsArray(arr)) {
      int n = cJSON_GetArraySize(arr);
      for (int k = 0; k < n; k++) {
        cJSON *it = cJSON_GetArrayItem(arr, k);
        if (cJSON_IsString(it) && it->valuestring) printf("%s\n", it->valuestring);
      }
    }
    cJSON_Delete(j);
    return 0;
  }
  if (stat_only) {
    cJSON *h = cJSON_GetObjectItemCaseSensitive(j, "hash");
    cJSON *prov = cJSON_GetObjectItemCaseSensitive(j, "provider");
    cJSON *m = cJSON_GetObjectItemCaseSensitive(j, "message");
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(j, "staged_files");
    printf("commit %s\n", h && cJSON_IsString(h) ? h->valuestring : "?");
    if (prov && cJSON_IsString(prov) && prov->valuestring && prov->valuestring[0])
      printf("provider %s\n", prov->valuestring);
    if (m && cJSON_IsString(m)) printf("    %s\n\n", m->valuestring);
    if (cJSON_IsArray(arr)) {
      int n = cJSON_GetArraySize(arr);
      printf(" %d file%s changed\n", n, n == 1 ? "" : "s");
      for (int k = 0; k < n; k++) {
        cJSON *it = cJSON_GetArrayItem(arr, k);
        if (cJSON_IsString(it) && it->valuestring) printf(" %s\n", it->valuestring);
      }
    }
    cJSON_Delete(j);
    return 0;
  }
  char *pretty = cJSON_Print(j);
  cJSON_Delete(j);
  if (!pretty) bao_die("out of memory");
  printf("%s", pretty);
  free(pretty);
  return 0;
}

int cmd_diff(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int cached = 0;
  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "--cached") || !strcmp(argv[i], "--staged")) {
      cached = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    break;
  }
  int off = i;
  if (cached) {
    char *head = bao_resolve_head_full();
    if (!head || !bao_is_hex64(head)) {
      free(head);
      bao_die("cannot resolve HEAD for diff --cached");
    }
    char **st = NULL;
    size_t n = 0;
    if (bao_staged_load(&st, &n) != 0) {
      free(head);
      bao_die("read staged");
    }
    char path[512];
    if (bao_commit_json_path_full(head, path, sizeof(path)) != 0) {
      free(head);
      bao_staged_free(st, n);
      bao_die("commit path error");
    }
    free(head);
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(path, &buf, &len) != 0) {
      bao_staged_free(st, n);
      printf("diff --cached: no commit at HEAD (nothing to compare).\n");
      return 0;
    }
    cJSON *j = cJSON_Parse((char *)buf);
    free(buf);
    if (!j) {
      bao_staged_free(st, n);
      bao_die("invalid commit JSON");
    }
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(j, "staged_files");
    printf("diff --cached (HEAD commit vs index)\n");
    if (cJSON_IsArray(arr)) {
      int nc = cJSON_GetArraySize(arr);
      for (int a = 0; a < (int)n; a++) {
        const char *p = st[a];
        int in_head = 0;
        for (int b = 0; b < nc; b++) {
          cJSON *it = cJSON_GetArrayItem(arr, b);
          if (cJSON_IsString(it) && it->valuestring && !strcmp(it->valuestring, p)) {
            in_head = 1;
            break;
          }
        }
        if (!in_head) printf("+ staged: %s (not in HEAD commit)\n", p);
      }
      for (int b = 0; b < nc; b++) {
        cJSON *it = cJSON_GetArrayItem(arr, b);
        if (!cJSON_IsString(it) || !it->valuestring) continue;
        int in_idx = 0;
        for (size_t a = 0; a < n; a++) {
          if (st[a] && !strcmp(st[a], it->valuestring)) {
            in_idx = 1;
            break;
          }
        }
        if (!in_idx) printf("- commit: %s (not in index)\n", it->valuestring);
      }
    }
    cJSON_Delete(j);
    bao_staged_free(st, n);
    return 0;
  }
  if (argc - off >= 2) {
    char *a = bao_resolve_revision(argv[off]);
    if (!a) a = bao_rev_parse(argv[off]);
    char *b = bao_resolve_revision(argv[off + 1]);
    if (!b) b = bao_rev_parse(argv[off + 1]);
    if (!a || !b || !bao_is_hex64(a) || !bao_is_hex64(b)) {
      free(a);
      free(b);
      bao_die("could not resolve refs");
    }
    char pa[512], pb[512];
    if (bao_commit_json_path_full(a, pa, sizeof(pa)) != 0 || bao_commit_json_path_full(b, pb, sizeof(pb)) != 0) {
      free(a);
      free(b);
      bao_die("commit path error");
    }
    free(a);
    free(b);
    unsigned char *ba = NULL, *bb = NULL;
    size_t la = 0, lb = 0;
    if (bao_read_file(pa, &ba, &la) != 0 || bao_read_file(pb, &bb, &lb) != 0) {
      free(ba);
      free(bb);
      bao_die("could not read commit objects");
    }
    cJSON *ja = cJSON_Parse((char *)ba);
    cJSON *jb = cJSON_Parse((char *)bb);
    free(ba);
    free(bb);
    if (!ja || !jb) {
      cJSON_Delete(ja);
      cJSON_Delete(jb);
      bao_die("invalid JSON");
    }
    const char *ha = NULL, *hb = NULL;
    cJSON *ja_h = cJSON_GetObjectItemCaseSensitive(ja, "hash");
    cJSON *jb_h = cJSON_GetObjectItemCaseSensitive(jb, "hash");
    if (ja_h && cJSON_IsString(ja_h)) ha = ja_h->valuestring;
    if (jb_h && cJSON_IsString(jb_h)) hb = jb_h->valuestring;
    printf("diff commits %s vs %s\n", ha ? ha : "?", hb ? hb : "?");
    cJSON_Delete(ja);
    cJSON_Delete(jb);
    printf("(full eval diff is not implemented; compare commit JSON under %s)\n", BAO_COMMITS_DIR);
    return 0;
  }
  char **st = NULL;
  size_t n = 0;
  if (bao_staged_load(&st, &n) != 0) bao_die("read staged");
  printf("Staged paths (%zu):\n", n);
  for (size_t k = 0; k < n; k++) printf("  %s\n", st[k]);
  bao_staged_free(st, n);
  return 0;
}

int cmd_stash(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int off = (argc > 0 && argv[0] && strcmp(argv[0], "stash") == 0) ? 1 : 0;
  const char *sub = "list";
  if (argc - off >= 1) sub = argv[off];
  if (strcmp(sub, "list") == 0) {
    if (!bao_file_exists(BAO_STASH_FILE)) {
      printf("stash@{0}: empty\n");
      return 0;
    }
    printf("stash@{0}: present (see %s)\n", BAO_STASH_FILE);
    return 0;
  }
  if (strcmp(sub, "clear") == 0) {
    if (bao_file_exists(BAO_STASH_FILE)) (void)remove(BAO_STASH_FILE);
    printf("Stash cleared.\n");
    return 0;
  }
  if (strcmp(sub, "drop") == 0) {
    if (!bao_file_exists(BAO_STASH_FILE)) bao_die("no stash");
    if (remove(BAO_STASH_FILE) != 0) bao_die("drop failed");
    printf("Dropped stash.\n");
    return 0;
  }
  if (strcmp(sub, "push") == 0 || strcmp(sub, "save") == 0) {
    int pi = off + 1;
    while (pi < argc) {
      if (!strcmp(argv[pi], "-m") || !strcmp(argv[pi], "--message")) {
        pi++;
        if (pi < argc) pi++;
        continue;
      }
      if (argv[pi][0] == '-') bao_die("unknown option: %s", argv[pi]);
      break;
    }
    char **st = NULL;
    size_t n = 0;
    if (bao_staged_load(&st, &n) != 0) bao_die("read staged");
    if (n == 0) {
      bao_staged_free(st, n);
      printf("No staged changes to stash.\n");
      return 0;
    }
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += strlen(st[i]) + 1;
    char *buf = (char *)malloc(total + 1);
    if (!buf) {
      bao_staged_free(st, n);
      bao_die("oom");
    }
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
      memcpy(buf + o, st[i], strlen(st[i]));
      o += strlen(st[i]);
      buf[o++] = '\n';
    }
    if (bao_write_file_atomic(BAO_STASH_FILE, (const unsigned char *)buf, o) != 0) {
      free(buf);
      bao_staged_free(st, n);
      bao_die("write stash");
    }
    free(buf);
    bao_staged_free(st, n);
    bao_staged_clear();
    printf("Stashed %zu path(s); index cleared.\n", n);
    return 0;
  }
  if (strcmp(sub, "pop") == 0 || strcmp(sub, "apply") == 0) {
    if (!bao_file_exists(BAO_STASH_FILE)) bao_die("no stash");
    FILE *f = fopen(BAO_STASH_FILE, "r");
    if (!f) bao_die("read stash");
    char line[1024];
    char **lines = NULL;
    size_t nlines = 0;
    while (fgets(line, sizeof(line), f)) {
      char *e = line + strlen(line);
      while (e > line && (e[-1] == '\n' || e[-1] == '\r')) e--;
      *e = 0;
      if (!line[0]) continue;
      char **np = (char **)realloc(lines, (nlines + 1) * sizeof(char *));
      if (!np) {
        fclose(f);
        for (size_t i = 0; i < nlines; i++) free(lines[i]);
        free(lines);
        bao_die("oom");
      }
      lines = np;
      lines[nlines++] = strdup(line);
    }
    fclose(f);
    if (nlines == 0) {
      free(lines);
      bao_die("empty stash");
    }
    if (bao_staged_merge_in(lines, nlines) != 0) {
      for (size_t i = 0; i < nlines; i++) free(lines[i]);
      free(lines);
      bao_die("merge stash into index");
    }
    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    if (strcmp(sub, "pop") == 0) (void)remove(BAO_STASH_FILE);
    printf("Applied stash to staging area.\n");
    return 0;
  }
  bao_die("usage: bao stash [list|push|pop|apply|drop|clear]");
  return 1;
}

int cmd_restore(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int arg0 = 0;
  if (argc > 0 && strcmp(argv[0], "restore") == 0) arg0 = 1;
  int only_staged = 0;
  const char *source = NULL;
  while (arg0 < argc && argv[arg0][0] == '-') {
    if (strcmp(argv[arg0], "--staged") == 0) {
      only_staged = 1;
      arg0++;
      continue;
    }
    if (!strncmp(argv[arg0], "--source=", 9)) {
      source = argv[arg0] + 9;
      arg0++;
      continue;
    }
    if (!strcmp(argv[arg0], "-s") || !strcmp(argv[arg0], "--source")) {
      if (arg0 + 1 >= argc) bao_die("usage: bao restore -s <commit> [--staged] [paths]");
      source = argv[arg0 + 1];
      arg0 += 2;
      continue;
    }
    if (strcmp(argv[arg0], "--worktree") == 0 || strcmp(argv[arg0], "-W") == 0) {
      bao_warn("worktree restore is not implemented");
      arg0++;
      continue;
    }
    bao_die("unknown option: %s", argv[arg0]);
  }
    if (source) {
    char *sh = bao_resolve_revision(source);
    if (!sh) sh = bao_rev_parse(source);
    if (!sh) bao_die("could not resolve --source");
    if (!bao_is_hex64(sh) || verify_commit_object_misc(sh) != 0) {
      free(sh);
      bao_die("unknown commit: %s", source);
    }
    if (bao_staged_restore_from_commit_short(sh) != 0) {
      free(sh);
      bao_die("restore from commit failed");
    }
    free(sh);
    printf("Restored index from commit %s\n", source);
    return 0;
  }
  if (only_staged || arg0 >= argc) {
    bao_staged_clear();
    printf("Unstaged all paths (--staged).\n");
    return 0;
  }
  if (bao_staged_remove_paths((const char **)&argv[arg0], (size_t)(argc - arg0)) != 0) bao_die("restore failed");
  printf("Removed %zu path(s) from staging.\n", (size_t)(argc - arg0));
  return 0;
}

int cmd_rm(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int cached = 0;
  int force = 0;
  int recursive = 0;
  int i = 1;
  if (argc > 0 && strcmp(argv[0], "rm") == 0) i = 1;
  else
    i = 0;
  while (i < argc && argv[i][0] == '-') {
    if (strcmp(argv[i], "--cached") == 0) cached = 1;
    else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0)
      force = 1;
    else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--recursive") == 0)
      recursive = 1;
    else
      bao_die("unknown option: %s", argv[i]);
    i++;
  }
  (void)force;
  if (i >= argc) bao_die("usage: bao rm [--cached] [-f] [-r] <path> [...]");
  if (cached) {
    if (bao_staged_remove_paths((const char **)&argv[i], (size_t)(argc - i)) != 0) bao_die("rm failed");
    printf("Removed %d path(s) from staging.\n", argc - i);
    return 0;
  }
  for (int j = i; j < argc; j++) {
    if (!bao_is_safe_relpath(argv[j])) bao_die("unsafe path: %s", argv[j]);
    if (bao_remove_path(argv[j], recursive) != 0) bao_die("failed to remove: %s", argv[j]);
  }
  if (bao_staged_remove_paths((const char **)&argv[i], (size_t)(argc - i)) != 0) bao_die("rm index update failed");
  printf("Removed %d path(s) from disk and staging.\n", argc - i);
  return 0;
}

int cmd_rev_parse(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");
  int i = 0;
  if (argc > 0 && strcmp(argv[0], "rev-parse") == 0) i = 1;
  /* Default: print full hash; shorten only with --short / --short=N */
  int short_len = 0;
  int show_toplevel = 0, show_gitdir = 0;
  int verify = 0;
  for (; i < argc; i++) {
    if (!strncmp(argv[i], "--short=", 8)) {
      short_len = atoi(argv[i] + 8);
      if (short_len < 4) short_len = 4;
      if (short_len > 40) short_len = 40;
      continue;
    }
    if (!strcmp(argv[i], "--short")) {
      short_len = 7;
      continue;
    }
    if (!strcmp(argv[i], "--show-toplevel")) {
      show_toplevel = 1;
      continue;
    }
    if (!strcmp(argv[i], "--git-dir")) {
      show_gitdir = 1;
      continue;
    }
    if (!strcmp(argv[i], "--verify")) {
      verify = 1;
      continue;
    }
    if (argv[i][0] == '-') continue;
    break;
  }
  if (show_toplevel) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) bao_die("getcwd failed");
    printf("%s\n", cwd);
  }
  if (show_gitdir) printf("%s\n", BAO_DIR);
  if (i >= argc) return 0;
  for (; i < argc; i++) {
    char *r = bao_resolve_revision(argv[i]);
    if (!r) r = bao_rev_parse(argv[i]);
    if (!r) {
      if (verify) {
        fprintf(stderr, "bao: %s\n", argv[i]);
        return 1;
      }
      fprintf(stderr, "bao: %s\n", argv[i]);
      return 1;
    }
    if (short_len > 0 && (size_t)short_len < strlen(r)) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%.*s", short_len, r);
      printf("%s\n", buf);
    } else {
      printf("%s\n", r);
    }
    free(r);
  }
  return 0;
}

static int config_get_line(const char *path, const char *key, char *out, size_t outsz) {
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    char *p = strchr(line, '#');
    if (p) *p = 0;
    char *colon = strchr(line, ':');
    if (!colon) continue;
    *colon = 0;
    char *k = line;
    while (*k == ' ' || *k == '\t') k++;
    char *ke = k + strlen(k);
    while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
    *ke = 0;
    if (strcmp(k, key) != 0) continue;
    char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    size_t n = strlen(v);
    while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r')) n--;
    v[n] = 0;
    snprintf(out, outsz, "%s", v);
    fclose(f);
    return 0;
  }
  fclose(f);
  return -1;
}

int cmd_config(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  const char *cfgpath = "bao.yaml";
  int off = (argc > 0 && strcmp(argv[0], "config") == 0) ? 1 : 0;
  if (argc - off < 1) bao_die("usage: bao config --list | bao config [--get] <key> [<value>]");
  if (!strcmp(argv[off], "--unset") && argc - off >= 2) {
    bao_warn("bao config --unset is not implemented; edit %s manually", cfgpath);
    return 1;
  }
  if (!strcmp(argv[off], "--get") && argc - off >= 2) off += 1;
  if (strcmp(argv[off], "--profiles") == 0) {
    const char *base = "prompts";
    char val[1024];
    if (config_get_line(cfgpath, "prompts_dir", val, sizeof(val)) == 0 && val[0]) base = val;
    if (!bao_file_exists(base) || !bao_is_dir(base)) bao_die("prompts_dir not found: %s", base);
    DIR *d = opendir(base);
    if (!d) bao_die("failed to open: %s", base);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (!de->d_name[0] || de->d_name[0] == '.') continue;
      char *p = bao_strdup_printf("%s/%s", base, de->d_name);
      if (!p) {
        closedir(d);
        bao_die("out of memory");
      }
      if (bao_is_dir(p)) {
        /* A profile is valid only if its directory has at least one regular file under it. */
        int has_file = 0;
        DIR *pd = opendir(p);
        if (pd) {
          struct dirent *pe;
          while ((pe = readdir(pd)) != NULL) {
            if (!pe->d_name[0] || pe->d_name[0] == '.') continue;
            char *pp = bao_strdup_printf("%s/%s", p, pe->d_name);
            if (!pp) {
              closedir(pd);
              free(p);
              closedir(d);
              bao_die("out of memory");
            }
            if (!bao_is_dir(pp) && bao_file_exists(pp)) has_file = 1;
            free(pp);
            if (has_file) break;
          }
          closedir(pd);
        }
        if (has_file) printf("%s\n", de->d_name);
      }
      free(p);
    }
    closedir(d);
    return 0;
  }
  if (strcmp(argv[off], "--list") == 0 || strcmp(argv[off], "-l") == 0) {
    int raw = 0;
    const char *profile_override = NULL;
    for (int i = off + 1; i < argc; i++) {
      if (!strcmp(argv[i], "--all") || !strcmp(argv[i], "--raw")) raw = 1;
      else if (!strcmp(argv[i], "--profile")) {
        if (i + 1 >= argc) bao_die("usage: bao config --list --profile <name>");
        profile_override = argv[i + 1];
        i++;
      }
      else if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    }
    if (raw) {
      unsigned char *buf = NULL;
      size_t len = 0;
      if (bao_read_file(cfgpath, &buf, &len) != 0) bao_die("cannot read %s", cfgpath);
      printf("%s", (char *)buf);
      free(buf);
      return 0;
    }

    BaoConfig *cfg = profile_override ? load_config_with_profile(cfgpath, profile_override) : load_config(cfgpath);
    if (!cfg) bao_die("failed to load %s", cfgpath);
    if (cfg->provider && cfg->provider[0]) printf("provider: %s\n", cfg->provider);
    if (cfg->profile && cfg->profile[0]) printf("profile: %s\n", cfg->profile);
    if (cfg->model && cfg->model[0]) printf("model: %s\n", cfg->model);
    printf("temperature: %.6g\n", cfg->temperature);
    printf("max_tokens: %d\n", cfg->max_tokens);
    if (cfg->prompt_filepath && cfg->prompt_filepath[0]) printf("prompt_file: %s\n", cfg->prompt_filepath);
    if (cfg->prompts_dir && cfg->prompts_dir[0]) printf("prompts_dir: %s\n", cfg->prompts_dir);
    if (cfg->dataset_path && cfg->dataset_path[0]) printf("dataset: %s\n", cfg->dataset_path);
    free_config(cfg);
    return 0;
  }
  if (argc - off == 1) {
    char val[1024];
    if (config_get_line(cfgpath, argv[off], val, sizeof(val)) != 0) bao_die("key not found: %s", argv[off]);
    printf("%s\n", val);
    return 0;
  }
  bao_warn("setting keys in bao.yaml is not implemented; edit the file manually");
  return 1;
}

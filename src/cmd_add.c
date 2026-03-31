#include "bao.h"

#include "stage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_add(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  int dry = 0;
  int all = 0;
  int verbose = 0;
  int update = 0;
  (void)verbose;
  char **paths = NULL;
  size_t np = 0, cap = 0;

  int i = 1;
  for (; i < argc; i++) {
    if (!strcmp(argv[i], "--")) {
      i++;
      break;
    }
    if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-n") == 0) {
      dry = 1;
      continue;
    }
    if (strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--all") == 0) {
      all = 1;
      continue;
    }
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
      verbose = 1;
      continue;
    }
    if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--update") == 0) {
      update = 1;
      continue;
    }
    if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);

    if (np == cap) {
      size_t ncap = cap ? cap * 2 : 8;
      char **n = (char **)realloc(paths, ncap * sizeof(char *));
      if (!n) {
        free(paths);
        bao_die("out of memory");
      }
      paths = n;
      cap = ncap;
    }
    paths[np++] = argv[i];
  }
  for (; i < argc; i++) {
    if (np == cap) {
      size_t ncap = cap ? cap * 2 : 8;
      char **n = (char **)realloc(paths, ncap * sizeof(char *));
      if (!n) {
        free(paths);
        bao_die("out of memory");
      }
      paths = n;
      cap = ncap;
    }
    paths[np++] = argv[i];
  }

  if (update) {
    if (dry || all) bao_die("cannot combine -u with --dry-run or -A");
    if (np > 0) bao_warn("paths with -u are ignored; updating tracked paths in index");
    if (bao_staged_update_index() != 0) bao_die("index update failed");
    printf("Updated staged paths (removed missing files from index).\n");
    free(paths);
    return 0;
  }

  if (dry) {
    if (all) {
      if (np > 0) bao_warn("paths after -A are ignored in dry-run");
      char **out = NULL;
      size_t n = 0;
      if (bao_staged_collect_add_all_paths(&out, &n) != 0) bao_die("dry-run: collect failed");
      printf("Would stage %zu path(s):\n", n);
      for (size_t j = 0; j < n; j++) printf("  %s\n", out[j]);
      bao_staged_free(out, n);
      free(paths);
      return 0;
    }
    if (np == 0) bao_die("usage: bao add [--dry-run|-n] [-A|--all] <path> [...]");
    size_t total = 0;
    for (size_t j = 0; j < np; j++) {
      char **out = NULL;
      size_t n = 0;
      if (bao_staged_collect_from_path(paths[j], &out, &n) != 0) bao_die("dry-run: %s", paths[j]);
      for (size_t k = 0; k < n; k++) {
        printf("  %s\n", out[k]);
        total++;
      }
      bao_staged_free(out, n);
    }
    printf("Would stage %zu path(s) total.\n", total);
    free(paths);
    return 0;
  }

  if (all) {
    if (np > 0) bao_warn("paths after -A are ignored");
    free(paths);
    if (bao_staged_add_all() != 0) bao_die("bao add -A failed");
    printf("Staged default paths (bao.yaml, prompts/, dataset when configured)\n");
    return 0;
  }

  if (np == 0) bao_die("usage: bao add [-u|--update] [--dry-run|-n] [-A|--all] [-v] <path> [...]");

  int total = 0;
  for (size_t j = 0; j < np; j++) {
    int n = bao_staged_add_path(paths[j]);
    if (n < 0) bao_die("failed to add: %s", paths[j]);
    total += n;
    if (verbose) printf("add '%s' (%d path(s) this call)\n", paths[j], n);
  }
  free(paths);
  if (!verbose) printf("Staged %d path(s)\n", total);
  return 0;
}

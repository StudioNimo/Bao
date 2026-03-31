#include "bao.h"

#include "util.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s <command> [args]\n"
          "\n"
          "Core: init, add, commit, log, checkout, switch, status, version\n"
          "Refs: branch, tag, show, rev-parse, reset, restore, rm, stash\n"
          "Other: config, diff, remote, run, eval, push, pull\n"
          "Stubs: clone, merge, fetch, rebase, filter-branch, cherry-pick, revert, blame\n",
          argv0);
}

int main(int argc, char **argv) {
  bao_ctx_t ctx = {.argv0 = (argc > 0) ? argv[0] : "bao"};
  if (argc < 2) {
    usage(ctx.argv0);
    return 2;
  }

  const char *cmd = argv[1];

  if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help")) return cmd_help(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "help")) return cmd_help(&ctx, argc - 1, argv + 1);

  if (!strcmp(cmd, "init")) return cmd_init(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "add")) return cmd_add(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "commit")) return cmd_commit(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "log")) return cmd_log(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "checkout")) return cmd_checkout(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "switch")) return cmd_switch(&ctx, argc - 1, argv + 1);

  if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) return cmd_version(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "status")) return cmd_status(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "diff")) return cmd_diff(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "reset")) return cmd_reset(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "branch")) return cmd_branch(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "tag")) return cmd_tag(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "show")) return cmd_show(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "stash")) return cmd_stash(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "restore")) return cmd_restore(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "rm")) return cmd_rm(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "rev-parse")) return cmd_rev_parse(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "config") || !strcmp(cmd, "comfig")) return cmd_config(&ctx, argc - 1, argv + 1);

  if (!strcmp(cmd, "clone")) return cmd_clone(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "merge")) return cmd_merge(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "fetch")) return cmd_fetch(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "rebase")) return cmd_rebase(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "filter-branch")) return cmd_filter_branch(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "cherry-pick")) return cmd_cherry_pick(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "revert")) return cmd_revert(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "blame")) return cmd_blame(&ctx, argc - 1, argv + 1);

  if (!strcmp(cmd, "run")) return cmd_run(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "eval")) return cmd_eval(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "push") || !strcmp(cmd, "pull")) return cmd_remote(&ctx, argc - 1, argv + 1);
  if (!strcmp(cmd, "remote")) return cmd_remote(&ctx, argc - 2, argv + 2);

  bao_warn("unknown command: %s", cmd);
  usage(ctx.argv0);
  return 2;
}

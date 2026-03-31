#include "bao.h"

#include <stdio.h>
#include <string.h>

int cmd_help(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  const char *topic = (argc >= 2) ? argv[1] : NULL;
  if (topic && strcmp(topic, "bao")) {
    fprintf(stderr, "bao: no help for '%s' (try: bao help)\n", topic);
    return 1;
  }
  printf(
      "Bao - local-first experiment tracker (git-like CLI subset)\n\n"
      "Usage: bao <command> [options]\n\n"
      "Core:\n"
      "  init              Create .bao/ and empty main branch\n"
      "  add               Stage paths (-A, -u/--update, -n/--dry-run, -v)\n"
      "  commit            -m, -F, -a, -q, --amend\n"
      "  log               -n, --oneline, -1\n"
      "  checkout          -b, -B, --detach, <branch|hash>\n"
      "  switch            -c, -C, --detach (git switch style)\n"
      "  status            -s, -b, --porcelain\n"
      "  version           (--version)\n\n"
      "Refs & index:\n"
      "  branch            -d/-D, -m/-M, -v, -c, list\n"
      "  tag               -l, -d\n"
      "  show              --stat, --name-only\n"
      "  rev-parse         --short=N, --show-toplevel, --git-dir, --verify\n"
      "  reset             --soft/--mixed/--hard [<commit>]\n"
      "  restore           --staged, -s/--source <commit>\n"
      "  rm                --cached, -f, -r\n"
      "  stash             list, push/save [-m], pop, apply, clear, drop\n"
      "  diff              --cached, [--eval] [<commit> <commit>]\n"
      "  run               [-d dataset.jsonl] [--dry-run] (OpenAI; needs OPENAI_API_KEY)\n"
      "  eval              interactive 1/2/3 scoring (TTY required)\n\n"
      "Config & remote:\n"
      "  config            --list, --get, (set = edit bao.yaml)\n"
      "  remote            -v, add, remove, rename, set-url, get-url, show\n\n"
      "Other: push/pull (stubs), clone/merge/fetch/rebase/... (stubs)\n");
  return 0;
}

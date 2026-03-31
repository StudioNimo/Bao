#include "bao.h"

#include <stdio.h>
#include <string.h>

int cmd_version(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) return cmd_help(ctx, argc - 1, argv + 1);
  printf("bao version %s\n", BAO_VERSION);
  return 0;
}

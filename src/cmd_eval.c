#include "bao.h"

#include "util.h"

int cmd_eval(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  (void)argc;
  (void)argv;
  bao_die("eval is not implemented yet (planned: termios RAW mode + 1/2/3 scoring)");
  return 1;
}


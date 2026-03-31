#include "bao.h"

#include "util.h"

int cmd_run(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  (void)argc;
  (void)argv;
  bao_die("run is not implemented yet (planned: libcurl provider abstraction + store executions)");
  return 1;
}


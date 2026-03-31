#include "bao.h"

#include "db.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static int write_text_file_if_missing(const char *path, const char *content) {
  if (bao_file_exists(path)) return 0;
  return bao_write_file_atomic(path, (const unsigned char *)content, strlen(content));
}

int cmd_init(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  (void)argc;
  (void)argv;

  if (bao_mkdir_p(BAO_COMMITS_DIR) != 0) bao_die("failed to create %s", BAO_COMMITS_DIR);
  if (bao_mkdir_p(BAO_RESULTS_DIR) != 0) bao_die("failed to create %s", BAO_RESULTS_DIR);
  if (bao_mkdir_p(BAO_HEADS_DIR) != 0) bao_die("failed to create %s", BAO_HEADS_DIR);

  // HEAD points to refs/heads/main by default.
  if (write_text_file_if_missing(BAO_HEAD_FILE, "ref: refs/heads/main\n") != 0)
    bao_die("failed to write %s", BAO_HEAD_FILE);
  if (write_text_file_if_missing(BAO_HEADS_DIR "/main", "\n") != 0)
    bao_die("failed to write %s", BAO_HEADS_DIR "/main");

  bao_db_t db;
  if (bao_db_open(&db, BAO_DB_FILE) != 0) bao_die("failed to open %s", BAO_DB_FILE);
  if (bao_db_init_schema(&db) != 0) {
    bao_db_close(&db);
    bao_die("failed to init db schema");
  }
  bao_db_close(&db);

  printf("Initialized empty Bao repository in %s/\n", BAO_DIR);
  return 0;
}


#include "bao.h"

#include "commits.h"
#include "db.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_log(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  int max_count = -1;
  int oneline = 0;
  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--max-count") == 0) && i + 1 < argc) {
      max_count = atoi(argv[i + 1]);
      if (max_count < 0) max_count = 0;
      i++;
      continue;
    }
    if (!strcmp(argv[i], "-1")) {
      max_count = 1;
      continue;
    }
    if (!strcmp(argv[i], "--oneline") || !strcmp(argv[i], "--pretty=oneline")) {
      oneline = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
  }

  if (!bao_file_exists(BAO_DB_FILE)) bao_die("not a bao repo (run `bao init` first)");

  bao_db_t db;
  if (bao_db_open(&db, BAO_DB_FILE) != 0) bao_die("failed to open db");
  if (bao_db_init_schema(&db) != 0) {
    bao_db_close(&db);
    bao_die("failed to init schema");
  }

  char *sql = NULL;
  if (max_count >= 0) {
    sql = sqlite3_mprintf(
        "SELECT c.hash, c.created_at, c.model, c.provider, c.temperature,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash AND e.score='GOOD') AS good,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash AND e.score='BAD') AS bad,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash) AS total_eval"
        " FROM commits c"
        " ORDER BY c.created_at DESC LIMIT %d;",
        max_count);
  } else {
    sql = sqlite3_mprintf(
        "SELECT c.hash, c.created_at, c.model, c.provider, c.temperature,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash AND e.score='GOOD') AS good,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash AND e.score='BAD') AS bad,"
        "  (SELECT COUNT(*) FROM evaluations e JOIN executions x ON x.id=e.execution_id WHERE x.commit_hash=c.hash) AS total_eval"
        " FROM commits c"
        " ORDER BY c.created_at DESC;");
  }
  if (!sql) {
    bao_db_close(&db);
    bao_die("out of memory");
  }

  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_free(sql);
    bao_db_close(&db);
    bao_die("sqlite prepare failed");
  }
  sqlite3_free(sql);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *hash = (const char *)sqlite3_column_text(stmt, 0);
    const char *created_at = (const char *)sqlite3_column_text(stmt, 1);
    const char *model = (const char *)sqlite3_column_text(stmt, 2);
    const char *provider = (const char *)sqlite3_column_text(stmt, 3);
    double temp = sqlite3_column_double(stmt, 4);
    int good = sqlite3_column_int(stmt, 5);
    int bad = sqlite3_column_int(stmt, 6);
    int total = sqlite3_column_int(stmt, 7);

    const char *hshow = hash;
    char *disp = NULL;
    if (hash && bao_is_hex64(hash)) {
      disp = bao_display_short_from_full(hash);
      if (disp) hshow = disp;
    } else if (hash && strlen(hash) >= 7) {
      /* レガシー DB 行など */
      hshow = hash;
    }
    if (oneline) {
      printf("%s %s model=%s", hshow ? hshow : "", created_at ? created_at : "", model ? model : "");
      if (provider && provider[0]) printf(" provider=%s", provider);
      printf("\n");
      free(disp);
      continue;
    }
    printf("%s %s\n", hshow ? hshow : "", created_at ? created_at : "");
    free(disp);
    if (provider && provider[0])
      printf("    provider=%s model=%s temp=%.6g\n", provider, model ? model : "", temp);
    else
      printf("    model=%s temp=%.6g\n", model ? model : "", temp);
    if (total > 0) {
      printf("    eval: GOOD=%d BAD=%d total=%d\n", good, bad, total);
    }
  }

  sqlite3_finalize(stmt);
  bao_db_close(&db);
  return 0;
}

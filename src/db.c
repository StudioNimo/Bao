#include "db.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int bao_db_open(bao_db_t *out, const char *path) {
  memset(out, 0, sizeof(*out));
  if (sqlite3_open(path, &out->db) != SQLITE_OK) {
    return -1;
  }
  sqlite3_busy_timeout(out->db, 3000);
  return 0;
}

void bao_db_close(bao_db_t *db) {
  if (!db) return;
  if (db->db) sqlite3_close(db->db);
  db->db = NULL;
}

static int get_user_version(sqlite3 *db) {
  sqlite3_stmt *st = NULL;
  int v = 0;
  if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &st, NULL) != SQLITE_OK) return -1;
  if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);
  return v;
}

static int set_user_version(sqlite3 *db, int v) {
  char *sql = sqlite3_mprintf("PRAGMA user_version = %d;", v);
  if (!sql) return -1;
  char *err = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
  sqlite3_free(sql);
  if (err) {
    sqlite3_free(err);
  }
  return (rc == SQLITE_OK) ? 0 : -1;
}

static int exec_sql_retry(sqlite3 *db, const char *sql) {
  for (int attempt = 0; attempt < 3; attempt++) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc == SQLITE_OK) return 0;
    if (rc == SQLITE_BUSY && attempt < 2) {
      if (errmsg) sqlite3_free(errmsg);
      usleep(1000u * (unsigned)(attempt + 1));
      continue;
    }
    if (errmsg) {
      bao_warn("sqlite error: %s", errmsg);
      sqlite3_free(errmsg);
    }
    return -1;
  }
  return -1;
}

static void migrate_if_needed(sqlite3 *db) {
  int v = get_user_version(db);
  if (v >= 2) return;
  (void)exec_sql_retry(db, "DROP TABLE IF EXISTS evaluations;");
  (void)exec_sql_retry(db, "DROP TABLE IF EXISTS executions;");
  (void)exec_sql_retry(db, "DROP TABLE IF EXISTS commits;");
  (void)set_user_version(db, 2);
}

static int commits_has_provider_column(sqlite3 *db) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, "PRAGMA table_info(commits)", -1, &st, NULL) != SQLITE_OK) return 0;
  int found = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *name = (const char *)sqlite3_column_text(st, 1);
    if (name && strcmp(name, "provider") == 0) {
      found = 1;
      break;
    }
  }
  sqlite3_finalize(st);
  return found;
}

static void migrate_v3_provider(sqlite3 *db) {
  int v = get_user_version(db);
  if (v >= 3) return;
  if (!commits_has_provider_column(db)) {
    (void)exec_sql_retry(db, "ALTER TABLE commits ADD COLUMN provider TEXT NOT NULL DEFAULT '';");
  }
  (void)set_user_version(db, 3);
}

int bao_db_init_schema(bao_db_t *db) {
  migrate_if_needed(db->db);

  const char *sql =
      "PRAGMA journal_mode=WAL;"
      "PRAGMA foreign_keys=ON;"
      "CREATE TABLE IF NOT EXISTS commits ("
      "  hash TEXT PRIMARY KEY,"
      "  provider TEXT NOT NULL DEFAULT '',"
      "  model TEXT NOT NULL,"
      "  temperature REAL,"
      "  prompt_text TEXT NOT NULL,"
      "  dataset_hash TEXT,"
      "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
      "  synced INTEGER NOT NULL DEFAULT 0"
      ");"
      "CREATE TABLE IF NOT EXISTS executions ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  commit_hash TEXT NOT NULL,"
      "  dataset_id TEXT,"
      "  input_text TEXT NOT NULL,"
      "  output_text TEXT NOT NULL,"
      "  latency_ms INTEGER,"
      "  tokens_used INTEGER,"
      "  FOREIGN KEY(commit_hash) REFERENCES commits(hash)"
      ");"
      "CREATE TABLE IF NOT EXISTS evaluations ("
      "  execution_id INTEGER PRIMARY KEY,"
      "  score TEXT CHECK(score IN ('GOOD', 'NEUTRAL', 'BAD')),"
      "  note TEXT,"
      "  evaluated_at TEXT NOT NULL DEFAULT (datetime('now')),"
      "  FOREIGN KEY(execution_id) REFERENCES executions(id)"
      ");";

  if (exec_sql_retry(db->db, sql) != 0) return -1;
  migrate_v3_provider(db->db);
  return 0;
}

int bao_db_insert_commit(bao_db_t *db,
                         const char *hash_short,
                         const char *provider,
                         const char *model,
                         double temperature,
                         const char *prompt_text,
                         const char *dataset_hash,
                         const char *created_at_iso8601) {
  const char *sql =
      "INSERT OR IGNORE INTO commits(hash,provider,model,temperature,prompt_text,dataset_hash,created_at,synced)"
      " VALUES(?,?,?,?,?,?,?,0);";

  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

  sqlite3_bind_text(stmt, 1, hash_short, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, provider ? provider : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, model, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(stmt, 4, temperature);
  sqlite3_bind_text(stmt, 5, prompt_text, -1, SQLITE_TRANSIENT);
  if (dataset_hash) {
    sqlite3_bind_text(stmt, 6, dataset_hash, -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 6);
  }
  sqlite3_bind_text(stmt, 7, created_at_iso8601, -1, SQLITE_TRANSIENT);

  int step = SQLITE_BUSY;
  for (int attempt = 0; attempt < 3 && step == SQLITE_BUSY; attempt++) {
    step = sqlite3_step(stmt);
    if (step == SQLITE_BUSY && attempt < 2) {
      usleep(1000u * (unsigned)(attempt + 1));
    }
  }
  sqlite3_finalize(stmt);
  return (step == SQLITE_DONE) ? 0 : -1;
}

int bao_db_nth_commit_hash(bao_db_t *db, unsigned offset, char **out_hash) {
  if (!db || !db->db || !out_hash) return -1;
  *out_hash = NULL;
  const char *sql = "SELECT hash FROM commits ORDER BY created_at DESC LIMIT 1 OFFSET ?;";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int(stmt, 1, (int)offset);
  int step = sqlite3_step(stmt);
  if (step != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return -1;
  }
  const char *h = (const char *)sqlite3_column_text(stmt, 0);
  if (!h) {
    sqlite3_finalize(stmt);
    return -1;
  }
  *out_hash = strdup(h);
  sqlite3_finalize(stmt);
  return (*out_hash) ? 0 : -1;
}

int bao_db_insert_execution(bao_db_t *db,
                            const char *commit_hash,
                            const char *dataset_id,
                            const char *input_text,
                            const char *output_text,
                            int latency_ms,
                            int tokens_used) {
  const char *sql =
      "INSERT INTO executions(commit_hash,dataset_id,input_text,output_text,latency_ms,tokens_used)"
      " VALUES(?,?,?,?,?,?);";

  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

  sqlite3_bind_text(stmt, 1, commit_hash, -1, SQLITE_TRANSIENT);
  if (dataset_id) {
    sqlite3_bind_text(stmt, 2, dataset_id, -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 2);
  }
  sqlite3_bind_text(stmt, 3, input_text, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, output_text, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, latency_ms);
  sqlite3_bind_int(stmt, 6, tokens_used);

  int step = SQLITE_BUSY;
  for (int attempt = 0; attempt < 3 && step == SQLITE_BUSY; attempt++) {
    step = sqlite3_step(stmt);
    if (step == SQLITE_BUSY && attempt < 2) {
      usleep(1000u * (unsigned)(attempt + 1));
    }
  }
  sqlite3_finalize(stmt);
  return (step == SQLITE_DONE) ? 0 : -1;
}

int bao_db_update_commit_dataset_hash(bao_db_t *db, const char *commit_full_hash, const char *dataset_hash_hex) {
  if (!db || !db->db || !commit_full_hash || !dataset_hash_hex) return -1;
  const char *sql = "UPDATE commits SET dataset_hash=? WHERE hash=?;";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, dataset_hash_hex, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, commit_full_hash, -1, SQLITE_TRANSIENT);
  int step = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (step == SQLITE_DONE) ? 0 : -1;
}

void bao_uneval_exec_clear(bao_uneval_exec_t *e) {
  if (!e) return;
  free(e->input_text);
  free(e->output_text);
  e->input_text = e->output_text = NULL;
  e->execution_id = 0;
}

int bao_db_next_unevaluated_for_commit(bao_db_t *db, const char *commit_hash, bao_uneval_exec_t *out) {
  if (!db || !db->db || !commit_hash || !out) return -1;
  memset(out, 0, sizeof(*out));
  const char *sql =
      "SELECT e.id, e.input_text, e.output_text FROM executions e "
      "WHERE e.commit_hash=? AND NOT EXISTS (SELECT 1 FROM evaluations ev WHERE ev.execution_id=e.id) "
      "ORDER BY e.id LIMIT 1;";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, commit_hash, -1, SQLITE_TRANSIENT);
  int step = sqlite3_step(stmt);
  if (step != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return (step == SQLITE_DONE) ? 1 : -1;
  }
  out->execution_id = sqlite3_column_int64(stmt, 0);
  const char *in = (const char *)sqlite3_column_text(stmt, 1);
  const char *ot = (const char *)sqlite3_column_text(stmt, 2);
  out->input_text = in ? strdup(in) : strdup("");
  out->output_text = ot ? strdup(ot) : strdup("");
  sqlite3_finalize(stmt);
  if (!out->input_text || !out->output_text) {
    bao_uneval_exec_clear(out);
    return -1;
  }
  return 0;
}

int bao_db_insert_evaluation(bao_db_t *db, sqlite3_int64 exec_id, const char *score) {
  if (!db || !db->db || !score) return -1;
  const char *sql = "INSERT INTO evaluations(execution_id,score) VALUES(?,?);";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, exec_id);
  sqlite3_bind_text(stmt, 2, score, -1, SQLITE_TRANSIENT);
  int step = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (step == SQLITE_DONE) ? 0 : -1;
}

void bao_db_free_eval_rows(bao_eval_row_t *rows, size_t n) {
  if (!rows) return;
  for (size_t i = 0; i < n; i++) {
    free(rows[i].dataset_id);
    free(rows[i].score);
  }
  free(rows);
}

int bao_db_list_evaluations_for_commit(bao_db_t *db, const char *commit_hash, bao_eval_row_t **out_rows, size_t *out_n) {
  if (!db || !db->db || !commit_hash || !out_rows || !out_n) return -1;
  *out_rows = NULL;
  *out_n = 0;
  const char *sql =
      "SELECT e.dataset_id, ev.score FROM executions e "
      "JOIN evaluations ev ON ev.execution_id=e.id WHERE e.commit_hash=? ORDER BY e.id;";
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, commit_hash, -1, SQLITE_TRANSIENT);
  size_t cap = 0;
  bao_eval_row_t *rows = NULL;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *did = (const char *)sqlite3_column_text(stmt, 0);
    const char *sc = (const char *)sqlite3_column_text(stmt, 1);
    if (*out_n + 1 > cap) {
      size_t ncap = cap ? cap * 2 : 8;
      bao_eval_row_t *nr = (bao_eval_row_t *)realloc(rows, ncap * sizeof(*rows));
      if (!nr) {
        sqlite3_finalize(stmt);
        bao_db_free_eval_rows(rows, *out_n);
        return -1;
      }
      rows = nr;
      cap = ncap;
    }
    bao_eval_row_t *r = &rows[*out_n];
    r->dataset_id = did && *did ? strdup(did) : strdup("");
    r->score = sc && *sc ? strdup(sc) : strdup("?");
    if (!r->dataset_id || !r->score) {
      sqlite3_finalize(stmt);
      bao_db_free_eval_rows(rows, *out_n + 1);
      return -1;
    }
    (*out_n)++;
  }
  sqlite3_finalize(stmt);
  *out_rows = rows;
  return 0;
}

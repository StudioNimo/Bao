#pragma once

#include <stddef.h>
#include <sqlite3.h>

typedef struct {
  sqlite3 *db;
} bao_db_t;

int bao_db_open(bao_db_t *out, const char *path);
void bao_db_close(bao_db_t *db);

// Creates schema; migrates from older versions (drops legacy tables when upgrading to v2).
int bao_db_init_schema(bao_db_t *db);

int bao_db_insert_commit(bao_db_t *db,
                         const char *hash_short,
                         const char *provider,
                         const char *model,
                         double temperature,
                         const char *prompt_text,
                         const char *dataset_hash,
                         const char *created_at_iso8601);

int bao_db_insert_execution(bao_db_t *db,
                            const char *commit_hash,
                            const char *dataset_id,
                            const char *input_text,
                            const char *output_text,
                            int latency_ms,
                            int tokens_used);

int bao_db_update_commit_dataset_hash(bao_db_t *db, const char *commit_full_hash, const char *dataset_hash_hex);

typedef struct {
  sqlite3_int64 execution_id;
  char *input_text;
  char *output_text;
} bao_uneval_exec_t;
void bao_uneval_exec_clear(bao_uneval_exec_t *e);
// 0 = row found (caller must free input_text/output_text), 1 = no row, -1 = error
int bao_db_next_unevaluated_for_commit(bao_db_t *db, const char *commit_hash, bao_uneval_exec_t *out);

int bao_db_insert_evaluation(bao_db_t *db, sqlite3_int64 exec_id, const char *score);

typedef struct {
  char *dataset_id;
  char *score;
} bao_eval_row_t;
void bao_db_free_eval_rows(bao_eval_row_t *rows, size_t n);
// 0 ok, -1 error. Caller frees *out_rows with bao_db_free_eval_rows.
int bao_db_list_evaluations_for_commit(bao_db_t *db, const char *commit_hash, bao_eval_row_t **out_rows, size_t *out_n);

// Nth newest commit by created_at (offset 0 = newest). Caller frees *out_hash.
int bao_db_nth_commit_hash(bao_db_t *db, unsigned offset, char **out_hash);

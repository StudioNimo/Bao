#pragma once

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

// Nth newest commit by created_at (offset 0 = newest). Caller frees *out_hash.
int bao_db_nth_commit_hash(bao_db_t *db, unsigned offset, char **out_hash);

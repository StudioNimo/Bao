#pragma once

#include <stddef.h>

// Load sorted unique relative paths (one malloc block for strings + pointer array). Caller frees with bao_staged_free.
int bao_staged_load(char ***paths_out, size_t *count_out);
void bao_staged_free(char **paths, size_t count);

int bao_staged_contains(const char *const *paths, size_t n, const char *path);

// Add file or recursively add files under directory. Returns number of paths newly added to the index, or -1 on error.
int bao_staged_add_path(const char *path);

// Stage bao.yaml, prompt file(s), prompts/*, and dataset from bao.yaml when present.
int bao_staged_add_all(void);

// Persist merged list (internal: load + merge + sort unique + write).
int bao_staged_merge_in(char **new_paths, size_t new_n);

void bao_staged_clear(void);

// Remove normalized paths from the staging list (--cached rm / restore).
int bao_staged_remove_paths(const char **paths, size_t npaths);

// Collect paths that would be added (for --dry-run); caller frees with bao_staged_free.
int bao_staged_collect_from_path(const char *path, char ***out, size_t *n);
int bao_staged_collect_add_all_paths(char ***out, size_t *n);

// Like git add -u: drop staged paths whose files no longer exist; keep the rest.
int bao_staged_update_index(void);
// Replace index with staged_files from commit JSON.
/* commit_id: HEAD / rev / フル64 / プレフィックス / レガシー7桁ファイル名 */
int bao_staged_restore_from_commit_short(const char *commit_id);

#pragma once

#include <stddef.h>

// 64 hex lowercase SHA-256 object id validation.
int bao_is_hex64(const char *s);

// Full hash of HEAD commit (malloc). NULL if missing or empty.
char *bao_resolve_head_full(void);

// 7-char display id (malloc). NULL if no HEAD.
char *bao_display_short_from_full(const char *full64);

// One line of ref file -> full hash (malloc). Legacy 7-hex reads full_hash from JSON.
char *bao_ref_line_to_commit_full(const char *line);

// Any rev spec -> full64 (HEAD / HEAD~n / branch / tag / full64 / short prefix).
// NULL if ambiguous or not found.
char *bao_resolve_commit_full(const char *spec);

// Write path to commit JSON for full hash into out (returns 0 on success).
int bao_commit_json_path_full(const char *full64, char *out, size_t outsz);

// Read commit JSON by id (full, short, or legacy). Sets full_out to resolved full64 (malloc).
int bao_read_commit_json_any(const char *id, char **full_out, unsigned char **buf, size_t *len);

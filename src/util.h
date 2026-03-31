#pragma once

#include <stddef.h>

int bao_read_file(const char *path, unsigned char **out_buf, size_t *out_len);
int bao_write_file_atomic(const char *path, const unsigned char *buf, size_t len);
int bao_file_exists(const char *path);
int bao_is_dir(const char *path);
int bao_mkdir_p(const char *path);
int bao_copy_file(const char *src, const char *dst);

// Return 1 if the path is a safe repository-relative path.
// Rejects absolute paths, backslashes, control chars, ".." segments, and ".bao" paths.
int bao_is_safe_relpath(const char *path);

// Remove file or (if recursive) directory tree. Returns 0 on success.
int bao_remove_path(const char *path, int recursive);

void bao_die(const char *fmt, ...);
void bao_warn(const char *fmt, ...);

// Returns newly malloc'ed string or NULL on error.
char *bao_strdup_printf(const char *fmt, ...);


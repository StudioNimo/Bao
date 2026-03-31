#include "util.h"

#include "bao.h"

#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void bao_die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fputs("bao: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  exit(1);
}

void bao_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fputs("bao: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
}

char *bao_strdup_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) return NULL;

  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf) return NULL;

  va_start(ap, fmt);
  vsnprintf(buf, (size_t)n + 1, fmt, ap);
  va_end(ap);
  return buf;
}

int bao_file_exists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f) {
    fclose(f);
    return 1;
  }
  return 0;
}

int bao_is_dir(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return S_ISDIR(st.st_mode);
}

int bao_is_safe_relpath(const char *path) {
  if (!path || !*path) return 0;
  // Absolute paths and home shortcuts are not allowed.
  if (path[0] == '/' || path[0] == '~') return 0;
  // Windows-ish separators are not allowed.
  if (strchr(path, '\\')) return 0;
  // Control characters and NUL are not allowed (NUL cannot appear in C strings anyway).
  for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
    if (iscntrl(*p)) return 0;
  }
  // Prevent traversal and messing with Bao internals.
  if (strstr(path, "..")) return 0;
  if (strcmp(path, BAO_DIR) == 0) return 0;
  if (strncmp(path, BAO_DIR "/", strlen(BAO_DIR) + 1) == 0) return 0;
  return 1;
}

int bao_read_file(const char *path, unsigned char **out_buf, size_t *out_len) {
  *out_buf = NULL;
  *out_len = 0;
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return -1;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return -1;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return -1;
  }
  unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return -1;
  }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (n != (size_t)sz) {
    free(buf);
    return -1;
  }
  buf[sz] = 0;
  *out_buf = buf;
  *out_len = (size_t)sz;
  return 0;
}

static int bao_write_file(const char *path, const unsigned char *buf, size_t len) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  size_t n = fwrite(buf, 1, len, f);
  if (n != len) {
    fclose(f);
    return -1;
  }
  if (fclose(f) != 0) return -1;
  return 0;
}

int bao_write_file_atomic(const char *path, const unsigned char *buf, size_t len) {
  char *tmp = bao_strdup_printf("%s.tmp.%ld", path, (long)getpid());
  if (!tmp) return -1;
  if (bao_write_file(tmp, buf, len) != 0) {
    free(tmp);
    return -1;
  }
  if (rename(tmp, path) != 0) {
    (void)remove(tmp);
    free(tmp);
    return -1;
  }
  free(tmp);
  return 0;
}

int bao_copy_file(const char *src, const char *dst) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(src, &buf, &len) != 0) return -1;
  int rc = bao_write_file_atomic(dst, buf, len);
  free(buf);
  return rc;
}

static int remove_recursive_inner(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) return -1;
  if (!S_ISDIR(st.st_mode)) return unlink(path);
  DIR *d = opendir(path);
  if (!d) return -1;
  struct dirent *de;
  while ((de = readdir(d)) != NULL) {
    if (de->d_name[0] == '.' &&
        (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0)))
      continue;
    char *child = bao_strdup_printf("%s/%s", path, de->d_name);
    if (!child) {
      closedir(d);
      return -1;
    }
    int rc = remove_recursive_inner(child);
    free(child);
    if (rc != 0) {
      closedir(d);
      return rc;
    }
  }
  closedir(d);
  return rmdir(path);
}

int bao_remove_path(const char *path, int recursive) {
  if (!path || !*path) return -1;
  if (!bao_is_safe_relpath(path)) return -1;
  struct stat st;
  if (stat(path, &st) != 0) return -1;
  if (S_ISDIR(st.st_mode)) {
    if (!recursive) return -1;
    return remove_recursive_inner(path);
  }
  return unlink(path);
}

void bao_free_test_case(BaoTestCase *tc) {
  if (!tc) return;
  free(tc->test_id);
  free(tc->raw_json);
  free(tc);
}

void bao_free_llm_response(BaoLLMResponse *r) {
  if (!r) return;
  free(r->output_text);
  free(r);
}


#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int bao_mkdir_one(const char *path) {
  if (mkdir(path, 0777) == 0) return 0;
  if (errno == EEXIST) return 0;
  return -1;
}

int bao_mkdir_p(const char *path) {
  if (!path || !*path) return -1;
  char *tmp = strdup(path);
  if (!tmp) return -1;

  size_t n = strlen(tmp);
  if (n > 0 && tmp[n - 1] == '/') tmp[n - 1] = 0;

  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (bao_mkdir_one(tmp) != 0) {
        free(tmp);
        return -1;
      }
      *p = '/';
    }
  }
  int rc = bao_mkdir_one(tmp);
  free(tmp);
  return rc;
}


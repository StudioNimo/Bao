#include "template.h"

#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int append_buf(char **out, size_t *cap, size_t *len, const char *s, size_t n) {
  if (*len + n + 1 > *cap) {
    size_t ncap = (*cap == 0) ? 64 : *cap * 2;
    while (ncap < *len + n + 1) ncap *= 2;
    char *nb = (char *)realloc(*out, ncap);
    if (!nb) return 0;
    *out = nb;
    *cap = ncap;
  }
  memcpy(*out + *len, s, n);
  *len += n;
  (*out)[*len] = 0;
  return 1;
}

char *render_template(const char *tmpl, cJSON *variables) {
  if (!tmpl) return NULL;
  if (!variables) {
    return strdup(tmpl);
  }

  char *out = NULL;
  size_t cap = 0, len = 0;

  const char *p = tmpl;
  while (*p) {
    if (p[0] == '{' && p[1] == '{') {
      const char *q = strstr(p + 2, "}}");
      if (!q) {
        free(out);
        return NULL;
      }
      const char *key_start = p + 2;
      while (key_start < q && isspace((unsigned char)*key_start)) key_start++;
      const char *key_end = q;
      while (key_end > key_start && isspace((unsigned char)key_end[-1])) key_end--;
      size_t key_len = (size_t)(key_end - key_start);
      char *key = (char *)malloc(key_len + 1);
      if (!key) {
        free(out);
        return NULL;
      }
      memcpy(key, key_start, key_len);
      key[key_len] = 0;

      cJSON *item = cJSON_GetObjectItemCaseSensitive(variables, key);
      free(key);

      const char *rep = "";
      char *printed = NULL;
      if (item) {
        if (cJSON_IsString(item) && item->valuestring) {
          rep = item->valuestring;
        } else {
          printed = cJSON_PrintUnformatted(item);
          rep = printed ? printed : "";
        }
      }

      if (!append_buf(&out, &cap, &len, rep, strlen(rep))) {
        free(printed);
        free(out);
        return NULL;
      }
      free(printed);
      p = q + 2;
      continue;
    }
    if (!append_buf(&out, &cap, &len, p, 1)) {
      free(out);
      return NULL;
    }
    p++;
  }

  if (!out) {
    out = strdup("");
    if (!out) bao_die("out of memory");
  }
  return out;
}

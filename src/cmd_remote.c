#include "bao.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_safe_remote_name(const char *s) {
  if (!s || !*s) return 0;
  if (s[0] == '-') return 0;
  if (strstr(s, "..")) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p <= 0x20 || *p == 0x7f) return 0; // no whitespace/control
    if (*p == '/' || *p == '\\') return 0;
  }
  return 1;
}

static int is_safe_remote_field(const char *s) {
  if (!s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '\n' || *p == '\r') return 0;
    if (*p == 0x7f) return 0;
  }
  return 1;
}

static int stub_push(void) {
  bao_die("push is not implemented yet (planned: libcurl HTTP sync + INSERT OR IGNORE merge)");
  return 1;
}

static int stub_pull(void) {
  bao_die("pull is not implemented yet (planned: libcurl HTTP sync + INSERT OR IGNORE merge)");
  return 1;
}

static int remote_list(int verbose) {
  if (!bao_file_exists(BAO_REMOTES_FILE)) {
    printf("(no remotes configured)\n");
    return 0;
  }
  FILE *f = fopen(BAO_REMOTES_FILE, "r");
  if (!f) return -1;
  char line[2048];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L - 1] == '\n' || p[L - 1] == '\r')) L--;
    p[L] = 0;
    if (!*p || p[0] == '#') continue;
    char *sp = strchr(p, ' ');
    char *tab = strchr(p, '\t');
    char *sep = NULL;
    if (sp && tab) sep = (sp < tab) ? sp : tab;
    else if (sp) sep = sp;
    else if (tab) sep = tab;
    if (sep) {
      *sep = 0;
      char *name = p;
      char *url = sep + 1;
      while (*url == ' ' || *url == '\t') url++;
      if (verbose) {
        printf("%s\t%s\n", name, url);
      } else {
        printf("%s\n", name);
      }
    } else {
      printf("%s\n", p);
    }
  }
  fclose(f);
  return 0;
}

static int remote_add(const char *name, const char *url) {
  if (bao_mkdir_p(BAO_DIR) != 0) return -1;
  unsigned char *old = NULL;
  size_t olen = 0;
  if (bao_file_exists(BAO_REMOTES_FILE)) (void)bao_read_file(BAO_REMOTES_FILE, &old, &olen);
  char *line = bao_strdup_printf("%s %s\n", name, url);
  if (!line) {
    free(old);
    return -1;
  }
  size_t nlen = strlen(line);
  char *out = (char *)malloc(olen + nlen + 1);
  if (!out) {
    free(line);
    free(old);
    return -1;
  }
  size_t o = 0;
  if (old) {
    memcpy(out, old, olen);
    o = olen;
  }
  memcpy(out + o, line, nlen);
  o += nlen;
  free(line);
  free(old);
  int rc = bao_write_file_atomic(BAO_REMOTES_FILE, (const unsigned char *)out, o);
  free(out);
  return rc;
}

static int remote_find_url(const char *name, char *out, size_t outsz) {
  if (!bao_file_exists(BAO_REMOTES_FILE)) return -1;
  FILE *f = fopen(BAO_REMOTES_FILE, "r");
  if (!f) return -1;
  char line[2048];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t keylen = strcspn(p, " \t\n\r");
    if (keylen == 0) continue;
    char key[256];
    if (keylen >= sizeof(key)) continue;
    memcpy(key, p, keylen);
    key[keylen] = 0;
    if (strcmp(key, name) != 0) continue;
    char *sep = p + keylen;
    while (*sep == ' ' || *sep == '\t') sep++;
    size_t L = strlen(sep);
    while (L > 0 && (sep[L - 1] == '\n' || sep[L - 1] == '\r')) L--;
    sep[L] = 0;
    snprintf(out, outsz, "%s", sep);
    fclose(f);
    return 0;
  }
  fclose(f);
  return -1;
}

static int remote_set_url(const char *name, const char *newurl) {
  if (!bao_file_exists(BAO_REMOTES_FILE)) return -1;
  FILE *f = fopen(BAO_REMOTES_FILE, "r");
  if (!f) return -1;
  char line[2048];
  char *out = NULL;
  size_t o = 0, cap = 0;
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t keylen = strcspn(p, " \t\n\r");
    if (keylen == 0) continue;
    char key[256];
    if (keylen >= sizeof(key)) continue;
    memcpy(key, p, keylen);
    key[keylen] = 0;
    if (strcmp(key, name) == 0) {
      found = 1;
      char *nl = bao_strdup_printf("%s %s\n", name, newurl);
      if (!nl) {
        fclose(f);
        free(out);
        return -1;
      }
      size_t L = strlen(nl);
      if (o + L + 1 > cap) {
        cap = cap ? cap * 2 : 512;
        while (o + L + 1 > cap) cap *= 2;
        char *n = (char *)realloc(out, cap);
        if (!n) {
          free(nl);
          fclose(f);
          free(out);
          return -1;
        }
        out = n;
      }
      memcpy(out + o, nl, L);
      o += L;
      free(nl);
      continue;
    }
    size_t L = strlen(line);
    if (o + L + 1 > cap) {
      cap = cap ? cap * 2 : 512;
      while (o + L + 1 > cap) cap *= 2;
      char *n = (char *)realloc(out, cap);
      if (!n) {
        fclose(f);
        free(out);
        return -1;
      }
      out = n;
    }
    memcpy(out + o, line, L);
    o += L;
  }
  fclose(f);
  if (!found) {
    free(out);
    return -1;
  }
  int rc = bao_write_file_atomic(BAO_REMOTES_FILE, (const unsigned char *)out, o);
  free(out);
  return rc;
}

static int remote_rename(const char *oldname, const char *newname) {
  if (!bao_file_exists(BAO_REMOTES_FILE)) return -1;
  FILE *f = fopen(BAO_REMOTES_FILE, "r");
  if (!f) return -1;
  char line[2048];
  char *out = NULL;
  size_t o = 0, cap = 0;
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t keylen = strcspn(p, " \t\n\r");
    if (keylen == 0) continue;
    char key[256];
    if (keylen >= sizeof(key)) continue;
    memcpy(key, p, keylen);
    key[keylen] = 0;
    if (strcmp(key, oldname) == 0) {
      found = 1;
      char *url = p + keylen;
      while (*url == ' ' || *url == '\t') url++;
      size_t L = strlen(url);
      while (L > 0 && (url[L - 1] == '\n' || url[L - 1] == '\r')) L--;
      url[L] = 0;
      char *nl = bao_strdup_printf("%s %s\n", newname, url);
      if (!nl) {
        fclose(f);
        free(out);
        return -1;
      }
      L = strlen(nl);
      if (o + L + 1 > cap) {
        cap = cap ? cap * 2 : 512;
        while (o + L + 1 > cap) cap *= 2;
        char *n = (char *)realloc(out, cap);
        if (!n) {
          free(nl);
          fclose(f);
          free(out);
          return -1;
        }
        out = n;
      }
      memcpy(out + o, nl, L);
      o += L;
      free(nl);
      continue;
    }
    size_t L = strlen(line);
    if (o + L + 1 > cap) {
      cap = cap ? cap * 2 : 512;
      while (o + L + 1 > cap) cap *= 2;
      char *n = (char *)realloc(out, cap);
      if (!n) {
        fclose(f);
        free(out);
        return -1;
      }
      out = n;
    }
    memcpy(out + o, line, L);
    o += L;
  }
  fclose(f);
  if (!found) {
    free(out);
    return -1;
  }
  int rc = bao_write_file_atomic(BAO_REMOTES_FILE, (const unsigned char *)out, o);
  free(out);
  return rc;
}

static int remote_remove(const char *name) {
  if (!bao_file_exists(BAO_REMOTES_FILE)) return -1;
  FILE *f = fopen(BAO_REMOTES_FILE, "r");
  if (!f) return -1;
  char line[2048];
  char *out = NULL;
  size_t o = 0, cap = 0;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    size_t keylen = strcspn(p, " \t\n\r");
    if (keylen == 0) continue;
    char key[256];
    if (keylen >= sizeof(key)) continue;
    memcpy(key, p, keylen);
    key[keylen] = 0;
    if (strcmp(key, name) == 0) continue;
    size_t L = strlen(line);
    if (o + L + 1 > cap) {
      cap = cap ? cap * 2 : 512;
      while (o + L + 1 > cap) cap *= 2;
      char *n = (char *)realloc(out, cap);
      if (!n) {
        fclose(f);
        free(out);
        return -1;
      }
      out = n;
    }
    memcpy(out + o, line, L);
    o += L;
  }
  fclose(f);
  int rc = bao_write_file_atomic(BAO_REMOTES_FILE, (const unsigned char *)out, o);
  free(out);
  return rc;
}

int cmd_remote(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  if (argc >= 1 && strcmp(argv[0], "push") == 0) return stub_push();
  if (argc >= 1 && strcmp(argv[0], "pull") == 0) return stub_pull();

  if (argc == 0) return remote_list(0);
  if (argc >= 1 && (strcmp(argv[0], "-v") == 0 || strcmp(argv[0], "-vv") == 0)) return remote_list(1);
  if (argc >= 1 && strcmp(argv[0], "add") == 0) {
    if (argc < 3) bao_die("usage: bao remote add <name> <url>");
    if (!is_safe_remote_name(argv[1])) bao_die("invalid remote name");
    if (!is_safe_remote_field(argv[2])) bao_die("invalid remote url");
    if (remote_add(argv[1], argv[2]) != 0) bao_die("remote add failed");
    printf("Added remote %s\n", argv[1]);
    return 0;
  }
  if (argc >= 1 && (strcmp(argv[0], "remove") == 0 || strcmp(argv[0], "rm") == 0)) {
    if (argc < 2) bao_die("usage: bao remote remove <name>");
    if (!is_safe_remote_name(argv[1])) bao_die("invalid remote name");
    if (remote_remove(argv[1]) != 0) bao_die("remote remove failed");
    printf("Removed remote %s\n", argv[1]);
    return 0;
  }
  if (argc >= 1 && strcmp(argv[0], "rename") == 0) {
    if (argc < 3) bao_die("usage: bao remote rename <old> <new>");
    if (!is_safe_remote_name(argv[1]) || !is_safe_remote_name(argv[2])) bao_die("invalid remote name");
    if (remote_rename(argv[1], argv[2]) != 0) bao_die("remote rename failed");
    printf("Renamed remote to %s\n", argv[2]);
    return 0;
  }
  if (argc >= 1 && strcmp(argv[0], "set-url") == 0) {
    if (argc < 3) bao_die("usage: bao remote set-url <name> <newurl>");
    if (!is_safe_remote_name(argv[1])) bao_die("invalid remote name");
    if (!is_safe_remote_field(argv[2])) bao_die("invalid remote url");
    if (remote_set_url(argv[1], argv[2]) != 0) bao_die("remote set-url failed");
    printf("Updated URL for %s\n", argv[1]);
    return 0;
  }
  if (argc >= 1 && strcmp(argv[0], "get-url") == 0) {
    if (argc < 2) bao_die("usage: bao remote get-url <name>");
    if (!is_safe_remote_name(argv[1])) bao_die("invalid remote name");
    char url[2048];
    if (remote_find_url(argv[1], url, sizeof(url)) != 0) bao_die("remote not found");
    printf("%s\n", url);
    return 0;
  }
  if (argc >= 1 && strcmp(argv[0], "show") == 0) {
    if (argc < 2) bao_die("usage: bao remote show <name>");
    if (!is_safe_remote_name(argv[1])) bao_die("invalid remote name");
    char url[2048];
    if (remote_find_url(argv[1], url, sizeof(url)) != 0) bao_die("remote not found");
    printf("* remote %s\n  Fetch URL: %s\n  Push  URL: %s\n", argv[1], url, url);
    return 0;
  }
  bao_die("usage: bao remote [-v] | add | remove | rename | set-url | get-url | show");
  return 1;
}

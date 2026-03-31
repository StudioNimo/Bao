#include "bao.h"

#include "commits.h"
#include "config.h"
#include "db.h"
#include "hash.h"
#include "refs.h"
#include "stage.h"
#include "util.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct sort_entry {
  char *path;
};

static int cmp_sort_entry(const void *a, const void *b) {
  const struct sort_entry *ea = (const struct sort_entry *)a;
  const struct sort_entry *eb = (const struct sort_entry *)b;
  return strcmp(ea->path, eb->path);
}

static int iso8601_now(char *out, size_t out_len) {
  time_t t = time(NULL);
  struct tm tmv;
  if (!gmtime_r(&t, &tmv)) return -1;
  if (strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) return -1;
  return 0;
}

static int hash_file_hex(const char *path, char out_hex[65]) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(path, &buf, &len) != 0) return -1;
  int rc = bao_sha256_hex(buf, len, out_hex, 65);
  free(buf);
  return rc;
}

static int is_prompt_staged_path(const char *path, const BaoConfig *cfg) {
  if (!path) return 0;
  if (strncmp(path, "prompts/", 8) == 0) return 1;
  /* プロバイダー別ディレクトリ（例: providers/openai/system.txt） */
  if (strncmp(path, "providers/", 10) == 0) return 1;
  if (cfg->prompt_filepath && strcmp(path, cfg->prompt_filepath) == 0) return 1;
  return 0;
}

static int hash_staged_prompt_files(const char *const *staged, size_t nst, const BaoConfig *cfg, char out_hex[65]) {
  struct sort_entry *ents = NULL;
  size_t n = 0, cap = 8;
  ents = (struct sort_entry *)malloc(cap * sizeof(*ents));
  if (!ents) return -1;

  for (size_t i = 0; i < nst; i++) {
    const char *p = staged[i];
    if (!p) continue;
    struct stat st;
    if (stat(p, &st) != 0) continue;
    if (!S_ISREG(st.st_mode)) continue;
    if (!is_prompt_staged_path(p, cfg)) continue;
    if (n == cap) {
      cap *= 2;
      struct sort_entry *ne = (struct sort_entry *)realloc(ents, cap * sizeof(*ents));
      if (!ne) {
        for (size_t k = 0; k < n; k++) free(ents[k].path);
        free(ents);
        return -1;
      }
      ents = ne;
    }
    ents[n].path = strdup(p);
    if (!ents[n].path) {
      for (size_t k = 0; k < n; k++) free(ents[k].path);
      free(ents);
      return -1;
    }
    n++;
  }

  if (n == 0) {
    free(ents);
    return -1;
  }

  qsort(ents, n, sizeof(*ents), cmp_sort_entry);

  bao_sha256_ctx *h = bao_sha256_new();
  if (!h) {
    for (size_t k = 0; k < n; k++) free(ents[k].path);
    free(ents);
    return -1;
  }

  for (size_t i = 0; i < n; i++) {
    unsigned char *buf = NULL;
    size_t len = 0;
    if (bao_read_file(ents[i].path, &buf, &len) != 0) {
      bao_sha256_free(h);
      for (size_t k = 0; k < n; k++) free(ents[k].path);
      free(ents);
      return -1;
    }
    (void)bao_sha256_update(h, ents[i].path, strlen(ents[i].path) + 1);
    (void)bao_sha256_update(h, buf, len);
    free(buf);
  }

  int rc = bao_sha256_final_hex(h, out_hex, 65);
  bao_sha256_free(h);
  for (size_t k = 0; k < n; k++) free(ents[k].path);
  free(ents);
  return rc;
}

static int read_head_target(char **out_ref_path) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(BAO_HEAD_FILE, &buf, &len) != 0) return -1;
  const char *s = (const char *)buf;
  const char *p = strstr(s, "ref:");
  if (!p) {
    free(buf);
    return -1;
  }
  p += 4;
  while (*p == ' ' || *p == '\t') p++;
  const char *e = p;
  while (*e && *e != '\n' && *e != '\r') e++;
  size_t n = (size_t)(e - p);
  char *ref = (char *)malloc(n + 1);
  if (!ref) {
    free(buf);
    return -1;
  }
  memcpy(ref, p, n);
  ref[n] = 0;
  free(buf);

  char *full = bao_strdup_printf("%s/%s", BAO_DIR, ref);
  free(ref);
  if (!full) return -1;
  *out_ref_path = full;
  return 0;
}

static int update_current_ref_to(const char *full_hash_64) {
  if (!bao_is_hex64(full_hash_64)) return -1;
  char *ref_path = NULL;
  if (read_head_target(&ref_path) == 0) {
    char *content = bao_strdup_printf("%s\n", full_hash_64);
    if (!content) {
      free(ref_path);
      return -1;
    }
    int rc = bao_write_file_atomic(ref_path, (const unsigned char *)content, strlen(content));
    free(content);
    free(ref_path);
    return rc;
  }

  char *content = bao_strdup_printf("%s\n", full_hash_64);
  if (!content) return -1;
  int rc = bao_write_file_atomic(BAO_HEAD_FILE, (const unsigned char *)content, strlen(content));
  free(content);
  return rc;
}

static const char *env_first_nonempty(const char *a, const char *b, const char *c) {
  const char *v = NULL;
  if (a && (v = getenv(a)) && *v) return v;
  if (b && (v = getenv(b)) && *v) return v;
  if (c && (v = getenv(c)) && *v) return v;
  return NULL;
}

static int env_is_true(const char *key) {
  const char *v = getenv(key);
  if (!v || !*v) return 0;
  return (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "TRUE") || !strcmp(v, "yes") || !strcmp(v, "YES"));
}

static int count_staged_under_prefix(const char *const *staged, size_t n, const char *prefix) {
  int c = 0;
  if (!prefix) return 0;
  size_t pl = strlen(prefix);
  for (size_t i = 0; i < n; i++) {
    const char *p = staged[i];
    if (!p) continue;
    if (!strncmp(p, prefix, pl)) c++;
  }
  return c;
}

static char *generate_commit_message(const BaoConfig *cfg, const char *const *staged, size_t n_staged, int amend) {
  const char *provider = (cfg && cfg->provider && cfg->provider[0]) ? cfg->provider : NULL;
  int n_prompts = count_staged_under_prefix(staged, n_staged, "prompts/");
  int n_providers = count_staged_under_prefix(staged, n_staged, "providers/");
  int n_cfg = count_staged_under_prefix(staged, n_staged, "bao.yaml");
  int n_data = (cfg && cfg->dataset_path && cfg->dataset_path[0]) ? count_staged_under_prefix(staged, n_staged, cfg->dataset_path) : 0;

  const char *kind = "files";
  if (n_prompts + n_providers > 0) kind = "prompts";
  else if (n_cfg > 0) kind = "config";
  else if (n_data > 0) kind = "dataset";

  char *subject = NULL;
  if (amend) {
    if (provider)
      subject = bao_strdup_printf("amend(%s): update %s (%zu files)", provider, kind, n_staged);
    else
      subject = bao_strdup_printf("amend: update %s (%zu files)", kind, n_staged);
  } else {
    if (provider)
      subject = bao_strdup_printf("update(%s): %s (%zu files)", provider, kind, n_staged);
    else
      subject = bao_strdup_printf("update: %s (%zu files)", kind, n_staged);
  }
  if (!subject) return NULL;

  size_t cap = strlen(subject) + 256 + n_staged * 128;
  char *out = (char *)malloc(cap);
  if (!out) {
    free(subject);
    return NULL;
  }
  size_t off = 0;
  off += (size_t)snprintf(out + off, cap - off, "%s\n\n", subject);
  free(subject);
  if (provider) off += (size_t)snprintf(out + off, cap - off, "provider: %s\n", provider);
  if (cfg && cfg->model) off += (size_t)snprintf(out + off, cap - off, "model: %s\n", cfg->model);
  off += (size_t)snprintf(out + off, cap - off, "\nfiles:\n");
  for (size_t i = 0; i < n_staged; i++) {
    if (!staged[i]) continue;
    off += (size_t)snprintf(out + off, cap - off, "  - %s\n", staged[i]);
    if (off + 128 >= cap) break;
  }
  return out;
}

static char *read_message_from_file_strip_comments(const char *path) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(path, &buf, &len) != 0) return NULL;
  char *s = (char *)buf;

  /* Git 風: '#' 行はコメントとして無視。空行は保持しつつ前後の空白は落とす。 */
  char *out = (char *)malloc(len + 1);
  if (!out) {
    free(buf);
    return NULL;
  }
  size_t w = 0;
  char *line = s;
  while (*line) {
    char *eol = strpbrk(line, "\r\n");
    if (eol) *eol = 0;
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '#') {
      size_t L = strlen(line);
      memcpy(out + w, line, L);
      w += L;
      out[w++] = '\n';
    }
    if (!eol) break;
    line = eol + 1;
    if (*line == '\n' || *line == '\r') line++;
  }
  out[w] = 0;
  free(buf);

  /* 先頭・末尾の空行を削る */
  while (out[0] == '\n') memmove(out, out + 1, strlen(out));
  size_t n = strlen(out);
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = 0;
  if (n == 0) {
    free(out);
    return NULL;
  }
  return out;
}

static char *maybe_edit_commit_message(const char *initial, const char *const *staged, size_t n_staged, int allow_editor) {
  if (!initial) return NULL;
  if (!allow_editor) return strdup(initial);

  if (bao_mkdir_p(BAO_DIR) != 0) return strdup(initial);
  char *edit_path = bao_strdup_printf("%s/COMMIT_EDITMSG", BAO_DIR);
  if (!edit_path) return strdup(initial);

  /* テンプレート: 初期メッセージ + コメントで staged ファイル一覧 */
  size_t cap = strlen(initial) + 256 + n_staged * 128;
  char *tmpl = (char *)malloc(cap);
  if (!tmpl) {
    free(edit_path);
    return strdup(initial);
  }
  size_t off = 0;
  off += (size_t)snprintf(tmpl + off, cap - off, "%s\n", initial);
  off += (size_t)snprintf(tmpl + off, cap - off, "\n# Please enter the commit message. Lines starting\n# with '#' will be ignored, and an empty message aborts.\n#\n# Staged files:\n");
  for (size_t i = 0; i < n_staged; i++) {
    if (!staged[i]) continue;
    off += (size_t)snprintf(tmpl + off, cap - off, "#\t%s\n", staged[i]);
    if (off + 128 >= cap) break;
  }

  if (bao_write_file_atomic(edit_path, (const unsigned char *)tmpl, strlen(tmpl)) != 0) {
    free(tmpl);
    free(edit_path);
    return strdup(initial);
  }
  free(tmpl);

  const char *editor = env_first_nonempty("BAO_EDITOR", "EDITOR", "VISUAL");
  if (!editor) {
    free(edit_path);
    return strdup(initial);
  }

  char *cmd = bao_strdup_printf("%s %s", editor, edit_path);
  if (!cmd) {
    free(edit_path);
    return strdup(initial);
  }
  int rc = system(cmd);
  free(cmd);
  if (rc != 0) {
    free(edit_path);
    bao_die("editor exited with status %d", rc);
  }

  char *msg = read_message_from_file_strip_comments(edit_path);
  free(edit_path);
  return msg;
}

int cmd_commit(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  const char *message = NULL;
  char *msg_alloc = NULL;
  int quiet = 0;
  int all = 0;
  int amend = 0;
  int opt_edit = 0;
  int opt_no_edit = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      message = argv[i + 1];
      i++;
      continue;
    }
    if ((strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--file") == 0) && i + 1 < argc) {
      unsigned char *buf = NULL;
      size_t len = 0;
      if (bao_read_file(argv[i + 1], &buf, &len) != 0) bao_die("cannot read %s", argv[i + 1]);
      char *nl = memchr(buf, '\n', len);
      size_t msglen = nl ? (size_t)(nl - (char *)buf) : len;
      char *m = (char *)malloc(msglen + 1);
      if (!m) {
        free(buf);
        bao_die("out of memory");
      }
      memcpy(m, buf, msglen);
      m[msglen] = 0;
      free(buf);
      free(msg_alloc);
      msg_alloc = m;
      message = m;
      i++;
      continue;
    }
    if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
      continue;
    }
    if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
      all = 1;
      continue;
    }
    if (strcmp(argv[i], "--amend") == 0) {
      amend = 1;
      continue;
    }
    if (!strcmp(argv[i], "--edit")) {
      opt_edit = 1;
      continue;
    }
    if (!strcmp(argv[i], "--no-edit")) {
      opt_no_edit = 1;
      continue;
    }
    if (strcmp(argv[i], "--dry-run") == 0) {
      bao_die("commit --dry-run is not implemented");
    }
  }
  if (all) {
    if (bao_staged_add_all() != 0) bao_die("bao add -A failed");
  }
  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  // If --amend and message not provided, reuse previous commit message.
  char *amend_parent = NULL;
  char **amend_staged = NULL;
  size_t amend_nst = 0;
  if (amend) {
    char *head_full = bao_resolve_head_full();
    if (!head_full || !bao_is_hex64(head_full)) {
      free(head_full);
      bao_die("cannot amend: no commit at HEAD");
    }
    unsigned char *obuf = NULL;
    size_t olen = 0;
    char *resolved = NULL;
    if (bao_read_commit_json_any(head_full, &resolved, &obuf, &olen) != 0) {
      free(head_full);
      bao_die("cannot amend: HEAD commit object missing");
    }
    free(head_full);
    free(resolved);
    cJSON *oj = cJSON_Parse((char *)obuf);
    free(obuf);
    if (!oj) {
      bao_die("cannot amend: invalid HEAD commit JSON");
    }
    if (!message || !*message) {
      cJSON *m = cJSON_GetObjectItemCaseSensitive(oj, "message");
      if (m && cJSON_IsString(m) && m->valuestring) {
        msg_alloc = strdup(m->valuestring);
        message = msg_alloc;
      }
    }
    cJSON *p = cJSON_GetObjectItemCaseSensitive(oj, "parent");
    if (p && cJSON_IsString(p) && p->valuestring) {
      if (bao_is_hex64(p->valuestring))
        amend_parent = strdup(p->valuestring);
      else
        amend_parent = bao_resolve_commit_full(p->valuestring);
    }
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(oj, "staged_files");
    if (cJSON_IsArray(arr)) {
      int n = cJSON_GetArraySize(arr);
      amend_staged = (char **)calloc((size_t)n + 1, sizeof(char *));
      if (!amend_staged) {
        cJSON_Delete(oj);
        bao_die("oom");
      }
      for (int k = 0; k < n; k++) {
        cJSON *it = cJSON_GetArrayItem(arr, k);
        if (cJSON_IsString(it) && it->valuestring) {
          amend_staged[amend_nst] = strdup(it->valuestring);
          if (amend_staged[amend_nst]) amend_nst++;
        }
      }
    }
    cJSON_Delete(oj);
  }

  char **staged = NULL;
  size_t n_staged = 0;
  if (bao_staged_load(&staged, &n_staged) != 0) bao_die("failed to read staging area");
  if (n_staged == 0 && amend) {
    bao_staged_free(staged, n_staged);
    staged = amend_staged;
    n_staged = amend_nst;
    amend_staged = NULL;
    amend_nst = 0;
  }
  if (n_staged == 0) {
    bao_staged_free(staged, n_staged);
    for (size_t k = 0; k < amend_nst; k++) free(amend_staged[k]);
    free(amend_staged);
    free(amend_parent);
    free(msg_alloc);
    bao_die("nothing to commit (use `bao add` to stage files first)");
  }

  if (!bao_staged_contains((const char *const *)staged, n_staged, "bao.yaml")) {
    bao_staged_free(staged, n_staged);
    bao_die("bao.yaml must be staged (e.g. `bao add bao.yaml`)");
  }

  BaoConfig *cfg = load_config("bao.yaml");
  if (!cfg) {
    bao_staged_free(staged, n_staged);
    bao_die("failed to load bao.yaml (model, prompt_file or prompts_dir required)");
  }

  /* メッセージ: -m/-F が無い場合は自動生成。エディタ編集は --edit / (EDITORあり) で可能。 */
  if (!message || !*message) {
    char *auto_msg = generate_commit_message(cfg, (const char *const *)staged, n_staged, amend);
    if (!auto_msg) {
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      bao_die("failed to generate commit message");
    }
    int allow_editor = 0;
    if (!opt_no_edit && !env_is_true("BAO_NO_EDITOR")) {
      const char *ed = env_first_nonempty("BAO_EDITOR", "EDITOR", "VISUAL");
      if (ed && *ed) allow_editor = 1;
    }
    if (opt_edit) allow_editor = 1;

    msg_alloc = maybe_edit_commit_message(auto_msg, (const char *const *)staged, n_staged, allow_editor);
    free(auto_msg);
    if (!msg_alloc || !*msg_alloc) {
      free(msg_alloc);
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      bao_die("aborting commit due to empty commit message");
    }
    message = msg_alloc;
  }

  if (cfg->prompt_filepath && !bao_staged_contains((const char *const *)staged, n_staged, cfg->prompt_filepath)) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    bao_die("prompt file must be staged: %s", cfg->prompt_filepath);
  }

  char prompt_files_hash[65];
  if (hash_staged_prompt_files((const char *const *)staged, n_staged, cfg, prompt_files_hash) != 0) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    bao_die("stage at least one prompt file (e.g. `bao add prompts/` or your prompt_file)");
  }

  char temp_buf[64];
  snprintf(temp_buf, sizeof(temp_buf), "%.17g", cfg->temperature);
  char mt_buf[32];
  snprintf(mt_buf, sizeof(mt_buf), "%d", cfg->max_tokens);

  const char *dataset_hash_val = NULL;
  char dataset_hash_hex[65];
  if (cfg->dataset_path && bao_staged_contains((const char *const *)staged, n_staged, cfg->dataset_path) &&
      bao_file_exists(cfg->dataset_path)) {
    if (hash_file_hex(cfg->dataset_path, dataset_hash_hex) == 0) {
      dataset_hash_val = dataset_hash_hex;
    }
  }

  bao_sha256_ctx *h = bao_sha256_new();
  if (!h) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    bao_die("sha256 init failed");
  }
  {
    const char *pv = cfg->provider && cfg->provider[0] ? cfg->provider : "";
    (void)bao_sha256_update(h, pv, strlen(pv));
    (void)bao_sha256_update(h, "\n", 1);
  }
  (void)bao_sha256_update(h, cfg->model, strlen(cfg->model));
  (void)bao_sha256_update(h, "\n", 1);
  (void)bao_sha256_update(h, temp_buf, strlen(temp_buf));
  (void)bao_sha256_update(h, "\n", 1);
  (void)bao_sha256_update(h, mt_buf, strlen(mt_buf));
  (void)bao_sha256_update(h, "\n", 1);
  (void)bao_sha256_update(h, prompt_files_hash, strlen(prompt_files_hash));
  (void)bao_sha256_update(h, "\n", 1);
  if (dataset_hash_val) {
    (void)bao_sha256_update(h, dataset_hash_val, strlen(dataset_hash_val));
    (void)bao_sha256_update(h, "\n", 1);
  }
  // Parent is part of commit identity.
  if (amend) {
    if (amend_parent) {
      (void)bao_sha256_update(h, amend_parent, strlen(amend_parent));
    }
  } else {
    char *ph = bao_resolve_head_full();
    if (ph && bao_is_hex64(ph)) (void)bao_sha256_update(h, ph, 64);
    free(ph);
  }
  (void)bao_sha256_update(h, "\n", 1);
  (void)bao_sha256_update(h, message, strlen(message));

  char full_hex[65];
  if (bao_sha256_final_hex(h, full_hex, 65) != 0) {
    bao_sha256_free(h);
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    bao_die("sha256 final failed");
  }
  bao_sha256_free(h);

  char short_hash[8];
  bao_short_hash_from_hex(full_hex, short_hash);

  char *disp = bao_display_short_from_full(full_hex);
  if (!disp) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    bao_die("display hash failed");
  }

  char created_at[32];
  if (iso8601_now(created_at, sizeof(created_at)) != 0) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("time failed");
  }

  if (bao_mkdir_p(BAO_COMMITS_DIR) != 0) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("missing repo; run `bao init`");
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("out of memory");
  }
  cJSON_AddStringToObject(root, "hash", short_hash);
  cJSON_AddStringToObject(root, "full_hash", full_hex);
  if (amend) {
    if (amend_parent) cJSON_AddStringToObject(root, "parent", amend_parent);
    else
      cJSON_AddNullToObject(root, "parent");
  } else {
    char *ph = bao_resolve_head_full();
    if (ph && bao_is_hex64(ph))
      cJSON_AddStringToObject(root, "parent", ph);
    else
      cJSON_AddNullToObject(root, "parent");
    free(ph);
  }
  cJSON_AddStringToObject(root, "message", message);
  if (cfg->provider && cfg->provider[0])
    cJSON_AddStringToObject(root, "provider", cfg->provider);
  else
    cJSON_AddNullToObject(root, "provider");
  if (cfg->profile && cfg->profile[0])
    cJSON_AddStringToObject(root, "profile", cfg->profile);
  else
    cJSON_AddNullToObject(root, "profile");
  cJSON_AddStringToObject(root, "model", cfg->model);
  cJSON_AddNumberToObject(root, "temperature", cfg->temperature);
  cJSON_AddNumberToObject(root, "max_tokens", cfg->max_tokens);
  if (cfg->prompt_filepath) {
    cJSON_AddStringToObject(root, "prompt_filepath", cfg->prompt_filepath);
  } else {
    cJSON_AddNullToObject(root, "prompt_filepath");
  }
  cJSON_AddStringToObject(root, "prompt_text", cfg->prompt_text);
  cJSON_AddStringToObject(root, "prompt_files_hash", prompt_files_hash);
  if (dataset_hash_val) {
    cJSON_AddStringToObject(root, "dataset_hash", dataset_hash_val);
  } else {
    cJSON_AddNullToObject(root, "dataset_hash");
  }
  if (cfg->dataset_path) {
    cJSON_AddStringToObject(root, "dataset_path", cfg->dataset_path);
  } else {
    cJSON_AddNullToObject(root, "dataset_path");
  }
  cJSON_AddStringToObject(root, "created_at", created_at);

  cJSON *st = cJSON_CreateArray();
  if (st) {
    for (size_t i = 0; i < n_staged; i++) {
      cJSON_AddItemToArray(st, cJSON_CreateString(staged[i]));
    }
    cJSON_AddItemToObject(root, "staged_files", st);
  }

  char *json = cJSON_Print(root);
  cJSON_Delete(root);
  if (!json) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("out of memory");
  }

  char path[512];
  if (bao_commit_json_path_full(full_hex, path, sizeof(path)) != 0) {
    free(json);
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("commit path too long");
  }
  int commit_object_already_exists = 0;
  if (bao_file_exists(path)) {
    unsigned char *ex = NULL;
    size_t exl = 0;
    if (bao_read_file(path, &ex, &exl) != 0) {
      free(json);
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      free(disp);
      bao_die("existing commit file unreadable: %s", path);
    }
    cJSON *ej = cJSON_Parse((char *)ex);
    free(ex);
    if (!ej) {
      free(json);
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      free(disp);
      bao_die("existing commit file invalid: %s", path);
    }
    cJSON *of = cJSON_GetObjectItemCaseSensitive(ej, "full_hash");
    if (of && cJSON_IsString(of) && of->valuestring && strcmp(of->valuestring, full_hex) != 0) {
      cJSON_Delete(ej);
      free(json);
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      free(disp);
      bao_die("object store collision: %s", path);
    }
    cJSON_Delete(ej);
    commit_object_already_exists = 1;
  }
  if (!commit_object_already_exists) {
    int wrc = bao_write_file_atomic(path, (const unsigned char *)json, strlen(json));
    if (wrc != 0) {
      free(json);
      free_config(cfg);
      bao_staged_free(staged, n_staged);
      free(disp);
      bao_die("failed to write commit object");
    }
  }
  free(json);

  bao_db_t db;
  if (bao_db_open(&db, BAO_DB_FILE) != 0) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("failed to open db (run `bao init`?)");
  }
  if (bao_db_init_schema(&db) != 0) {
    bao_db_close(&db);
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("failed to init schema");
  }
  if (bao_db_insert_commit(&db,
                           full_hex,
                           cfg->provider && cfg->provider[0] ? cfg->provider : "",
                           cfg->model,
                           cfg->temperature,
                           cfg->prompt_text,
                           dataset_hash_val,
                           created_at) != 0) {
    bao_db_close(&db);
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("failed to insert commit (duplicate hash?)");
  }
  bao_db_close(&db);

  if (update_current_ref_to(full_hex) != 0) {
    free_config(cfg);
    bao_staged_free(staged, n_staged);
    free(disp);
    bao_die("failed to update branch ref");
  }

  bao_staged_clear();
  bao_staged_free(staged, n_staged);
  free_config(cfg);

  if (!quiet) printf("[%s] %s\n", disp, message);
  free(disp);
  free(amend_parent);
  for (size_t k = 0; k < amend_nst; k++) free(amend_staged[k]);
  free(amend_staged);
  free(msg_alloc);
  return 0;
}

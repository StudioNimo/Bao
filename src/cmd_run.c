#include "bao.h"

#include "commits.h"
#include "config.h"
#include "db.h"
#include "hash.h"
#include "template.h"
#include "util.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct curl_buf {
  char *p;
  size_t len;
};

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  struct curl_buf *b = (struct curl_buf *)userdata;
  size_t n = size * nmemb;
  char *np = (char *)realloc(b->p, b->len + n + 1);
  if (!np) return 0;
  b->p = np;
  memcpy(b->p + b->len, ptr, n);
  b->len += n;
  b->p[b->len] = 0;
  return n;
}

static int hash_dataset_file(const char *path, char out_hex[65]) {
  unsigned char *buf = NULL;
  size_t len = 0;
  if (bao_read_file(path, &buf, &len) != 0) return -1;
  int r = bao_sha256_hex(buf, len, out_hex, 65);
  free(buf);
  return r;
}

static const char *json_line_dataset_id(cJSON *row) {
  cJSON *it = cJSON_GetObjectItemCaseSensitive(row, "test_id");
  if (!it) it = cJSON_GetObjectItemCaseSensitive(row, "id");
  if (it && cJSON_IsString(it) && it->valuestring) return it->valuestring;
  if (it && cJSON_IsNumber(it)) {
    static char numbuf[32];
    snprintf(numbuf, sizeof(numbuf), "%.0f", it->valuedouble);
    return numbuf;
  }
  return NULL;
}

// Caller frees return value. *tokens_out = -1 if unknown.
static char *openai_chat_completion(const char *api_key,
                                    const char *model,
                                    double temperature,
                                    int max_tokens,
                                    const char *user_message,
                                    long *latency_ms,
                                    int *tokens_out) {
  *tokens_out = -1;
  *latency_ms = -1;
  cJSON *root = cJSON_CreateObject();
  if (!root) return NULL;
  cJSON_AddStringToObject(root, "model", model);
  cJSON_AddNumberToObject(root, "temperature", temperature);
  cJSON_AddNumberToObject(root, "max_tokens", max_tokens);
  cJSON *msgs = cJSON_CreateArray();
  cJSON *m = cJSON_CreateObject();
  cJSON_AddStringToObject(m, "role", "user");
  cJSON_AddStringToObject(m, "content", user_message);
  cJSON_AddItemToArray(msgs, m);
  cJSON_AddItemToObject(root, "messages", msgs);
  char *post_body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!post_body) return NULL;

  struct curl_buf resp = {0};
  static int curl_inited;
  if (!curl_inited) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
      free(post_body);
      return NULL;
    }
    curl_inited = 1;
  }
  CURL *curl = curl_easy_init();
  if (!curl) {
    free(post_body);
    return NULL;
  }
  struct curl_slist *hdrs = NULL;
  char authhdr[256];
  snprintf(authhdr, sizeof(authhdr), "Authorization: Bearer %s", api_key);
  hdrs = curl_slist_append(hdrs, authhdr);
  hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

  CURLcode cc = curl_easy_perform(curl);
  double total_sec = 0;
  if (cc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_sec);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  free(post_body);

  if (cc != CURLE_OK) {
    free(resp.p);
    bao_warn("curl: %s", curl_easy_strerror(cc));
    return NULL;
  }
  *latency_ms = (long)(total_sec * 1000.0);

  char *out_text = NULL;
  cJSON *jr = resp.p ? cJSON_Parse(resp.p) : NULL;
  free(resp.p);
  if (!jr) return NULL;
  cJSON *err = cJSON_GetObjectItemCaseSensitive(jr, "error");
  if (err) {
    cJSON *em = cJSON_GetObjectItemCaseSensitive(err, "message");
    if (em && cJSON_IsString(em) && em->valuestring) bao_warn("openai error: %s", em->valuestring);
    cJSON_Delete(jr);
    return NULL;
  }
  cJSON *choices = cJSON_GetObjectItemCaseSensitive(jr, "choices");
  cJSON *ch0 = choices && cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
  cJSON *msg = ch0 ? cJSON_GetObjectItemCaseSensitive(ch0, "message") : NULL;
  cJSON *content = msg ? cJSON_GetObjectItemCaseSensitive(msg, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring) out_text = strdup(content->valuestring);
  cJSON *usage = cJSON_GetObjectItemCaseSensitive(jr, "usage");
  if (usage) {
    cJSON *tt = cJSON_GetObjectItemCaseSensitive(usage, "total_tokens");
    if (tt && cJSON_IsNumber(tt)) *tokens_out = (int)tt->valuedouble;
  }
  cJSON_Delete(jr);
  return out_text;
}

int cmd_run(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  const char *dataset_override = NULL;
  int dry_run = 0;
  int start = 0;
  if (argc > 0 && argv[0] && !strcmp(argv[0], "run")) start = 1;
  for (int i = start; i < argc; i++) {
    if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dataset")) {
      if (i + 1 >= argc) bao_die("usage: bao run [-d <dataset.jsonl>] [--dry-run]");
      dataset_override = argv[++i];
      continue;
    }
    if (!strcmp(argv[i], "--dry-run")) {
      dry_run = 1;
      continue;
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    bao_die("usage: bao run [-d <dataset.jsonl>] [--dry-run]");
  }

  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  BaoConfig *cfg = load_config("bao.yaml");
  if (!cfg) bao_die("failed to load bao.yaml");

  const char *dataset_path = dataset_override ? dataset_override : cfg->dataset_path;
  if (!dataset_path || !*dataset_path) {
    free_config(cfg);
    bao_die("no dataset (set dataset: in bao.yaml or use -d)");
  }
  if (!bao_is_safe_relpath(dataset_path) || !bao_file_exists(dataset_path)) {
    free_config(cfg);
    bao_die("dataset not found or unsafe path: %s", dataset_path);
  }

  char *commit_hash = bao_resolve_head_full();
  if (!commit_hash || !bao_is_hex64(commit_hash)) {
    free(commit_hash);
    free_config(cfg);
    bao_die("cannot resolve HEAD (make a commit first)");
  }

  char dhash[65];
  if (hash_dataset_file(dataset_path, dhash) != 0) {
    free(commit_hash);
    free_config(cfg);
    bao_die("cannot hash dataset file");
  }

  bao_db_t db;
  if (bao_db_open(&db, BAO_DB_FILE) != 0 || bao_db_init_schema(&db) != 0) {
    bao_db_close(&db);
    free(commit_hash);
    free_config(cfg);
    bao_die("failed to open database");
  }
  if (bao_db_update_commit_dataset_hash(&db, commit_hash, dhash) != 0) {
    bao_db_close(&db);
    free(commit_hash);
    free_config(cfg);
    bao_die("failed to update commit dataset_hash (unknown HEAD commit in db?)");
  }

  /* Provider dispatch: OpenAI below; extend for more vendors (issue #7, docs/ISSUE_FOOTHOLD.md). */
  const char *prov = cfg->provider && cfg->provider[0] ? cfg->provider : "openai";
  if (strcmp(prov, "openai") != 0 && !dry_run) {
    bao_db_close(&db);
    free(commit_hash);
    free_config(cfg);
    bao_die("provider '%s' is not supported yet (only 'openai'; use --dry-run to preview; issue #7)", prov);
  }

  FILE *fp = fopen(dataset_path, "r");
  if (!fp) {
    bao_db_close(&db);
    free(commit_hash);
    free_config(cfg);
    bao_die("cannot open dataset");
  }

  char line[65536];
  size_t line_no = 0;
  while (fgets(line, sizeof(line), fp)) {
    line_no++;
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    if (!line[0]) continue;
    cJSON *row = cJSON_Parse(line);
    if (!row) {
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("invalid JSON on line %zu", line_no);
    }
    char *rendered = render_template(cfg->prompt_text, row);
    if (!rendered) {
      cJSON_Delete(row);
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("template render failed on line %zu", line_no);
    }
    const char *dsid = json_line_dataset_id(row);
    char *did_copy = dsid ? strdup(dsid) : NULL;
    char *input_json = cJSON_PrintUnformatted(row);
    cJSON_Delete(row);
    if (!input_json) {
      free(rendered);
      free(did_copy);
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("out of memory");
    }

    if (dry_run) {
      printf("--- line %zu ---\n%s\n", line_no, rendered);
      free(rendered);
      free(input_json);
      free(did_copy);
      continue;
    }

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key) {
      free(rendered);
      free(input_json);
      free(did_copy);
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("OPENAI_API_KEY is not set");
    }

    long lat = 0;
    int tok = -1;
    char *answer = openai_chat_completion(api_key, cfg->model, cfg->temperature, cfg->max_tokens, rendered, &lat, &tok);
    free(rendered);
    if (!answer) {
      free(input_json);
      free(did_copy);
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("API request failed on line %zu", line_no);
    }
    if (bao_db_insert_execution(&db, commit_hash, did_copy, input_json, answer, (int)lat, tok) != 0) {
      free(answer);
      free(input_json);
      free(did_copy);
      fclose(fp);
      bao_db_close(&db);
      free(commit_hash);
      free_config(cfg);
      bao_die("failed to insert execution on line %zu", line_no);
    }
    free(answer);
    free(input_json);
    free(did_copy);
  }
  fclose(fp);

  bao_db_close(&db);
  free(commit_hash);
  free_config(cfg);
  if (!dry_run) printf("run finished\n");
  return 0;
}

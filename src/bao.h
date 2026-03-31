#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Repo layout
#define BAO_DIR ".bao"
#define BAO_OBJECTS_DIR ".bao/objects"
#define BAO_COMMITS_DIR ".bao/objects/commits"
#define BAO_RESULTS_DIR ".bao/objects/results"
#define BAO_REFS_DIR ".bao/refs"
#define BAO_HEADS_DIR ".bao/refs/heads"
#define BAO_TAGS_DIR ".bao/refs/tags"
#define BAO_HEAD_FILE ".bao/HEAD"
#define BAO_DB_FILE ".bao/index.db"
#define BAO_STAGED_FILE ".bao/staged"
#define BAO_STASH_FILE ".bao/stash"
#define BAO_REMOTES_FILE ".bao/remotes"

#define BAO_VERSION "0.1.0"

#define BAO_SHORT_HASH_LEN 7

// Prompt settings (config.c)
// provider: API vendor id (openai, anthropic, google, ...); included in commit identity when switching vendors in one repo.
// profile: optional; with prompts_dir "prompts", selects prompts/<profile>/ (if omitted, use prompts_dir as before).
typedef struct {
  char *provider;
  char *profile;
  char *model;
  double temperature;
  int max_tokens;
  char *prompt_filepath;
  char *prompts_dir;
  char *dataset_path;
  char *prompt_text;
} BaoConfig;

// One test row (template.c / future cmd_run)
typedef struct {
  char *test_id;
  char *raw_json;
} BaoTestCase;

// LLM response (future cmd_run)
typedef struct {
  char *output_text;
  int latency_ms;
  int prompt_tokens;
  int completion_tokens;
} BaoLLMResponse;

void bao_free_test_case(BaoTestCase *tc);
void bao_free_llm_response(BaoLLMResponse *r);

typedef struct {
  const char *argv0;
} bao_ctx_t;

int cmd_init(bao_ctx_t *ctx, int argc, char **argv);
int cmd_add(bao_ctx_t *ctx, int argc, char **argv);
int cmd_commit(bao_ctx_t *ctx, int argc, char **argv);
int cmd_log(bao_ctx_t *ctx, int argc, char **argv);
int cmd_checkout(bao_ctx_t *ctx, int argc, char **argv);
int cmd_run(bao_ctx_t *ctx, int argc, char **argv);
int cmd_eval(bao_ctx_t *ctx, int argc, char **argv);
int cmd_remote(bao_ctx_t *ctx, int argc, char **argv);

int cmd_version(bao_ctx_t *ctx, int argc, char **argv);
int cmd_help(bao_ctx_t *ctx, int argc, char **argv);
int cmd_status(bao_ctx_t *ctx, int argc, char **argv);
int cmd_switch(bao_ctx_t *ctx, int argc, char **argv);
int cmd_diff(bao_ctx_t *ctx, int argc, char **argv);
int cmd_reset(bao_ctx_t *ctx, int argc, char **argv);
int cmd_branch(bao_ctx_t *ctx, int argc, char **argv);
int cmd_tag(bao_ctx_t *ctx, int argc, char **argv);
int cmd_show(bao_ctx_t *ctx, int argc, char **argv);
int cmd_stash(bao_ctx_t *ctx, int argc, char **argv);
int cmd_restore(bao_ctx_t *ctx, int argc, char **argv);
int cmd_rm(bao_ctx_t *ctx, int argc, char **argv);
int cmd_rev_parse(bao_ctx_t *ctx, int argc, char **argv);
int cmd_config(bao_ctx_t *ctx, int argc, char **argv);
int cmd_clone(bao_ctx_t *ctx, int argc, char **argv);
int cmd_merge(bao_ctx_t *ctx, int argc, char **argv);
int cmd_fetch(bao_ctx_t *ctx, int argc, char **argv);
int cmd_rebase(bao_ctx_t *ctx, int argc, char **argv);
int cmd_filter_branch(bao_ctx_t *ctx, int argc, char **argv);
int cmd_cherry_pick(bao_ctx_t *ctx, int argc, char **argv);
int cmd_revert(bao_ctx_t *ctx, int argc, char **argv);
int cmd_blame(bao_ctx_t *ctx, int argc, char **argv);

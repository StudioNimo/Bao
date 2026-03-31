#pragma once

// 表示用の短いプレフィックス（先頭7 hex）。HEAD が無ければ NULL。Caller frees。
char *bao_resolve_head_short(void);

// Branch name if HEAD is refs/heads/*, else NULL (detached). Caller frees.
char *bao_current_branch(void);

// Resolve ref name (HEAD, branch name, refs/..., or hex prefix). Caller frees.
char *bao_rev_parse(const char *name);

int bao_branch_exists(const char *name);
// Write HEAD as symbolic ref to refs/heads/<branch_name>.
int bao_write_head_symbolic(const char *branch_name);
// Detached HEAD at full 64 hex commit id.
int bao_write_head_detached(const char *full_hash_64);
// Update current branch ref (or detached HEAD) to full commit id.
int bao_update_head_to_commit(const char *full_hash_64);

// HEAD, HEAD~n (chronological offset in DB; not full git graph), branch/tag/hash.
char *bao_resolve_revision(const char *spec);

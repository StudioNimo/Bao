#pragma once

#include <stddef.h>

// 64 hex lowercase SHA-256 object id validation.
int bao_is_hex64(const char *s);

// HEAD のコミットをフルハッシュで返す（malloc）。無い・空なら NULL。
char *bao_resolve_head_full(void);

// 表示用 7 文字（malloc）。HEAD が無ければ NULL。
char *bao_display_short_from_full(const char *full64);

// ref ファイル1行の中身 → フルハッシュ（malloc）。レガシー7桁は JSON から full_hash を取得。
char *bao_ref_line_to_commit_full(const char *line);

// 任意の rev 指定 → フル64（HEAD / HEAD~n / ブランチ / タグ / フル64 / 短縮プレフィックス）。
// ambiguous / 見つからないとき NULL。
char *bao_resolve_commit_full(const char *spec);

// フルハッシュの JSON パスを out に書く（成功 0）。
int bao_commit_json_path_full(const char *full64, char *out, size_t outsz);

// id（フル・短縮・レガシー）でコミット JSON を読む。full_out に確定フル64を入れる（malloc）。
int bao_read_commit_json_any(const char *id, char **full_out, unsigned char **buf, size_t *len);

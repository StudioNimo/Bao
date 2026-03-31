#include "bao.h"

#include "commits.h"
#include "db.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int tty_set_raw(struct termios *saved) {
  if (!isatty(STDIN_FILENO)) return -1;
  if (tcgetattr(STDIN_FILENO, saved) != 0) return -1;
  struct termios t = *saved;
  t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0) return -1;
  return 0;
}

static void tty_restore(const struct termios *saved) { (void)tcsetattr(STDIN_FILENO, TCSANOW, saved); }

int cmd_eval(bao_ctx_t *ctx, int argc, char **argv) {
  (void)ctx;
  int start = 0;
  if (argc > 0 && argv[0] && !strcmp(argv[0], "eval")) start = 1;
  for (int i = start; i < argc; i++) {
    if (!strcmp(argv[i], "--json")) {
      bao_die("eval --json is not implemented yet");
    }
    if (argv[i][0] == '-') bao_die("unknown option: %s", argv[i]);
    bao_die("usage: bao eval");
  }

  if (!bao_file_exists(BAO_HEAD_FILE)) bao_die("not a bao repo (run `bao init` first)");

  char *commit_hash = bao_resolve_head_full();
  if (!commit_hash || !bao_is_hex64(commit_hash)) {
    free(commit_hash);
    bao_die("cannot resolve HEAD");
  }

  bao_db_t db;
  if (bao_db_open(&db, BAO_DB_FILE) != 0 || bao_db_init_schema(&db) != 0) {
    bao_db_close(&db);
    free(commit_hash);
    bao_die("failed to open database");
  }

  struct termios saved;
  if (tty_set_raw(&saved) != 0) {
    bao_db_close(&db);
    free(commit_hash);
    bao_die("eval requires an interactive terminal (TTY)");
  }

  int had_any = 0;
  for (;;) {
    bao_uneval_exec_t u;
    int gr = bao_db_next_unevaluated_for_commit(&db, commit_hash, &u);
    if (gr == 1) break;
    if (gr != 0) {
      tty_restore(&saved);
      bao_db_close(&db);
      free(commit_hash);
      bao_die("database error");
    }
    had_any = 1;
    printf("\033[1m--- input ---\033[0m\n%s\n\033[1m--- output ---\033[0m\n%s\n", u.input_text, u.output_text);
    printf("Score: 1=BAD  2=NEUTRAL  3=GOOD  (q=quit)\n");

    const char *score = NULL;
    for (;;) {
      unsigned char c;
      if (read(STDIN_FILENO, &c, 1) != 1) {
        bao_uneval_exec_clear(&u);
        tty_restore(&saved);
        bao_db_close(&db);
        free(commit_hash);
        bao_die("read stdin");
      }
      if (c == 'q' || c == 'Q') {
        bao_uneval_exec_clear(&u);
        tty_restore(&saved);
        bao_db_close(&db);
        free(commit_hash);
        printf("\n");
        return 1;
      }
      if (c == '1') {
        score = "BAD";
        break;
      }
      if (c == '2') {
        score = "NEUTRAL";
        break;
      }
      if (c == '3') {
        score = "GOOD";
        break;
      }
    }

    if (bao_db_insert_evaluation(&db, u.execution_id, score) != 0) {
      bao_uneval_exec_clear(&u);
      tty_restore(&saved);
      bao_db_close(&db);
      free(commit_hash);
      bao_die("failed to save evaluation");
    }
    bao_uneval_exec_clear(&u);
    printf("saved: %s\n", score);
  }

  tty_restore(&saved);
  bao_db_close(&db);
  free(commit_hash);
  if (!had_any) {
    fprintf(stderr, "bao: nothing to evaluate (run `bao run` first, or all rows are already scored)\n");
    return 1;
  }
  return 0;
}

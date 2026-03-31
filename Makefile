CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
ifeq ($(RELEASE),1)
  CFLAGS += -Werror
endif

.DEFAULT_GOAL := all

CPPFLAGS ?= -Isrc -Ithird_party/cJSON
CPPFLAGS += -D_DEFAULT_SOURCE

LDFLAGS ?=
LDLIBS ?= -lsqlite3 -lcurl

ifeq ($(BAO_USE_OPENSSL),1)
  CPPFLAGS += -DBAO_USE_OPENSSL
  LDLIBS += -lcrypto
endif

BIN_DIR := bin
SRC_DIR := src
OBJ_DIR := build
MAN1_DIR := man

SRCS := \
  $(SRC_DIR)/bao.c \
  $(SRC_DIR)/cmd_init.c \
  $(SRC_DIR)/cmd_add.c \
  $(SRC_DIR)/cmd_commit.c \
  $(SRC_DIR)/cmd_log.c \
  $(SRC_DIR)/cmd_checkout.c \
  $(SRC_DIR)/cmd_run.c \
  $(SRC_DIR)/cmd_eval.c \
  $(SRC_DIR)/cmd_remote.c \
  $(SRC_DIR)/db.c \
  $(SRC_DIR)/hash.c \
  $(SRC_DIR)/fs.c \
  $(SRC_DIR)/util.c \
  $(SRC_DIR)/config.c \
  $(SRC_DIR)/template.c \
  $(SRC_DIR)/stage.c \
  $(SRC_DIR)/refs.c \
  $(SRC_DIR)/commits.c \
  $(SRC_DIR)/cmd_version.c \
  $(SRC_DIR)/cmd_help.c \
  $(SRC_DIR)/cmd_misc.c

.PHONY: install-man
install-man:
	@echo "To enable: sudo install -m 0644 $(MAN1_DIR)/bao.1 /usr/local/share/man/man1/bao.1 && sudo mandb"

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS)) $(OBJ_DIR)/cJSON.o

.PHONY: all clean test

all: $(BIN_DIR)/bao

test: $(BIN_DIR)/bao
	./tests/integration_internal.sh
	./tests/integration_external.sh
	./tests/integration_multi_provider.sh
	./tests/scenarios_20_internal.sh
	./tests/scenarios_20_external.sh
	./tests/scenarios_21_internal.sh
	./tests/scenarios_21_external.sh
	./tests/cli_smoke.sh
	./tests/stub_options_fail.sh
	./tests/assert_strict.sh

$(BIN_DIR)/bao: $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJ_DIR)/cJSON.o: third_party/cJSON/cJSON.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

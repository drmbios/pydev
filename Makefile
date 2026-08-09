CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDFLAGS ?=
BIN_DIR := bin
COMMON := c/common.c
TOOLS := cntr codebreaker parser_html qpipper rdr2dot0 readjson sql contacts ttt write2file

.PHONY: all clean check sanitize
all: $(TOOLS:%=$(BIN_DIR)/%)

$(BIN_DIR):
	mkdir -p $@

$(BIN_DIR)/cntr: c/cntr.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/codebreaker: c/codebreaker.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/parser_html: c/parser_html.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/rdr2dot0: c/rdr2dot0.c c/jsonfmt.c $(COMMON) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@
$(BIN_DIR)/readjson: c/readjson.c c/jsonfmt.c $(COMMON) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@
$(BIN_DIR)/contacts: c/contacts.c $(COMMON) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@
$(BIN_DIR)/sql: c/sql.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -lsqlite3 -o $@
$(BIN_DIR)/%: c/%.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

check: all
	sh tests/test_c_tools.sh

sanitize: clean
	$(MAKE) CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" check

clean:
	rm -rf $(BIN_DIR)

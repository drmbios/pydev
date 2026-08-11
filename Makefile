CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDFLAGS ?=
BIN_DIR := bin
COMMON := c/common.c
TOOLS := cntr codebreaker parser_html qpipper rdr2dot0 readjson sql contacts ttt write2file \
	checksum hexview stringsx randpass syscallx lsx
SQLITE_PROBE_CFLAGS := $(filter-out -g,$(CFLAGS))
SQLITE_AVAILABLE ?= $(shell $(CC) $(CPPFLAGS) $(SQLITE_PROBE_CFLAGS) c/sqlite_probe.c $(LDFLAGS) -lsqlite3 -o /dev/null >/dev/null 2>&1 && echo 1 || echo 0)

ifeq ($(SQLITE_AVAILABLE),1)
SQLITE_CPPFLAGS := -DPYDEV_HAVE_SQLITE3=1
SQLITE_LIBS := -lsqlite3
else
SQLITE_CPPFLAGS := -DPYDEV_HAVE_SQLITE3=0
SQLITE_LIBS :=
endif

.PHONY: all clean check check-no-sqlite sanitize
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
$(BIN_DIR)/checksum: c/checksum.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/hexview: c/hexview.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/stringsx: c/stringsx.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/randpass: c/randpass.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/syscallx: c/syscallx.c $(COMMON) c/common.h | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(COMMON) $(LDFLAGS) -o $@
$(BIN_DIR)/sql: c/sql.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(SQLITE_CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) $(SQLITE_LIBS) -o $@
$(BIN_DIR)/%: c/%.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

check: all
	sh tests/test_c_tools.sh

check-no-sqlite: clean
	$(MAKE) SQLITE_AVAILABLE=0 $(BIN_DIR)/sql
	$(BIN_DIR)/sql --backend | grep -q '^unavailable'
	! $(BIN_DIR)/sql test.db list >/dev/null 2>&1

sanitize: clean
	$(MAKE) CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" check

clean:
	rm -rf $(BIN_DIR)

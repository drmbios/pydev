#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static int initialize(sqlite3 *db) {
    const char *statement = "CREATE TABLE IF NOT EXISTS category ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE,answer TEXT NOT NULL)";
    char *error = NULL;
    int result = sqlite3_exec(db, statement, NULL, NULL, &error);
    if (result != SQLITE_OK) { fprintf(stderr, "sqlite: %s\n", error); sqlite3_free(error); }
    return result;
}

int main(int argc, char **argv) {
    sqlite3 *db = NULL; sqlite3_stmt *statement = NULL; int result = 1;
    if (argc < 3 || (strcmp(argv[2], "list") && strcmp(argv[2], "add"))) {
        fprintf(stderr, "usage: %s DATABASE list\n       %s DATABASE add NAME ANSWER\n", argv[0], argv[0]); return 2;
    }
    if (!strcmp(argv[2], "add") && argc != 5) { fputs("add requires NAME and ANSWER\n", stderr); return 2; }
    if (argc > 5 || (argc == 3 && strcmp(argv[2], "list"))) return 2;
    if (argc == 5 && (strlen(argv[3]) > 4096 || strlen(argv[4]) > 65536)) {
        fputs("name or answer exceeds size limit\n", stderr); return 2;
    }
    if (sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", db ? sqlite3_errmsg(db) : "open failed"); goto done;
    }
    sqlite3_busy_timeout(db, 3000);
    if (initialize(db) != SQLITE_OK) goto done;
    if (!strcmp(argv[2], "add")) {
        const char *query = "INSERT INTO category(name,answer) VALUES(?,?) "
                            "ON CONFLICT(name) DO UPDATE SET answer=excluded.answer";
        if (sqlite3_prepare_v2(db, query, -1, &statement, NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, argv[3], -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(statement, 2, argv[4], -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            fprintf(stderr, "sqlite: %s\n", sqlite3_errmsg(db)); goto done;
        }
        printf("Saved: %s\n", argv[3]); result = 0;
    } else {
        if (sqlite3_prepare_v2(db, "SELECT id,name,answer FROM category ORDER BY name COLLATE NOCASE", -1,
                               &statement, NULL) != SQLITE_OK) goto done;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW)
            printf("%lld: %s - %s\n", sqlite3_column_int64(statement, 0),
                   sqlite3_column_text(statement, 1), sqlite3_column_text(statement, 2));
        result = result == SQLITE_DONE ? 0 : 1;
    }
done:
    if (statement) sqlite3_finalize(statement);
    if (db) sqlite3_close(db);
    return result;
}

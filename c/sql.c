#include <stdio.h>
#include <string.h>

#ifndef PYDEV_HAVE_SQLITE3
#define PYDEV_HAVE_SQLITE3 0
#endif

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --backend\n"
            "       %s DATABASE list\n"
            "       %s DATABASE add NAME ANSWER\n",
            program, program, program);
}

#if PYDEV_HAVE_SQLITE3

#include <sqlite3.h>

static int initialize(sqlite3 *database) {
    const char *query =
        "CREATE TABLE IF NOT EXISTS category ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "answer TEXT NOT NULL)";
    char *error = NULL;
    int result = sqlite3_exec(database, query, NULL, NULL, &error);

    if (result != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", error ? error : sqlite3_errmsg(database));
        sqlite3_free(error);
    }
    return result;
}

static int add_category(sqlite3 *database, const char *name, const char *answer) {
    const char *query =
        "INSERT INTO category(name,answer) VALUES(?,?) "
        "ON CONFLICT(name) DO UPDATE SET answer=excluded.answer";
    sqlite3_stmt *statement = NULL;
    int result = 1;

    if (sqlite3_prepare_v2(database, query, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, answer, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_DONE) {
        fprintf(stderr, "sqlite: %s\n", sqlite3_errmsg(database));
    } else {
        printf("Saved: %s\n", name);
        result = 0;
    }
    if (statement) sqlite3_finalize(statement);
    return result;
}

static int list_categories(sqlite3 *database) {
    const char *query =
        "SELECT id,name,answer FROM category ORDER BY name COLLATE NOCASE";
    sqlite3_stmt *statement = NULL;
    int step;
    int result = 1;

    if (sqlite3_prepare_v2(database, query, -1, &statement, NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", sqlite3_errmsg(database));
        return 1;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        const unsigned char *answer = sqlite3_column_text(statement, 2);
        printf("%lld: %s - %s\n", sqlite3_column_int64(statement, 0),
               name ? (const char *)name : "", answer ? (const char *)answer : "");
    }
    if (step == SQLITE_DONE) result = 0;
    else fprintf(stderr, "sqlite: %s\n", sqlite3_errmsg(database));
    sqlite3_finalize(statement);
    return result;
}

int main(int argc, char **argv) {
    sqlite3 *database = NULL;
    int result = 1;

    if (argc == 2 && strcmp(argv[1], "--backend") == 0) {
        printf("sqlite3 %s\n", sqlite3_libversion());
        return 0;
    }
    if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        usage(argv[0]);
        return 0;
    }
    if (argc < 3 || argc > 5 ||
        (strcmp(argv[2], "list") != 0 && strcmp(argv[2], "add") != 0)) {
        usage(argv[0]);
        return 2;
    }
    if ((!strcmp(argv[2], "list") && argc != 3) ||
        (!strcmp(argv[2], "add") && argc != 5)) {
        usage(argv[0]);
        return 2;
    }
    if (argc == 5 && (strlen(argv[3]) > 4096U || strlen(argv[4]) > 65536U)) {
        fputs("sql: name or answer exceeds size limit\n", stderr);
        return 2;
    }
    if (sqlite3_open_v2(argv[1], &database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", database ? sqlite3_errmsg(database) : "open failed");
        goto done;
    }
    if (sqlite3_busy_timeout(database, 3000) != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", sqlite3_errmsg(database));
        goto done;
    }
    if (initialize(database) != SQLITE_OK) goto done;
    result = !strcmp(argv[2], "add")
        ? add_category(database, argv[3], argv[4])
        : list_categories(database);

done:
    if (database && sqlite3_close(database) != SQLITE_OK) {
        fputs("sqlite: failed to close database cleanly\n", stderr);
        result = 1;
    }
    return result;
}

#else

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--backend") == 0) {
        puts("unavailable (SQLite 3 headers or library not found at build time)");
        return 0;
    }
    if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        usage(argv[0]);
        return 0;
    }
    fputs(
        "sql: SQLite 3 support is unavailable in this build.\n"
        "Install the SQLite development package and rebuild:\n"
        "  Debian/Ubuntu: libsqlite3-dev\n"
        "  Fedora/RHEL:   sqlite-devel\n"
        "  Arch Linux:    sqlite\n",
        stderr);
    return 3;
}

#endif

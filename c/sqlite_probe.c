#include <sqlite3.h>

int main(void) {
    return sqlite3_libversion_number() > 0 ? 0 : 1;
}

#include "jsonfmt.h"
#include <ctype.h>
#include <stdio.h>

#define MAX_JSON_DEPTH 128

static void indent(int depth) { int i; for (i = 0; i < depth * 2; ++i) putchar(' '); }

int print_json(const char *text, size_t length, int pretty) {
    int depth = 0, in_string = 0, escaped = 0;
    size_t i;
    char stack[MAX_JSON_DEPTH];
    for (i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (in_string) {
            putchar(ch);
            if (escaped) escaped = 0;
            else if (ch == '\\') escaped = 1;
            else if (ch == '"') in_string = 0;
            else if (ch < 0x20) { fputs("\ninvalid control byte in JSON string\n", stderr); return -1; }
            continue;
        }
        if (ch == '"') { in_string = 1; putchar(ch); continue; }
        if (isspace(ch)) { if (!pretty) putchar(ch); continue; }
        if (ch == '{' || ch == '[') {
            if (depth == MAX_JSON_DEPTH) { fputs("JSON nesting limit exceeded\n", stderr); return -1; }
            stack[depth++] = (char)ch; putchar(ch);
            if (pretty) { putchar('\n'); indent(depth); }
        } else if (ch == '}' || ch == ']') {
            char expected = ch == '}' ? '{' : '[';
            if (!depth || stack[depth - 1] != expected) { fputs("\nunbalanced JSON\n", stderr); return -1; }
            depth--; if (pretty) { putchar('\n'); indent(depth); } putchar(ch);
        } else if (ch == ',' && pretty) {
            puts(","); indent(depth);
        } else if (ch == ':' && pretty) {
            fputs(": ", stdout);
        } else putchar(ch);
    }
    if (in_string || depth) { fputs("\nincomplete JSON\n", stderr); return -1; }
    putchar('\n');
    return 0;
}

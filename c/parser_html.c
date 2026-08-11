#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int equal_ci(const char *left, const char *right, size_t length) {
    size_t index;
    for (index = 0; index < length; ++index)
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index])) return 0;
    return 1;
}

static size_t tag_end(const char *html, size_t length, size_t start) {
    char quote = '\0';
    size_t index;
    for (index = start; index < length; ++index) {
        if (quote) {
            if (html[index] == quote) quote = '\0';
        } else if (html[index] == '\'' || html[index] == '"') quote = html[index];
        else if (html[index] == '>') return index;
    }
    return length;
}

static size_t parse_tag(const char *html, size_t start, size_t end) {
    size_t position = start + 1U;
    size_t found = 0;

    while (position < end && isspace((unsigned char)html[position])) ++position;
    if (position < end && (html[position] == '/' || html[position] == '!' || html[position] == '?'))
        ++position;
    while (position < end && !isspace((unsigned char)html[position]) && html[position] != '>') ++position;

    while (position < end) {
        size_t iteration_start = position;
        size_t name_start;
        size_t name_length;
        size_t value_start = 0;
        size_t value_length = 0;
        int has_value = 0;

        while (position < end && (isspace((unsigned char)html[position]) || html[position] == '/'))
            ++position;
        if (position >= end || html[position] == '>') break;
        name_start = position;
        while (position < end && !isspace((unsigned char)html[position]) &&
               html[position] != '=' && html[position] != '/' && html[position] != '>')
            ++position;
        name_length = position - name_start;
        while (position < end && isspace((unsigned char)html[position])) ++position;
        if (position < end && html[position] == '=') {
            char quote = '\0';
            has_value = 1;
            ++position;
            while (position < end && isspace((unsigned char)html[position])) ++position;
            if (position < end && (html[position] == '\'' || html[position] == '"'))
                quote = html[position++];
            value_start = position;
            if (quote) while (position < end && html[position] != quote) ++position;
            else while (position < end && !isspace((unsigned char)html[position]) && html[position] != '>')
                ++position;
            value_length = position - value_start;
            if (quote && position < end) ++position;
        }
        if (has_value && name_length == 4U && equal_ci(html + name_start, "href", 4U)) {
            printf("%.*s\n", (int)value_length, html + value_start);
            ++found;
        }
        if (position == iteration_start) ++position;
    }
    return found;
}

int main(int argc, char **argv) {
    char *html = NULL;
    size_t length = 0;
    size_t position = 0;
    size_t found = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return 2;
    }
    if (read_file_bounded(argv[1], &html, &length) != 0) return 1;
    while (position < length) {
        size_t end;
        if (html[position] != '<') {
            ++position;
            continue;
        }
        if (length - position >= 4U && !memcmp(html + position, "<!--", 4U)) {
            const char *comment_end = strstr(html + position + 4U, "-->");
            if (!comment_end) break;
            position = (size_t)(comment_end - html) + 3U;
            continue;
        }
        end = tag_end(html, length, position + 1U);
        if (end == length) break;
        found += parse_tag(html, position, end);
        position = end + 1U;
    }
    free(html);
    fprintf(stderr, "parser_html: %zu link(s) found\n", found);
    return 0;
}

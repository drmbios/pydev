#include "jsonfmt.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_JSON_DEPTH 128U
#define MAX_JSON_OUTPUT (64U * 1024U * 1024U)

struct parser {
    const char *text;
    size_t length;
    size_t position;
};

static void skip_space(struct parser *parser) {
    while (parser->position < parser->length &&
           (parser->text[parser->position] == ' ' || parser->text[parser->position] == '\t' ||
            parser->text[parser->position] == '\r' || parser->text[parser->position] == '\n'))
        ++parser->position;
}

static int parse_value(struct parser *parser, unsigned depth);

static int parse_string(struct parser *parser) {
    if (parser->position >= parser->length || parser->text[parser->position++] != '"') return -1;
    while (parser->position < parser->length) {
        unsigned char byte = (unsigned char)parser->text[parser->position++];
        if (byte == '"') return 0;
        if (byte < 0x20) return -1;
        if (byte == '\\') {
            size_t index;
            if (parser->position >= parser->length) return -1;
            byte = (unsigned char)parser->text[parser->position++];
            if (strchr("\"\\/bfnrt", byte)) continue;
            if (byte != 'u' || parser->length - parser->position < 4U) return -1;
            for (index = 0; index < 4U; ++index)
                if (!isxdigit((unsigned char)parser->text[parser->position++])) return -1;
        }
    }
    return -1;
}

static int parse_number(struct parser *parser) {
    size_t start = parser->position;
    if (parser->position < parser->length && parser->text[parser->position] == '-') ++parser->position;
    if (parser->position >= parser->length) return -1;
    if (parser->text[parser->position] == '0') ++parser->position;
    else if (parser->text[parser->position] >= '1' && parser->text[parser->position] <= '9')
        while (parser->position < parser->length && isdigit((unsigned char)parser->text[parser->position]))
            ++parser->position;
    else return -1;
    if (parser->position < parser->length && parser->text[parser->position] == '.') {
        ++parser->position;
        if (parser->position >= parser->length || !isdigit((unsigned char)parser->text[parser->position])) return -1;
        while (parser->position < parser->length && isdigit((unsigned char)parser->text[parser->position]))
            ++parser->position;
    }
    if (parser->position < parser->length &&
        (parser->text[parser->position] == 'e' || parser->text[parser->position] == 'E')) {
        ++parser->position;
        if (parser->position < parser->length &&
            (parser->text[parser->position] == '+' || parser->text[parser->position] == '-'))
            ++parser->position;
        if (parser->position >= parser->length || !isdigit((unsigned char)parser->text[parser->position])) return -1;
        while (parser->position < parser->length && isdigit((unsigned char)parser->text[parser->position]))
            ++parser->position;
    }
    return parser->position > start ? 0 : -1;
}

static int parse_compound(struct parser *parser, unsigned depth, char closing, int object) {
    if (depth >= MAX_JSON_DEPTH) return -2;
    ++parser->position;
    skip_space(parser);
    if (parser->position < parser->length && parser->text[parser->position] == closing) {
        ++parser->position;
        return 0;
    }
    for (;;) {
        int result;
        if (object) {
            if (parse_string(parser) != 0) return -1;
            skip_space(parser);
            if (parser->position >= parser->length || parser->text[parser->position++] != ':') return -1;
            skip_space(parser);
        }
        result = parse_value(parser, depth + 1U);
        if (result != 0) return result;
        skip_space(parser);
        if (parser->position >= parser->length) return -1;
        if (parser->text[parser->position] == closing) {
            ++parser->position;
            return 0;
        }
        if (parser->text[parser->position++] != ',') return -1;
        skip_space(parser);
    }
}

static int parse_value(struct parser *parser, unsigned depth) {
    const char *remaining;
    size_t available;
    skip_space(parser);
    if (parser->position >= parser->length) return -1;
    remaining = parser->text + parser->position;
    available = parser->length - parser->position;
    switch (*remaining) {
        case '"': return parse_string(parser);
        case '{': return parse_compound(parser, depth, '}', 1);
        case '[': return parse_compound(parser, depth, ']', 0);
        case 't': if (available >= 4U && !memcmp(remaining, "true", 4U)) { parser->position += 4U; return 0; } break;
        case 'f': if (available >= 5U && !memcmp(remaining, "false", 5U)) { parser->position += 5U; return 0; } break;
        case 'n': if (available >= 4U && !memcmp(remaining, "null", 4U)) { parser->position += 4U; return 0; } break;
        default: return parse_number(parser);
    }
    return -1;
}

static int validate_json(const char *text, size_t length) {
    struct parser parser = {text, length, 0};
    int result = parse_value(&parser, 0);
    skip_space(&parser);
    if (result == -2) fputs("json: nesting limit exceeded\n", stderr);
    else if (result != 0 || parser.position != length)
        fprintf(stderr, "json: invalid syntax near byte %zu\n", parser.position);
    return result == 0 && parser.position == length ? 0 : -1;
}

static int emit_character(FILE *output, int character, size_t *count) {
    if (*count == MAX_JSON_OUTPUT) return -1;
    ++*count;
    return output && fputc(character, output) == EOF ? -2 : 0;
}

static int emit_indent(FILE *output, unsigned depth, size_t *count) {
    unsigned index;
    for (index = 0; index < depth * 2U; ++index) {
        int result = emit_character(output, ' ', count);
        if (result != 0) return result;
    }
    return 0;
}

static int format_pretty(const char *text, size_t length, FILE *output, size_t *count) {
    size_t index;
    unsigned depth = 0;
    int in_string = 0;
    int escaped = 0;

    for (index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        int result;
        if (in_string) {
            result = emit_character(output, byte, count);
            if (result) return result;
            if (escaped) escaped = 0;
            else if (byte == '\\') escaped = 1;
            else if (byte == '"') in_string = 0;
            continue;
        }
        if (byte == '"') {
            in_string = 1;
            if ((result = emit_character(output, byte, count)) != 0) return result;
        } else if (byte == '{' || byte == '[') {
            size_t next = index + 1U;
            char closing = byte == '{' ? '}' : ']';
            if ((result = emit_character(output, byte, count)) != 0) return result;
            ++depth;
            while (next < length && isspace((unsigned char)text[next])) ++next;
            if (next < length && text[next] != closing) {
                if ((result = emit_character(output, '\n', count)) != 0 ||
                    (result = emit_indent(output, depth, count)) != 0) return result;
            }
        } else if (byte == '}' || byte == ']') {
            size_t previous = index;
            char opening = byte == '}' ? '{' : '[';
            while (previous > 0 && isspace((unsigned char)text[previous - 1U])) --previous;
            --depth;
            if (previous > 0 && text[previous - 1U] != opening) {
                if ((result = emit_character(output, '\n', count)) != 0 ||
                    (result = emit_indent(output, depth, count)) != 0) return result;
            }
            if ((result = emit_character(output, byte, count)) != 0) return result;
        } else if (byte == ',') {
            if ((result = emit_character(output, ',', count)) != 0 ||
                (result = emit_character(output, '\n', count)) != 0 ||
                (result = emit_indent(output, depth, count)) != 0) return result;
        } else if (byte == ':') {
            if ((result = emit_character(output, ':', count)) != 0 ||
                (result = emit_character(output, ' ', count)) != 0) return result;
        } else if (!isspace(byte)) {
            if ((result = emit_character(output, byte, count)) != 0) return result;
        }
    }
    return emit_character(output, '\n', count);
}

int print_json(const char *text, size_t length, int pretty) {
    size_t output_size = 0;
    int result;
    if (!text || validate_json(text, length) != 0) return -1;
    if (!pretty) {
        if (fwrite(text, 1, length, stdout) != length || putchar('\n') == EOF) {
            perror("stdout");
            return -1;
        }
        return 0;
    }
    result = format_pretty(text, length, NULL, &output_size);
    if (result == -1) {
        fprintf(stderr, "json: formatted output exceeds %u-byte limit\n", MAX_JSON_OUTPUT);
        return -1;
    }
    output_size = 0;
    if (format_pretty(text, length, stdout, &output_size) != 0) {
        perror("stdout");
        return -1;
    }
    return 0;
}

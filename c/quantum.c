#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "quantum.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define Q_INPUT 262144U
typedef struct { const char *p; char token[64]; int bad; } Lexer;
static void next(Lexer *l) {
    size_t n = 0;
    for (;;) {
        while (*l->p && isspace((unsigned char)*l->p)) ++l->p;
        if (l->p[0] != '/' || l->p[1] != '/') break;
        while (*l->p && *l->p != '\n') ++l->p;
    }
    if (*l->p == '"') {
        l->token[n++] = *l->p++;
        while (*l->p && *l->p != '"' && *l->p != '\n' && *l->p != '\r') {
            if (n + 2 < sizeof l->token) l->token[n++] = *l->p;
            else l->bad = 1;
            ++l->p;
        }
        if (*l->p != '"') l->bad = 1;
        else { if (n + 1 < sizeof l->token) l->token[n++] = '"'; ++l->p; }
    } else if (isalnum((unsigned char)*l->p) || *l->p == '_') {
        while (isalnum((unsigned char)*l->p) || *l->p == '_' || *l->p == '.') {
            if (n + 1 < sizeof l->token) l->token[n++] = *l->p;
            else l->bad = 1;
            ++l->p;
        }
    } else if (*l->p) l->token[n++] = *l->p++;
    l->token[n] = '\0';
}
static int take(Lexer *l, const char *s) {
    if (l->bad || strcmp(l->token, s)) return -1;
    next(l); return 0;
}
int q_number(const char *s, unsigned maximum, unsigned *value) {
    unsigned n = 0;
    if (!*s || strlen(s) > 63U) return -1;
    for (; *s; ++s) {
        unsigned digit;
        if (*s < '0' || *s > '9') return -1;
        digit = (unsigned)(*s - '0');
        if (digit > maximum || n > (maximum - digit) / 10U) return -1;
        n = n * 10U + digit;
    }
    *value = n; return 0;
}
static int number(Lexer *l, unsigned max, unsigned *v) {
    if (l->bad || q_number(l->token, max, v)) return -1;
    next(l); return 0;
}
static int operand(Lexer *l, unsigned n, unsigned *v) {
    return take(l, "q") || take(l, "[") || number(l, n - 1U, v) || take(l, "]");
}
static int parse(const char *text, QCircuit *c) {
    Lexer l = {text, "", 0};
    unsigned layers[Q_MAX_QUBITS] = {0};
    int measured = 0, creg = 0;
    memset(c, 0, sizeof *c); next(&l);
    if (take(&l,"OPENQASM") || take(&l,"2.0") || take(&l,";") ||
        take(&l,"include") || take(&l,"\"qelib1.inc\"") ||
        take(&l,";") || take(&l,"qreg") || take(&l,"q") ||
        take(&l,"[") || number(&l,Q_MAX_QUBITS,&c->qubits) || !c->qubits ||
        take(&l,"]") || take(&l,";")) return -1;
    if (!strcmp(l.token,"creg")) {
        unsigned bits;
        next(&l);
        if (take(&l,"c") || take(&l,"[") || number(&l,Q_MAX_QUBITS,&bits) ||
            bits != c->qubits || take(&l,"]") || take(&l,";")) return -1;
        creg = 1;
    }
    while (*l.token) {
        QGate g = {{0}, 0, 0}; unsigned depth;
        if (measured) return -1;
        if (!strcmp(l.token,"measure")) {
            next(&l);
            if (!creg || take(&l,"q") || take(&l,"-") || take(&l,">") ||
                take(&l,"c") || take(&l,";")) return -1;
            measured = 1; continue;
        }
        if (strcmp(l.token,"h") && strcmp(l.token,"x") && strcmp(l.token,"z") &&
            strcmp(l.token,"s") && strcmp(l.token,"t") && strcmp(l.token,"cx") &&
            strcmp(l.token,"cz")) return -1;
        if (c->count == Q_MAX_GATES) return -1;
        strcpy(g.name, l.token); next(&l);
        if (operand(&l,c->qubits,&g.a)) return -1;
        if (g.name[0] == 'c' && (take(&l,",") || operand(&l,c->qubits,&g.b) || g.a == g.b)) return -1;
        if (take(&l,";")) return -1;
        depth = layers[g.a];
        if (g.name[0] == 'c' && layers[g.b] > depth) depth = layers[g.b];
        layers[g.a] = ++depth;
        if (g.name[0] == 'c') layers[g.b] = depth;
        if (depth > c->depth) c->depth = depth;
        c->gates[c->count++] = g;
    }
    return l.bad ? -1 : 0;
}
int q_load(const char *path, QCircuit *c) {
    struct stat st;
    size_t used = 0;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    char *data = NULL;
    int result = -1;
    if (fd < 0) goto done;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size < 0 || st.st_size > Q_INPUT) goto done;
    data = malloc(Q_INPUT + 2U);
    if (!data) goto done;
    while (used <= Q_INPUT) {
        ssize_t n = read(fd,data+used,Q_INPUT+1U-used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto done;
        if (!n) break;
        used += (size_t)n;
    }
    if (used > Q_INPUT || memchr(data,0,used)) goto done;
    for (size_t i=0;i<used;i++) if ((unsigned char)data[i] > 127U) goto done;
    data[used] = 0;
    result = parse(data,c);
done:
    if (fd >= 0) close(fd);
    free(data);
    if (result) fputs("invalid, unsupported, or over-budget QASM input (see docs/QUANTUM.md)\n",stderr);
    return result;
}

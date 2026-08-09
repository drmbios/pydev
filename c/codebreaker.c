#include "common.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned random_value(void) {
    unsigned value;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0 && read(fd, &value, sizeof value) == (ssize_t)sizeof value) {
        close(fd); return value;
    }
    if (fd >= 0) close(fd);
    return (unsigned)time(NULL) ^ (unsigned)getpid();
}

static void make_code(char code[5]) {
    char digits[] = "0123456789";
    size_t i;
    unsigned state = random_value();
    for (i = 0; i < 4; ++i) {
        size_t pick;
        state = state * 1103515245U + 12345U;
        pick = i + state % (10U - i);
        { char tmp = digits[i]; digits[i] = digits[pick]; digits[pick] = tmp; }
        code[i] = digits[i];
    }
    code[4] = '\0';
}

static int valid_guess(const char *guess) {
    size_t i;
    if (strlen(guess) != 4) return 0;
    for (i = 0; i < 4; ++i) if (guess[i] < '0' || guess[i] > '9') return 0;
    return 1;
}

int main(int argc, char **argv) {
    char code[5], input[64];
    long attempts = 8, used = 0;
    if (argc > 2 || (argc == 2 && parse_long(argv[1], 1, 100, &attempts))) {
        fprintf(stderr, "usage: %s [ATTEMPTS:1-100]\n", argv[0]); return 2;
    }
    make_code(code);
    while (used < attempts) {
        int exact = 0, common = 0, seen[10] = {0};
        size_t i;
        fputs("Guess the 4-digit code: ", stdout); fflush(stdout);
        if (!fgets(input, sizeof input, stdin)) { fputs("input ended\n", stderr); return 1; }
        if (!strchr(input, '\n') && !feof(stdin)) {
            int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
            puts("Input too long."); continue;
        }
        input[strcspn(input, "\r\n")] = '\0';
        if (!valid_guess(input)) { puts("Enter exactly four digits."); continue; }
        used++;
        if (!strcmp(input, code)) { printf("Unlocked in %ld attempt(s)!\n", used); return 0; }
        for (i = 0; i < 4; ++i) {
            if (input[i] == code[i]) exact++;
            if (!seen[input[i] - '0']) {
                int a = 0, b = 0; size_t j;
                seen[input[i] - '0'] = 1;
                for (j = 0; j < 4; ++j) { a += input[j] == input[i]; b += code[j] == input[i]; }
                common += a < b ? a : b;
            }
        }
        printf("%d exact, %d misplaced; %ld left.\n", exact, common - exact, attempts - used);
    }
    printf("Game over. The code was %s.\n", code);
    return 1;
}

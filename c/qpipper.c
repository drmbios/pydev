#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char **args;
    char *install_args[6];
    char *search_args[7];
    if (argc != 3 || (strcmp(argv[1], "install") && strcmp(argv[1], "search"))) {
        fprintf(stderr, "usage: %s {search|install} PACKAGE\n", argv[0]); return 2;
    }
    if (strlen(argv[2]) == 0 || strlen(argv[2]) > 512) {
        fputs("invalid package specification length\n", stderr); return 2;
    }
    install_args[0] = "python3"; install_args[1] = "-m"; install_args[2] = "pip";
    install_args[3] = "install"; install_args[4] = argv[2]; install_args[5] = NULL;
    search_args[0] = "python3"; search_args[1] = "-m"; search_args[2] = "pip";
    search_args[3] = "index"; search_args[4] = "versions"; search_args[5] = argv[2]; search_args[6] = NULL;
    args = !strcmp(argv[1], "search") ? search_args : install_args;
    execvp("python3", args);
    perror("python3");
    return 127;
}

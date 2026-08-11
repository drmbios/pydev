#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TEXT_SIZE (1024U * 1024U)

int main(int argc, char **argv) {
    size_t length;
    size_t written = 0;
    int descriptor;
    int saved_error = 0;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;

    if (argc != 3) {
        fprintf(stderr, "usage: %s FILE TEXT\n", argv[0]);
        return 2;
    }
    length = strlen(argv[2]);
    if (length > MAX_TEXT_SIZE) {
        fputs("write2file: text exceeds 1 MiB limit\n", stderr);
        return 2;
    }
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    descriptor = open(argv[1], flags, 0666);
    if (descriptor < 0) {
        perror(argv[1]);
        return 1;
    }
    (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    while (written < length) {
        ssize_t count = write(descriptor, argv[2] + written, length - written);
        if (count > 0) written += (size_t)count;
        else if (count < 0 && errno == EINTR) continue;
        else {
            saved_error = count < 0 ? errno : EIO;
            break;
        }
    }
    if (close(descriptor) != 0 && !saved_error) saved_error = errno;
    if (saved_error) {
        fprintf(stderr, "%s: write failed: %s\n", argv[1], strerror(saved_error));
        return 1;
    }
    printf("saved %zu bytes to %s\n", length, argv[1]);
    return 0;
}

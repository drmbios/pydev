#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "pqcommon.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int save(int dir, const char *name, const unsigned char *data, size_t length) {
    int fd = openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    size_t at = 0;
    if (fd < 0) return -1;
    while (at < length) {
        ssize_t n = write(fd,data+at,length-at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return -1; }
        at += (size_t)n;
    }
    if (fsync(fd)) { close(fd); return -1; }
    return close(fd);
}
int main(int argc, char **argv) {
    unsigned char data[PQ_OUTPUT]; size_t n = 0;
    int dir = -1, key = -1, result = 2;
    struct stat info;
    char *gen[] = {"openssl","genpkey","-provider","default","-algorithm",NULL,NULL};
    char *pub[] = {"openssl","pkey","-provider","default","-pubout",NULL};
    if (argc != 3 || !pq_algorithm(argv[1])) {
        fputs("usage: pqkey {ML-KEM-768|ML-KEM-1024|ML-DSA-65|ML-DSA-87} NEW_DIRECTORY\n",stderr); return 2;
    }
    gen[5] = argv[1];
    umask(077);
    if (mkdir(argv[2],0700)) { perror("new key directory"); goto done; }
    dir = open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (dir < 0 || fstat(dir,&info) || info.st_uid != geteuid() || (info.st_mode & 077U)) goto done;
    if (pq_run(gen,-1,data,&n) || !n || save(dir,"private.pem",data,n)) goto done;
    pq_clear(data,sizeof data);
    key = openat(dir,"private.pem",O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if (key < 0 || fstat(key,&info) || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        info.st_size > PQ_OUTPUT || pq_run(pub,key,data,&n) || !n || save(dir,"public.pem",data,n)) goto done;
    puts("Created private.pem (UNENCRYPTED, mode 0600) and public.pem. Protect and back up the private key.");
    result = ferror(stdout) ? 1 : 0;
done:
    pq_clear(data,sizeof data);
    if (key >= 0) close(key);
    if (dir >= 0) close(dir);
    if (result) fputs("Key creation incomplete; any newly created directory/files were retained for inspection. No existing files overwritten.\n",stderr);
    return result;
}

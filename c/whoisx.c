#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define RESPONSE_LIMIT (4U * 1024U * 1024U)

static int connect_server(const char *server) {
    struct addrinfo hints, *addresses = NULL, *address;
    int descriptor = -1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(server, "43", &hints, &addresses) != 0) return -1;
    for (address = addresses; address; address = address->ai_next) {
        struct timeval timeout = {10, 0};
        int original_flags;
        descriptor = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (descriptor == -1) continue;
        (void)setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        (void)setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
        original_flags = fcntl(descriptor, F_GETFL, 0);
        if (original_flags != -1 && fcntl(descriptor, F_SETFL, original_flags | O_NONBLOCK) == 0) {
            int result = connect(descriptor, address->ai_addr, address->ai_addrlen);
            if (result == 0) {
                if (fcntl(descriptor, F_SETFL, original_flags) == 0) break;
            }
            if (errno == EINPROGRESS) {
                fd_set writable;
                int socket_error = 0;
                socklen_t error_size = sizeof socket_error;
                FD_ZERO(&writable);
                FD_SET(descriptor, &writable);
                result = select(descriptor + 1, NULL, &writable, NULL, &timeout);
                if (result > 0 && getsockopt(descriptor, SOL_SOCKET, SO_ERROR,
                                             &socket_error, &error_size) == 0 &&
                    socket_error == 0) {
                    if (fcntl(descriptor, F_SETFL, original_flags) == 0) break;
                }
            }
        }
        (void)close(descriptor); descriptor = -1;
    }
    freeaddrinfo(addresses);
    return descriptor;
}

static int valid_query(const char *query) {
    size_t length = strlen(query), index;
    if (length == 0U || length > 253U) return 0;
    for (index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)query[index];
        if (!((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
              (byte >= '0' && byte <= '9') || byte == '.' || byte == '-' || byte == ':')) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *server = "whois.iana.org";
    const char *query;
    int descriptor;
    char request[300], buffer[4096];
    size_t sent = 0, request_length, received_total = 0;
    if (argc == 4 && strcmp(argv[1], "--server") == 0) { server = argv[2]; query = argv[3]; }
    else if (argc == 2) query = argv[1];
    else { fprintf(stderr, "usage: %s [--server HOST] DOMAIN_OR_IP\n", argv[0]); return 2; }
    if (!valid_query(query) || !valid_query(server)) { fputs("whoisx: invalid query or server\n", stderr); return 2; }
    (void)signal(SIGPIPE, SIG_IGN);
    descriptor = connect_server(server);
    if (descriptor == -1) { fprintf(stderr, "whoisx: cannot connect to %s\n", server); return 1; }
    request_length = (size_t)snprintf(request, sizeof request, "%s\r\n", query);
    while (sent < request_length) {
        ssize_t amount = send(descriptor, request + sent, request_length - sent, 0);
        if (amount <= 0) { (void)close(descriptor); return 1; }
        sent += (size_t)amount;
    }
    for (;;) {
        ssize_t amount = recv(descriptor, buffer, sizeof buffer, 0);
        size_t index;
        if (amount == 0) break;
        if (amount < 0) { if (errno == EINTR) continue; (void)close(descriptor); return 1; }
        if (received_total + (size_t)amount > RESPONSE_LIMIT) { fputs("whoisx: response limit exceeded\n", stderr); (void)close(descriptor); return 1; }
        received_total += (size_t)amount;
        for (index = 0; index < (size_t)amount; ++index) {
            unsigned char byte = (unsigned char)buffer[index];
            putchar(byte == '\n' || byte == '\r' || byte == '\t' || (byte >= 32U && byte <= 126U) ? (int)byte : '?');
        }
    }
    return close(descriptor) == 0 ? 0 : 1;
}

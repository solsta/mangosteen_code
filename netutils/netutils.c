//
// Created by se00598 on 10/10/23.
//

#include "netutils.h"
#include <netinet/in.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;

    fcntl(fd, F_SETFL, new_option);
}
#define DO_SO_REUSEPORT 1

int open_listenfd(uint16_t port) {
    printf("Opening listening socket\n");
    struct sockaddr_in sevr_addr;

    sevr_addr.sin_family = AF_INET;
    sevr_addr.sin_port = htons(port);
    sevr_addr.sin_addr.s_addr = INADDR_ANY;

    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) {
        fprintf(stderr, "Error while opening listenfd\n");
        return -1;
    }

#if DO_SO_REUSEPORT
    int optval = 1;
    /*
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Error while setting socket option.");
        return -1;
    }
    */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) != 0) {
        perror("Error while setting socket option.");
        return -1;
    }
#endif

    if (bind(listenfd, (struct sockaddr*)&sevr_addr, sizeof(sevr_addr)) != 0) {
        perror("Error while binding listenfd to address\n");
        return -1;
    }
    if (listen(listenfd, MAX_CONN) < 0) {
        fprintf(stderr, "Error while listening on listenfd\n");
        return -1;
    }

    return listenfd;
}


#ifndef RDMA_EXAMPLE_SERVER_H
#define RDMA_EXAMPLE_SERVER_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define error(msg, args...) do { \
    fprintf(stderr, "%s : %d : ERROR : " msg, __FILE__, __LINE__, ## args);\
}while(0);

#define info(msg, args...) do { \
    fprintf(stdout, "%s : %d : ERROR : " msg, __FILE__, __LINE__, ## args);\
}while(0);

int get_addr(char *dst, struct sockaddr *addr)
{
    struct addrinfo *res;
    int ret = -1;
    ret = getaddrinfo(dst, NULL, NULL, &res);
    if (ret) {
        error("getaddrinfo failed - invalid hostname or IP address\n");
        return ret;
    }
    memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
    return ret;
}

#endif //RDMA_EXAMPLE_SERVER_H

#include "client.h"
#include "../s_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr_temp(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int server_socket, numbytes;
    char buf[WORD_BUF_SIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype,
                                    p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr_temp((struct sockaddr *) p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    char username[WORD_BUF_SIZE] = "Bueno";
    send(server_socket, username,WORD_BUF_SIZE, 0);
    unsigned char s_header_flag;
    recv(server_socket, &s_header_flag, sizeof(s_header_flag), 0);
    if(s_header_flag == INVALID_USERNAME) {
        fprintf(stderr, "Invalid username! Terminating...");
        exit(1);
    } else {
        recv(server_socket, buf, sizeof(buf), 0);
        printf("%s", buf);
    }

//    while(1) {
//        if ((numbytes = recv(server_socket, buf, WORD_BUF_SIZE - 1, 0)) == -1) {
//            perror("recv");
//            exit(1);
//        }
//
//        buf[numbytes] = '\0';
//
//        printf("client: received '%s'\n", buf);
//    }
    sleep(10);
    close(server_socket);

    return 0;
}
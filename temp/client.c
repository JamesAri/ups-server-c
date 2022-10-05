#include "client.h"
#include "../utils/sock_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

void print_buf_to_hex(void *buffer, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (i > 0) printf(":");
        printf("%02X", ((char *) buffer)[i]);
    }
    printf("\n");
}


struct Buffer *new_buffer() {
    struct Buffer *b = malloc(sizeof(struct Buffer));

    b->data = malloc(10);
    b->size = 10;
    b->next = 0;

    return b;
}

void reserve_space(struct Buffer *b, int bytes) {
    if ((b->next + bytes) > b->size) {
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void free_buffer(struct Buffer **buffer) {
    free((*buffer)->data);
    free((*buffer));
    (*buffer) = NULL;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr_temp(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
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

    freeaddrinfo(servinfo);

    struct SocketHeader *socket_header = malloc(sizeof(struct SocketHeader));
    struct Buffer *buffer = new_buffer();
    int temp;

    temp = sizeof(struct SocketHeader);
    reserve_space(buffer, temp);
    socket_header->flag = INPUT_USERNAME;
    memcpy(buffer->data, socket_header, temp);
    buffer->next += temp;

    temp = sizeof(int);
    reserve_space(buffer, temp);
    int htonl_str_len = htonl(strlen(argv[1]));
    memcpy(buffer->data + buffer->next, &htonl_str_len, temp);
    buffer->next += temp;

    temp = strlen(argv[1]);
    reserve_space(buffer, temp);
    memcpy(buffer->data + buffer->next, argv[1], temp);
    buffer->next += temp;

    print_buf_to_hex(buffer->data, buffer->next);

    // send username...
    send(server_socket, buffer->data, buffer->next, 0);

    memset(buffer->data, 0, buffer->size);
    buffer->next = 0;

    reserve_space(buffer, sizeof(struct SocketHeader));
    socket_header->flag = CHAT;
    memcpy(buffer->data, socket_header, sizeof(struct SocketHeader));
    buffer->size += sizeof(struct SocketHeader);

    char send_buffer[WORD_BUF_SIZE], recv_buffer[WORD_BUF_SIZE];
    int cpid = fork();
    if (cpid == 0) {
        // child
        while (1) {
            bzero(&send_buffer, sizeof(send_buffer));
            fgets(send_buffer, 10000, stdin);
            int str_len = strlen(send_buffer);
            int htonl_str_len = htonl(str_len);

            reserve_space(buffer, sizeof(int));
            memcpy(buffer->data + buffer->size, &htonl_str_len, sizeof(int));
            buffer->size += sizeof(int);

            reserve_space(buffer, str_len);
            memcpy(buffer->data + buffer->size, send_buffer, str_len);
            buffer->size += str_len;

            send(server_socket, buffer->data, buffer->next, 0);
            memset(buffer->data, 0, buffer->size);
            buffer->next = 0;
        }
    } else {
        // parent
        while (1) {
            bzero(&recv_buffer, sizeof(recv_buffer));
            recv(server_socket, recv_buffer, sizeof(recv_buffer), 0);
            printf("SERVER : %s", recv_buffer);
        }
    }

    char username[WORD_BUF_SIZE] = "Bueno";
    send(server_socket, username, WORD_BUF_SIZE, 0);
    unsigned char s_header_flag;
    recv(server_socket, &s_header_flag, sizeof(s_header_flag), 0);
    if (s_header_flag == INVALID_USERNAME) {
        fprintf(stderr, "Invalid username! Terminating...");
        exit(1);
    } else {
        recv(server_socket, buf, sizeof(buf), 0);
        printf("%s", buf);
    }

    close(server_socket);
    return 0;
}
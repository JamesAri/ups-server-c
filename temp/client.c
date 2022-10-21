#include "client.h"
#include "../utils/serialization.h"
#include "../shared/sock_header.h"
#include "../utils/sock_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../utils/debug.h"
#include "../shared/definitions.h"

#include <arpa/inet.h>


int recv_buffer(int fd, struct Buffer *buffer, int size) {
    int res_recv, temp = size;
    reserve_space(buffer, size);
    res_recv = recvall(fd, buffer->data + buffer->next, &temp);
    buffer->next += size;
    return res_recv;
}

int recv_buffer_sock_header(int fd, struct Buffer *buffer) {
    return recv_buffer(fd, buffer, sizeof(struct SocketHeader));
}

int recv_buffer_int(int fd, struct Buffer *buffer) {
    return recv_buffer(fd, buffer, sizeof(int));
}

int recv_buffer_string(int fd, struct Buffer *buffer, int str_len) {
    return recv_buffer(fd, buffer, str_len);
}

int recv_buffer_time_t(int fd, struct Buffer *buffer) {
    return recv_buffer(fd, buffer, sizeof(time_t));
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr_temp(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int send_buffer(int dest_fd, struct Buffer *buffer) {
    int temp = buffer->next;
    return sendall(dest_fd, buffer->data, &temp);
}

int main(int argc, char *argv[]) {
    int server_socket;
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

    // send username
    char *username = argv[1];
    struct Buffer *out_buffer = new_buffer(), *in_buffer = new_buffer();
    serialize_sock_header(INPUT_USERNAME, out_buffer);
    serialize_int(strlen(username), out_buffer);
    serialize_string(username, out_buffer);

    send_buffer(server_socket, out_buffer);
    clear_buffer(out_buffer);
    fprintf(stderr, "<username: %s>\n\n", username);

    int server_msg_len, cur_players_count, min_players_count;
    int int_offset = sizeof(struct SocketHeader);
    int string_offset = int_offset + sizeof(int);
    time_t time_end;
    char chat_buffer[WORD_BUF_SIZE], server_response_buf[WORD_BUF_SIZE];
    int pid = fork();
    if (pid == 0) {
        while (1) {
            clear_buffer(out_buffer);
            memset(chat_buffer, 0, sizeof(chat_buffer));

            fgets(chat_buffer, 1000, stdin);
            chat_buffer[strcspn(chat_buffer, "\r\n")] = 0;

            serialize_sock_header(CHAT, out_buffer);
            serialize_int(strlen(chat_buffer), out_buffer);
            serialize_string(chat_buffer, out_buffer);
            int send_res = send_buffer(server_socket, out_buffer);
            if (send_res <= 0) {
                fprintf(stderr, "error, couldn't send data\n");
                exit(1);
            }
        }
    } else {
        while (1) {
            clear_buffer(in_buffer);
            memset(server_response_buf, 0, sizeof(server_response_buf));

            reserve_space(in_buffer, sizeof(struct SocketHeader));
            int recv_res;
            if ((recv_res = recv(server_socket, in_buffer->data, sizeof(struct SocketHeader), 0)) == 0) {
                free_buffer(&in_buffer);
                fprintf(stderr, "lost server connection - server down or not responding\n");
                exit(1);
            } else if (recv_res < 0) {
                perror("ERRNO(received < 0 value): ");
            }
            in_buffer->next += sizeof(struct SocketHeader);

            int recv_flag = ((struct SocketHeader *) in_buffer->data)->flag;

            switch (recv_flag) {
                case GAME_IN_PROGRESS:
                    fprintf(stdout, "received GAME_IN_PROGRESS flag\n");
                    recv_buffer_time_t(server_socket, in_buffer);
                    time_end = ntohll(*(time_t *) (in_buffer->data + int_offset));
                    fprintf(stdout, "game ends in %lu seconds\n", time_end - time(NULL));
                    break;
                case CANVAS:
                    fprintf(stdout, "received CANVAS flag\n");
                    break;
                case CHAT:
                    fprintf(stdout, "received CHAT flag\n");
                    recv_buffer_int(server_socket, in_buffer);
                    server_msg_len = ntohl(*(int *) (in_buffer->data + int_offset));
                    recv_buffer_string(server_socket, in_buffer, server_msg_len);
                    fprintf(stdout, "%s\n", (char *) (in_buffer->data + string_offset));
                    break;
                case START_AND_GUESS:
                    fprintf(stdout, "received START_AND_GUESS flag\n");
                    recv_buffer_time_t(server_socket, in_buffer);
                    time_end = ntohll(*(time_t *) (in_buffer->data + int_offset));
                    fprintf(stdout, "game starts in %lu seconds\n", time_end - time(NULL) - GAME_DURATION_SEC);
                    fprintf(stdout, "game ends in %lu seconds\n", time_end - time(NULL));
                    char temp2[1000];
                    buf_to_hex_string(in_buffer->data, in_buffer->next, temp2);
                    fprintf(stdout, "buffer: %s", temp2);
                    break;
                case START_AND_DRAW:
                    fprintf(stdout, "received START_AND_DRAW flag\n");
                    recv_buffer_int(server_socket, in_buffer);
                    server_msg_len = ntohl(*(int *) (in_buffer->data + int_offset));
                    recv_buffer_string(server_socket, in_buffer, server_msg_len);
                    recv_buffer_time_t(server_socket, in_buffer);
                    time_end = ntohll(*(time_t *) (in_buffer->data + string_offset + server_msg_len));
                    fprintf(stdout, "draw: %s\n", (char *) (in_buffer->data + string_offset));
                    fprintf(stdout, "game starts in %lu seconds\n", time_end - time(NULL) - GAME_DURATION_SEC);
                    fprintf(stdout, "game ends in %lu seconds\n", time_end - time(NULL));
                    break;
                case CORRECT_GUESS:
                    fprintf(stdout, "received CORRECT_GUESS flag\n");
                    break;
                case WRONG_GUESS:
                    fprintf(stdout, "received WRONG_GUESS flag\n");
                    break;
                case CORRECT_GUESS_ANNOUNCEMENT:
                    fprintf(stdout, "received CORRECT_GUESS_ANNOUNCEMENT flag\n");

                    recv_buffer_int(server_socket, in_buffer);

                    unpack_int(in_buffer, &server_msg_len);

                    recv_buffer_string(server_socket, in_buffer, server_msg_len);

                    fprintf(stdout, "%s\n", (char *) (in_buffer->data + string_offset));
                    break;
                case INVALID_USERNAME:
                    fprintf(stdout, "received INVALID_USERNAME flag\n");
                    break;
                case WAITING_FOR_PLAYERS:
                    fprintf(stdout, "received WAITING_FOR_PLAYERS flag\n");
                    recv_buffer_int(server_socket, in_buffer);
                    recv_buffer_int(server_socket, in_buffer);
                    unpack_int(in_buffer, &cur_players_count);
                    unpack_int_var(in_buffer, &min_players_count, STRING_OFFSET);
                    printf("%d/%d players, waiting for %d player(s)\n",
                           cur_players_count, min_players_count, min_players_count - cur_players_count);
                    break;
                case GAME_ENDS:
                    fprintf(stdout, "received GAME_ENDS flag\n");
                    break;
                default:
                    fprintf(stderr, "UNKNOWN SOCKET HEADER: received flag: %d\n", recv_flag);
            }
        }
    }

    close(server_socket);
    return 0;
}
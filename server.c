#include "server.h"
#include "model/word_generator.h"
#include "shared/sock_header.h"
#include "utils/sock_utils.h"
#include "utils/log.h"
#include "utils/stream_handlers.h"
#include "game.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <ctype.h>


// ======================================================================= //
//                                MISC                                     //
// ======================================================================= //

void add_to_lobby_pfds(int new_fd) {
    add_to_pfds(&(lobby.pfds), new_fd, &(lobby.fd_count), &(lobby.fd_size));
}

void disconnect_fd(int fd) {
    if (fd < 0) {
        log_error("trying to disconnect negative fd");
        return;
    }
    close(fd);
    del_from_pfds_by_fd(lobby.pfds, fd, &(lobby.fd_count));
}

void remove_player_from_server(struct Player *player) {
    if (player == NULL) {
        log_error("internal err: player is NULL");
        return;
    }
    log_debug("removing player %s (fd: %d) from game", player->username, player->fd);

    disconnect_fd(player->fd);

    player->is_online = false;
    player->fd = -1;

    if (player->game != NULL) {
        player->game->cur_capacity--;
        broadcast_player_list_change(player->game, player);
    }

}

int reassign_player(struct Player *player, int lobby_capacity, int game_capacity) {
    struct Game *game;

    // player had to be reassigned, because the game is now full or empty
    if (player->game != NULL) {
        remove_player(player->game->players, player->username);
        player->game = NULL;
        log_debug("reassigning player %s to another game", player->username);
    }

    // player has currently no game
    for (int i = 0; i < lobby_capacity; i++) {
        game = lobby.games[i];
        if (game->cur_capacity < game_capacity) {
            player->game = game;
            game->cur_capacity++;
            if (add_player(game->players, player) < 0) return -1;
            log_debug("reassigned player %s to a new game", player->username);
            return 0;
        }
    }
    log_warn("lobby is full, player %s has to wait", player->username);
    send_server_full(player->fd);
    player->is_online = false;
    return -1;
}


// ======================================================================= //
//                            LOGGING UTILS                                //
// ======================================================================= //

void log_sigterm() {
    log_info("terminating server: SIGTERM (%d) (┛ಠ_ಠ)┛彡┻━┻", SIGTERM);
    exit(SIGTERM);
}

void setup_signal_handling() {
    struct sigaction action_term;
    memset(&action_term, 0, sizeof(struct sigaction));
    action_term.sa_handler = log_sigterm;
    sigaction(SIGTERM, &action_term, NULL);
}

void log_game_start(time_t *start, time_t *end) {
    char s1[MAX_LOG_MSG_LEN], s2[MAX_LOG_MSG_LEN];
    memset(s1, 0, sizeof(s1));
    memset(s2, 0, sizeof(s2));
    strftime(s1, sizeof(s1), "%c", localtime(start));
    strftime(s2, sizeof(s2), "%c", localtime(end));
    log_trace("round starts at %s and ends at %s (%d sec rounds, %d sec counter)",
              s1, s2, GAME_DURATION_SEC, TIME_BEFORE_START_SEC);
}


// ======================================================================= //
//               SERVER LOGIC AND CLIENT (PLAYER) MANAGEMENT               //
// ======================================================================= //

// TODO allow client to ask for reassignment instead of auto-reassignment
//      when game empty/not in progress/full
// client is trying to LOG IN
int manage_logging_player(int new_fd, int lobby_capacity, int game_capacity) {
    struct Player *player;
    struct Buffer *buffer = new_buffer();
    int username_len, flag, res, res_update;
    char username[STD_STRING_BFR_LEN];

    if ((res = recv_buffer_sock_header(new_fd, buffer)) <= 0) {
        free_buffer(&buffer);
        log_warn("received incomplete data from socket %d", new_fd);
        return res;
    }

    flag = ((struct SocketHeader *) buffer->data)->flag;

    if (flag != LOGIN) {
        free_buffer(&buffer);
        log_warn("invalid register-username header (received flag: 0x%02X)", flag);
        return -1;
    }

    if ((res = recv_buffer_int(new_fd, buffer)) <= 0) return res;

    unpack_int(buffer, &username_len);


    // VALIDATING LENGTH
    if (username_len > MAX_USERNAME_LEN) {
        free_buffer(&buffer);
        log_warn("username too long (fd: %d)", new_fd);
        return -1;
    }

    if ((res = recv_buffer_string(new_fd, buffer, username_len)) <= 0) return res;

    strcpy(username, buffer->data + STRING_OFFSET);

    free_buffer(&buffer);

    // VALIDATING CHARACTERS
    for (int i = 0; i < strlen(username); i++) {
        if (isalpha(username[i]) || isdigit(username[i]) || username[i] == '_' || username[i] == '-')
            continue;
        else {
            log_warn("invalid characters in username: %s", username);
            return -1;
        }
    }

    res_update = update_players(lobby.all_players, username, new_fd);

    switch (res_update) {
        case PLAYER_ALREADY_ONLINE:
            send_invalid_username(new_fd);
            log_warn("invalid username, player %s already logged in", username);
            return -1;
        case PLAYER_CREATION_ERROR:
            send_server_error(new_fd);
            log_error("failed to create or update a player with username %s", username);
            return -1;
        case PLAYER_RECONNECTED:
            log_debug("player %s reconnected", username);
            player = get_player_by_fd(lobby.all_players, new_fd);
            if (player->game == NULL                            // no game assigned
                || player->game->cur_capacity >= game_capacity  // game is full
                || !player->game->in_progress) {                // player would be waiting for other players to join
                if (reassign_player(player, lobby_capacity, game_capacity) < 0) return -1;
            }
            send_ok(new_fd);
            break;
        case PLAYER_CREATED:
            player = get_player_by_fd(lobby.all_players, new_fd);
            if (reassign_player(player, lobby_capacity, game_capacity) < 0) return -1;
            send_ok(new_fd);
            break;
        default:
            return -1;
    }
    return res;
}

// client is trying to send CANVAS changes
int manage_drawing_player(struct Player *player, struct Buffer *buffer) {
    int drawing_fd = player->fd;
    int recv_res, number_of_diffs, temp;
    int serialized_canvas_offset = sizeof(struct SocketHeader) + sizeof(int); // header + num of diffs

    if (((struct SocketHeader *) buffer->data)->flag != CANVAS) {
        log_warn("received invalid header, drawing_fd: %d", drawing_fd);
        return -1;
    }

    if ((recv_res = recv_buffer_int(drawing_fd, buffer)) <= 0) {
        log_debug("couldn't receive canvas buffer size (fd: %d)", drawing_fd);
        return recv_res;
    }
    unpack_int(buffer, &number_of_diffs);
    if ((recv_res = recv_buffer(drawing_fd, buffer, number_of_diffs * (int) sizeof(int))) <= 0) {
        log_debug("couldn't receive canvas buffer (fd: %d)", drawing_fd);
        return recv_res;
    }

    for (int i = 0; i < number_of_diffs; i++) {
        unpack_int_var(buffer, &temp, serialized_canvas_offset + (int) sizeof(int) * i);
        if (temp < 0 || temp >= CANVAS_SIZE) {
            log_debug("received canvas index is out of range: %d (fd: %d)", temp, drawing_fd);
            return -1;
        }
        toggle_bit(player->game->canvas->bitarray_grid, temp);
    }

    return recv_res;
}

// TODO: dont allow chat if player sent correct guess (they might reconnect and guess again atm)
// client is trying to send a GUESS
int manage_guessing_player(struct Player *player, struct Buffer *buffer) {
    int guess_str_len, recv_res, sender_fd = player->fd;
    char *guess_word = player->game->guess_word;
    char buff_client_guess[STD_STRING_BFR_LEN + 1], broadcast_response_msg[STD_STRING_BFR_LEN];

    if (((struct SocketHeader *) buffer->data)->flag != CHAT) {
        log_warn("invalid header: guessing player err");
        return -1;
    }

    memset(broadcast_response_msg, 0, sizeof(broadcast_response_msg));
    memset(buff_client_guess, 0, sizeof(buff_client_guess));

    if ((recv_res = recv_buffer_int(sender_fd, buffer)) <= 0) return recv_res;

    guess_str_len = ntohl(*(int *) (buffer->data + INT_OFFSET));

    if (guess_str_len > MAX_GUESS_LEN) {
        log_warn("received large string buffer with user guess (fd: %d, %s)", player->fd, player->username);
        return -1;
    }

    if ((recv_res = recv_buffer_string(sender_fd, buffer, guess_str_len)) <= 0) return recv_res;

    strcpy(buff_client_guess, buffer->data + STRING_OFFSET);
    buff_client_guess[MAX_GUESS_LEN] = '\0';

    log_trace("client guess word: %s", buff_client_guess);

    int correct_guess = !strcmp(guess_word, buff_client_guess);

    if (correct_guess) {
        sprintf(broadcast_response_msg, "Player %s got the answer!", player->username);
        clear_buffer(buffer);
        serialize_his(CORRECT_GUESS_ANNOUNCEMENT, broadcast_response_msg, buffer);
    } else {
        sprintf(broadcast_response_msg, "%s: %s", player->username, buff_client_guess);
        clear_buffer(buffer);
        serialize_his(CHAT, broadcast_response_msg, buffer);
    }

    if (correct_guess) return send_correct_guess(sender_fd);
    else return send_wrong_guess(sender_fd);
}

int manage_player_action(struct Player *player) {
    struct Game *game = player->game;
    struct Buffer *buffer = new_buffer();
    int recv_res, sender_fd = player->fd;

    if ((recv_res = recv_buffer_sock_header(sender_fd, buffer)) <= 0) return recv_res;

    if (game->drawing_player_list->player->fd == sender_fd)
        recv_res = manage_drawing_player(player, buffer);
    else
        recv_res = manage_guessing_player(player, buffer);

    if (recv_res > 0)
        broadcast_buffer(game, sender_fd, buffer);
    else {
        log_warn("couldn't broadcast, because sender_fd %d is invalid", sender_fd);
    }

    free_buffer(&buffer);
    return recv_res;
}


// ======================================================================= //
//                          ROUNDS MANAGEMENT                              //
// ======================================================================= //

void start_round(struct Game *game) {
    game->in_progress = true;

    if (get_next_drawing_fd(game)) {
        log_fatal("internal err: there should be available players for drawing");
        free_lobby();
        exit(EXIT_FAILURE);
    }

    struct Player *drawing_player = game->drawing_player_list->player;
    int drawing_fd = drawing_player->fd;

    log_debug("next drawing player is %s (fd: %d)", drawing_player->username, drawing_fd);

    if (send_game_draw_start(drawing_fd, game->guess_word, game->end_sec) <= 0) {
        log_warn("drawing fd %d err, closing", drawing_fd);
        remove_player_from_server(drawing_player);
    }

    // we don't care if drawing player is offline, he might reconnect
    broadcast_game_guess_start(game, drawing_fd);

    log_debug("new round started");
}

// Client is trying to join game in one of these game states:
// WAITING FOR PLAYERS / ROUND STARTING
void validate_round(struct Game *game) {
    int connected_players = get_active_players(game);
    // TODO check if any other games are waiting - reassign players so they can play asap
    if (connected_players < MIN_PLAYERS) {
        log_trace("waiting for players to join (%d/%d), broadcasting", connected_players, MIN_PLAYERS);
        broadcast_waiting_for_players(game, connected_players);
    } else {
        get_random_word(game->guess_word);
        log_trace("generated new guess word: %s", game->guess_word); // segfault todo

        game->start_sec = time(NULL) + TIME_BEFORE_START_SEC;
        game->end_sec = game->start_sec + GAME_DURATION_SEC;

        start_round(game);

        log_game_start(&game->start_sec, &game->end_sec);
    }
}

void end_round(struct Game *game) {
    game->in_progress = false;

//    remove_offline_player_lists(game);

    broadcast_round_ends(game);

    memset(game->canvas->bitarray_grid, 0, sizeof(game->canvas->bitarray_grid));

    validate_round(game);

    log_debug("round ended");
}


// ======================================================================= //
//                              MAIN LOOP                                  //
// ======================================================================= //

void start(char *addr, char *port, int lobby_size, int game_size) {
    setup_signal_handling();

    int listener, timeout_sec, timeout_sec_new, new_fd, sender_fd, recv_res, poll_res;

    struct sockaddr_storage remote_addr; // Client address
    socklen_t addr_len;
    char remote_ip[INET6_ADDRSTRLEN];

    struct Player *cur_player;
    struct Game *game;

    if ((listener = get_listener_socket(addr, port, lobby_size * game_size)) == -1) {
        log_fatal("err getting listening socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    initialize_lobby(listener, lobby_size);

    add_to_lobby_pfds(listener);

    for (;;) {
        timeout_sec = GAME_DURATION_SEC;
        for (int i = 0; i < lobby_size; i++) {
            game = lobby.games[i];
            timeout_sec_new = (game->in_progress) ? (int) (game->end_sec - time(NULL)) : GAME_DURATION_SEC;
            timeout_sec = (timeout_sec_new < timeout_sec) ? timeout_sec_new : timeout_sec;
        }

        log_trace("timeout set to %d second(s)", timeout_sec);
        poll_res = poll(lobby.pfds, lobby.fd_count, timeout_sec * 1000);

        if (poll_res == -1) {
            log_fatal("poll-err: %s", strerror(errno));
            free_lobby();
            exit(EXIT_FAILURE);
        } else if (poll_res == 0) {
            log_trace("poll-timeout");
        }

        for (int i = 0; i < lobby_size; i++) {
            if (lobby.games[i]->in_progress && time(NULL) + SOCKOPT_TIMEOUT_SEC >= lobby.games[i]->end_sec)
                end_round(lobby.games[i]);
        }

        for (int i = 0; i < lobby.fd_count; i++) {

            if (!(lobby.pfds[i].revents & POLLIN)) continue;

            if (lobby.pfds[i].fd == listener) {

                addr_len = sizeof remote_addr;
                new_fd = accept(listener, (struct sockaddr *) &remote_addr, &addr_len);

                if (new_fd == -1) {
                    log_error("listener: accept err");
                } else {
                    log_debug("poll-server: new connection (fd: %d), processing...", new_fd);

                    if (set_socket_timeout(new_fd, SOCKOPT_TIMEOUT_SEC) < 0) {
                        log_error("setsockopt err: couldn't set timeout for socket with fd d%", new_fd);
                        close(new_fd);
                        continue;
                    }

                    if (manage_logging_player(new_fd, lobby_size, game_size) <= 0) {
                        log_warn("login err: closing connection for socket %d", new_fd);
                        close(new_fd);
                        continue;
                    }

                    if ((cur_player = get_player_by_fd(lobby.all_players, new_fd)) == NULL) {
                        log_error("internal err: created player missing in lobby list, server might not work properly");
                        disconnect_fd(new_fd);
                        continue;
                    }

                    add_to_lobby_pfds(cur_player->fd);

                    log_debug("poll-server: new connection from %s on socket %d (user: %s) was successful",
                              inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr *) &remote_addr),
                                        remote_ip, sizeof(remote_ip)),
                              new_fd, cur_player->username);

                    log_debug("poll-server: %d connected client(s)", lobby.fd_count - 1); // -1 for listener

                    if (send_player_list(cur_player->fd, cur_player->game) <= 0) {
                        log_warn("unable to send player list to fd %d (%s)", cur_player->fd, cur_player->username);
                        remove_player_from_server(cur_player);
                        continue;
                    }

                    broadcast_player_list_change(cur_player->game, cur_player);

                    if (cur_player->game->in_progress) {
                        log_trace("fd %d joining game in progress", cur_player->fd);
                        if (send_game_in_progress(cur_player) <= 0) {
                            log_warn("fd %d (%s) unable to join current round, closing", cur_player->fd,
                                     cur_player->username);
                            remove_player_from_server(cur_player);
                        }
                    } else {
                        validate_round(cur_player->game);
                    }
                }
            } else {
                sender_fd = lobby.pfds[i].fd;

                // Unknown player
                if ((cur_player = get_player_by_fd(lobby.all_players, sender_fd)) == NULL) {
                    log_error("internal err: unknown player connected - disconnecting malicious client");
                    disconnect_fd(sender_fd);
                    continue;
                }

                // No game in progress or client not waiting for start
                if (!(cur_player->game->in_progress) || time(NULL) < cur_player->game->start_sec) {
                    log_debug("socket %d (%s) trying to communicate outside the game loop, closing",
                              sender_fd, cur_player->username);
                    remove_player_from_server(cur_player);
                    continue;
                }

                recv_res = manage_player_action(cur_player);

                // Known player error
                if (recv_res <= 0) {
                    if (recv_res == 0)
                        log_debug("poll-server: socket %d hung up (user: %s)", sender_fd, cur_player->username);
                    else
                        log_warn("recv err (%d): socket: %d, user: %s, closing", recv_res, sender_fd,
                                 cur_player->username);
                    remove_player_from_server(cur_player);
                }
            }
        }
    }
}
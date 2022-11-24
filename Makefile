CC = gcc
CFLAGS = -Wall -ansi -g
BIN = tcp_server
OBJ = model/word_generator.c server.c model/player.c shared/sock_header.c utils/sock_utils.c utils/debug.c utils/serialization.c utils/log.c model/canvas.c game.c lobby.c utils/stream_handlers.c main.c 

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(BIN): $(OBJ)
	$(CC) $^ -o $@

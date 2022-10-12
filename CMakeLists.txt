cmake_minimum_required(VERSION 3.23)
project(ups_server_c C)

set(CMAKE_C_STANDARD 99)

add_executable(ups_server_c main.c model/word_generator.c model/word_generator.h server.c server.h model/player.c model/player.h utils/sock_header.c utils/sock_header.h utils/sock_utils.c utils/sock_utils.h utils/debug.c utils/debug.h utils/serialization.c utils/serialization.h utils/log.c utils/log.h definitions.h)

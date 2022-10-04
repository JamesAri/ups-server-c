cmake_minimum_required(VERSION 3.23)
project(ups_server_c C)

set(CMAKE_C_STANDARD 99)

add_executable(ups_server_c main.c word_generator.c word_generator.h server.c server.h player.c player.h s_header.c s_header.h ser_buffer.c ser_buffer.h socket_utils.c socket_utils.h)

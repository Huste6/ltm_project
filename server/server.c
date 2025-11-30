#include "server.h"
#include "auth/auth.h"
// #include "room/room.h"
// #include "exam/exam.h"
// #include "practice/practice.h"
// #include "logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// global server instance
Server *g_server = NULL;

/**
 * @brief Initialize the server
 */
// int server_init(Server *server, int port) {
//     g_server = server;
//     memset(server, 0, sizeof(Server));

//     // 
// }
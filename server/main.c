#include "server.h"
#include <unistd.h> 

// main function
int main(){
    Server server;
    
    printf("===========================================\n");
    printf("   ONLINE EXAM SYSTEM SERVER\n");
    printf("===========================================\n\n");

    if (server_init(&server, SERVER_PORT) != 0) {
        fprintf(stderr, "Server initialization failed. Exiting.\n");
        return 1;
    }

    server_start(&server);

    // cleanup
    if (server.db) {
        db_disconnect(server.db);
        free(server.db);
        server.db = NULL;
    }
    close(server.server_fd);
    logger_close();

    printf("\nServer shut down cleanly\n");
    log_event(LOG_INFO, NULL, "SERVER", "Server shut down cleanly");
    return 0;
}

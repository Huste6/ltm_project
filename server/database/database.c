#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int db_connect(Database *db, const char *host, const char *user, const char *password, const char *dbname) {
    return db_connect_with_port(db, host, user, password, dbname, 3306);
}

int db_connect_with_port(Database *db, const char *host, const char *user, const char *password, const char *dbname, unsigned int port) {
    db->conn = mysql_init(NULL);
    if (db->conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return -1;
    }

    // Kết nối với port cụ thể
    if (mysql_real_connect(db->conn, host, user, password, dbname, port, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        return -1;
    }

    if (pthread_mutex_init(&db->mutex, NULL) != 0) {
        fprintf(stderr, "Mutex init failed\n");
        mysql_close(db->conn);
        return -1;
    }

    return 0;
}

void db_disconnect(Database *db) {
    pthread_mutex_destroy(&db->mutex);
    mysql_close(db->conn);
}

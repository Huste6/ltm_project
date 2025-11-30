#ifndef DATABASE_H
#define DATABASE_H

#include <mysql/mysql.h>
#include <pthread.h>

typedef struct {
    MYSQL *conn;
    pthread_mutex_t mutex;
} Database;

// connect to the database
int db_connect(Database *db, const char *host, const char *user, const char *password, const char *dbname);
// connect to the database with specific port
int db_connect_with_port(Database *db, const char *host, const char *user, const char *password, const char *dbname, unsigned int port);
// disconnect from the database
void db_disconnect(Database *db);

#endif // DATABASE_H
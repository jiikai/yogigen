#ifndef __yogidb_h__
#define __yogidb_h__

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/random.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <signal.h>
#include <libpq-fe.h>
#include "./dependencies/bstrlib.h"

#define MAX_CONN_NUMBER 20
#define CONN_TIMEOUT 10

#define EXPR_SQL_SELECT_TEMPLATE "SELECT id-%d as id, field_1 AS field_1, field_2 AS field_2, type AS type, flags AS flags FROM Expression WHERE type=%d ORDER BY flags ASC;"

#define FRMT_SQL_SELECT_TEMPLATE "SELECT id as id, str AS str, data AS data, count AS count FROM Format_String;"

#define GENS_SQL_INSERT_TEMPLATE "INSERT INTO Generated (id, phrase) VALUES (X'%lx'::bigint, '%s');"

#define GENS_SQL_SELECT_TEMPLATE "SELECT phrase FROM Generated WHERE id=X'%s'::bigint;"

#define SQL_EXPR_COUNT_PG "SELECT count(*) FILTER (WHERE type=0) AS verb_count, count(*) FILTER (WHERE type=1) AS a_count, count(*) FILTER (WHERE type=2) AS c_count, count(*) FILTER (WHERE type=3) AS o_count FROM Expression;"

#define SQL_FRMT_COUNT_PG "SELECT count(*) AS f_count FROM Format_String;"

typedef struct connection {
    PGconn *db;
    bstring conn_info;
    volatile uint_fast8_t conn_count;
    uint8_t max_conn;
    uint8_t cur_max;
} Connection;

typedef void (*handle_result) (void *a, PGresult *res_set);

Connection *open_conn();
void close_conn(Connection *conn);
int postgres_select_cycle(PGconn *db, char* query, handle_result function, void *arg);
PGresult *postgres_select_concurrent(Connection *conn, char* query);
int postgres_insert_concurrent(Connection *conn, char* query);

#endif
